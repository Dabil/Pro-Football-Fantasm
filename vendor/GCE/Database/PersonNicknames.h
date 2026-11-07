#pragma once
#include "GCE/Database/TeamDataSql.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <icu.h>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>

// PEOPLE-NAMES-NICKNAMES-2026-11-05-R1. Use the Windows system Unicode
// implementation, never locale-sensitive lowercase or SQLite NOCASE.
namespace PersonNicknames
{
inline constexpr const char *Policy = "NFC_CASEFOLD_V1";
class Unicode
{
    HMODULE module_ = nullptr;
    template <class T> T symbol(const char *name)
    {
        auto address = GetProcAddress(module_, name);
        if (!address)
            throw std::runtime_error("Person nicknames require Windows ICU Unicode support");
        return reinterpret_cast<T>(address);
    }

  public:
    decltype(&u_strFromUTF8) fromUtf8;
    decltype(&u_strToUTF8) toUtf8;
    decltype(&unorm2_getNFCInstance) nfcInstance;
    decltype(&unorm2_normalize) nfc;
    decltype(&u_strFoldCase) fold;
    decltype(&u_getUnicodeVersion) version;
    decltype(&u_getVersion) implementationVersion;
    decltype(&u_isUWhiteSpace) whitespace;
    decltype(&u_charType) category;
    Unicode()
    {
        module_ = LoadLibraryExW(L"icu.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!module_)
            throw std::runtime_error("Person nicknames require Windows ICU Unicode support");
        try
        {
            fromUtf8 = symbol<decltype(fromUtf8)>("u_strFromUTF8");
            toUtf8 = symbol<decltype(toUtf8)>("u_strToUTF8");
            nfcInstance = symbol<decltype(nfcInstance)>("unorm2_getNFCInstance");
            nfc = symbol<decltype(nfc)>("unorm2_normalize");
            fold = symbol<decltype(fold)>("u_strFoldCase");
            version = symbol<decltype(version)>("u_getUnicodeVersion");
            implementationVersion = symbol<decltype(implementationVersion)>("u_getVersion");
            whitespace = symbol<decltype(whitespace)>("u_isUWhiteSpace");
            category = symbol<decltype(category)>("u_charType");
        }
        catch (...)
        {
            FreeLibrary(module_);
            module_ = nullptr;
            throw;
        }
    }
    ~Unicode()
    {
        if (module_)
            FreeLibrary(module_);
    }
    Unicode(const Unicode &) = delete;
};
inline const Unicode &unicode()
{
    static const Unicode instance;
    return instance;
}
inline std::string unicodeVersion()
{
    UVersionInfo v{};
    unicode().version(v);
    return std::to_string(v[0]) + "." + std::to_string(v[1]) + "." + std::to_string(v[2]) + "." +
           std::to_string(v[3]);
}
inline std::string implementationVersion()
{
    UVersionInfo v{};
    unicode().implementationVersion(v);
    return std::to_string(v[0]) + "." + std::to_string(v[1]) + "." + std::to_string(v[2]) + "." +
           std::to_string(v[3]);
}
inline void compatible(const std::string &policy, const std::string &version)
{
    if (policy != Policy || version != unicodeVersion())
        throw std::runtime_error("Unsupported Person nickname normalization/Unicode version");
}
struct Value
{
    std::string display, normalized;
};
inline Value parse(const std::string &input)
{
    const auto &u = unicode();
    if (input.size() >= static_cast<std::size_t>((std::numeric_limits<int32_t>::max)()))
        throw std::runtime_error("Nickname input is too large to decode");
    UErrorCode status = U_ZERO_ERROR;
    std::vector<UChar> text(input.size() + 1);
    int32_t size = 0;
    u.fromUtf8(text.data(), static_cast<int32_t>(text.size()), &size, input.data(),
               static_cast<int32_t>(input.size()), &status);
    if (U_FAILURE(status))
        throw std::runtime_error("Nickname must be valid UTF-8");
    // Decode scalar boundaries before trimming so embedded controls cannot
    // disappear.
    std::vector<int32_t> offsets;
    std::vector<UChar32> points;
    for (int32_t i = 0; i < size;)
    {
        offsets.push_back(i);
        UChar32 c = text[i++];
        if (c >= 0xD800 && c <= 0xDBFF)
            c = 0x10000 + ((c - 0xD800) << 10) + (text[i++] - 0xDC00);
        if (u.category(c) == U_CONTROL_CHAR)
            throw std::runtime_error("Nickname contains a control character");
        points.push_back(c);
    }
    offsets.push_back(size);
    std::size_t first = 0, last = points.size();
    while (first < last && u.whitespace(points[first]))
        ++first;
    while (last > first && u.whitespace(points[last - 1]))
        --last;
    if (last == first || last - first > 256)
        throw std::runtime_error("Nickname must contain 1..256 Unicode code points");
    std::vector<UChar> trimmed(text.begin() + offsets[first], text.begin() + offsets[last]);
    auto utf8 = [&](const std::vector<UChar> &v) {
        std::string out(v.size() * 4 + 1, '\0');
        int32_t len = 0;
        UErrorCode error = U_ZERO_ERROR;
        u.toUtf8(out.data(), static_cast<int32_t>(out.size()), &len, v.data(), static_cast<int32_t>(v.size()),
                 &error);
        if (U_FAILURE(error))
            throw std::runtime_error("Nickname UTF-8 conversion failed");
        out.resize(len);
        return out;
    };
    auto normalize = [&](const std::vector<UChar> &v) {
        UErrorCode error = U_ZERO_ERROR;
        const auto *normalizer = u.nfcInstance(&error);
        if (U_FAILURE(error))
            throw std::runtime_error("Nickname NFC unavailable");
        std::vector<UChar> out(v.size() * 4 + 32);
        auto len = u.nfc(normalizer, v.data(), static_cast<int32_t>(v.size()), out.data(),
                         static_cast<int32_t>(out.size()), &error);
        if (U_FAILURE(error))
            throw std::runtime_error("Nickname NFC failed");
        out.resize(len);
        return out;
    };
    auto normalized = normalize(trimmed);
    std::vector<UChar> folded(normalized.size() * 4 + 32);
    status = U_ZERO_ERROR;
    auto length = u.fold(folded.data(), static_cast<int32_t>(folded.size()), normalized.data(),
                         static_cast<int32_t>(normalized.size()), U_FOLD_CASE_DEFAULT, &status);
    if (U_FAILURE(status))
        throw std::runtime_error("Nickname Unicode case fold failed");
    folded.resize(length);
    return {utf8(trimmed), utf8(normalize(folded))};
}
inline std::string setting(sqlite3 *db, std::int64_t world, const char *key)
{
    TeamDataSql::Query q(db, "SELECT SettingValue FROM WorldSettings WHERE WorldID=? AND SettingKey=?",
                         {world, std::string(key)});
    if (!q.row())
        throw std::runtime_error("Missing Person nickname normalization metadata");
    return q.text();
}
inline std::string coreSetting(sqlite3 *db, const char *key)
{
    TeamDataSql::Query q(db, "SELECT MetadataValue FROM CoreMetadata WHERE MetadataKey=?",
                         {std::string(key)});
    if (!q.row())
        throw std::runtime_error("Missing Core Person nickname metadata");
    return q.text();
}
inline void validateCore(sqlite3 *db)
{
    namespace S = TeamDataSql;
    if (coreSetting(db, "CoreSchemaVersion") != "4" ||
        coreSetting(db, "PersonNicknamesContractVersion") != "1")
        throw std::runtime_error("Unsupported Core Person nickname contract; rebuild a fresh Core4");
    compatible(coreSetting(db, "PersonNicknamesNormalizationPolicy"),
               coreSetting(db, "PersonNicknamesUnicodeVersion"));
    S::Query q(db, "SELECT CorePersonKey,Nickname,NormalizedNickname,DisplayOrder "
                   "FROM CorePersonNicknames ORDER BY CorePersonKey COLLATE "
                   "BINARY,NormalizedNickname COLLATE BINARY");
    std::string previous;
    std::int64_t order = 0;
    while (q.row())
    {
        if (q.text() != previous)
        {
            previous = q.text();
            order = 0;
        }
        const auto value = parse(q.text(1));
        if (value.display != q.text(1) || value.normalized != q.text(2) || q.integer(3) != order++)
            throw std::runtime_error("Invalid Core Person nickname text, normalization or order");
        if (S::scalar(db,
                      "SELECT count(*) FROM CorePersonNicknameSources WHERE "
                      "CorePersonKey=? AND NormalizedNickname=?",
                      {q.text(), q.text(2)}) == 0)
            throw std::runtime_error("Core Person nickname is missing file/line evidence");
    }
    S::scalar(db, "SELECT count(*) FROM CorePersonNicknameSources");
    if (S::scalar(db, "SELECT count(*) FROM pragma_foreign_key_check('CorePersonNicknames')") ||
        S::scalar(db, "SELECT count(*) FROM pragma_foreign_key_check('CorePersonNicknameSources')"))
        throw std::runtime_error("Invalid Core Person nickname references");
}
inline void validateWorld(sqlite3 *db, std::int64_t world)
{
    namespace S = TeamDataSql;
    if (S::scalar(db,
                  "SELECT count(*) FROM WorldFeatureVersions WHERE WorldID=? AND "
                  "FeatureKey='PERSON_NICKNAMES' AND ContractVersion=1",
                  {world}) != 1)
        throw std::runtime_error("Unsupported PERSON_NICKNAMES contract");
    compatible(setting(db, world, "PersonNicknamesNormalizationPolicy"),
               setting(db, world, "PersonNicknamesUnicodeVersion"));
    S::Query q(db,
               "SELECT PersonID,Nickname,NormalizedNickname,OriginKind FROM "
               "WorldPersonNicknames WHERE WorldID=?",
               {world});
    while (q.row())
    {
        const auto value = parse(q.text(1));
        if (value.display != q.text(1) || value.normalized != q.text(2))
            throw std::runtime_error("Invalid World Person nickname");
        if (q.text(3) == "CORE" && S::scalar(db,
                                             "SELECT count(*) FROM WorldPersonNicknameSources WHERE "
                                             "WorldID=? AND PersonID=? AND NormalizedNickname=?",
                                             {world, q.integer(), q.text(2)}) == 0)
            throw std::runtime_error("Core nickname has no preserved source evidence");
    }
    // Also require the evidence table for Worlds whose nickname collection is
    // empty.
    S::scalar(db, "SELECT count(*) FROM WorldPersonNicknameSources WHERE WorldID=?", {world});
    if (S::scalar(db, "SELECT count(*) FROM pragma_foreign_key_check('WorldPersonNicknames')") ||
        S::scalar(db, "SELECT count(*) FROM pragma_foreign_key_check('WorldPersonNicknameSources')") ||
        S::scalar(db,
                  "SELECT count(*) FROM WorldPersonNicknames WHERE WorldID=? AND ChangedRevision>(SELECT "
                  "Revision FROM WorldDataRevisions WHERE WorldID=?)",
                  {world, world}))
        throw std::runtime_error("Invalid World Person nickname references/revision");
}
} // namespace PersonNicknames
