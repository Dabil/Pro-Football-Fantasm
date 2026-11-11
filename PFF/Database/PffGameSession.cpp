#include "Database/PffGameSession.h"
#include "Database/PbpRecorder.h"
#include "GCE/Database/ClockTime.h"
#include "GCE/Database/TeamDataSql.h"
#include "GCE/Database/WorldDatabase.h"
#include "GCE/Database/WorldStorageHash.h"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <map>
#include <random>
#include <sstream>
#include <stdexcept>

namespace PffTest
{
namespace S = TeamDataSql;
namespace
{
int number(const std::string &v)
{
    std::size_t used = 0;
    int n = std::stoi(v, &used);
    if (used != v.size())
        throw std::runtime_error("Unsupported numeric chart token: " + v);
    return n;
}
struct ReadDb
{
    sqlite3 *db = nullptr;
    explicit ReadDb(const std::filesystem::path &path)
    {
        if (sqlite3_open_v2(path.u8string().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK)
        {
            if (db)
                sqlite3_close(db);
            db = nullptr;
            throw std::runtime_error("Cannot open source database");
        }
    }
    ~ReadDb()
    {
        if (db)
            sqlite3_close(db);
    }
};
using Fields = std::map<std::string, std::string>;
struct Record
{
    std::int64_t id = 0, parent = 0;
    std::string family, type;
    Fields fields;
};
struct Play
{
    std::int64_t id = 0;
    std::string name, type, key;
    std::map<std::string, int> adjustments;
    std::array<std::array<std::string, 10>, 20> outcomes;
};
struct Player
{
    std::string position;
    Fields ratings;
};
std::string field(const Fields &f, const std::string &key)
{
    auto i = f.find(key);
    if (i == f.end())
        throw std::runtime_error("Missing chart field " + key);
    return i->second;
}
std::string clockText(int seconds)
{
    std::ostringstream o;
    o << seconds / 60 << ':' << std::setfill('0') << std::setw(2) << seconds % 60;
    return o.str();
}
} // namespace
struct PffGameSession::Impl
{
    std::filesystem::path world;
    std::int64_t season = 0;
    std::uint32_t seed = 0;
    std::string bindings, coreHash;
    mutable PffPbp::Recorder pbp;
    State s;
    WorldTestMatchup game;
    std::vector<std::string> lines;
    std::vector<WorldTestGameStep> pending;
    bool computed = false;
    bool suspended = false;
    std::optional<WorldTestQueuedEvent> interruption;
    int pauseThrough = -1;
    std::mt19937 random;
    std::vector<Play> plays;
    std::vector<Record> records;
    std::map<std::string, int> playAction;
    std::map<std::string, std::string> defenseKeys;
    std::map<std::string, std::vector<Player>> rosters;
    std::array<std::string, 2> teamKeys;
    std::array<int, 2> planTier{}; // -2..2, trailing is negative.
    WorldTestSavedState snapshot(bool final = false) const
    {
        WorldTestSavedState saved;
        saved.gameId = game.gameId;
        saved.inputHash = game.inputHash;
        saved.game = s;
        saved.game.final = final;
        saved.planTier = planTier;
        if (!final)
            saved.log = lines;
        saved.replayHistory = final ? std::string{} : pbp.history();
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << random;
        saved.randomState = out.str();
        return saved;
    }
    std::string taggedFinal(std::string_view committedLog) const
    {
        return pbp.final({{"FinalAwayScore", std::to_string(s.teams[0].score())},
                         {"FinalHomeScore", std::to_string(s.teams[1].score())},
                         {"FinalPeriod", std::to_string(s.quarter)}, {"FinalClock", clockText(s.remaining)},
                         {"EndReason", s.quarter == 5 && s.teams[0].score() != s.teams[1].score()
                                           ? "SUDDEN_DEATH" : "CLOCK_EXPIRED"}}, committedLog);
    }
    int dice(int sides, std::string_view purpose, std::string_view color = "")
    {
        const int face = int(random() % unsigned(sides)) + (sides == 10 ? 0 : 1);
        pbp.roll(sides, purpose, color, face);
        return face;
    }
    void beginRecord(std::string_view kind, std::string_view playKind = "", int kickingTeam = -1)
    {
        const bool scrimmage = kickingTeam < 0;
        pbp.begin(pbp.nextSequence(), kind,
                  {{"QUARTER", std::to_string(s.quarter)}, {"CLOCK", clockText(s.remaining)},
                   {"CLOCK_STATUS", "COUNTDOWN"}, {"GAME_ELAPSED_SECONDS", std::to_string(s.elapsed)},
                   {"AWAY_SCORE", std::to_string(s.teams[0].score())}, {"HOME_SCORE", std::to_string(s.teams[1].score())},
                   {"DOWN", scrimmage ? std::to_string(s.down) : ""},
                   {"TOGO", scrimmage ? std::to_string(s.lineToGain - s.spot) : ""},
                   {"POSSESSION_TEAM_KEY", scrimmage ? teamKeys[s.offense] : ""},
                   {"BALL_AT_TEAM_KEY", scrimmage ? teamKeys[s.spot <= 50 ? s.offense : 1 - s.offense] : teamKeys[kickingTeam]},
                   {"BALL_AT_YARDLINE", std::to_string(scrimmage ? std::min(s.spot, 100 - s.spot) : 35)}});
        if (kind == "PLAY")
            pbp.block("PLAY_CONTEXT", {{"PlayKind", std::string(playKind)},
                      {"KickingTeamKey", scrimmage ? "" : teamKeys[kickingTeam]},
                      {"ReceivingTeamKey", scrimmage ? "" : teamKeys[1 - kickingTeam]}});
    }
    const Record &find(const std::string &family, const std::string &type, const Fields &wanted) const
    {
        const Record *found = nullptr;
        for (const auto &r : records)
            if (r.family == family && r.type == type)
            {
                bool match = true;
                for (const auto &w : wanted)
                {
                    auto i = r.fields.find(w.first);
                    if (i == r.fields.end() || i->second != w.second)
                    {
                        match = false;
                        break;
                    }
                }
                if (match)
                {
                    if (found)
                        throw std::runtime_error("Ambiguous chart selector " + family + "/" + type);
                    found = &r;
                }
            }
        if (!found)
            throw std::runtime_error("Missing chart selector " + family + "/" + type);
        return *found;
    }
    int rating(int team, const std::string &position, const std::string &key) const
    {
        auto r = rosters.find(teamKeys[team]);
        if (r == rosters.end())
            return 0;
        for (const auto &p : r->second)
            if (p.position == position)
            {
                auto i = p.ratings.find(key);
                if (i != p.ratings.end())
                {
                    pbp.block("RATING", {{"TeamKey", teamKeys[team]}, {"Position", position},
                                         {"RatingKey", key}, {"RatingValue", i->second}});
                    return number(i->second);
                }
            }
        return 0;
    }
    std::string simpleChart(const std::string &family, int row, int column)
    {
        const int effective = std::clamp(row, 1, 20);
        const auto &record = find(family, "CHART_ROW", {{"SOURCE_ROW", std::to_string(effective)}});
        const auto token = field(record.fields, "OUTCOME_" + std::to_string(column));
        const bool breakawayLookup = family == "BREAKAWAY";
        pbp.lookup(family + ":" + std::to_string(record.id), row, effective, column, token,
                   breakawayLookup ? 0 : 1, breakawayLookup ? 1 : 0);
        return token;
    }
    int breakaway(const std::string &defense, int team, const std::string &position)
    {
        const int behind = defense == "D" || defense == "B" ? 3 : defense == "M" || defense == "Z" ? 5 : 4;
        const int column = dice(10, "BREAKAWAY_COLUMN", "RED");
        const int row = dice(20, "BREAKAWAY_ROW", "WHITE") + rating(team, position, "BREAK_SPEED") - behind;
        return number(simpleChart("BREAKAWAY", row, column));
    }
    std::string special(const std::string &name, int row, int col)
    {
        const auto &parent = find("SPECIAL_TEAMS", "SPECIAL_TEAM_PLAY", {{"NAME", name}});
        for (const auto &r : records)
            if (r.parent == parent.id && (r.type == "DISTANCE_ROW" || r.type == "RETURN_ROW") &&
                field(r.fields, "D20") == std::to_string(std::clamp(row, 1, 20)))
            {
                const auto token = field(r.fields, "OUTCOME_" + std::to_string(col));
                pbp.lookup(name + ":" + std::to_string(r.id), row, std::clamp(row, 1, 20), col, token);
                return token;
            }
        throw std::runtime_error("Missing special-teams chart row");
    }
    void load()
    {
        const auto loaded = loadWorldDatabase(world);
        if (!loaded.success)
            throw std::runtime_error(loaded.errorMessage);
        auto cs = std::find_if(loaded.competitionSeasons.begin(), loaded.competitionSeasons.end(),
                               [&](const auto &v) { return v.competitionSeasonId == season; });
        if (cs == loaded.competitionSeasons.end())
            throw std::runtime_error("Season is not in this World");
        auto snapshot =
            std::find_if(loaded.coreSnapshots.begin(), loaded.coreSnapshots.end(),
                         [&](const auto &v) { return v.worldCoreSnapshotId == cs->worldCoreSnapshotId; });
        if (snapshot == loaded.coreSnapshots.end())
            throw std::runtime_error("Missing preserved Core snapshot");
        auto path = snapshot->snapshotPath;
        if (path.is_relative())
            path = world.parent_path() / path;
        if (WorldStorageHash::file(path) != snapshot->snapshotFileHash)
            throw std::runtime_error("Preserved Core checksum changed");
        coreHash = snapshot->snapshotFileHash;
        ReadDb core(path);
        auto db = core.db;
        S::Query rule(db, "SELECT RulesetID FROM Rulesets WHERE RulesetKey=?",
                      {snapshot->simulationRulesProfileKey});
        if (!rule.row())
            throw std::runtime_error("Snapshot ruleset is unavailable");
        const auto ruleset = rule.integer();
        S::Query p(db,
                   "SELECT OffensivePlayID,Name,PlayType,PlayKey FROM OffensivePlays WHERE RulesetID=? ORDER "
                   "BY SourceCsvLine,OffensivePlayID",
                   {ruleset});
        while (p.row())
        {
            Play play;
            play.id = p.integer();
            play.name = p.text(1);
            play.type = p.text(2);
            play.key = p.text(3);
            S::Query a(
                db,
                "SELECT AdjustmentKey,AdjustmentValue FROM OffensivePlayAdjustments WHERE OffensivePlayID=?",
                {play.id});
            while (a.row())
                play.adjustments[a.text()] = int(a.integer(1));
            S::Query outcomes(db,
                              "SELECT m.WhiteD20,o.D10,o.ResultToken FROM OffensivePlayMatchups m JOIN "
                              "OffensivePlayOutcomes o USING(MatchupID) WHERE m.OffensivePlayID=?",
                              {play.id});
            int count = 0;
            while (outcomes.row())
            {
                int row = int(outcomes.integer()), col = int(outcomes.integer(1));
                if (row < 1 || row > 20 || col < 0 || col > 9)
                    throw std::runtime_error("Invalid card dimensions");
                play.outcomes[row - 1][col] = outcomes.text(2);
                ++count;
            }
            if (count != 200)
                throw std::runtime_error("Incomplete offensive card " + play.name);
            plays.push_back(std::move(play));
        }
        if (plays.empty())
            throw std::runtime_error("No offensive cards installed");
        S::Query d(db, "SELECT Name,PlayActionModifier,DefenseKey FROM DefensivePlays WHERE RulesetID=?", {ruleset});
        while (d.row())
        {
            playAction[d.text()] = int(d.integer(1));
            defenseKeys[d.text()] = d.text(2);
        }
        S::Query rec(db,
                     "SELECT r.RecordID,r.ParentRecordID,d.Family,r.RecordType FROM ChartRecords r JOIN "
                     "ChartDefinitions d USING(ChartID) WHERE d.RulesetID=? ORDER BY r.RecordOrder",
                     {ruleset});
        while (rec.row())
        {
            Record r;
            r.id = rec.integer();
            r.parent = rec.integer(1);
            r.family = rec.text(2);
            r.type = rec.text(3);
            S::Query v(db, "SELECT FieldKey,ValueToken FROM ChartValues WHERE RecordID=? ORDER BY ValueIndex",
                       {r.id});
            while (v.row())
                r.fields[v.text()] = v.text(1);
            records.push_back(std::move(r));
        }
        S::Query players(db, "SELECT t.TeamSeasonKey,p.PlayerID,p.Position FROM Players p JOIN TeamSeasons t "
                             "USING(TeamSeasonID) ORDER BY t.TeamSeasonKey,p.SourceCsvLine,p.PlayerID");
        while (players.row())
        {
            Player player;
            player.position = players.text(2);
            S::Query values(db,
                            "SELECT v.RatingKey,v.RatingValue FROM PlayerRatingProfiles p JOIN PlayerRatings "
                            "v USING(PlayerRatingProfileID) WHERE p.PlayerID=? AND p.RulesetID=?",
                            {players.integer(1), ruleset});
            while (values.row())
                player.ratings[values.text()] = values.text(1);
            rosters[players.text()].push_back(std::move(player));
        }
        // Validate the exact sources the test uses before admitting any fixture.
        for (const auto &side : {"OFFENSE", "DEFENSE"})
            for (int roll = 0; roll < 10; ++roll)
                find(
                    "SOLITAIRE", "PLAY_CALL_ROW",
                    {{"SIDE", side}, {"COACHING_PHILOSOPHY", "BALANCED"}, {"RED_D10", std::to_string(roll)}});
        for (int roll = 1; roll <= 20; ++roll)
        {
            find("SOLITAIRE", "PLAY_SELECTION_ROW",
                 {{"COACHING_PHILOSOPHY", "BALANCED"}, {"BLUE_D20", std::to_string(roll)}});
            find("SOLITAIRE", "PLAYER_SELECTION_ROW", {{"WHITE_D20", std::to_string(roll)}});
        }
        find("SPECIAL_TEAMS", "SPECIAL_TEAM_PLAY", {{"NAME", "Normal Punt"}});
        find("SPECIAL_TEAMS", "SPECIAL_TEAM_PLAY", {{"NAME", "Normal Punt Return"}});
        for (const auto &chart : {"Normal Kick", "Normal Kick Return", "Onsides Kick", "Onsides Kick Return"})
            for (int row = 1; row <= 20; ++row)
                for (int column = 0; column < 10; ++column)
                    special(chart, row, column);
        bindings = "Provider=PFF_GENERIC_QUEUE_TEST/2\nCoreSHA256=" + snapshot->snapshotFileHash +
                   "\nSeed=" + std::to_string(seed) +
                   "\nQuarters=4x900 plus sudden-death OT; chart kickoffs, PAT and FG.\nNo penalties, wild plays, injury, endurance, or player statistics. "
                   "Return checkpoints support deterministic resume.\nTiming is original PFF 40/10/20 "
                   "seconds; historical elapsed equals consumed "
                   "clock in this test.\nUndated unrelated season-event templates remain unexecuted. "
                   "A=play-action pass; RH=resulting Run-Holes rating; chart limits clamp.\n"
                   "Persistence=MEMORY_UNTIL_SAVE_OR_FINALIZE; no per-play database or file access.\n";
    }
    std::string name(int team) const
    {
        return team ? game.homeName : game.awayName;
    }
    void possession(int team, int spot, std::string &log, const std::string &why)
    {
        pbp.action("POSSESSION", teamKeys[team], std::clamp(spot, 1, 99), why);
        s.offense = team;
        s.spot = std::clamp(spot, 1, 99);
        s.down = 1;
        s.lineToGain = std::min(100, s.spot + 10);
        log += "--- POSSESSION: " + name(team) + " (" + why + "), own " + std::to_string(s.spot) + " ---\n";
    }
    void points(int team, int value, std::string &log, const std::string &why)
    {
        pbp.action("SCORE", teamKeys[team], value, why);
        s.teams[team].quarters[s.quarter - 1] += value;
        s.spot = std::clamp(s.spot, 1, 99);
        s.lineToGain = std::max(s.lineToGain, s.spot);
        log += name(team) + " " + why + " (+" + std::to_string(value) + ").\n";
        if (value == 6 && s.quarter < 5)
        {
            pbp.block("TRY_START", {{"PlayKind", "EXTRA_POINT"}, {"POSSESSION_TEAM_KEY", teamKeys[team]},
                {"QUARTER", std::to_string(s.quarter)}, {"CLOCK", clockText(s.remaining)},
                {"AWAY_SCORE", std::to_string(s.teams[0].score())}, {"HOME_SCORE", std::to_string(s.teams[1].score())}});
            if (placeKick(team, true, log))
            {
                pbp.action("EXTRA_POINT", teamKeys[team], 1);
                ++s.teams[team].quarters[s.quarter - 1];
                log += name(team) + " EXTRA POINT (+1).\n";
            }
        }
    }
    int openingReceiver() const
    {
        std::mt19937 opening(seed);
        return int(opening() % 2);
    }
    int blockRoll(int team, const std::string &position, std::string &log)
    {
        const int white = dice(20, "KICK_BLOCK_CHECK", "WHITE"), blue = dice(20, "KICK_BLOCK_SPECIAL", "BLUE");
        const int adjusted = white + rating(team, position, "BLOCKED") + rating(1 - team, "P", "BLOCKED");
        log += "Block attempt: white=" + std::to_string(white) + ", special=" + std::to_string(blue) +
               ", adjusted=" + std::to_string(adjusted) + ".\n";
        pbp.block("BLOCK_CHECK", {{"RollRef", pbp.rollRef(1)}, {"RollRef", pbp.rollRef()},
                                  {"AdjustedValue", std::to_string(adjusted)}});
        return adjusted;
    }
    void blockedKick(int team, std::string &log)
    {
        const int white = dice(20, "KICK_ROW", "WHITE"), red = dice(10, "KICK_COLUMN", "RED");
        const auto result = simpleChart("INTERCEPTIONRETURN", white, red);
        const int yards = result == "B" ? breakaway("R", 1 - team, "WR") : number(result);
        const int spot = 100 - s.spot + yards;
        pbp.action("KICK_BLOCKED", teamKeys[1 - team], yards);
        possession(1 - team, spot, log, "blocked kick recovery");
        if (spot >= 100)
            points(1 - team, 6, log, "BLOCKED KICK RETURN TOUCHDOWN");
    }
    bool placeKick(int team, bool extraPoint, std::string &log)
    {
        const std::string label = extraPoint ? "Extra point" : "Field goal";
        if (blockRoll(team, "K", log) <= 1)
        {
            pbp.action(extraPoint ? "EXTRA_POINT_BLOCKED" : "FIELD_GOAL_BLOCKED", teamKeys[1 - team], 0);
            log += label + " blocked.\n";
            if (!extraPoint)
                blockedKick(team, log);
            return false;
        }
        const int yard = 100 - s.spot;
        const auto key = extraPoint ? "EXTRA_POINTS" :
                         yard <= 12 ? "FG_1_TO_12" : yard <= 22 ? "FG_13_TO_22" :
                         yard <= 32 ? "FG_23_TO_32" : yard <= 37 ? "FG_33_TO_37" : "FG_38_TO_42";
        const int roll = dice(20, extraPoint ? "EXTRA_POINT_CHECK" : "FIELD_GOAL_CHECK", "WHITE"), target = rating(team, "K", key);
        const bool good = (extraPoint || yard <= 42) && roll <= target;
        pbp.block("KICK_CHECK", {{"PlayKind", extraPoint ? "EXTRA_POINT" : "FIELD_GOAL"},
                                {"RollRef", pbp.rollRef()}, {"SourceRef", key},
                                {"RatingValue", std::to_string(target)}, {"CheckOutcome", good ? "GOOD" : "MISSED"}});
        log += label + " [D20=" + std::to_string(roll) + ", rating=" + std::to_string(target) +
               "]: " + (good ? "good.\n" : "no good.\n");
        if (!extraPoint && !good)
            possession(1 - team, std::max(20, yard), log, "missed field goal");
        return good;
    }
    void kickoff(int team, std::string &log, bool allowOnside = false)
    {
        const int receiver = 1 - team;
        const bool onside = allowOnside && s.quarter == 4 && s.remaining < 180 &&
                            s.teams[team].score() < s.teams[receiver].score();
        const std::string chart = onside ? "Onsides Kick" : "Normal Kick";
        const int kicker = rating(team, "K", onside ? "KICK_ONSIDES" : "KICK_NORMAL");
        const int distanceRoll = dice(20, "KICKOFF_DISTANCE_ROW", "WHITE"), distanceColumn = dice(10, "KICKOFF_DISTANCE_COLUMN", "RED");
        const auto distance = special(chart, distanceRoll + kicker, distanceColumn);
        pbp.block("KICKOFF_DISTANCE", {{"KickingTeamKey", teamKeys[team]}, {"LookupResult", distance}});
        log += name(team) + " " + chart + ": " + distance + ".\n";
        if (distance == "TB" || (distance != "OB" && !onside && number(distance) >= 65))
        {
            possession(receiver, 20, log, "kickoff touchback");
            return;
        }
        if (distance == "OB")
        {
            possession(receiver, 35, log, "kickoff out of bounds");
            return;
        }
        const int landing = 35 + number(distance);
        const int adjustment = onside ? rating(receiver, "P", "ONSIDE_RETURN") + kicker :
            rating(receiver, "P", "KICK_RETURN_A") + rating(team, "K", "KICK_COVER");
        const int returnRoll = dice(20, "KICK_RETURN_ROW", "WHITE"), returnColumn = dice(10, "KICK_RETURN_COLUMN", "RED");
        const auto result = special(chart + " Return", returnRoll + adjustment, returnColumn);
        const bool kickingRecovery = result.rfind("KICK_REC_", 0) == 0;
        const int returnTeam = kickingRecovery ? team : receiver;
        const auto token = kickingRecovery ? result.substr(9) : result;
        const int yards = token == "B" ? breakaway("R", returnTeam, "WR") : number(token);
        const int spot = (kickingRecovery ? landing : 100 - landing) + yards;
        pbp.action(kickingRecovery ? "ONSIDE_RECOVERY" : "KICKOFF_RETURN", teamKeys[returnTeam], yards);
        possession(returnTeam, spot, log, chart + " return " + result);
        if (spot < 100)
            return;
        points(returnTeam, 6, log, "KICKOFF RETURN TOUCHDOWN");
        if (s.quarter == 5)
        {
            s.final = true;
            return;
        }
        s.pendingKick = returnTeam;
    }
    void gamePlan(const std::string &checkpoint)
    {
        const auto &f = find("SOLITAIRE", "GAME_PLAN_CHECKPOINT", {{"CHECKPOINT_KEY", checkpoint}}).fields;
        for (int t = 0; t < 2; ++t)
        {
            const int diff = s.teams[t].score() - s.teams[1 - t].score();
            int tier = 0;
            for (int i = 1; i <= 2; ++i)
            {
                const auto suffix = std::to_string(i) + "_THRESHOLD";
                if (diff >= number(field(f, "AHEAD_TIER_" + suffix)))
                    tier = i;
                else if (diff <= number(field(f, "BEHIND_TIER_" + suffix)))
                    tier = -i;
            }
            planTier[t] = tier;
        }
    }
    int plan(int team, const std::string &context)
    {
        int tier = planTier[team];
        if (!tier)
            return 0;
        const auto key = std::string(tier > 0 ? "AHEAD" : "BEHIND") + "_TIER_" +
                         std::to_string(std::abs(tier)) + "_MODIFIER";
        return number(
            field(find("SOLITAIRE", "GAME_PLAN_DICE_MODIFIER", {{"DICE_CONTEXT", context}}).fields, key));
    }
    std::string stats() const
    {
        std::ostringstream o;
        o << "Team | Q1 Q2 Q3 Q4 | Total | First downs | Rush att/yds | Pass cmp/att/yds | INT | Fumbles "
             "lost | Punts/yds | Sacks/yds\n";
        for (int t = 0; t < 2; ++t)
        {
            const auto &a = s.teams[t];
            o << name(t) << " |";
            for (int q : a.quarters)
                o << ' ' << q;
            o << " | " << a.score() << " | " << a.firstDowns << " | " << a.rushes << '/' << a.rushYards
              << " | " << a.completions << '/' << a.attempts << '/' << a.passYards << " | " << a.interceptions
              << " | " << a.fumblesLost << " | " << a.punts << '/' << a.puntYards << " | " << a.sacks << '/'
              << a.sackYards << '\n';
        }
        return o.str();
    }
    int toss(std::string_view occasion)
    {
        pbp.begin(pbp.nextSequence(), "COIN_TOSS", {});
        const int receiver = dice(2, "COIN_TOSS") - 1;
        pbp.block("COIN_TOSS", {{"TossOccasion", std::string(occasion)}, {"TossProcedure", "SIMPLIFIED"},
                               {"TossResult", receiver ? "HEADS" : "TAILS"}, {"RollRef", pbp.rollRef()},
                               {"KickingTeamKey", teamKeys[1 - receiver]}, {"ReceivingTeamKey", teamKeys[receiver]}});
        return receiver;
    }
    WorldTestGameStep advance()
    {
        WorldTestGameStep p;
        p.generation = ++s.generation;
        const int remainingBefore = s.remaining, elapsedBefore = s.elapsed;
        std::string text;
        if (s.quarter == 1 && s.playOrdinal == 0)
        {
            const int receiver = toss("OPENING");
            pbp.complete();
            beginRecord("PLAY", "KICKOFF", 1 - receiver);
            text = "Opening coin flip: " + std::string(receiver ? "heads" : "tails") +
                   ", " + name(receiver) + " receives.\n";
            kickoff(1 - receiver, text);
            s.remaining -= 20;
            s.elapsed += 20;
        }
        else if (s.remaining == 0)
        {
            ++s.quarter;
            s.remaining = 900;
            p.play = false;
            beginRecord("PERIOD_CHANGE");
            pbp.block("PERIOD_CHANGE", {{"QUARTER", std::to_string(s.quarter)}, {"CLOCK", clockText(s.remaining)}});
            if (s.quarter == 5)
                pbp.complete();
            text = "=== QUARTER " + std::to_string(s.quarter) + " ===\n";
            if (s.quarter == 3)
                s.pendingKick = openingReceiver();
            if (s.quarter == 5)
            {
                const int receiver = toss("OVERTIME");
                text += "Overtime coin flip: " + std::string(receiver ? "heads" : "tails") +
                        ", " + name(receiver) + " receives.\n";
                s.pendingKick = 1 - receiver;
            }
        }
        else if (s.pendingKick >= 0)
        {
            const int kickingTeam = s.pendingKick;
            beginRecord("PLAY", "KICKOFF", kickingTeam);
            s.pendingKick = -1;
            const int consumed = std::min(20, s.remaining);
            kickoff(kickingTeam, text, true);
            s.remaining -= consumed;
            s.elapsed += consumed;

            if (!s.remaining && s.quarter == 4)
                s.pendingKick = -1;
        }
        else
        {
            const int team = s.offense, defense = 1 - team, before = s.spot, down = s.down,
                      remaining = s.remaining, toGo = s.lineToGain - s.spot;
            const int awayBefore = s.teams[0].score(), homeBefore = s.teams[1].score();
            beginRecord("PLAY", down != 4 ? "SCRIMMAGE" : before >= 70 ? "FIELD_GOAL" : "PUNT");
            auto &stats = s.teams[team];
            int seconds = 40;
            bool safety = false;
            std::string detail;
            const auto situation = down == 1   ? (s.lineToGain - s.spot > 10    ? "A"
                                                  : s.lineToGain - s.spot == 10 ? "B"
                                                                                : "C")
                                   : down == 2 ? (s.lineToGain - s.spot > 7    ? "A"
                                                  : s.lineToGain - s.spot >= 4 ? "B"
                                                                               : "C")
                                               : (s.lineToGain - s.spot > 6    ? "A"
                                                  : s.lineToGain - s.spot >= 3 ? "B"
                                                                               : "C");
            if (down == 4)
            {
                seconds = 20;
                if (s.spot >= 70)
                {
                    if (placeKick(team, false, detail))
                        points(team, 3, detail, "FIELD GOAL");
                }
                else
                {
                    const bool attemptBlock = s.quarter >= 4 && stats.score() > s.teams[defense].score();
                    const int block = attemptBlock ? blockRoll(team, "P", detail) : 3;
                    if (block <= 1)
                    {
                        blockedKick(team, detail);
                    }
                    else
                    {
                        const int white = dice(20, "KICK_ROW", "WHITE"), red = dice(10, "KICK_COLUMN", "RED");
                        const auto token = special("Normal Punt", white + rating(team, "P", "PUNT_NORMAL"), red);
                        ++stats.punts;
                        int distance = token == "OB" ? 100 - before : number(token);
                        if (block == 2)
                        {
                            pbp.action("PUNT_PARTIALLY_BLOCKED", teamKeys[defense], 0);
                            distance /= 2;
                        }
                        stats.puntYards += std::min(distance, 100 - before);
                        pbp.action("PUNT", teamKeys[team], distance);
                        detail += "Punt travels " + std::to_string(distance) + " yards.\n";
                        if (before + distance >= 100)
                        {
                            possession(defense, 20, detail, "punt touchback");
                        }
                        else
                        {
                            std::string result = "FC";
                            if (block == 2)
                                result = "0";
                            else if (!attemptBlock && token != "OB")
                            {
                                const int returnRoll = dice(20, "KICK_RETURN_ROW", "WHITE"), returnColumn = dice(10, "KICK_RETURN_COLUMN", "RED");
                                const int adjustment = rating(defense, "P", "PUNT_RETURN_A") +
                                                       rating(team, "P", "PUNT_COVER");
                                result = special("Normal Punt Return", returnRoll + adjustment, returnColumn);
                            }
                            const int yards = result == "FC" || result == "OB" ? 0 :
                                              result == "B" ? breakaway("R", defense, "WR") : number(result);
                            pbp.action("PUNT_RETURN", teamKeys[defense], yards, result);
                            const int spot = 100 - before - distance + yards;
                            possession(defense, spot, detail, "punt return " + result);
                            if (spot >= 100)
                                points(defense, 6, detail, "PUNT RETURN TOUCHDOWN");
                        }
                    }
                }
            }
            else
            {
                const int callRoll = std::clamp(dice(10, "OFFENSE_CALL", "RED") + plan(team, "OFFENSE_RED_D10"), 0, 9);
                const auto call = field(find("SOLITAIRE", "PLAY_CALL_ROW",
                                             {{"SIDE", "OFFENSE"},
                                              {"COACHING_PHILOSOPHY", "BALANCED"},
                                              {"RED_D10", std::to_string(callRoll)}})
                                            .fields,
                                        std::string("SITUATION_") + situation);
                const int selection = std::clamp(dice(20, "OFFENSE_SELECTION", "BLUE") + plan(team, "OFFENSE_BLUE_D20"), 1, 20),
                          playerRoll = dice(20, "PLAYER_SELECTION", "WHITE");
                const int index = number(field(find("SOLITAIRE", "PLAY_SELECTION_ROW",
                                                    {{"COACHING_PHILOSOPHY", "BALANCED"},
                                                     {"BLUE_D20", std::to_string(selection)}})
                                                   .fields,
                                               call == "R" ? "RUN_PLAY_NUMBER" : "PASS_PLAY_NUMBER")) -
                                  1;
                if (index < 0 || index >= int(plays.size()))
                    throw std::runtime_error("Solitaire references an unavailable offensive card");
                const auto &card = plays[index];
                const auto playerToken = field(
                    find("SOLITAIRE", "PLAYER_SELECTION_ROW", {{"WHITE_D20", std::to_string(playerRoll)}})
                        .fields,
                    call == "R" ? "RUN_ALL_PHILOSOPHIES" : "PASS_BALANCED");
                const auto position = playerToken == "QB"   ? "QB"
                                      : playerToken == "TE" ? "TE"
                                      : playerToken == "HB" || playerToken == "FB" || playerToken == "B1"
                                          ? "RB"
                                          : "WR";
                const int defRoll = std::clamp(dice(10, "DEFENSE_CALL", "RED") + plan(defense, "DEFENSE_RED_D10"), 0, 9);
                const auto dc = field(find("SOLITAIRE", "PLAY_CALL_ROW",
                                           {{"SIDE", "DEFENSE"},
                                            {"COACHING_PHILOSOPHY", "BALANCED"},
                                            {"RED_D10", std::to_string(defRoll)}})
                                          .fields,
                                      std::string("SITUATION_") + situation);
                const std::map<std::string, std::string> adjust{
                    {"S", "SHORT"},
                    {"K", playerToken == "B1" || playerToken == "HB" ? "KEY_RIGHT" : "KEY_WRONG"},
                    {"N", "RUN"},
                    {"R", "READ"},
                    {"D", "DOG"},
                    {"B", "BLITZ"},
                    {"M", "PASS_MAN"},
                    {"Z", "PASS_ZONE"}};
                const int raw = dice(20, "MAIN_CARD_ROW", "WHITE"), red = dice(10, "MAIN_CARD_COLUMN", "RED"),
                          blue = dice(20, "MAIN_CARD_SPECIAL", "BLUE");
                const std::map<std::string, std::string> names{
                    {"S", "Short"}, {"K", "Run-Key RB1"}, {"N", "Run"}, {"R", "Read"},
                    {"D", "Dog"}, {"B", "Blitz"}, {"M", "Pass-Man"}, {"Z", "Pass-Zone"}};
                const auto &defenseName = names.at(dc);
                const int defenseAdjustment = card.adjustments.at(adjust.at(dc));
                const int playActionAdjustment = call == "A" ? playAction.at(defenseName) : 0;
                const int adjustment = defenseAdjustment + playActionAdjustment;
                const int row = std::clamp(raw + adjustment, 1, 20);
                const auto token = card.outcomes[row - 1][red];
                const auto specialRollRef = pbp.rollRef();
                pbp.block("MAIN_CARD_LOOKUP", {{"OffensePlay", std::to_string(index + 1)},
                    {"OffensePlayName", card.name}, {"OffensePlayKey", card.key},
                    {"DefensePlayName", defenseName}, {"DefensePlayKey", defenseKeys.at(defenseName)},
                    {"WhiteD20", std::to_string(raw)}, {"BlueD20", std::to_string(blue)},
                    {"RedD10", std::to_string(red)}, {"DefensePlayAdjustment", std::to_string(defenseAdjustment)},
                    {"PlayResult", token}});
                if (call == "A")
                    pbp.block("ADJUSTMENT", {{"AdjustmentName", "Play action"},
                                             {"AdjustmentValue", std::to_string(playActionAdjustment)}});
                pbp.lookup(card.key, raw + adjustment, row, red, token, 2, 1);
                pbp.block("PLAY_CALLS", {{"OffenseCall", call}, {"OffensePlaySelection", std::to_string(selection)},
                    {"OffenseCallRollRef", pbp.rollRef(6)}, {"OffenseSelectionRollRef", pbp.rollRef(5)},
                    {"PlayerSelectionRollRef", pbp.rollRef(4)}, {"DefenseCallRollRef", pbp.rollRef(3)},
                    {"DefenseCallValue", std::to_string(defRoll)}, {"OffenseCallValue", std::to_string(callRoll)},
                    {"SelectedPosition", position}, {"SpecialRollRef", pbp.rollRef()}});
                detail = card.name + " / defense " + dc + " [call D10=" + std::to_string(callRoll) +
                         ", play D20=" + std::to_string(selection) +
                         ", player D20=" + std::to_string(playerRoll) +
                         ", defense D10=" + std::to_string(defRoll) + "; white=" + std::to_string(raw) +
                         " adj=" + std::to_string(adjustment) + " row=" + std::to_string(row) +
                         ", red=" + std::to_string(red) + ", blue=" + std::to_string(blue) + "] => " + token +
                         ".\n";
                bool pass = card.type == "pass", incomplete = token == "INC", intercepted = false;
                int gain = 0;
                if (token == "INT")
                {
                    const int ir = dice(20, "INTERCEPTION_CHECK", "WHITE") + rating(team, "QB", "INTERCEPTION") +
                                   rating(defense, "CB", "INTERCEPTION"),
                              pointRoll = dice(20, "INTERCEPTION_POINT", "WHITE");
                    intercepted = ir <= 10;
                    pbp.block("CHECK", {{"LookupPurpose", "INTERCEPTION_CHECK"}, {"RollRef", pbp.rollRef(1)},
                                        {"AdjustedValue", std::to_string(ir)}, {"CheckOutcome", intercepted ? "INTERCEPTED" : "INCOMPLETE"}});
                    incomplete = !intercepted;
                    detail += "Interception check=" + std::to_string(ir) +
                              ", point D20=" + std::to_string(pointRoll) + ".\n";
                    if (intercepted)
                    {
                        const auto type = index <= 18                                 ? "SCREEN"
                                          : index <= 21                               ? "QUICK"
                                          : index <= 24                               ? "SHORT"
                                          : index <= 28 || index == 30 || index == 31 ? "MEDIUM"
                                                                                      : "LONG";
                        const auto &point = find("INTERCEPTIONPOINT", "CHART_ROW",
                                                 {{"SOURCE_ROW", std::to_string(pointRoll)}});
                        const auto depthToken = field(point.fields, type);
                        pbp.block("CHECK", {{"LookupPurpose", "INTERCEPTION_POINT"}, {"RollRef", pbp.rollRef()},
                            {"SourceRef", "INTERCEPTIONPOINT:" + std::to_string(point.id)},
                            {"LookupRow", std::to_string(pointRoll)}, {"ColumnKey", type}, {"LookupResult", depthToken}});
                        const int depth = number(depthToken);
                        const int returnRoll = dice(20, "INTERCEPTION_RETURN_ROW", "WHITE"), returnRed = dice(10, "INTERCEPTION_RETURN_COLUMN", "RED");
                        auto rt = simpleChart("INTERCEPTIONRETURN", returnRoll, returnRed);
                        const int yards = rt == "B" ? breakaway(dc, defense, "CB") : number(rt);
                        ++stats.interceptions;
                        possession(defense, before + depth >= 100 ? 20 : std::max(1, 100 - before - depth),
                                   detail, "interception");
                        if (before + depth < 100)
                        {
                            s.spot += yards;
                            pbp.action("INTERCEPTION_RETURN", teamKeys[defense], yards);
                            detail += "Interception return=" + std::to_string(yards) + " yards.\n";
                            if (s.spot >= 100)
                                points(defense, 6, detail, "INTERCEPTION RETURN TOUCHDOWN");
                            else
                                s.lineToGain = std::min(100, s.spot + 10);
                        }
                    }
                }
                else if (token == "B")
                    gain = breakaway(dc, team, position);
                else if (token == "RH")
                    gain = rating(team, "RB", "RUN_HOLES");
                else if (token == "SR")
                {
                    pass = false;
                    for (int chain = 0;; ++chain)
                    {
                        if (chain == 32)
                            throw std::runtime_error("Scramble chain exceeded test limit");
                        const int scramble = dice(20, "SCRAMBLE", "WHITE") - 1;
                        const int scrambleRating = std::clamp(rating(team, "QB", "SCRAMBLE_RUN"), 0, 4);
                        const auto &record = find("SCRAMBLE", "CHART_ROW", {{"SOURCE_RATING", std::to_string(scrambleRating)}});
                        auto result = field(record.fields, "OUTCOME_" + std::to_string(scramble));
                        pbp.lookup("SCRAMBLE:" + std::to_string(record.id), scrambleRating, scrambleRating,
                                   scramble, result, 0, 0);
                        if (result == "INC")
                        {
                            incomplete = true;
                            pass = true;
                            break;
                        }
                        bool again = result.back() == '+';
                        if (again)
                            result.pop_back();
                        gain += number(result);
                        if (!again)
                            break;
                    }
                }
                else if (!incomplete)
                    gain = number(token);
                if (pass)
                {
                    ++stats.attempts;
                    if (!incomplete && !intercepted)
                    {
                        if (gain < 0 && card.name.find("Screen") == std::string::npos)
                        {
                            --stats.attempts;
                            ++stats.sacks;
                            stats.sackYards -= std::max(gain, -before);
                        }
                        else
                        {
                            ++stats.completions;
                            stats.passYards += std::clamp(gain, -before, 100 - before);
                        }
                    }
                }
                else if (!incomplete)
                {
                    ++stats.rushes;
                    stats.rushYards += std::clamp(gain, -before, 100 - before);
                }
                if (incomplete || intercepted)
                    seconds = 10;
                if (!intercepted)
                {
                    pbp.action(incomplete ? "INCOMPLETE_PASS" : !pass ? "RUSH" :
                               gain < 0 && card.name.find("Screen") == std::string::npos ? "SACK" : "COMPLETE_PASS",
                               teamKeys[team], gain);
                    if (!incomplete)
                        s.spot += gain;
                    detail += incomplete ? "Incomplete.\n" : "Gain " + std::to_string(gain) + " yards.\n";
                    if (s.spot >= 100)
                        points(team, 6, detail, "TOUCHDOWN");
                    else if (s.spot <= 0)
                    {
                        s.teams[defense].quarters[s.quarter - 1] += 2;
                        pbp.action("SAFETY", teamKeys[defense], 2);
                        safety = true;
                        detail += name(defense) + " SAFETY (+2).\n";
                        possession(defense, 20, detail, "safety restart");
                    }
                    else
                    {
                        if (s.spot >= s.lineToGain)
                        {
                            pbp.action("FIRST_DOWN", teamKeys[team], s.spot);
                            ++stats.firstDowns;
                            s.down = 1;
                            s.lineToGain = std::min(100, s.spot + 10);
                        }
                        else
                        {
                            ++s.down;
                            pbp.action("ADVANCE_DOWN", teamKeys[team], s.down);
                        }
                        if (blue == 1 && !incomplete)
                        {
                            const int fumbleRoll =
                                std::clamp(dice(20, "FUMBLE_CHECK", "WHITE") + rating(team, position, "FUMBLE"), 1, 20);
                            const auto &f =
                                find("FUMBLE", "CHART_ROW", {{"SOURCE_ROW", std::to_string(fumbleRoll)}});
                            const auto result = field(f.fields, "SOURCE_RESULT");
                            pbp.block("CHECK", {{"LookupPurpose", "FUMBLE"}, {"RollRef", pbp.rollRef()},
                                {"TriggerRollRef", specialRollRef},
                                {"SourceRef", "FUMBLE:" + std::to_string(f.id)},
                                {"LookupRow", std::to_string(fumbleRoll)}, {"LookupResult", result},
                                {"CheckOutcome", field(f.fields, "SOURCE_CODE")}});
                            detail += "Fumble check=" + std::to_string(fumbleRoll) + ".\n";
                            if (field(f.fields, "SOURCE_CODE") == "D")
                            {
                                ++stats.fumblesLost;
                                possession(defense, 100 - s.spot, detail, "fumble recovery");
                                const int yards =
                                    result == "B" ? breakaway(dc, defense, "CB") : number(result);
                                s.spot += yards;
                                pbp.action("FUMBLE_RETURN", teamKeys[defense], yards);
                                detail += "Fumble return " + std::to_string(yards) + " yards.\n";
                                if (s.spot >= 100)
                                    points(defense, 6, detail, "FUMBLE RETURN TOUCHDOWN");
                                else
                                    s.lineToGain = std::min(100, s.spot + 10);
                            }
                            else
                            {
                                pbp.action("FUMBLE_RETAINED", teamKeys[team], 0);
                                detail += "Offense retains possession.\n";
                            }
                        }
                    }
                }
            }
            text = "Q" + std::to_string(s.quarter) + " " + clockText(remaining) + " | " + name(team) + " | " +
                   std::to_string(down) + " & " + std::to_string(toGo) + " at own " + std::to_string(before) +
                   "\n" + detail;
            const int consumed = std::min(seconds, remaining);
            s.remaining -= consumed;
            s.elapsed += consumed;
            const bool awayScored = s.teams[0].score() != awayBefore;
            const bool scored = awayScored || s.teams[1].score() != homeBefore;
            if (s.quarter == 5 && scored)
                s.final = true;
            if (scored && s.quarter < 5 && (s.remaining || s.quarter == 1 || s.quarter == 3) &&
                !safety)
                s.pendingKick = awayScored ? 0 : 1;
        }
        if (s.elapsed != elapsedBefore)
        {
            if (remainingBefore > 450 && s.remaining <= 450 && s.quarter >= 2 && s.quarter <= 4)
                gamePlan(s.quarter == 2 ? "MID_Q2" : s.quarter == 3 ? "MID_Q3" : "MID_Q4");
            if (!s.remaining)
            {
                if (s.quarter < 4)
                    gamePlan(s.quarter == 1 ? "END_Q1" : s.quarter == 2 ? "END_Q2" : "END_Q3");
                pbp.block("PERIOD_END", {{"QUARTER", std::to_string(s.quarter)}});
                text += "=== END QUARTER " + std::to_string(s.quarter) + " ===\n";
                if (s.quarter == 5 || (s.quarter == 4 && s.teams[0].score() != s.teams[1].score()))
                    s.final = true;
            }
        }
        if (s.final)
        {
                text += "=== FINAL: " + game.awayName + " " + std::to_string(s.teams[0].score()) + " - " +
                            game.homeName + " " + std::to_string(s.teams[1].score()) +
                            (s.teams[0].score() == s.teams[1].score() ? " (TIE)" : "") + " ===\n";
        }
        pbp.block("CLOCK_ADVANCE", {{"ElapsedSeconds", std::to_string(s.elapsed - elapsedBefore)},
                                    {"RestartKind", s.pendingKick >= 0 ? "KICKOFF" : ""},
                                    {"KickingTeamKey", s.pendingKick >= 0 ? teamKeys[s.pendingKick] : ""}});
        p.period = s.quarter;
        p.remaining = s.remaining;
        p.elapsed = s.elapsed;
        p.awayScore = s.teams[0].score();
        p.homeScore = s.teams[1].score();
        p.final = s.final;
        p.text = text;
        p.teamStatistics = stats();
        return p;
    }
};
PffGameSession::PffGameSession(const std::filesystem::path &world, std::int64_t season, std::uint32_t seed)
    : m(std::make_unique<Impl>())
{
    m->world = world;
    m->season = season;
    m->seed = seed ? seed : std::uint32_t(std::chrono::steady_clock::now().time_since_epoch().count());
    m->random.seed(m->seed);
    m->load();
}
PffGameSession::~PffGameSession() = default;
WorldEditResult<WorldTestMatchup> PffGameSession::start(const WorldActor &actor, std::int64_t revision)
{
    auto r = beginTestMatchup(actor, m->season, revision, m->bindings);
    if (r.success)
    {
        m->game = r.value;
        m->s = State{};
        m->planTier = {};
        m->teamKeys = {m->game.awayCoreKey, m->game.homeCoreKey};
        m->seed = m->game.seed;
        m->random.seed(m->seed);
        m->lines = {"=== GAME: " + r.value.awayName + " at " + r.value.homeName + " ===",
                    "=== QUARTER 1 ==="};
        if (r.value.saved)
        {
            const auto &saved = *r.value.saved;
            m->s = saved.game;
            m->planTier = saved.planTier;
            m->lines = saved.log;
            std::istringstream in(saved.randomState);
            in.imbue(std::locale::classic());
            in >> m->random;
        }
        m->pbp.reset({{"GameID", std::to_string(m->game.gameId)}, {"FixtureKey", std::to_string(m->game.fixtureId)},
                      {"CompetitionSeasonKey", std::to_string(m->season)}, {"AwayTeamKey", m->teamKeys[0]},
                      {"HomeTeamKey", m->teamKeys[1]}, {"ProviderKey", "PFF_GENERIC_QUEUE_TEST"},
                      {"ProviderVersion", "2"}, {"ResolutionLevel", "GENERIC"}, {"CoreSnapshotRef", m->coreHash},
                      {"GameRulesProfileRef", "PFF_QUEUE_TEST/2"}, {"TimingProfileRef", "PFF_QUEUE_TEST_TIMING/1"},
                      {"GamePlanRef", "CORE_SOLITAIRE_BALANCED"}, {"InputBindingsHash", m->game.inputHash},
                      {"OutputCoverage", "TEAM_ONLY_TEST"}, {"TraceVersion", "1"}}, r.value.saved ? r.value.saved->replayHistory : "");
        m->interruption = r.value.interruption;
        m->pauseThrough = r.value.pauseThrough;
        m->suspended = bool(m->interruption);
        m->computed = false;
        m->pending.clear();
    }
    return r;
}
WorldEditResult<WorldTestMatchup> PffGameSession::viewFinal(const WorldActor &actor, std::int64_t game)
{
    auto result = loadFinalTestMatchup(actor, game);
    if (result.success)
    {
        try
        {
            std::istringstream input(result.value.saved->replayHistory);
            auto lines = PffPbp::readDisplayLog(input);
            m->game = result.value;
            m->s = result.value.saved->game;
            m->lines = std::move(lines);
        }
        catch (const std::exception &error)
        {
            result.success = false;
            result.error = WorldEditError::IntegrityFailure;
            result.message = error.what();
        }
    }
    return result;
}
WorldEditResult<WorldEditCommit> PffGameSession::next(const WorldActor &actor)
{
    WorldEditResult<WorldEditCommit> result;
    if (m->suspended)
    {
        result.error = WorldEditError::ControlConflict;
        result.message = "Resume the saved game before advancing";
        return result;
    }
    if (m->s.final)
    {
        result.message = "Game is already final";
        return result;
    }
    if (!m->game.gameId)
    {
        result.message = "Initialize the game before playing";
        return result;
    }
    if (!m->computed)
    {
        if (m->interruption)
            return save(actor); // Retry a failed automatic save without another play.
        // All current reduced-provider plays consume at most 40 seconds. Stop
        // before computing a crossing play; do not guess a decision or expose a
        // speculative outcome. A timing-capable provider can later split steps.
        const int through = m->s.elapsed + std::min(40, m->s.remaining);
        for (const auto &event : m->game.queuedEvents)
            if (!event.blocker.empty() && event.elapsedSeconds <= through)
            {
                m->interruption = event;
                m->pauseThrough = through;
                return save(actor);
            }
        // Snapshot only mutable football/RNG state, not thousands of cached cards
        // and chart cells. No database or filesystem access occurs in this block.
        const auto previous = m->s;
        const auto random = m->random;
        const auto plan = m->planTier;
        const auto replayMark = m->pbp.mark();
        const auto lineCount = m->lines.size(), stepCount = m->pending.size();
        try
        {
            auto p = m->advance();
           if (p.play)
                ++m->s.playOrdinal;
            p.playOrdinal = m->s.playOrdinal;
            std::string preview = p.text;
            for (const auto &event : m->game.queuedEvents)
                if (event.elapsedSeconds > previous.elapsed && event.elapsedSeconds <= p.elapsed)
                {
                    if (!event.blocker.empty())
                        throw std::runtime_error(event.blocker);
                    preview += event.notice;
                }
            m->pending.push_back(std::move(p));
            std::istringstream in(preview);
            std::string line;
            while (std::getline(in, line))
                m->lines.push_back(line);
            m->pbp.complete();
            m->computed = m->s.final;
            m->s.final = false; // FINAL means the buffered game was durably published.
        }
        catch (const std::exception &e)
        {
            m->s = previous;
            m->random = random;
            m->planTier = plan;
            m->computed = false;
            m->lines.resize(lineCount);
            m->pending.resize(stepCount);
            result.message = e.what();
            try
            {
                const auto folder = m->game.pbpPath.parent_path();
                m->pbp.writePartial(folder / ("Game-" + std::to_string(m->game.gameId) + ".partial.pbp"),
                                    "RESOLUTION", result.message);
            }
            catch (const std::exception &diagnostic)
            {
                result.message += "; ";
                result.message += diagnostic.what();
            }
            m->pbp.rewind(replayMark);
            result.error = WorldEditError::IntegrityFailure;
            return result;
        }
    }
    if (m->computed)
    {
        // A failed final save retains this exact completed buffer for retry;
        // it never rerolls a play or discards the in-memory PBP.
        const auto saved = m->snapshot(true);
        std::string committedLog;
        result = finalizeTestMatchup(actor, m->game.gameId, m->pending, m->game.commit.revision, &committedLog, &saved,
                                     [this](std::string_view log) { return m->taggedFinal(log); });
        if (result.success)
        {
            std::istringstream input(committedLog);
            m->lines.clear();
            for (std::string line; std::getline(input, line);)
                m->lines.push_back(std::move(line));
            m->s.final = true;
            m->game.commit = result.value;
        }
        else
        {
            try
            {
                const auto folder = m->game.pbpPath.parent_path();
                m->pbp.writePartial(folder / ("Game-" + std::to_string(m->game.gameId) + ".partial.pbp"),
                                    "FINAL_PUBLICATION", result.message);
            }
            catch (const std::exception &diagnostic)
            {
                result.message += "; ";
                result.message += diagnostic.what();
            }
        }
        return result;
    }
    result.success = true;
    result.value = m->game.commit;
    result.value.changed = false;
    return result;
}
WorldEditResult<WorldEditCommit> PffGameSession::save(const WorldActor &actor)
{
    if (m->computed && !m->s.final)
        return next(actor);
    WorldEditResult<WorldEditCommit> result;
    if (!m->game.gameId)
    {
        result.error = WorldEditError::InvalidValue;
        result.message = "No initialized game";
        return result;
    }
    if (m->s.final)
    {
        result.success = true;
        result.value = m->game.commit;
        result.value.changed = false;
        return result;
    }
    auto saved = m->snapshot();
    if (m->pending.empty() && !m->suspended)
        ++saved.game.generation;
    result = saveTestMatchup(actor, m->game.gameId, m->pending, saved, m->game.commit.revision,
                            m->pauseThrough);
    if (result.success)
    {
        m->s = saved.game;
        m->game.commit = result.value;
        m->pending.clear();
        m->suspended = true;
    }
    return result;
}
WorldEditResult<WorldTestMatchup> PffGameSession::resume(const WorldActor &actor)
{
    WorldEditResult<WorldTestMatchup> result;
    if (!m->interruption)
    {
        result.error = WorldEditError::InvalidValue;
        result.message = "The game is not waiting for an event";
        return result;
    }
    if (!m->suspended)
    {
        const auto saved = save(actor);
        if (!saved.success)
        {
            result.error = saved.error;
            result.message = saved.message;
            return result;
        }
    }
    const auto current = loadWorldEditState(actor);
    if (!current.success)
    {
        result.error = current.error;
        result.message = current.message;
        return result;
    }
    return start(actor, current.value.revision);
}
const std::optional<WorldTestQueuedEvent> &PffGameSession::interruption() const
{
    return m->interruption;
}
const State &PffGameSession::state() const
{
    return m->s;
}
const WorldTestMatchup &PffGameSession::matchup() const
{
    return m->game;
}
const std::vector<std::string> &PffGameSession::log() const
{
    return m->lines;
}
std::string_view PffGameSession::taggedHistory() const
{
    if (m->s.final && m->game.saved && m->game.saved->game.final)
        return m->game.saved->replayHistory;
    return m->pbp.history();
}
std::string PffGameSession::statistics() const
{
    return m->stats();
}
} // namespace PffTest
