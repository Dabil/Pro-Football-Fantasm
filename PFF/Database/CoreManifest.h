#pragma once

// PFF manifest format v0.1. No framework or schema contract is introduced here.
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace CoreManifest
{
    struct Package
    {
        std::filesystem::path path;
        std::string key, version, displayName, type;
        int year = 0;
        std::vector<std::filesystem::path> files;
    };

    inline std::string trim(std::string value)
    {
        const auto first = value.find_first_not_of(" \t\r\n");
        return first == std::string::npos ? std::string{} :
            value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
    }

    inline std::string bytes(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot read " + path.u8string());
        std::ostringstream data;
        data << input.rdbuf();
        if (input.bad()) throw std::runtime_error("Read failed: " + path.u8string());
        return data.str();
    }

    inline bool inside(const std::filesystem::path& root, const std::filesystem::path& path)
    {
        auto r = root.begin(), p = path.begin();
        for (; r != root.end(); ++r, ++p)
            if (p == path.end() || *r != *p) return false;
        return true;
    }

    inline Package read(const std::filesystem::path& path, const std::string& type)
    {
        Package result;
        result.path = std::filesystem::canonical(path);
        result.type = type;
        const auto root = result.path.parent_path();
        std::istringstream input(bytes(result.path));
        std::map<std::string, std::string> fields;
        std::set<std::filesystem::path> seen;
        int state = 0; // begin, metadata, files, files closed, package closed
        std::size_t lineNumber = 0;
        auto fail = [&](const std::string& message) {
            throw std::runtime_error(path.u8string() + ":" +
                std::to_string(lineNumber) + ": " + message);
        };
        std::string line;
        while (std::getline(input, line))
        {
            ++lineNumber;
            if (lineNumber == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
            line = trim(line);
            if (line.empty() || line.rfind("//", 0) == 0) continue;
            const auto close = line.find(']');
            if (line.front() != '[' || close == std::string::npos) fail("Expected [TAG].");
            const auto tag = line.substr(1, close - 1);
            const auto value = trim(line.substr(close + 1));
            if (state == 0 && tag == type + "_BEGIN" && value.empty()) { state = 1; continue; }
            if (state == 1 && tag == "FILES_BEGIN" && value.empty()) { state = 2; continue; }
            if (state == 2 && tag == "FILES_END" && value.empty()) { state = 3; continue; }
            if (state == 3 && tag == type + "_END" && value.empty()) { state = 4; continue; }
            if (state == 2 && tag == "FILE")
            {
                const std::filesystem::path relative = std::filesystem::u8path(value);
                if (relative.empty() || relative.is_absolute() || relative.has_root_name() ||
                    value.find(':') != std::string::npos) fail("Unsafe source path.");
                for (const auto& part : relative)
                    if (part == ".." || part == ".") fail("Path traversal is not permitted.");
                if (relative.extension() != ".pffdata") fail("Expected .pffdata source.");
                std::error_code ec;
                auto resolved = std::filesystem::canonical(root / relative, ec);
                if (ec || !inside(root, resolved) || !std::filesystem::is_regular_file(resolved))
                    fail("Missing source or path outside package: " + value);
                for (const auto& previous : seen)
                    if (std::filesystem::equivalent(previous, resolved)) fail("Duplicate source: " + value);
                seen.insert(resolved);
                (void)bytes(resolved);
                result.files.push_back(resolved);
                continue;
            }
            if (state != 1 || value.empty()) fail("Unexpected tag or empty value: " + tag);
            if (tag != "MANIFEST_VERSION" && tag != "PACKAGE_KEY" && tag != "PACKAGE_VERSION" &&
                tag != "DISPLAY_NAME" && tag != "PACKAGE_TYPE" && tag != "SEASON_YEAR")
                fail("Unsupported field: " + tag);
            if (!fields.emplace(tag, value).second) fail("Duplicate field: " + tag);
        }
        if (state != 4 || result.files.empty()) fail("Incomplete package or empty source list.");
        for (const auto& key : { "MANIFEST_VERSION", "PACKAGE_KEY", "PACKAGE_VERSION", "DISPLAY_NAME", "PACKAGE_TYPE" })
            if (fields[key].empty()) fail(std::string("Missing ") + key);
        if (fields["MANIFEST_VERSION"] != "0.1" || fields["PACKAGE_TYPE"] != type)
            fail("Unsupported manifest version or package type.");
        result.key = fields["PACKAGE_KEY"];
        result.version = fields["PACKAGE_VERSION"];
        result.displayName = fields["DISPLAY_NAME"];
        if (type == "SEASON")
        {
            auto year = fields["SEASON_YEAR"];
            if (year.size() != 4 || year.find_first_not_of("0123456789") != std::string::npos)
                fail("SEASON_YEAR must contain four digits.");
            result.year = std::stoi(year);
            if (result.year <= 0) fail("Invalid season year.");
        }
        return result;
    }

    inline std::vector<Package> discover(const std::filesystem::path& root,
        const std::string& extension, const std::string& type, std::vector<std::string>& errors)
    {
        std::vector<Package> packages;
        try
        {
            if (!std::filesystem::exists(root)) return packages;
            const auto canonicalRoot = std::filesystem::canonical(root);
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
            {
                if (!entry.is_regular_file() || entry.path().extension() != extension) continue;
                try {
                    if (!inside(canonicalRoot, std::filesystem::canonical(entry.path())))
                        throw std::runtime_error("Manifest outside discovery root: " + entry.path().u8string());
                    packages.push_back(read(entry.path(), type));
                }
                catch (const std::exception& ex) { errors.push_back(ex.what()); }
            }
        }
        catch (const std::exception& ex) { errors.push_back(ex.what()); }
        std::sort(packages.begin(), packages.end(), [](const auto& a, const auto& b) {
            return a.displayName == b.displayName ? a.path < b.path : a.displayName < b.displayName;
        });
        return packages;
    }

    inline std::string basenameError(const std::string& name)
    {
        if (name.empty() || name.size() > 64 ||
            name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_- ") != std::string::npos)
            return "Use 1-64 letters, digits, spaces, underscores or hyphens (without .db).";
        if (name.back() == ' ')
            return "Database names cannot end with a space.";
        std::string upper = name;
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        });
        if (upper == "CON" || upper == "PRN" || upper == "AUX" || upper == "NUL" ||
            (upper.size() == 4 && (upper.substr(0, 3) == "COM" || upper.substr(0, 3) == "LPT") &&
                upper[3] >= '1' && upper[3] <= '9')) return "That filename is reserved by Windows.";
        return {};
    }

    // Source-byte checksum only; this is not a canonical semantic content identity.
    inline std::string checksum(const std::string& data)
    {
        unsigned long long hash = 14695981039346656037ULL;
        for (unsigned char c : data) { hash ^= c; hash *= 1099511628211ULL; }
        std::ostringstream out;
        out << std::hex << hash;
        return out.str();
    }
}
