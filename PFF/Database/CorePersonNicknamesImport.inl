// Canonical People mappings are complete before this compiler participant runs.
void buildCorePersonNicknames(BuildContext &context,
                              const std::map<std::string, std::filesystem::path> &selected)
{
    namespace S = TeamDataSql;
    auto *db = context.db;
    for (const auto *filename : {"players.pffdata", "people.pffdata"})
    {
        auto file = selected.find(filename);
        if (file == selected.end())
            continue;
        const bool player = std::string(filename) == "players.pffdata";
        const std::string kind = player ? "PLAYER" : "PERSON";
        std::string key;
        bool inside = false, inProfile = false;
        std::vector<TagLine> aliases;
        for (const auto &tag : readTaggedFile(file->second))
        {
            if (tag.tag == "NICKNAME" && !inside)
                throw BuildException("NICKNAME requires a Person/player block", file->second, tag.line);
            if (tag.tag == kind + "_BEGIN")
            {
                inside = true;
                inProfile = false;
                key.clear();
                aliases.clear();
            }
            else if (inside && tag.tag == "RATING_PROFILE_BEGIN")
                inProfile = true;
            else if (inside && tag.tag == "RATING_PROFILE_END")
                inProfile = false;
            else if (inside && !inProfile && tag.tag == (player ? "SOURCE_LOCAL_KEY" : "PERSON_KEY"))
                key = tag.value;
            else if (inside && tag.tag == "NICKNAME")
            {
                if (inProfile)
                    throw BuildException("NICKNAME belongs to a Person, not a rating profile", file->second,
                                         tag.line);
                aliases.push_back(tag);
            }
            else if (inside && tag.tag == kind + "_END")
            {
                inside = false;
                if (aliases.empty())
                    continue;
                std::string person = key;
                if (player)
                {
                    S::Query mapping(db,
                                     "SELECT m.CorePersonKey FROM Players p JOIN CorePlayerPeople m "
                                     "ON m.PlayerID=p.PlayerID WHERE p.SourceLocalKey=?",
                                     {key});
                    if (!mapping.row())
                        throw BuildException("Nickname player has no Person mapping", file->second, tag.line);
                    person = mapping.text();
                }
                const auto source =
                    S::scalar(db, "SELECT SourceFileID FROM SourceFiles WHERE RelativePath=?",
                              {pathUtf8(std::filesystem::relative(file->second, context.sourceRoot))});
                std::set<std::string> seen;
                for (const auto &alias : aliases)
                {
                    try
                    {
                        const auto value = PersonNicknames::parse(alias.value);
                        if (!seen.insert(value.normalized).second)
                            throw std::runtime_error("Duplicate normalized NICKNAME in the same block");
                        S::Query existing(db,
                                          "SELECT Nickname FROM CorePersonNicknames WHERE "
                                          "CorePersonKey=? AND NormalizedNickname=?",
                                          {person, value.normalized});
                        if (existing.row())
                        {
                            if (existing.text() != value.display)
                                throw std::runtime_error(
                                    "Conflicting nickname display spellings for the same "
                                    "Person; resolve the sources explicitly");
                        }
                        else
                            S::run(db, "INSERT INTO CorePersonNicknames VALUES(?,?,?,0)",
                                   {person, value.normalized, value.display});
                        S::run(db, "INSERT INTO CorePersonNicknameSources VALUES(?,?,?,?)",
                               {person, value.normalized, source, static_cast<std::int64_t>(alias.line)});
                        context.inserted();
                    }
                    catch (const std::exception &e)
                    {
                        throw BuildException(e.what(), file->second, alias.line);
                    }
                }
            }
        }
        if (inside && !aliases.empty())
            throw BuildException("Unclosed Person/player nickname block", file->second, aliases.front().line);
    }
    S::exec(db, "UPDATE CorePersonNicknames AS n SET DisplayOrder=(SELECT "
                "count(*) FROM CorePersonNicknames b WHERE "
                "b.CorePersonKey=n.CorePersonKey AND b.NormalizedNickname "
                "COLLATE BINARY<n.NormalizedNickname COLLATE BINARY)");
    insertMetadata(db, context, "PersonNicknamesContractVersion", "1");
    insertMetadata(db, context, "PersonNicknamesNormalizationPolicy", PersonNicknames::Policy);
    insertMetadata(db, context, "PersonNicknamesUnicodeVersion", PersonNicknames::unicodeVersion());
    insertMetadata(db, context, "PersonNicknamesUnicodeImplementation", "WINDOWS_ICU");
    insertMetadata(db, context, "PersonNicknamesUnicodeImplementationVersion",
                   PersonNicknames::implementationVersion());
    // Authority declaration is scoped to the supplied historical 1989 package.
    insertMetadata(db, context, "PersonNameAuthorityPolicy",
                   S::scalar(db, "SELECT count(*) FROM CoreMetadata WHERE MetadataKey LIKE "
                                 "'ManifestPackage.%.Key' AND MetadataValue='PFF_CLASSIC_1989_SEASON'")
                       ? "PFR_PERSON_NAMES_2026_11_05"
                       : "AUTHORED_IDENTITY_EVIDENCE");
    std::string composition;
    S::Query rows(db, "SELECT CorePersonKey,NormalizedNickname,Nickname,DisplayOrder "
                      "FROM CorePersonNicknames ORDER BY CorePersonKey COLLATE "
                      "BINARY,DisplayOrder,NormalizedNickname COLLATE BINARY");
    while (rows.row())
        for (int i = 0; i < 4; ++i)
        {
            const auto value = rows.text(i);
            composition += std::to_string(value.size()) + ":" + value;
        }
    insertMetadata(db, context, "PersonNicknamesCompositionHash", ClockTime::digest(composition));
}
