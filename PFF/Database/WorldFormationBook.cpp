#include "Database/WorldFormationBook.h"
#include "sqlite3.h"
#include <algorithm>
#include <cctype>
#include <set>
#include <iomanip>
#include <iterator>
#include <locale>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace
{
using namespace PffFormation;
using Database = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

// Stored IDs are fixed by format version 1, independent of enum numbering.
constexpr Role roles[] = {Role::QB, Role::RB, Role::WR, Role::TE, Role::RC, Role::RN,
    Role::T, Role::G, Role::C, Role::OL, Role::DE, Role::DT, Role::DL, Role::LB,
    Role::CB, Role::S, Role::DB};

void check(sqlite3* db, int result, int expected = SQLITE_OK)
{
    if (result != expected) throw std::runtime_error(sqlite3_errmsg(db));
}

Statement prepare(sqlite3* db, const char* sql)
{
    sqlite3_stmt* raw = nullptr;
    const int result = sqlite3_prepare_v2(db, sql, -1, &raw, nullptr);
    Statement statement(raw, sqlite3_finalize);
    check(db, result);
    return statement;
}

void bindText(sqlite3* db, sqlite3_stmt* statement, int index, const std::string& value)
{
    check(db, sqlite3_bind_text64(statement, index, value.data(), value.size(), SQLITE_TRANSIENT, SQLITE_UTF8));
}

Database open(const LoadedWorldContext& world, std::int64_t team, int mode)
{
    if (!world.hasActiveCompetitionSeason() || team <= 0)
        throw std::runtime_error("Select a team in a loaded league.");
    sqlite3* raw = nullptr;
    const int result = sqlite3_open_v2(world.databasePath.u8string().c_str(), &raw, mode, nullptr);
    Database db(raw, sqlite3_close);
    if (!db) throw std::runtime_error("Unable to open the World database.");
    check(db.get(), result);
    check(db.get(), sqlite3_busy_timeout(db.get(), 2000));
    auto query = prepare(db.get(), "SELECT 1 FROM TeamSeasons, Worlds WHERE TeamSeasonID=?1 "
        "AND CompetitionSeasonID=?2 AND WorldID=1 AND WorldKey=?3");
    check(db.get(), sqlite3_bind_int64(query.get(), 1, team));
    check(db.get(), sqlite3_bind_int64(query.get(), 2, world.activeCompetitionSeasonId));
    bindText(db.get(), query.get(), 3, world.worldKey);
    const int row = sqlite3_step(query.get());
    if (row == SQLITE_DONE) throw std::runtime_error("The selected team is unavailable in this league.");
    check(db.get(), row, SQLITE_ROW);
    return db;
}

std::int64_t organization(sqlite3* db, std::int64_t team)
{
    auto query = prepare(db, "SELECT WorldOrganizationID FROM TeamSeasons WHERE TeamSeasonID=?1");
    check(db, sqlite3_bind_int64(query.get(), 1, team));
    check(db, sqlite3_step(query.get()), SQLITE_ROW);
    return sqlite3_column_int64(query.get(), 0);
}

std::string key(sqlite3* db, std::int64_t team)
{
    return "PFF.FormationBook.Organization." + std::to_string(organization(db, team));
}
std::string encode(const WorldFormationBook& book)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "PFFFormationBook 1\n";
    for (const auto& entry : book.entries)
    {
        out << (entry.draft.side() == Side::Offense ? 'O' : 'D') << ' '
            << std::quoted(entry.name) << ' ' << entry.draft.assignments().size() << '\n';
        for (const auto& [index, role] : entry.draft.assignments())
        {
            const auto& spot = stockBoard(entry.draft.side()).at(index);
            const auto type = std::find(std::begin(roles), std::end(roles), role.type);
            if (type == std::end(roles)) throw std::runtime_error("Unknown formation role.");
            out << spot.row << ' ' << spot.label << ' ' << (type - std::begin(roles))
                << ' ' << role.rank << '\n';
        }
    }
    return out.str();
}

std::vector<FormationBookEntry> decode(const std::string& value)
{
    std::istringstream input(value);
    input.imbue(std::locale::classic());
    std::string format;
    int version = 0;
    if (!(input >> format >> version) || format != "PFFFormationBook" || version != 1)
        throw std::runtime_error("Unsupported formation book format.");

    std::vector<FormationBookEntry> entries;
    char side;
    while (input >> side)
    {
        std::string name;
        std::size_t count = 0;
        if ((side != 'O' && side != 'D') ||
            !(input >> std::quoted(name) >> count) || name.empty() || count > 11)
            throw std::runtime_error("Invalid formation entry.");
        Draft draft(side == 'O' ? Side::Offense : Side::Defense);
        const auto& board = stockBoard(draft.side());
        for (std::size_t i = 0; i < count; ++i)
        {
            int row = 0;
            std::string label;
            std::size_t type = 0;
            unsigned rank = 0;
            if (!(input >> row >> label >> type >> rank) || type >= std::size(roles))
                throw std::runtime_error("Invalid formation assignment.");
            const auto spot = std::find_if(board.begin(), board.end(), [&](const Spot& candidate) {
                return candidate.row == row && candidate.label == label;
            });
            if (spot == board.end() ||
                !draft.place({roles[type], rank}, static_cast<std::size_t>(spot - board.begin())) ||
                draft.assignments().size() != i + 1)
                throw std::runtime_error("Invalid or duplicate formation assignment.");
        }
        entries.push_back({std::move(name), std::move(draft)});
    }
    return entries;
}
}

WorldFormationBook loadWorldFormationBook(const LoadedWorldContext& world, std::int64_t team)
{
    auto db = open(world, team, SQLITE_OPEN_READONLY);
    auto query = prepare(db.get(), "SELECT SettingValue FROM WorldSettings WHERE WorldID=1 AND SettingKey=?1");
    bindText(db.get(), query.get(), 1, key(db.get(), team));
    const int row = sqlite3_step(query.get());
    if (row == SQLITE_DONE)
    {
        // Read legacy books across all seasons of this organization. The first
        // successful save publishes the combined book under the stable key.
        WorldFormationBook book;
        std::set<std::pair<Side, std::string>> names;
        const auto nameKey = [](std::string name) {
            for (auto& ch : name)
                ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            return name;
        };
        auto legacy = prepare(db.get(),
            "SELECT s.SettingValue FROM TeamSeasons t JOIN WorldSettings s "
            "ON s.WorldID=1 AND s.SettingKey='PFF.FormationBook.TeamSeason.'||t.TeamSeasonID "
            "WHERE t.WorldOrganizationID=?1 ORDER BY (t.TeamSeasonID=?2) DESC, t.TeamSeasonID DESC");
        check(db.get(), sqlite3_bind_int64(legacy.get(), 1, organization(db.get(), team)));
        check(db.get(), sqlite3_bind_int64(legacy.get(), 2, team));
        int next;
        while ((next = sqlite3_step(legacy.get())) == SQLITE_ROW)
        {
            const auto* data = reinterpret_cast<const char*>(sqlite3_column_text(legacy.get(), 0));
            if (!data) throw std::runtime_error("Unable to read a season formation book.");
            for (auto& entry : decode(std::string(data, sqlite3_column_bytes(legacy.get(), 0))))
            {
                const auto side = entry.draft.side();
                const auto original = entry.name;
                for (unsigned suffix = 2; !names.emplace(side, nameKey(entry.name)).second; ++suffix)
                    entry.name = original + " (" + std::to_string(suffix) + ")";
                book.entries.push_back(std::move(entry));
            }
        }
        check(db.get(), next, SQLITE_DONE);
        return book;
    }
    check(db.get(), row, SQLITE_ROW);
    const auto* data = reinterpret_cast<const char*>(sqlite3_column_text(query.get(), 0));
    if (!data) throw std::runtime_error("Unable to read the saved formation book.");
    std::string payload(data, sqlite3_column_bytes(query.get(), 0));
    auto entries = decode(payload);
    return {std::move(entries), std::move(payload)};
}

void saveWorldFormationBook(const LoadedWorldContext& world, std::int64_t team, WorldFormationBook& book)
{
    auto db = open(world, team, SQLITE_OPEN_READWRITE);
    auto payload = encode(book);
    auto query = prepare(db.get(), book.snapshot ?
        "UPDATE WorldSettings SET SettingValue=?2 WHERE WorldID=1 AND SettingKey=?1 AND SettingValue=?3" :
        "INSERT INTO WorldSettings(WorldID, SettingKey, SettingValue) "
        "VALUES(1, ?1, ?2) ON CONFLICT(WorldID, SettingKey) DO NOTHING");
    bindText(db.get(), query.get(), 1, key(db.get(), team));
    bindText(db.get(), query.get(), 2, payload);
    if (book.snapshot) bindText(db.get(), query.get(), 3, *book.snapshot);
    check(db.get(), sqlite3_step(query.get()), SQLITE_DONE);
    if (sqlite3_changes(db.get()) != 1)
        throw std::runtime_error("The formation book changed. Reopen the editor to reload it.");
    book.snapshot = std::move(payload);
}
