    // Included by PFFDatabase.cpp. The compiler uses only declarations and
    // records from the selected packages, together with Core's SQLite helpers.
    class AddOnTags
    {
    public:
        AddOnTags(const std::filesystem::path& file, const std::string& bytes, bool sourceHeader = false)
            : file_(file)
        {
            std::istringstream input(bytes);
            std::string line;
            bool instructions = false;
            std::size_t number = 0;
            while (std::getline(input, line))
            {
                ++number;
                if (number == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
                    line.erase(0, 3);
                line = trim(line);
                if (instructions)
                {
                    instructions = line != "[INSTRUCTIONS_END]";
                    continue;
                }
                if (line.empty() || line.front() == '#' || line.rfind("//", 0) == 0)
                    continue;
                const auto close = line.find(']');
                if (line.front() != '[' || close == std::string::npos ||
                    line.find('\0') != std::string::npos)
                    throw BuildException("Invalid add-on tag.", file_, number);
                tags_.push_back({line.substr(1, close - 1), trim(line.substr(close + 1)), number});
                if (tags_.back().tag == "SCHEMA_BEGIN" || tags_.back().tag == "RECORDS_BEGIN")
                    sourceHeader = false;
                if (sourceHeader && tags_.back().tag == "INSTRUCTIONS_BEGIN" && tags_.back().value.empty())
                    instructions = true;
            }
            if (instructions)
                throw BuildException("Unclosed instructions block.", file_, number);
        }

        bool at(const std::string& tag) const
        {
            return index_ < tags_.size() && tags_[index_].tag == tag;
        }
        bool done() const { return index_ == tags_.size(); }
        [[noreturn]] void fail(const std::string& message) const
        {
            const auto line = done() ? (tags_.empty() ? 0 : tags_.back().line) : tags_[index_].line;
            throw BuildException(message, file_, line);
        }
        TagLine next()
        {
            if (done()) fail("Unexpected end of add-on source.");
            return tags_[index_++];
        }
        std::string value(const std::string& tag)
        {
            if (!at(tag) || tags_[index_].value.empty()) fail("Expected a value for [" + tag + "].");
            return next().value;
        }
        void marker(const std::string& tag)
        {
            if (!at(tag) || !tags_[index_].value.empty()) fail("Expected [" + tag + "].");
            next();
        }

    private:
        std::filesystem::path file_;
        std::vector<TagLine> tags_;
        std::size_t index_ = 0;
    };

    struct CoreAddOn
    {
        struct Compatibility
        {
            std::string seasonKey, rulesetKey, rulesetVersion;
        };
        std::filesystem::path manifest;
        std::string key, version, displayName, revision, provenance, rights;
        std::vector<Compatibility> compatibility;
        std::vector<std::string> features;
        std::map<std::string, std::string> dependencies;
        std::vector<std::filesystem::path> files;
    };

    using CoreSourceBytes = std::vector<std::pair<std::filesystem::path, std::string>>;

    CoreAddOn readCoreAddOn(const std::filesystem::path& file, const std::string& bytes)
    {
        AddOnTags tags(file, bytes);
        CoreAddOn addOn;
        addOn.manifest = file;
        tags.marker("ADDON_BEGIN");
        if (tags.value("MANIFEST_VERSION") != "0.1")
            tags.fail("Unsupported add-on manifest version.");
        addOn.key = tags.value("PACKAGE_KEY");
        addOn.version = tags.value("PACKAGE_VERSION");
        addOn.displayName = tags.value("DISPLAY_NAME");
        if (tags.value("PACKAGE_TYPE") != "ADDON")
            tags.fail("Expected ADDON package type.");
        addOn.revision = tags.value("SOURCE_FORMAT_REVISION");
        if (addOn.revision != "2")
            tags.fail("Unsupported schema-declared source format revision.");
        addOn.provenance = tags.value("PROVENANCE");
        addOn.rights = tags.value("RIGHTS");

        while (tags.at("COMPATIBILITY_BEGIN"))
        {
            tags.marker("COMPATIBILITY_BEGIN");
            addOn.compatibility.push_back({tags.value("SEASON_PACKAGE_KEY"),
                tags.value("RULESET_PACKAGE_KEY"), tags.value("RULESET_PACKAGE_VERSION")});
            tags.marker("COMPATIBILITY_END");
        }
        if (addOn.compatibility.empty()) tags.fail("Add-on requires a compatibility declaration.");
        while (tags.at("REQUIRED_FEATURE") || tags.at("DEPENDENCY"))
        {
            if (tags.at("REQUIRED_FEATURE"))
            {
                addOn.features.push_back(tags.value("REQUIRED_FEATURE"));
                continue;
            }
            std::istringstream dependency(tags.value("DEPENDENCY"));
            std::string key, version, extra;
            if (!(dependency >> key >> version) || (dependency >> extra) ||
                !addOn.dependencies.emplace(key, version).second)
                tags.fail("Expected a unique dependency package key and exact version.");
        }
        tags.marker("FILES_BEGIN");
        while (tags.at("FILE"))
        {
            const auto name = tags.value("FILE");
            const auto relative = std::filesystem::u8path(name);
            if (relative.has_root_path() || relative.extension() != ".pffdata" ||
                name.find(':') != std::string::npos)
                tags.fail("Expected a relative .pffdata path.");
            for (const auto& part : relative)
                if (part == "." || part == "..") tags.fail("Unsafe add-on source path.");
            const auto root = file.parent_path();
            const auto path = std::filesystem::canonical(root / relative);
            if (!CoreManifest::inside(root, path) || !std::filesystem::is_regular_file(path))
                tags.fail("Add-on source must be a file inside its package directory.");
            addOn.files.push_back(path);
        }
        if (addOn.files.empty()) tags.fail("Add-on requires at least one source file.");
        tags.marker("FILES_END");
        tags.marker("ADDON_END");
        if (!tags.done()) tags.fail("Unexpected content after add-on manifest.");
        return addOn;
    }

    std::vector<CoreAddOn> resolveCoreAddOns(const CoreDatabaseBuildRequest& request,
        const std::vector<CoreManifest::Package>& base, CoreSourceBytes& sources)
    {
        if (request.addOnManifests.empty()) return {};
        if (base.size() != 2) throw BuildException("Add-ons require ruleset and season manifests.");

        const auto sourceRoot = std::filesystem::canonical(request.sourceRoot);
        const auto addOnRoot = std::filesystem::canonical(sourceRoot / "AddOns");
        if (!CoreManifest::inside(sourceRoot, addOnRoot) || addOnRoot == sourceRoot)
            throw BuildException("AddOns directory must be inside the source root.", addOnRoot);
        std::set<std::filesystem::path> usedPaths;
        for (const auto& source : sources) usedPaths.insert(source.first);
        const auto capture = [&](const std::filesystem::path& path) -> const std::string& {
            if (!usedPaths.insert(path).second)
                throw BuildException("Source was selected more than once.", path);
            sources.emplace_back(path, CoreManifest::bytes(path));
            return sources.back().second;
        };
        std::map<std::string, std::string> versions{
            {base[0].key, base[0].version}, {base[1].key, base[1].version}};
        std::vector<CoreAddOn> pending;
        for (const auto& selected : request.addOnManifests)
        {
            const auto path = std::filesystem::canonical(selected);
            if (path.extension() != ".pffaddon" || !CoreManifest::inside(addOnRoot, path))
                throw BuildException("Expected a .pffaddon manifest inside AddOns.", path);
            auto addOn = readCoreAddOn(path, capture(path));
            if (!versions.emplace(addOn.key, addOn.version).second)
                throw BuildException("Duplicate package identity: " + addOn.key, path);
            const auto matches = [](const std::string& allowed, const std::string& actual) {
                return allowed == "ANY" || allowed == actual;
            };
            const auto compatible = std::any_of(addOn.compatibility.begin(), addOn.compatibility.end(),
                [&](const auto& rule) {
                    return matches(rule.seasonKey, base[1].key) && matches(rule.rulesetKey, base[0].key) &&
                        matches(rule.rulesetVersion, base[0].version);
                });
            if (!compatible) throw BuildException("Add-on is incompatible with this season and ruleset.", path);
            for (const auto& feature : addOn.features)
                if (std::find(request.supportedAddOnFeatures.begin(), request.supportedAddOnFeatures.end(),
                    feature) == request.supportedAddOnFeatures.end())
                    throw BuildException("Unsupported add-on engine feature: " + feature, path);
            for (const auto& file : addOn.files) capture(file);
            pending.push_back(std::move(addOn));
        }

        for (const auto& addOn : pending)
            for (const auto& dependency : addOn.dependencies)
            {
                const auto version = versions.find(dependency.first);
                if (version == versions.end() || version->second != dependency.second)
                    throw BuildException("Missing or incompatible add-on dependency: " + dependency.first,
                        addOn.manifest);
            }
        std::set<std::string> resolved{base[0].key, base[1].key};
        std::vector<CoreAddOn> ordered;
        while (!pending.empty())
        {
            const auto ready = std::find_if(pending.begin(), pending.end(), [&](const auto& addOn) {
                return std::all_of(addOn.dependencies.begin(), addOn.dependencies.end(),
                    [&](const auto& dependency) { return resolved.count(dependency.first) != 0; });
            });
            if (ready == pending.end())
                throw BuildException("Circular add-on dependency.", pending.front().manifest);
            resolved.insert(ready->key);
            ordered.push_back(std::move(*ready));
            pending.erase(ready);
        }
        return ordered;
    }

    struct AddOnField
    {
        std::string name, type;
        bool required = false;
        std::optional<sqlite3_int64> minimum, maximum;
        std::vector<std::string> allowed;
    };

    struct AddOnForeignKey
    {
        std::vector<std::string> fields;
        std::string target;
        std::vector<std::string> targetFields;
    };

    struct AddOnTable
    {
        std::string name;
        std::filesystem::path source;
        std::vector<AddOnField> fields;
        std::vector<std::string> primary;
        std::vector<std::vector<std::string>> unique;
        std::vector<AddOnForeignKey> foreign;
        std::optional<sqlite3_int64> expectedRecords;
    };

    sqlite3_int64 addOnInteger(const std::string& value)
    {
        sqlite3_int64 result = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size())
            throw BuildException("Invalid INTEGER value: " + value);
        return result;
    }

    std::vector<std::string> addOnList(const std::string& value)
    {
        std::vector<std::string> items;
        std::size_t start = 0;
        for (;;)
        {
            const auto separator = value.find('|', start);
            const auto item = trim(value.substr(start, separator - start));
            if (item.empty()) throw BuildException("Empty item in add-on list.");
            items.push_back(item);
            if (separator == std::string::npos) return items;
            start = separator + 1;
        }
    }

    std::string addOnIdentifier(const std::string& name)
    {
        constexpr std::string_view letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_";
        constexpr std::string_view characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789";
        if (name.empty() || letters.find(name.front()) == std::string_view::npos ||
            name.find_first_not_of(characters) != std::string::npos)
            throw BuildException("Invalid add-on SQL identifier: " + name);
        return "\"" + name + "\"";
    }

    AddOnField readAddOnField(AddOnTags& tags)
    {
        AddOnField field;
        field.name = tags.value("FIELD_BEGIN");
        addOnIdentifier(field.name);
        field.type = tags.value("TYPE");
        if (field.type != "INTEGER" && field.type != "TEXT")
            tags.fail("Unsupported add-on field type: " + field.type);
        const auto required = tags.value("REQUIRED");
        if (required != "YES" && required != "NO") tags.fail("REQUIRED must be YES or NO.");
        field.required = required == "YES";
        if (tags.at("MIN")) field.minimum = addOnInteger(tags.value("MIN"));
        if (tags.at("MAX")) field.maximum = addOnInteger(tags.value("MAX"));
        if ((field.minimum || field.maximum) && field.type != "INTEGER")
            tags.fail("MIN and MAX require an INTEGER field.");
        if (field.minimum && field.maximum && *field.minimum > *field.maximum)
            tags.fail("MIN exceeds MAX.");
        if (tags.at("ALLOWED_VALUES")) field.allowed = addOnList(tags.value("ALLOWED_VALUES"));
        tags.marker("FIELD_END");
        return field;
    }

    AddOnTable readAddOnTable(AddOnTags& tags, const std::filesystem::path& file)
    {
        AddOnTable table;
        table.name = tags.value("TABLE_BEGIN");
        addOnIdentifier(table.name);
        table.source = file;
        while (tags.at("FIELD_BEGIN"))
        {
            auto field = readAddOnField(tags);
            if (std::any_of(table.fields.begin(), table.fields.end(), [&](const auto& prior) {
                return sqlite3_stricmp(field.name.c_str(), prior.name.c_str()) == 0;
            })) tags.fail("Duplicate field: " + field.name);
            table.fields.push_back(std::move(field));
        }
        if (table.fields.empty()) tags.fail("Table requires at least one field.");
        table.primary = addOnList(tags.value("PRIMARY_KEY"));
        while (tags.at("UNIQUE")) table.unique.push_back(addOnList(tags.value("UNIQUE")));
        if (tags.at("EXPECTED_RECORD_COUNT"))
        {
            table.expectedRecords = addOnInteger(tags.value("EXPECTED_RECORD_COUNT"));
            if (*table.expectedRecords < 0) tags.fail("Expected record count cannot be negative.");
        }
        while (tags.at("FOREIGN_KEY_BEGIN"))
        {
            tags.marker("FOREIGN_KEY_BEGIN");
            table.foreign.push_back({addOnList(tags.value("FIELDS")), tags.value("REFERENCES_TABLE"),
                addOnList(tags.value("REFERENCES_FIELDS"))});
            tags.marker("FOREIGN_KEY_END");
        }
        tags.marker("TABLE_END");
        return table;
    }

    struct AddOnRecord
    {
        std::filesystem::path source;
        std::size_t line = 0;
        std::map<std::string, std::string> fields;
    };

    void readAddOnSource(const CoreAddOn& addOn, const std::filesystem::path& file,
        const std::string& bytes, std::map<std::string, AddOnTable>& tables,
        std::map<std::string, std::vector<AddOnRecord>>& records)
    {
        AddOnTags tags(file, bytes, true);
        tags.marker("PFFDATA_BEGIN");
        if (tags.value("FORMAT_REVISION") != addOn.revision)
            tags.fail("Source format revision differs from its manifest.");
        if (tags.value("DATA_TYPE") != "ADDON") tags.fail("Expected ADDON source type.");
        // Header tags are source annotations. The schema and record blocks
        // below are the only declarations that contribute SQL content.
        std::set<std::string> annotations;
        while (!tags.done() && !tags.at("SCHEMA_BEGIN") && !tags.at("RECORDS_BEGIN") && !tags.at("PFFDATA_END"))
        {
            if (tags.at("INSTRUCTIONS_BEGIN"))
                tags.marker("INSTRUCTIONS_BEGIN");
            else
            {
                const auto annotation = tags.next();
                if (annotation.value.empty() || !annotations.insert(annotation.tag).second)
                    tags.fail("Invalid or duplicate source annotation: " + annotation.tag);
            }
        }
        if (tags.at("SCHEMA_BEGIN"))
        {
            tags.marker("SCHEMA_BEGIN");
            do
            {
                auto table = readAddOnTable(tags, file);
                const auto name = table.name;
                if (std::any_of(tables.begin(), tables.end(), [&](const auto& prior) {
                    return sqlite3_stricmp(name.c_str(), prior.first.c_str()) == 0;
                })) tags.fail("Conflicting table declaration: " + name);
                tables.emplace(name, std::move(table));
            } while (tags.at("TABLE_BEGIN"));
            tags.marker("SCHEMA_END");
        }
        while (tags.at("RECORDS_BEGIN"))
        {
            auto& rows = records[tags.value("RECORDS_BEGIN")];
            while (tags.at("RECORD_BEGIN"))
            {
                const auto begin = tags.next();
                if (!begin.value.empty()) tags.fail("RECORD_BEGIN takes no value.");
                AddOnRecord row{file, begin.line, {}};
                while (!tags.at("RECORD_END"))
                {
                    const auto field = tags.next();
                    if (!row.fields.emplace(field.tag, field.value).second)
                        tags.fail("Duplicate record field: " + field.tag);
                }
                tags.marker("RECORD_END");
                rows.push_back(std::move(row));
            }
            tags.marker("RECORDS_END");
        }
        tags.marker("PFFDATA_END");
        if (!tags.done()) tags.fail("Unexpected content after add-on source.");
    }

    const AddOnField& addOnField(const AddOnTable& table, const std::string& name)
    {
        const auto field = std::find_if(table.fields.begin(), table.fields.end(),
            [&](const auto& column) { return column.name == name; });
        if (field == table.fields.end())
            throw BuildException("Undeclared field in " + table.name + ": " + name);
        return *field;
    }

    std::string addOnKey(const AddOnTable& table, const std::vector<std::string>& fields)
    {
        std::string columns;
        std::set<std::string> seen;
        for (const auto& name : fields)
        {
            addOnField(table, name);
            if (!seen.insert(name).second) throw BuildException("Duplicate field in key: " + name);
            if (!columns.empty()) columns += ", ";
            columns += addOnIdentifier(name);
        }
        return "(" + columns + ")";
    }

    std::string addOnLiteral(const AddOnField& field, const std::string& value)
    {
        if (field.type == "INTEGER") return std::to_string(addOnInteger(value));
        std::string quoted = "'";
        for (const char ch : value)
        {
            if (ch == '\'') quoted += '\'';
            quoted += ch;
        }
        return quoted + "'";
    }

    void createAddOnTable(sqlite3* db, const AddOnTable& table,
        const std::map<std::string, AddOnTable>& tables)
    {
        std::string sql = "CREATE TABLE " + addOnIdentifier(table.name) + " (";
        for (const auto& field : table.fields)
        {
            const auto name = addOnIdentifier(field.name);
            sql += name + " " + field.type;
            if (field.required) sql += " NOT NULL";
            if (field.minimum) sql += " CHECK (" + name + " >= " + std::to_string(*field.minimum) + ")";
            if (field.maximum) sql += " CHECK (" + name + " <= " + std::to_string(*field.maximum) + ")";
            if (!field.allowed.empty())
            {
                sql += " CHECK (" + name + " IN (" + addOnLiteral(field, field.allowed.front());
                for (std::size_t index = 1; index < field.allowed.size(); ++index)
                    sql += ", " + addOnLiteral(field, field.allowed[index]);
                sql += "))";
            }
            sql += ", ";
        }
        for (const auto& name : table.primary)
            if (!addOnField(table, name).required)
                throw BuildException("Primary key field must be required: " + name);
        sql += "PRIMARY KEY " + addOnKey(table, table.primary);
        for (const auto& key : table.unique) sql += ", UNIQUE " + addOnKey(table, key);
        for (const auto& reference : table.foreign)
        {
            const auto target = tables.find(reference.target);
            if (target == tables.end() || reference.fields.size() != reference.targetFields.size())
                throw BuildException("Invalid foreign key target: " + reference.target);
            const auto& targetTable = target->second;
            if (reference.targetFields != targetTable.primary &&
                std::find(targetTable.unique.begin(), targetTable.unique.end(), reference.targetFields) == targetTable.unique.end())
                throw BuildException("Foreign key must reference a declared key: " + reference.target);
            for (std::size_t index = 0; index < reference.fields.size(); ++index)
                if (addOnField(table, reference.fields[index]).type !=
                    addOnField(targetTable, reference.targetFields[index]).type)
                    throw BuildException("Foreign key field types differ: " + reference.target);
            sql += ", FOREIGN KEY " + addOnKey(table, reference.fields) + " REFERENCES " +
                addOnIdentifier(reference.target) + " " + addOnKey(targetTable, reference.targetFields) +
                " DEFERRABLE INITIALLY DEFERRED";
        }
        exec(db, (sql + ") STRICT;").c_str());
    }

    void insertAddOnRecords(BuildContext& context, const AddOnTable& table,
        const std::vector<AddOnRecord>& records, std::map<std::filesystem::path, std::uint64_t>& counts)
    {
        std::vector<std::string> names;
        std::string parameters;
        for (const auto& field : table.fields)
        {
            if (!parameters.empty()) parameters += ", ";
            parameters += "?";
            names.push_back(field.name);
        }
        const auto sql = "INSERT INTO " + addOnIdentifier(table.name) + " " +
            addOnKey(table, names) + " VALUES (" + parameters + ");";
        Statement insert(context.db, sql.c_str());
        for (const auto& record : records)
        {
            try
            {
                for (const auto& value : record.fields) addOnField(table, value.first);
                int index = 0;
                for (const auto& field : table.fields)
                {
                    ++index;
                    const auto value = record.fields.find(field.name);
                    if (value == record.fields.end())
                    {
                        if (field.required) throw BuildException("Missing required field: " + field.name);
                        bindNull(insert.get(), index);
                    }
                    else if (field.type == "INTEGER")
                        bindInt64(insert.get(), index, addOnInteger(value->second));
                    else
                        bindText(insert.get(), index, value->second);
                }
                insert.stepDone(context.db);
                insert.reset();
                ++counts.at(record.source);
                context.inserted();
            }
            catch (const BuildException& error)
            {
                throw BuildException(error.what(), record.source, record.line);
            }
        }
    }

    void buildCoreAddOns(BuildContext& context, const std::vector<CoreAddOn>& addOns,
        const CoreSourceBytes& sources, std::size_t packageOffset)
    {
        if (addOns.empty()) return;
        std::map<std::string, AddOnTable> tables;
        std::map<std::string, std::vector<AddOnRecord>> records;
        std::map<std::filesystem::path, std::uint64_t> counts;
        std::map<std::filesystem::path, const CoreAddOn*> owners;
        for (const auto& addOn : addOns)
        {
            for (const auto& file : addOn.files)
            {
                const auto source = std::find_if(sources.begin(), sources.end(),
                    [&](const auto& entry) { return entry.first == file; });
                if (source == sources.end())
                    throw BuildException("Add-on source is missing from the build snapshot.", file);
                owners.emplace(file, &addOn);
                counts.emplace(file, 0);
                try
                {
                    readAddOnSource(addOn, file, source->second, tables, records);
                }
                catch (const BuildException& error)
                {
                    if (!error.sourceFile().empty()) throw;
                    throw BuildException(error.what(), file);
                }
            }
            const auto prefix = "ManifestPackage." + std::to_string(packageOffset++) + ".";
            const auto metadata = [&](const std::string& key, const std::string& value) {
                insertMetadata(context.db, context, prefix + key, value);
            };
            metadata("Key", addOn.key);
            metadata("Version", addOn.version);
            metadata("Type", "ADDON");
            metadata("DisplayName", addOn.displayName);
            metadata("Path", pathUtf8(std::filesystem::relative(addOn.manifest, context.sourceRoot)));
            metadata("SourceFormatRevision", addOn.revision);
            metadata("Provenance", addOn.provenance);
            metadata("Rights", addOn.rights);
            for (std::size_t i = 0; i < addOn.files.size(); ++i)
                metadata("Source." + std::to_string(i), pathUtf8(
                    std::filesystem::relative(addOn.files[i], context.sourceRoot)));
            for (std::size_t i = 0; i < addOn.features.size(); ++i)
                metadata("RequiredFeature." + std::to_string(i), addOn.features[i]);
            for (std::size_t i = 0; i < addOn.compatibility.size(); ++i)
            {
                const auto binding = "Compatibility." + std::to_string(i) + ".";
                metadata(binding + "SeasonKey", addOn.compatibility[i].seasonKey);
                metadata(binding + "RulesetKey", addOn.compatibility[i].rulesetKey);
                metadata(binding + "RulesetVersion", addOn.compatibility[i].rulesetVersion);
            }
            std::size_t dependencyIndex = 0;
            for (const auto& dependency : addOn.dependencies)
            {
                const auto binding = "Dependency." + std::to_string(dependencyIndex++) + ".";
                metadata(binding + "Key", dependency.first);
                metadata(binding + "Version", dependency.second);
            }
        }
        if (tables.empty()) throw BuildException("Add-on composition declares no tables.");
        const auto checkOwner = [&](const auto& source, const auto& target, std::size_t line = 0) {
            const auto& caller = *owners.at(source);
            const auto& owner = *owners.at(target.source);
            if (&caller != &owner && !caller.dependencies.count(owner.key))
                throw BuildException("Table belongs to an add-on outside the declared dependencies: " +
                    target.name, source, line);
        };
        for (const auto& rows : records)
        {
            if (!tables.count(rows.first))
                throw BuildException("Records refer to an undeclared table: " + rows.first);
            for (const auto& record : rows.second)
                checkOwner(record.source, tables.at(rows.first), record.line);
        }
        for (const auto& entry : tables)
        {
            const auto& table = entry.second;
            try
            {
                const auto count = static_cast<sqlite3_int64>(records[table.name].size());
                if (table.expectedRecords && count != *table.expectedRecords)
                    throw BuildException("Unexpected record count for " + table.name +
                        ": expected " + std::to_string(*table.expectedRecords) + ", got " + std::to_string(count));
                for (const auto& reference : table.foreign)
                    if (const auto target = tables.find(reference.target); target != tables.end())
                        checkOwner(table.source, target->second);
                createAddOnTable(context.db, table, tables);
            }
            catch (const BuildException& error)
            {
                throw BuildException(error.what(), table.source);
            }
        }
        for (const auto& entry : tables)
            insertAddOnRecords(context, entry.second, records.at(entry.first), counts);
        for (const auto& file : counts)
        {
            registerSourceFile(context, file.first, "SCHEMA_ADDON", "IMPORTED", file.second);
            ++context.result->filesProcessed;
        }
    }
