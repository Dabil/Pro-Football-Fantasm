// GameTime model implementation. Resolves football plays, records tagged PBP, and saves or restores matchups.

#include "Database/GameTimeModel.h"
#include "Database/GroundRules.h"
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
    std::array<std::string, 20> defensivePositions;
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
struct GameTimeModel::Impl
{
    std::filesystem::path world;
    std::int64_t season = 0;
    std::uint32_t seed = 0;
    std::string bindings, coreHash;
    mutable PffPbp::Recorder pbp;
    State s;
    GroundRules::Settings groundRules;
    bool gameClockRunning = false;
    bool extendedPlayClock = false;
    int kickoffSpot = 35;
    struct TryKickoffPenalty
    {
        int foulingTeam = -1;
        int yards = 0;
    } tryKickoffPenalty;
    bool onsideRekickUsed = false;
    bool liveKickErased = false; // Per-step evidence, cleared before each resolution.
    bool acceptedKickContact = false;
    bool kickoffRekick = false;
    bool kickoffPenaltyPossession = false;
    bool snapPrevented = false;
    std::map<std::string, int> teamPenaltyRatings;
    std::array<std::array<int, 2>, 3> timeouts{};
    int timeoutTeam() const
    {
        return s.pendingKick >= 0 ? s.pendingKick : humanTeams[s.offense] ? s.offense : 1 - s.offense;
    }
    std::array<bool, 2> humanTeams{};
    bool cpuTakeover = false;
    bool humanControls(int team) const
    {
        return !cpuTakeover && humanTeams[team];
    }
    bool cpuCallsFourthDownKick() const
    {
        return s.down == 4 && !humanControls(s.offense);
    }
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
                   {"CLOCK_STATUS", gameClockRunning ? "RUNNING" : "STOPPED"}, {"GAME_ELAPSED_SECONDS", std::to_string(s.elapsed)},
                   {"AWAY_SCORE", std::to_string(s.teams[0].score())}, {"HOME_SCORE", std::to_string(s.teams[1].score())},
                   {"DOWN", scrimmage ? std::to_string(s.down) : ""},
                   {"TOGO", scrimmage ? std::to_string(s.lineToGain - s.spot) : ""},
                   {"POSSESSION_TEAM_KEY", scrimmage ? teamKeys[s.offense] : ""},
                   {"BALL_AT_TEAM_KEY", scrimmage ? teamKeys[s.spot <= 50 ? s.offense : 1 - s.offense] : teamKeys[kickingTeam]},
                   {"BALL_AT_YARDLINE", std::to_string(scrimmage ? std::min(s.spot, 100 - s.spot) : kickoffSpot)}});
        if (kind == "PLAY" || kind == "PENALTY")
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
    const Record* scrimmagePenalty(bool pass, int blue, int white)
    {
        if (!groundRules.enabled[GroundRules::Penalties] || (blue != 2 && blue != 3))
            return nullptr;
        const int roll = white;
        const auto& penalty = find("PENALTY", "PENALTY_RESULT",
            {{"D20", std::to_string(roll)}, {"PLAY_TYPE", pass ? "PASS" : "RUN"},
             {"PENALIZED_SIDE", blue == 3 ? "OFFENSE" : "DEFENSE"}});
        pbp.block("PENALTY_CHECK", {{"SourceRef", "PENALTY:" + std::to_string(penalty.id)},
            {"LookupRow", std::to_string(roll)}, {"CheckOutcome", field(penalty.fields, "RESULT_TYPE")}});
        return field(penalty.fields, "RESULT_TYPE") == "PENALTY" ? &penalty : nullptr;
    }

    bool automaticPenalty(int blue, int adjustment, int threshold, const char* foul)
    {
        if (blue != 4 || !groundRules.enabled[GroundRules::Penalties]) return false;
        // Blue 4 has its own threshold, separate from the Blue 2/3 table.
        // A cleared check leaves the original play dice available to the caller.
        const int roll = dice(20, "AUTOMATIC_PENALTY", "WHITE");
        const int adjusted = roll + adjustment;
        pbp.block("PENALTY_CHECK", {{"PenaltyKey", foul}, {"RollRef", pbp.rollRef()},
            {"LookupRow", std::to_string(roll)}, {"Adjustment", std::to_string(adjustment)},
            {"AdjustedValue", std::to_string(adjusted)}, {"Threshold", std::to_string(threshold)},
            {"CheckOutcome", adjusted <= threshold ? "PENALTY" : "NO_PENALTY"}});
        return adjusted <= threshold;
    }

    bool illegalProcedure(int team, int blue)
    {
        const auto rating = teamPenaltyRatings.find(teamKeys[team]);
        return automaticPenalty(blue, rating == teamPenaltyRatings.end() ? 0 : rating->second,
            10, "ILLEGAL_PROCEDURE");
    }

    void enforceIllegalProcedure(int team, std::string& log)
    {
        // The whistle prevents the snap. Keep the down and line to gain.
        const int yards = std::min(5, s.spot / 2);
        s.spot -= yards;
        snapPrevented = true;
        pbp.block("PENALTY_ENFORCEMENT", {{"PenaltyKey", "ILLEGAL_PROCEDURE"}, {"TeamKey", teamKeys[team]},
            {"Yards", std::to_string(yards)}, {"EnforcementSpot", "PREVIOUS_SPOT"}, {"PlayResultStands", "0"}});
        log += "Illegal procedure. Repeat the down.\n";
    }

    std::optional<int> enforceReturnBlock(int receiver, int start, int end, int blue, int white, std::string& log)
    {
        // Ruling 2: The shared kick chart does not identify when or where a block
        // occurred on a kick without a positive return. Keep the ordinary spot.
        if (end <= start || !groundRules.enabled[GroundRules::Penalties] || blue != 2)
            return std::nullopt;
        const int first = std::clamp(start, 1, 99);
        const int last = std::min(end, 99);
        if (last < first)
            return std::nullopt;
        const auto& penalty = find("PENALTY", "PENALTY_RESULT",
            {{"D20", std::to_string(white)}, {"PLAY_TYPE", "KICK"}, {"PENALIZED_SIDE", "RECEIVING_TEAM"}});
        const auto key = field(penalty.fields, "PENALTY_KEY");
        if (key != "CLIPPING" && key != "ILLEGAL_BLOCK" && key != "ILLEGAL_USE_OF_HANDS")
            return std::nullopt;

        // Ruling 1: Generic has no individual block locations. Give every
        // whole-yard spot along the positive return in the field an equal chance.
        const int foulSpot = std::uniform_int_distribution<int>(first, last)(random);
        const int nominal = number(field(penalty.fields, "PENALTY_YARDS"));
        const int yards = nominal * 2 > foulSpot ? foulSpot / 2 : nominal;
        const int finalSpot = foulSpot - yards;
        pbp.block("PENALTY_CHECK", {{"SourceRef", "PENALTY:" + std::to_string(penalty.id)},
            {"LookupRow", std::to_string(white)}, {"CheckOutcome", "PENALTY"}});
        pbp.block("PENALTY_ENFORCEMENT", {{"PenaltyKey", key}, {"TeamKey", teamKeys[receiver]},
            {"Yards", std::to_string(yards)}, {"EnforcementSpot", "SPOT_OF_FOUL"},
            {"ReturnStartSpot", std::to_string(start)}, {"ApparentEndSpot", std::to_string(end)},
            {"FoulSpot", std::to_string(foulSpot)}, {"FinalSpot", std::to_string(finalSpot)},
            {"PlayResultStands", "1"}});
        log += field(penalty.fields, "SOURCE_PENALTY_NAME") + " at own " + std::to_string(foulSpot) +
            ". " + std::to_string(yards) + " yards enforced. Ball at own " + std::to_string(finalSpot) + ".\n";
        return finalSpot;
    }

    struct OnsideKickResult
    {
        int receiverSpot;
        int recoverySpot;
        bool kickingRecovery = true;
        bool receiverAwarded = false;
    };

    bool enforceRekick(int team, int blue, int white, bool kickoff, int& spot, std::string& log,
                       OnsideKickResult* onside = nullptr)
    {
        if (!groundRules.enabled[GroundRules::Penalties] || blue != 3)
            return false;
        const auto& penalty = find("PENALTY", "PENALTY_RESULT",
            {{"D20", std::to_string(white)}, {"PLAY_TYPE", "KICK"}, {"PENALIZED_SIDE", "KICKING_TEAM"}});
        const auto key = field(penalty.fields, "PENALTY_KEY");
        // Ruling 3: Offsides applies to either kick. Motion and downfield
        // eligibility require a scrimmage formation, so apply them only on punts.
        if (field(penalty.fields, "REKICK") != "TRUE" ||
            (key != "OFFSIDES" && (kickoff || (key != "ILLEGAL_MOTION" && key != "ILLEGAL_MAN_DOWNFIELD"))))
            return false;
        pbp.block("PENALTY_CHECK", {{"SourceRef", "PENALTY:" + std::to_string(penalty.id)},
            {"LookupRow", std::to_string(white)}, {"CheckOutcome", "PENALTY"}});
        // The approved 1989 onside ruling treats offside as a live-ball foul.
        // After one re-kick the receivers get the dead-ball spot either way.
        // On the first attempt they keep a recovery and get five yards.
        if (onside && (onsideRekickUsed || !onside->kickingRecovery))
        {
            const int deadBallSpot = std::clamp(onside->receiverSpot, 1, 99);
            const int baseSpot = onsideRekickUsed ? deadBallSpot : std::clamp(onside->recoverySpot, 1, 99);
            const int yards = onsideRekickUsed ? 0 : std::min(5, (100 - baseSpot) / 2);
            const int finalSpot = baseSpot + yards;
            onside->receiverSpot = finalSpot;
            onside->receiverAwarded = true;
            kickoffPenaltyPossession = true;
            pbp.block("PENALTY_ENFORCEMENT", {{"PenaltyKey", key}, {"TeamKey", teamKeys[team]},
                {"Disposition", onsideRekickUsed ? "ONSIDE_LIMIT_AWARD" : "ONSIDE_RECEIVER_KEEPS"},
                {"EnforcementSpot", onsideRekickUsed ? "DEAD_BALL_SPOT" : "RECOVERY_SPOT"},
                {"RecoverySpot", std::to_string(onside->recoverySpot)}, {"DeadBallSpot", std::to_string(deadBallSpot)},
                {"FinalSpot", std::to_string(finalSpot)}, {"Yards", std::to_string(yards)},
                {"PlayResultStands", onside->kickingRecovery ? "0" : "1"}});
            log += onsideRekickUsed ? "Onside offsides again. No further re-kick.\n" :
                "Onside offsides. Receiving team keeps the ball with " + std::to_string(yards) + " yards enforced.\n";
            return false;
        }
        if (onside) onsideRekickUsed = true;
        kickoffRekick = kickoff;
        liveKickErased = true;
        const int previous = spot;
        const int yards = std::min(number(field(penalty.fields, "PENALTY_YARDS")), spot / 2);
        spot -= yards;
        pbp.block("PENALTY_ENFORCEMENT", {{"PenaltyKey", key}, {"TeamKey", teamKeys[team]},
            {"Yards", std::to_string(yards)}, {"EnforcementSpot", "PREVIOUS_SPOT"},
            {"OriginalKickSpot", std::to_string(previous)}, {"RekickSpot", std::to_string(spot)},
            {"OnsideRekick", onside ? "1" : "0"},
            {"PlayResultStands", "0"}});
        log += field(penalty.fields, "SOURCE_PENALTY_NAME") + ". Re-kick from " +
            std::to_string(spot) + " yards from the kicking team's goal line.\n";
        return true;
    }

    bool enforceKickContact(int team, int blue, int white, bool madeFieldGoal, std::string& log)
    {
        // Verified kicker contact uses the previous spot and can retain
        // possession even when the kick produced a fair catch or touchback.
        // Other loose-kick fouls need their own timing and spot rulings.
        if (!groundRules.enabled[GroundRules::Penalties] || blue != 2)
            return false;
        const auto& penalty = find("PENALTY", "PENALTY_RESULT",
            {{"D20", std::to_string(white)}, {"PLAY_TYPE", "KICK"}, {"PENALIZED_SIDE", "RECEIVING_TEAM"}});
        const auto key = field(penalty.fields, "PENALTY_KEY");
        const bool roughing = key == "ROUGHING_THE_KICKER";
        if (!roughing && key != "RUNNING_INTO_THE_KICKER")
            return false;
        const int nominal = number(field(penalty.fields, "PENALTY_YARDS"));
        const int distance = 100 - s.spot;
        const int yards = nominal * 2 > distance ? distance / 2 : nominal;
        const bool firstDown = roughing || s.spot + yards >= s.lineToGain;
        // Ruling 5: Keep a made field goal unless accepting the foul
        // earns a new series. Use actual enforcement, including half-distance.
        const bool declined = madeFieldGoal && !firstDown;
        pbp.block("PENALTY_CHECK", {{"SourceRef", "PENALTY:" + std::to_string(penalty.id)},
            {"LookupRow", std::to_string(white)}, {"CheckOutcome", "PENALTY"}});
        if (declined)
        {
            pbp.block("PENALTY_ENFORCEMENT", {{"PenaltyKey", key}, {"TeamKey", teamKeys[1 - team]},
                {"Yards", "0"}, {"Disposition", "DECLINED"}, {"PlayResultStands", "1"}});
            log += field(penalty.fields, "SOURCE_PENALTY_NAME") + " declined. The field goal stands.\n";
            return false;
        }
        s.spot += yards;
        liveKickErased = true;
        acceptedKickContact = true;
        if (firstDown)
        {
            ++s.teams[team].firstDowns;
            s.down = 1;
            s.lineToGain = std::min(100, s.spot + 10);
        }
        pbp.block("PENALTY_ENFORCEMENT", {{"PenaltyKey", key}, {"TeamKey", teamKeys[1 - team]},
            {"Yards", std::to_string(yards)}, {"EnforcementSpot", "PREVIOUS_SPOT"},
            {"Disposition", "ACCEPTED"}, {"ApparentFieldGoalGood", madeFieldGoal ? "1" : "0"},
            {"PlayResultStands", "0"}, {"FirstDownAwarded", firstDown ? "1" : "0"}});
        log += field(penalty.fields, "SOURCE_PENALTY_NAME") + " enforced for " +
            std::to_string(yards) + " yards. " + (firstDown ? "First down.\n" : "Repeat the down.\n");
        return true;
    }

    void enforceScrimmagePenalty(const Record& penalty, const Play& card, int index,
                                 const State& snap, std::string& detail)
    {
        const auto& fields = penalty.fields;
        const auto key = field(fields, "PENALTY_KEY");
        const bool offense = field(fields, "PENALIZED_SIDE") == "OFFENSE";
        const bool afterPlay = key == "LATE_HIT" || key == "FACE_MASK" || key == "UNNECESSARY_ROUGHNESS";
        if (!afterPlay) s = snap;
        const int team = offense ? snap.offense : 1 - snap.offense;
        const int direction = team == s.offense ? -1 : 1;
        int yards = 0;
        if (key == "PASS_INTERFERENCE" && fields.count("PENALTY_YARDS_TOKEN"))
        {
            const auto depth = index <= 18 ? "SCREEN" : index <= 21 ? "QUICK" :
                index <= 24 ? "SHORT" : index <= 28 || index == 30 || index == 31 ? "MEDIUM" : "LONG";
            const int roll = dice(20, "PENALTY_INTERFERENCE_SPOT", "WHITE");
            const auto& point = find("INTERCEPTIONPOINT", "CHART_ROW", {{"SOURCE_ROW", std::to_string(roll)}});
            yards = std::clamp(number(field(point.fields, depth)), 0, 99 - s.spot);
        }
        else
        {
            yards = number(field(fields, "PENALTY_YARDS"));
            if (key == "INTENTIONAL_GROUNDING")
                for (const auto& row : card.outcomes)
                    for (const auto& token : row)
                        if (card.name.find("Screen") == std::string::npos && !token.empty() &&
                            token.front() == '-' && token.find_first_not_of("0123456789", 1) == std::string::npos)
                            yards = std::max(yards, -number(token));
            const int distance = direction < 0 ? s.spot : 100 - s.spot;
            if (yards * 2 > distance) yards = distance / 2;
        }
        s.spot += direction * yards;
        const bool firstDown = (field(fields, "AUTOMATIC_FIRST_DOWN") == "TRUE" && direction > 0) ||
            s.spot >= s.lineToGain;
        if (firstDown)
        {
            ++s.teams[s.offense].firstDowns;
            s.down = 1;
            s.lineToGain = std::min(100, s.spot + 10);
        }
        else if (field(fields, "LOSS_OF_DOWN") == "TRUE")
            ++s.down;
        pbp.block("PENALTY_ENFORCEMENT", {{"PenaltyKey", key}, {"TeamKey", teamKeys[team]},
            {"Yards", std::to_string(yards)}, {"PlayResultStands", afterPlay ? "1" : "0"},
            {"FirstDownAwarded", firstDown ? "1" : "0"}});
        detail += field(fields, "SOURCE_PENALTY_NAME") + " enforced for " + std::to_string(yards) + " yards.\n";
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
    int breakaway(const std::string &defense)
    {
        const int behind = defense == "D" || defense == "B" ? 3 : defense == "M" || defense == "Z" ? 5 : 4;
        const int column = dice(10, "BREAKAWAY_COLUMN", "RED");
        const int row = dice(20, "BREAKAWAY_ROW", "WHITE") - behind;
        return number(simpleChart("BREAKAWAY", row, column));
    }
    std::string special(const std::string &name, int row, int col, bool allowPuntCatchInterference = false)
    {
        const auto &parent = find("SPECIAL_TEAMS", "SPECIAL_TEAM_PLAY", {{"NAME", name}});
        for (const auto &r : records)
            if (r.parent == parent.id && (r.type == "DISTANCE_ROW" || r.type == "RETURN_ROW") &&
                field(r.fields, "D20") == std::to_string(std::clamp(row, 1, 20)))
            {
                auto token = field(r.fields, "OUTCOME_" + std::to_string(col));
                // The explicit chart outcome is supported only for a punt
                // expiring the period with penalties enabled. Its ordinary
                // outcome is a catch at the punt spot with no return.
                if (token == "PUNT_CATCH_INTERFERENCE" && !allowPuntCatchInterference)
                    token = "0";
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
                              "SELECT m.WhiteD20,o.D10,o.ResultToken,m.DefensivePosition FROM OffensivePlayMatchups m JOIN "
                              "OffensivePlayOutcomes o USING(MatchupID) WHERE m.OffensivePlayID=?",
                              {play.id});
            int count = 0;
            while (outcomes.row())
            {
                int row = int(outcomes.integer()), col = int(outcomes.integer(1));
                if (row < 1 || row > 20 || col < 0 || col > 9)
                    throw std::runtime_error("Invalid card dimensions");
                play.outcomes[row - 1][col] = outcomes.text(2);
                play.defensivePositions[row - 1] = outcomes.text(3);
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
        // Core stores the printed team penalty rating under this legacy key.
        S::Query penalties(db, "SELECT t.TeamSeasonKey,r.RatingValue FROM TeamSeasonRatings r "
            "JOIN TeamSeasons t USING(TeamSeasonID) WHERE r.RulesetID=? AND r.RatingKey='DEFENSIVE_PENALTY'", {ruleset});
        while (penalties.row()) teamPenaltyRatings[penalties.text()] = int(penalties.integer(1));
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
                   "\nQuarters=4x900 plus sudden-death OT; chart kickoffs, kicker-card PAT and FG.\n"
                   "Automatic scrimmage penalties; no wild plays, weather, injury, endurance, or player statistics. "
                   "Return checkpoints support deterministic resume.\nTiming is original PFF 40/10/20 "
                   "seconds; historical elapsed equals consumed "
                   "clock in this test.\nUndated unrelated season-event templates remain unexecuted. "
                   "A=play-action pass; RH=zero Generic adjustment; chart limits clamp.\n"
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
            if (resolveTry(team, log))
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
    int blockRoll(std::string &log, int kickingTeam = -1, int* rawWhite = nullptr, int* rawBlue = nullptr)
    {
        const int white = dice(20, "KICK_BLOCK_CHECK", "WHITE"), blue = dice(20, "KICK_BLOCK_SPECIAL", "BLUE");
        if (rawWhite) *rawWhite = white;
        if (rawBlue) *rawBlue = blue;
        const int adjusted = kickingTeam < 0 ? white :
            white + rating(kickingTeam, "K", "BLOCKED") + rating(1 - kickingTeam, "P", "BLOCKED");
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
        const int yards = result == "B" ? breakaway("R") : number(result);
        const int spot = 100 - s.spot + yards;
        pbp.action("KICK_BLOCKED", teamKeys[1 - team], yards);
        possession(1 - team, spot, log, "blocked kick recovery");
        if (spot >= 100)
            points(1 - team, 6, log, "BLOCKED KICK RETURN TOUCHDOWN");
    }
    bool resolveTry(int team, std::string& log)
    {
        int trySpot = 98; // The 1989 one-point try starts at the opponent's two.
        for (;;)
        {
            int white = 0, blue = 0;
            pbp.block("TRY_ATTEMPT", {{"BallSpot", std::to_string(trySpot)}});
            bool prevented = false;
            const bool good = placeKick(team, true, log, nullptr, &white, &blue, &prevented);
            if (prevented)
            {
                const int yards = std::min(5, trySpot / 2);
                trySpot -= yards;
                pbp.block("TRY_PENALTY", {{"PenaltyKey", "ILLEGAL_PROCEDURE"}, {"TeamKey", teamKeys[team]},
                    {"Yards", std::to_string(yards)}, {"Disposition", "REPLAY_TRY"},
                    {"AdjustedSpot", std::to_string(trySpot)}, {"GameClockSeconds", "0"}});
                continue;
            }
            if (!groundRules.enabled[GroundRules::Penalties] || (blue != 2 && blue != 3))
                return good;
            const bool kickingFoul = blue == 3;
            const auto& penalty = find("PENALTY", "PENALTY_RESULT",
                {{"D20", std::to_string(white)}, {"PLAY_TYPE", "KICK"},
                 {"PENALIZED_SIDE", kickingFoul ? "KICKING_TEAM" : "RECEIVING_TEAM"}});
            const auto key = field(penalty.fields, "PENALTY_KEY");
            // Ruling 4: Use formation fouls and kicker contact on tries.
            // Return blocks and loose-kick fouls require their own rulings.
            if (kickingFoul ? key != "OFFSIDES" && key != "ILLEGAL_MOTION" && key != "ILLEGAL_MAN_DOWNFIELD"
                            : key != "ROUGHING_THE_KICKER" && key != "RUNNING_INTO_THE_KICKER")
                return good;
            // Ruling 6: Keep the point for receiving-team kicker contact and
            // move the next kickoff by the chart yardage. Running into the
            // kicker carries five yards under the approved Generic policy.
            // The 1989 Eagles gamebook shows a five-yard carryover but labels it roughing.
            const bool carry = good && !kickingFoul;
            const int nominal = number(field(penalty.fields, "PENALTY_YARDS"));
            const int yards = carry ? nominal : std::min(nominal, (kickingFoul ? trySpot : 100 - trySpot) / 2);
            if (carry)
                tryKickoffPenalty = {1 - team, yards};
            else
                trySpot += kickingFoul ? -yards : yards;
            pbp.block("PENALTY_CHECK", {{"SourceRef", "PENALTY:" + std::to_string(penalty.id)},
                {"LookupRow", std::to_string(white)}, {"CheckOutcome", "PENALTY"}});
            pbp.block("TRY_PENALTY", {{"PenaltyKey", key}, {"TeamKey", teamKeys[kickingFoul ? team : 1 - team]},
                {"Yards", std::to_string(yards)}, {"Disposition", carry ? "KICKOFF_CARRYOVER" : "REPLAY_TRY"},
                {"AdjustedSpot", carry ? "" : std::to_string(trySpot)}, {"GameClockSeconds", "0"}});
            log += field(penalty.fields, "SOURCE_PENALTY_NAME") + " on the try. " +
                (carry ? "The point stands. Enforce on the kickoff.\n" : "Replay the try.\n");
            if (carry)
                return true;
        }
    }

    bool placeKick(int team, bool extraPoint, std::string &log, bool* returned = nullptr,
                   int* tryWhite = nullptr, int* tryBlue = nullptr, bool* tryPrevented = nullptr)
    {
        const std::string label = extraPoint ? "Extra point" : "Field goal";
        int white = 0, blue = 0;
        const int block = blockRoll(log, team, &white, &blue);
        if (tryWhite) *tryWhite = white;
        if (tryBlue) *tryBlue = blue;
        if (illegalProcedure(team, blue))
        {
            if (tryPrevented) *tryPrevented = true;
            else enforceIllegalProcedure(team, log);
            return false;
        }
        if (block <= 1)
        {
            pbp.action(extraPoint ? "EXTRA_POINT_BLOCKED" : "FIELD_GOAL_BLOCKED", teamKeys[1 - team], 0);
            log += label + " blocked.\n";
            if (!extraPoint)
            {
                if (enforceKickContact(team, blue, white, false, log))
                    return false;
                blockedKick(team, log);
                if (returned) *returned = true;
            }
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
        if (!extraPoint && enforceKickContact(team, blue, white, good, log))
            return false;
        if (!extraPoint && !good)
            possession(1 - team, std::max(20, yard), log, "missed field goal");
        return good;
    }
    bool kickoff(int team, std::string &log, bool allowOnside = false, std::string_view choice = {})
    {
        const int receiver = 1 - team;
        const bool onside = choice == "Onsides Kick" || (choice.empty() && allowOnside && s.quarter == 4 &&
                            s.remaining < 180 && s.teams[team].score() < s.teams[receiver].score());
        // The sequence has already used its one re-kick even if the kicking
        // team changes its kick call on the second attempt.
        const bool onsideEnforcement = onside || onsideRekickUsed;
        const bool squib = choice == "Squib Kick";
        const std::string chart = onside ? "Onsides Kick" : squib ? "Squib Kick" : "Normal Kick";
        const int distanceRoll = dice(20, "KICKOFF_DISTANCE_ROW", "WHITE"), distanceColumn = dice(10, "KICKOFF_DISTANCE_COLUMN", "RED");
        const auto distance = special(chart, distanceRoll, distanceColumn);
        // Ruling 9: The kick's special die governs the whole sequence. The
        // return dice resolve yardage only, so a return cannot draw a second flag.
        const int blue = groundRules.enabled[GroundRules::Penalties]
            ? dice(20, "KICKOFF_SPECIAL", "BLUE") : 0;
        const auto ratingKey = onside ? "KICK_ONSIDES" : squib ? "KICK_SQUIB" : "KICK_NORMAL";
        const bool forcedOut = blue == 4 && automaticPenalty(blue, rating(team, "K", ratingKey),
            16, "KICKOFF_OUT_OF_BOUNDS");
        if (forcedOut)
        {
            // Use the original distance as the longitudinal spot when available.
            // The bare OB/TB chart tokens provide no in-field exit yard line.
            const int exit = distance == "OB" || distance == "TB" ? 0 :
                100 - kickoffSpot - number(distance);
            const int award = std::max(35, exit);
            pbp.block("KICKOFF_DISTANCE", {{"KickingTeamKey", teamKeys[team]}, {"LookupResult", "OB"}});
            pbp.block("PENALTY_ENFORCEMENT", {{"PenaltyKey", "KICKOFF_OUT_OF_BOUNDS"}, {"TeamKey", teamKeys[team]},
                {"Yards", "5"}, {"EnforcementSpot", "RECEIVER_AWARD"},
                {"FinalSpot", std::to_string(award)}, {"PlayResultStands", "1"}});
            possession(receiver, award, log, "kickoff out of bounds");
            kickoffSpot = 35;
            onsideRekickUsed = false;
            return false;
        }
        pbp.block("KICKOFF_DISTANCE", {{"KickingTeamKey", teamKeys[team]}, {"LookupResult", distance}});
        log += name(team) + " " + chart + ": " + distance + ".\n";
        if (distance == "TB" || (distance != "OB" && !onside && number(distance) >= 100 - kickoffSpot))
        {
            OnsideKickResult award{20, 20};
            if (enforceRekick(team, blue, distanceRoll, true, kickoffSpot, log, onsideEnforcement ? &award : nullptr))
            {
                s.pendingKick = team;
                return false;
            }
            possession(receiver, award.receiverSpot, log, "kickoff touchback");
            kickoffSpot = 35;
            onsideRekickUsed = false;
            return false;
        }
        if (distance == "OB")
        {
            OnsideKickResult award{35, 35};
            if (enforceRekick(team, blue, distanceRoll, true, kickoffSpot, log, onsideEnforcement ? &award : nullptr))
            {
                s.pendingKick = team;
                return false;
            }
            possession(receiver, award.receiverSpot, log, "kickoff out of bounds");
            kickoffSpot = 35;
            onsideRekickUsed = false;
            return false;
        }
        const int landing = kickoffSpot + number(distance);
        const int returnRoll = dice(20, "KICK_RETURN_ROW", "WHITE"), returnColumn = dice(10, "KICK_RETURN_COLUMN", "RED");
        const auto result = special(chart + " Return", returnRoll, returnColumn);
        const bool kickingRecovery = result.rfind("KICK_REC_", 0) == 0;
        int returnTeam = kickingRecovery ? team : receiver;
        const auto token = kickingRecovery ? result.substr(9) : result;
        const int yards = token == "B" ? breakaway("R") : number(token);
        const int start = kickingRecovery ? landing : 100 - landing;
        int spot = start + yards;
        pbp.action(kickingRecovery ? "ONSIDE_RECOVERY" : "KICKOFF_RETURN", teamKeys[returnTeam], yards);
        OnsideKickResult onsideResult{kickingRecovery ? 100 - spot : spot,
            kickingRecovery ? 100 - start : start, kickingRecovery};
        if (enforceRekick(team, blue, distanceRoll, true, kickoffSpot, log, onsideEnforcement ? &onsideResult : nullptr))
        {
            s.pendingKick = team;
            return true;
        }
        // A kicking-team recovery on the flagged re-kick flips the field
        // coordinates and the next possession. Unflagged recoveries stay put.
        if (onsideResult.receiverAwarded)
        {
            returnTeam = receiver;
            spot = onsideResult.receiverSpot;
        }
        if (!onside && !kickingRecovery && yards > 0 && groundRules.enabled[GroundRules::Penalties])
        {
            if (const auto enforced = enforceReturnBlock(receiver, start, spot, blue, distanceRoll, log))
            {
                spot = *enforced;
                kickoffPenaltyPossession = true;
            }
        }
        possession(returnTeam, spot, log, onsideResult.receiverAwarded ? "onside offsides enforcement" :
            chart + " return " + result);
        kickoffSpot = 35;
        onsideRekickUsed = false;
        if (spot < 100)
            return true;
        points(returnTeam, 6, log, "KICKOFF RETURN TOUCHDOWN");
        if (s.quarter == 5)
        {
            s.final = true;
            return true;
        }
        s.pendingKick = returnTeam;
        return true;
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
    WorldTestGameStep advance(std::string_view choice, int decisionSeconds)
    {
        snapPrevented = false;
        liveKickErased = false;
        acceptedKickContact = false;
        kickoffRekick = false;
        kickoffPenaltyPossession = false;
        const bool untimedDown = s.untimedDownPending;
        bool kickOrSnapPrevented = false;
        constexpr int runSeconds = 40;
        WorldTestGameStep p;
        p.generation = ++s.generation;
        const int remainingBefore = s.remaining, elapsedBefore = s.elapsed;
        const bool manualTiming = !choice.empty() && groundRules.enabled[GroundRules::PlayClock];
        std::string text;
        bool clockRunsAfterPlay = false;
        bool puntFirstTouch = false;
        bool puntCatchInterference = false;
        extendedPlayClock = false;
        if (decisionSeconds > 0)
        {
            const int consumed = std::min(s.remaining, decisionSeconds);
            s.remaining -= consumed;
            s.elapsed += consumed;
        }
        const bool warningDuringDecision = decisionSeconds > 0 && (s.quarter == 2 || s.quarter == 4) &&
            remainingBefore > 120 && s.remaining == 120;
        if (warningDuringDecision || (remainingBefore > 0 && s.remaining == 0))
        {
            p.play = false;
            beginRecord("DECISION_CLOCK_EXPIRED");
        }
        else if (s.quarter == 1 && s.playOrdinal == 0 && s.pendingKick < 0)
        {
            const int receiver = toss("OPENING");
            pbp.complete();
            text = "Opening coin flip: " + std::string(receiver ? "heads" : "tails") +
                   ", " + name(receiver) + " receives.\n";
            if (humanControls(1 - receiver))
            {
                s.pendingKick = 1 - receiver;
                p.play = false;
                beginRecord("KICKOFF_SETUP");
            }
            else
            {
                beginRecord("PLAY", "KICKOFF", 1 - receiver);
                const int consumed = kickoff(1 - receiver, text) ? runSeconds : 20;
                s.remaining -= consumed;
                s.elapsed += consumed;
            }
        }
        else if (s.remaining == 0 && !untimedDown)
        {
            extendedPlayClock = true;
            ++s.quarter;
            s.remaining = 900;
            p.play = false;
            beginRecord("PERIOD_CHANGE");
            pbp.block("PERIOD_CHANGE", {{"QUARTER", std::to_string(s.quarter)}, {"CLOCK", clockText(s.remaining)}});
            if (s.quarter == 5)
                pbp.complete();
            text = "=== QUARTER " + std::to_string(s.quarter) + " ===\n";
            // A kickoff opening a half or overtime starts a new sequence.
            if (onsideRekickUsed && (s.quarter == 3 || s.quarter == 5))
            {
                kickoffSpot = 35;
                onsideRekickUsed = false;
            }
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
        else if (choice == "TIMEOUT")
        {
            extendedPlayClock = true;
            const int team = timeoutTeam();
            ++timeouts[(s.quarter - 1) / 2][team];
            beginRecord("TIMEOUT");
            pbp.block("TEAM_TIMEOUT", {{"TeamKey", teamKeys[team]}, {"Period", std::to_string(s.quarter)}});
            pbp.action("TEAM_TIMEOUT", teamKeys[team], 0);
            text = name(team) + " Time-Out.\n";
            p.play = false;
        }
        else if (choice == "DELAY_OF_GAME")
        {
            kickOrSnapPrevented = true;
            const int team = s.pendingKick >= 0 ? s.pendingKick : s.offense;
            beginRecord("PENALTY", s.pendingKick >= 0 ? "KICKOFF" : "SCRIMMAGE", s.pendingKick);
            const int yards = std::min(5, (s.pendingKick < 0 ? s.spot : kickoffSpot) / 2);
            if (s.pendingKick < 0)
                s.spot = std::max(1, s.spot - yards);
            else
                kickoffSpot -= yards;
            pbp.action("DELAY_OF_GAME", teamKeys[team], yards);
            pbp.block("PENALTY_ENFORCEMENT", {{"PenaltyKey", "DELAY_OF_GAME"},
                {"TeamKey", teamKeys[team]}, {"Yards", std::to_string(yards)}, {"PlayResultStands", "0"}});
            text = name(team) + " Delay of Game.\n";
            p.play = false;
        }
        else if (s.pendingKick >= 0)
        {
            const int kickingTeam = s.pendingKick;
            // A try penalty is against the team that fouled. Halftime can
            // reverse who kicks, so establish the spot only after the actual
            // kicking team is known. Consume it once, even if a re-kick follows.
            const auto carry = tryKickoffPenalty;
            if (carry.yards)
                kickoffSpot = std::max(1, kickoffSpot + (kickingTeam == carry.foulingTeam ? -carry.yards : carry.yards));
            tryKickoffPenalty = {};
            beginRecord("PLAY", "KICKOFF", kickingTeam);
            if (carry.yards)
            {
                pbp.block("TRY_KICKOFF_CARRYOVER", {{"FoulingTeamKey", teamKeys[carry.foulingTeam]},
                    {"KickingTeamKey", teamKeys[kickingTeam]}, {"Yards", std::to_string(carry.yards)},
                    {"AdjustedSpot", std::to_string(kickoffSpot)}});
                text += "Extra-point penalty against " + name(carry.foulingTeam) + ". " + name(kickingTeam) +
                    " kicks off from its " + std::to_string(kickoffSpot) + ".\n";
            }
            s.pendingKick = -1;
            const int consumed = std::min(kickoff(kickingTeam, text, true, choice)
                ? (manualTiming ? 10 : runSeconds) : 20, s.remaining);
            s.remaining -= consumed;
            s.elapsed += consumed;

            // Offside and a block after the receivers gain possession settle
            // the kickoff without extending the period. A change of ends
            // preserves a re-kick or the enforced first-down spot. Halftime
            // and overtime start a separate kickoff sequence.
            if (!s.remaining && (kickoffRekick || kickoffPenaltyPossession))
            {
                const bool nextQuarter = s.quarter == 1 || s.quarter == 3;
                const auto next = nextQuarter ? std::to_string(s.quarter + 1) : "";
                if (kickoffRekick)
                {
                    pbp.block("KICKOFF_REKICK_TIMING", {{"KickingTeamKey", teamKeys[kickingTeam]},
                        {"Disposition", nextQuarter ? "NEXT_QUARTER" : "CANCELED"},
                        {"NextQuarter", next}, {"RekickSpot", nextQuarter ? std::to_string(kickoffSpot) : ""}});
                    text += nextQuarter ? "Re-kick to open quarter " + next + ".\n" :
                        "Re-kick canceled because the half ended.\n";
                }
                else
                {
                    pbp.block("KICKOFF_POSSESSION_TIMING", {{"ReceivingTeamKey", teamKeys[s.offense]},
                        {"Disposition", nextQuarter ? "NEXT_QUARTER" : "HALF_ENDED"},
                        {"NextQuarter", next}, {"AwardedSpot", std::to_string(s.spot)}});
                    text += nextQuarter ? "The receiving team opens quarter " + next + " at the awarded spot.\n" :
                        "The half ends before the receiving team can snap.\n";
                }
                if (!nextQuarter)
                {
                    s.pendingKick = -1;
                    kickoffSpot = 35;
                    onsideRekickUsed = false;
                }
            }
            if (!s.remaining && s.quarter == 4)
                s.pendingKick = -1;
        }
        else if (choice == "DOWN_BALL")
        {
            const int team = s.offense;
            beginRecord("PLAY", "SCRIMMAGE");
            pbp.action("DOWN_BALL", teamKeys[team], 0);
            ++s.teams[team].attempts;
            text = name(team) + " spikes the ball.\n";
            if (++s.down > 4)
                possession(1 - team, 100 - s.spot, text, "turnover on downs");
            else
                pbp.action("ADVANCE_DOWN", teamKeys[team], s.down);
            const int consumed = std::min(5, s.remaining);
            s.remaining -= consumed;
            s.elapsed += consumed;
        }
        else
        {
            const int team = s.offense, defense = 1 - team, before = s.spot, down = s.down,
                      remaining = s.remaining, toGo = s.lineToGain - s.spot;
            const int awayBefore = s.teams[0].score(), homeBefore = s.teams[1].score();
            const bool fieldGoal = choice == "Field Goal" || (cpuCallsFourthDownKick() && before >= 70);
            const bool coffin = choice == "Coffin Corner";
            const bool punt = choice == "Normal Punt" || coffin || (cpuCallsFourthDownKick() && !fieldGoal);
            beginRecord("PLAY", fieldGoal ? "FIELD_GOAL" : punt ? "PUNT" : "SCRIMMAGE");
            auto &stats = s.teams[team];
            int seconds = runSeconds;
            bool returnTiming = false;
            bool penaltyStopsClock = false;
            bool outOfBounds = false;
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
            if (fieldGoal || punt)
            {
                seconds = 20;
                if (fieldGoal)
                {
                    if (placeKick(team, false, detail, &returnTiming))
                        points(team, 3, detail, "FIELD GOAL");
                }
                else
                {
                    const bool attemptBlock = s.quarter >= 4 && stats.score() > s.teams[defense].score();
                    int white = 0, blue = 0;
                    const int block = attemptBlock ? blockRoll(detail, -1, &white, &blue) : 3;
                    if (attemptBlock && illegalProcedure(team, blue))
                        enforceIllegalProcedure(team, detail);
                    else if (block <= 1)
                    {
                        if (!enforceKickContact(team, blue, white, false, detail))
                        {
                            // A re-kick supersedes the blocked kick and any recovery.
                            if (enforceRekick(team, blue, white, false, s.spot, detail))
                                pbp.action("PUNT_BLOCKED", teamKeys[defense], 0);
                            else
                                blockedKick(team, detail);
                            returnTiming = true;
                        }
                    }
                    else
                    {
                        const int distanceRoll = dice(20, "KICK_ROW", "WHITE"), red = dice(10, "KICK_COLUMN", "RED");
                        const auto token = special(coffin ? "Coffin Corner" : "Normal Punt", distanceRoll, red);
                        // A block attempt already supplied the punt's special die.
                        // Reuse that check through the return to avoid a second foul.
                        const int specialRoll = attemptBlock ? blue : groundRules.enabled[GroundRules::Penalties]
                            ? dice(20, "PUNT_SPECIAL", "BLUE") : 0;
                        if (!attemptBlock && illegalProcedure(team, specialRoll))
                            enforceIllegalProcedure(team, detail);
                        else if (!enforceKickContact(team, specialRoll, attemptBlock ? white : distanceRoll, false, detail))
                        {
                            // Keep the original series when the chart erases the punt.
                            const auto beforePunt = s;
                            bool returnTouchdown = false;
                            bool firstTouch = false;
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
                                    result = special(coffin ? "Coffin Punt Return" : "Normal Punt Return",
                                        returnRoll, returnColumn, groundRules.enabled[GroundRules::Penalties] &&
                                            remaining <= (manualTiming ? 10 : seconds));
                                    // Only an explicit chart result identifies who touched
                                    // the punt first. The punt's ending spot is the touch
                                    // spot for this result, which has no return yardage.
                                    firstTouch = result == "FIRST_TOUCH";
                                    puntCatchInterference = result == "PUNT_CATCH_INTERFERENCE";
                                    returnTiming = !firstTouch && !puntCatchInterference && result != "FC" && result != "OB";
                                }
                                const int yards = firstTouch || puntCatchInterference || result == "FC" || result == "OB" ? 0 :
                                                  result == "B" ? breakaway("R") : number(result);
                                if (!firstTouch && !puntCatchInterference)
                                    pbp.action("PUNT_RETURN", teamKeys[defense], yards, result);
                                const int start = 100 - before - distance;
                                // Fair catches and other non-returns keep the kick's spot.
                                int spot = yards > 0
                                    ? enforceReturnBlock(defense, start, start + yards,
                                        specialRoll, attemptBlock ? white : distanceRoll, detail).value_or(start + yards)
                                    : start + yards;
                                if (puntCatchInterference)
                                {
                                    // The chart explicitly identifies interference at the
                                    // catch spot. The receivers take their untimed snap after
                                    // enforcing 15 yards (or half the distance) from there.
                                    const int enforced = std::min(15, (100 - start) / 2);
                                    spot += enforced;
                                    pbp.block("PENALTY_ENFORCEMENT", {{"PenaltyKey", "PUNT_CATCH_INTERFERENCE"},
                                        {"TeamKey", teamKeys[team]}, {"SpotTeamKey", teamKeys[defense]},
                                        {"EnforcementSpot", "SPOT_OF_FOUL"}, {"FoulSpot", std::to_string(start)},
                                        {"Yards", std::to_string(enforced)}, {"FinalSpot", std::to_string(spot)}});
                                    detail += "Punt catch interference. " + std::to_string(enforced) + " yards enforced.\n";
                                }
                                possession(defense, spot, detail, puntCatchInterference ? "punt catch interference" :
                                    firstTouch ? "punt first touching" : "punt return " + result);
                                returnTouchdown = spot >= 100;
                            }
                            int rekickSpot = beforePunt.spot;
                            if (!puntCatchInterference &&
                                enforceRekick(team, specialRoll, attemptBlock ? white : distanceRoll, false, rekickSpot, detail))
                            {
                                s = beforePunt;
                                s.spot = rekickSpot;
                            }
                            else if (firstTouch)
                            {
                                puntFirstTouch = true;
                                pbp.block("PUNT_FIRST_TOUCH", {{"KickingTeamKey", teamKeys[team]},
                                    {"ReceivingTeamKey", teamKeys[defense]}, {"Spot", std::to_string(s.spot)}});
                            }
                            else if (returnTouchdown)
                                points(defense, 6, detail, "PUNT RETURN TOUCHDOWN");
                        }
                    }
                }
            }
            else
            {
                const bool manualOffense = humanControls(team);
                const auto selected = std::find_if(plays.begin(), plays.end(),
                    [&](const Play& play) { return play.key == choice; });
                const int callRoll = manualOffense ? 0 : std::clamp(dice(10, "OFFENSE_CALL", "RED") + plan(team, "OFFENSE_RED_D10"), 0, 9);
                const auto call = manualOffense ?
                    (selected->type == "pass" ? "P" : "R") :
                    field(find("SOLITAIRE", "PLAY_CALL_ROW",
                                             {{"SIDE", "OFFENSE"},
                                              {"COACHING_PHILOSOPHY", "BALANCED"},
                                              {"RED_D10", std::to_string(callRoll)}})
                                            .fields,
                                        std::string("SITUATION_") + situation);
                const int selection = manualOffense ? 0 : std::clamp(dice(20, "OFFENSE_SELECTION", "BLUE") + plan(team, "OFFENSE_BLUE_D20"), 1, 20),
                          playerRoll = dice(20, "PLAYER_SELECTION", "WHITE");
                const int index = manualOffense ? static_cast<int>(selected - plays.begin()) :
                    number(field(find("SOLITAIRE", "PLAY_SELECTION_ROW",
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
                    const bool manualDefense = !manualOffense && humanControls(defense);
                const int defRoll = manualDefense ? 0 : std::clamp(dice(10, "DEFENSE_CALL", "RED") + plan(defense, "DEFENSE_RED_D10"), 0, 9);
                const auto dc = manualDefense ? std::string(choice) : field(find("SOLITAIRE", "PLAY_CALL_ROW",
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
                const auto& defender = card.defensivePositions[row - 1];
                auto token = card.outcomes[row - 1][red];
                if (token == "INT" && groundRules.supported[GroundRules::Interceptions] &&
                    !groundRules.enabled[GroundRules::Interceptions])
                    token = "INC";
                const auto specialRollRef = pbp.rollRef();
                const auto snap = s;
                if (illegalProcedure(team, blue))
                {
                    enforceIllegalProcedure(team, detail);
                    penaltyStopsClock = true;
                    seconds = 0;
                    p.play = false;
                }
                else
                {
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
                        {"OffenseCallRollRef", manualOffense ? "" : pbp.rollRef(manualDefense ? 5 : 6)},
                        {"OffenseSelectionRollRef", manualOffense ? "" : pbp.rollRef(manualDefense ? 4 : 5)},
                        {"PlayerSelectionRollRef", pbp.rollRef(manualDefense ? 3 : 4)},
                        {"DefenseCallRollRef", manualDefense ? "" : pbp.rollRef(3)},
                        {"DefenseCallValue", manualDefense ? "" : std::to_string(defRoll)},
                        {"OffenseCallValue", manualOffense ? "" : std::to_string(callRoll)},
                        {"SelectedPosition", position}, {"SpecialRollRef", pbp.rollRef()}});
                    detail = card.name + " / defense " + dc + " [" +
                             (manualOffense ? std::string("human offense") :
                                 "call D10=" + std::to_string(callRoll) + ", play D20=" + std::to_string(selection)) +
                             ", player D20=" + std::to_string(playerRoll) + ", " +
                             (manualDefense ? std::string("human defense") : "defense D10=" + std::to_string(defRoll)) +
                             "; white=" + std::to_string(raw) +
                             " adj=" + std::to_string(adjustment) + " row=" + std::to_string(row) +
                             ", red=" + std::to_string(red) + ", blue=" + std::to_string(blue) + "] => " + token +
                             ".\n";
                    bool pass = card.type == "pass", incomplete = token == "INC", intercepted = false;
                    int gain = 0;
                    if (token == "INT")
                    {
                        const int ir = dice(20, "INTERCEPTION_CHECK", "WHITE"),
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
                            const int yards = rt == "B" ? breakaway(dc) : number(rt);
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
                        gain = breakaway(dc);
                    else if (token == "RH")
                        gain = 0;
                    else if (token == "SR")
                    {
                        pass = false;
                        for (int chain = 0;; ++chain)
                        {
                            if (chain == 32)
                                throw std::runtime_error("Scramble chain exceeded test limit");
                            const int scramble = dice(20, "SCRAMBLE", "WHITE") - 1;
                            constexpr int scrambleRating = 0;
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
                    if (intercepted)
                        returnTiming = true;
                    else if (incomplete && token != "SR")
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
                            if (blue == 1 && !incomplete &&
                                (!groundRules.supported[GroundRules::Fumbles] || groundRules.enabled[GroundRules::Fumbles]))
                            {
                                const int fumbleRoll =
                                    dice(20, "FUMBLE_CHECK", "WHITE");
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
                                        result == "B" ? breakaway(dc) : number(result);
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
                    outOfBounds = !incomplete && !intercepted && s.offense == team &&
                        (defender == "CB" || defender == "S" || defender == "OLB" || defender == "LB" ||
                         defender == "A" || defender == "B2" || defender == "C2" || defender == "D");
                    if (outOfBounds)
                        pbp.block("BOUNDARY", {{"Result", "OUT_OF_BOUNDS"}});
                    if (blue == 2 || blue == 3)
                        if (const auto* penalty = scrimmagePenalty(card.type == "pass", blue, raw))
                        {
                            enforceScrimmagePenalty(*penalty, card, index, snap, detail);
                            penaltyStopsClock = true;
                        }
                }
            }
            if (s.down > 4)
                possession(defense, 100 - s.spot, detail, "turnover on downs");
            text = "Q" + std::to_string(s.quarter) + " " + clockText(remaining) + " | " + name(team) + " | " +
                   std::to_string(down) + " & " + std::to_string(toGo) + " at own " + std::to_string(before) +
                   "\n" + detail;
            if (returnTiming) seconds = runSeconds;
            if (snapPrevented)
            {
                seconds = 0;
                p.play = false;
                penaltyStopsClock = true;
            }
            const int consumed = std::min(manualTiming
                ? std::min(seconds, 10) : seconds, remaining);
            s.remaining -= consumed;
            s.elapsed += consumed;
            const bool awayScored = s.teams[0].score() != awayBefore;
            const bool scored = awayScored || s.teams[1].score() != homeBefore;
            const bool lateOutOfBounds = outOfBounds &&
                ((s.quarter == 2 && s.remaining <= 120) || (s.quarter == 4 && s.remaining <= 300));
            clockRunsAfterPlay = !lateOutOfBounds && !penaltyStopsClock && !fieldGoal && !punt && seconds == 40 && !scored &&
                s.offense == team && s.remaining > 0;
            if (s.quarter == 5 && scored)
                s.final = true;
            if (scored && s.quarter < 5 && (s.remaining || s.quarter == 1 || s.quarter == 3) &&
                !safety)
                s.pendingKick = awayScored ? 0 : 1;
        }
        gameClockRunning = clockRunsAfterPlay;
        // Accepted contact preserves the kicking team's series. Explicit punt
        // first touching and catch interference give the receivers a snap
        // from the awarded spot. A prevented snap leaves the extension pending.
        const bool untimedAward = !s.remaining && (acceptedKickContact || puntFirstTouch || puntCatchInterference);
        s.untimedDownPending = untimedAward || (untimedDown && !p.play);
        if (untimedAward || (untimedDown && p.play))
        {
            pbp.block("UNTIMED_DOWN", {{"Played", untimedDown && p.play ? "1" : "0"},
                {"Awarded", untimedAward ? "1" : "0"}, {"Quarter", std::to_string(s.quarter)},
                {"TeamKey", untimedAward ? teamKeys[s.offense] : ""},
                {"EnforcedSpot", untimedAward ? std::to_string(s.spot) : ""}});
            if (untimedAward)
                text += "The period is extended for an untimed down.\n";
        }
        if ((s.quarter == 2 || s.quarter == 4) && remainingBefore > 120 &&
            s.remaining > 0 && s.remaining <= 120)
        {
            extendedPlayClock = true;
            gameClockRunning = false;
            pbp.block("OFFICIAL_TIMEOUT", {{"Reason", "TWO_MINUTE_WARNING"}});
            text += "Two-minute warning.\n";
        }
        if (s.elapsed != elapsedBefore || (untimedDown && p.play))
        {
            if (remainingBefore > 450 && s.remaining <= 450 && s.quarter >= 2 && s.quarter <= 4)
                gamePlan(s.quarter == 2 ? "MID_Q2" : s.quarter == 3 ? "MID_Q3" : "MID_Q4");
            if (!s.remaining && !s.untimedDownPending)
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
            // The point counts, but a game ending in regulation has no
            // succeeding kickoff on which to enforce the accepted try foul.
            if (tryKickoffPenalty.yards)
            {
                pbp.block("TRY_KICKOFF_UNENFORCED", {{"FoulingTeamKey", teamKeys[tryKickoffPenalty.foulingTeam]},
                    {"Yards", std::to_string(tryKickoffPenalty.yards)}, {"Disposition", "NO_ENSUING_KICKOFF"}});
                text += "Extra-point penalty yardage unenforced. The game ends without another kickoff.\n";
                tryKickoffPenalty = {};
            }
            text += "=== FINAL: " + game.awayName + " " + std::to_string(s.teams[0].score()) + " - " +
                game.homeName + " " + std::to_string(s.teams[1].score()) +
                (s.teams[0].score() == s.teams[1].score() ? " (TIE)" : "") + " ===\n";
        }
        // Ruling 8: A dead-ball delay prevents the action. Charge any prior
        // decision time separately. A live kick keeps its normal clock charge
        // even when enforcement erases the result from the official statistics.
        if (liveKickErased || kickOrSnapPrevented || snapPrevented)
            pbp.block("PENALTY_CLOCK", {{"Action", liveKickErased ? "LIVE_KICK_ERASED" : "ACTION_PREVENTED"},
                {"GameClockSeconds", std::to_string(s.elapsed - elapsedBefore - decisionSeconds)}});
        pbp.block("CLOCK_ADVANCE", {{"ElapsedSeconds", std::to_string(s.elapsed - elapsedBefore)},
                                    {"GameClockRunning", gameClockRunning ? "1" : "0"},
                                    {"ExtendedPlayClock", extendedPlayClock ? "1" : "0"},
                                    {"KickoffSpot", std::to_string(kickoffSpot)},
                                    {"PendingTryFoulingTeam", std::to_string(tryKickoffPenalty.foulingTeam)},
                                    {"PendingTryKickoffYards", std::to_string(tryKickoffPenalty.yards)},
                                    {"OnsideRekickUsed", onsideRekickUsed ? "1" : "0"},
                                    {"RestartKind", s.pendingKick >= 0 ? "KICKOFF" : ""},
                                    {"KickingTeamKey", s.pendingKick >= 0 ? teamKeys[s.pendingKick] : ""}});
        p.period = s.quarter;
        p.remaining = s.remaining;
        p.elapsed = s.elapsed;
        p.awayScore = s.teams[0].score();
        p.homeScore = s.teams[1].score();
        p.final = s.final;
        p.untimedDownPending = s.untimedDownPending;
        p.text = text;
        p.teamStatistics = stats();
        return p;
    }
};
GameTimeModel::GameTimeModel(const std::filesystem::path &world, std::int64_t season, std::uint32_t seed)
    : m(std::make_unique<Impl>())
{
    m->world = world;
    m->season = season;
    m->seed = seed ? seed : std::uint32_t(std::chrono::steady_clock::now().time_since_epoch().count());
    m->random.seed(m->seed);
    m->load();
}
GameTimeModel::~GameTimeModel() = default;
WorldEditResult<WorldTestMatchup> GameTimeModel::start(const WorldActor &actor, std::int64_t revision)
{
    const auto options = loadWorldLeagueOptions(actor, m->season);
    if (!options.success)
    {
        WorldEditResult<WorldTestMatchup> failure;
        failure.error = options.error;
        failure.message = options.message;
        return failure;
    }
    auto r = beginTestMatchup(actor, m->season, revision, m->bindings);
    if (r.success)
    {
        m->groundRules = GroundRules::resolve(options.value);
        m->game = r.value;
        m->s = State{};
        m->gameClockRunning = false;
        m->extendedPlayClock = false;
        m->kickoffSpot = 35;
        m->tryKickoffPenalty = {};
        m->onsideRekickUsed = false;
        m->timeouts = {};
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
            for (std::size_t team = 0; team < m->teamKeys.size(); ++team)
            {
                const std::string key = "[TeamKey]" + m->teamKeys[team] + "\n[Period]";
                const auto& history = saved.replayHistory;
                for (auto at = history.find(key); at != std::string::npos; at = history.find(key, at + key.size()))
                {
                    const auto end = at + key.size();
                    if (end < history.size() && history[end] >= '1' && history[end] <= '5')
                        ++m->timeouts[(history[end] - '1') / 2][team];
                }
            }
            constexpr std::string_view clockTag = "[GameClockRunning]";
            const auto clock = saved.replayHistory.rfind(clockTag);
            m->gameClockRunning = clock != std::string::npos &&
                saved.replayHistory.compare(clock + clockTag.size(), 1, "1") == 0;
            constexpr std::string_view extendedTag = "[ExtendedPlayClock]";
            const auto extended = saved.replayHistory.rfind(extendedTag);
            m->extendedPlayClock = extended != std::string::npos &&
                saved.replayHistory.compare(extended + extendedTag.size(), 1, "1") == 0;
            constexpr std::string_view spotTag = "[KickoffSpot]";
            constexpr std::string_view onsideTag = "[OnsideRekickUsed]";
            const auto onside = saved.replayHistory.rfind(onsideTag);
            m->onsideRekickUsed = onside != std::string::npos &&
                saved.replayHistory.compare(onside + onsideTag.size(), 1, "1") == 0;
            const auto spot = saved.replayHistory.rfind(spotTag);
            if (spot != std::string::npos)
            {
                const auto start = spot + spotTag.size();
                const auto end = saved.replayHistory.find('\n', start);
                m->kickoffSpot = std::clamp(number(saved.replayHistory.substr(start, end - start)), 1, 50);
            }
            const auto checkpointValue = [&](std::string_view tag, int fallback) {
                const auto at = saved.replayHistory.rfind(tag);
                if (at == std::string::npos)
                    return fallback;
                const auto start = at + tag.size();
                return number(saved.replayHistory.substr(start, saved.replayHistory.find('\n', start) - start));
            };
            const int foulingTeam = checkpointValue("[PendingTryFoulingTeam]", -1);
            const int yards = checkpointValue("[PendingTryKickoffYards]", 0);
            if (foulingTeam >= 0 && foulingTeam <= 1 && yards > 0 && yards <= 15)
                m->tryKickoffPenalty = {foulingTeam, yards};
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
WorldEditResult<WorldTestMatchup> GameTimeModel::viewFinal(const WorldActor &actor, std::int64_t game)
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
void GameTimeModel::setHumanTeams(std::array<bool, 2> teams)
{
    m->humanTeams = teams;
}

void GameTimeModel::setCpuTakeover(bool enabled)
{
    m->cpuTakeover = enabled;
}

int GameTimeModel::playClockSeconds() const
{
    return m->groundRules.enabled[GroundRules::PlayClock] ? m->groundRules.clockSeconds : -1;
}

int GameTimeModel::decisionPlayClockSeconds() const
{
    const int normal = playClockSeconds();
    return normal > 0 && m->extendedPlayClock ? 90 : normal;
}

bool GameTimeModel::decisionGameClockRunning() const
{
    return m->gameClockRunning;
}

int GameTimeModel::decisionClockBoundary() const
{
    const auto& s = m->s;
    return (s.quarter == 2 || s.quarter == 4) && s.remaining > 120 ? 120 : 0;
}

bool GameTimeModel::canCallTimeout() const
{
    if (playCalls().empty()) return false;
    return m->timeouts[(m->s.quarter - 1) / 2][m->timeoutTeam()] < (m->s.quarter == 5 ? 2 : 3);
}

std::vector<PlayCall> GameTimeModel::playCalls() const
{
    const auto& s = m->s;
    if (s.final || (!s.remaining && !s.untimedDownPending) || m->interruption || m->computed ||
        (s.quarter == 1 && s.playOrdinal == 0 && s.pendingKick < 0))
        return {};
    if (s.pendingKick >= 0)
    {
        if (!m->humanControls(s.pendingKick)) return {};
        return {{CallGroup::Kickoff, "Normal Kick", "Normal Kick"}, {CallGroup::Kickoff, "Squib Kick", "Squib Kick"},
                {CallGroup::Kickoff, "Onsides Kick", "Onsides Kick"}};
    }
    if (m->humanControls(s.offense))
    {
        std::vector<PlayCall> calls;
        for (const auto& play : m->plays)
            calls.push_back({play.type == "pass" ? CallGroup::Passing : CallGroup::Running, play.name, play.key});
        calls.push_back({CallGroup::Passing, "Quarterback Spike", "DOWN_BALL"});
        for (const auto* name : {"Normal Punt", "Coffin Corner", "Field Goal"})
            calls.push_back({CallGroup::SpecialTeams, name, name});
        return calls;
    }
    if (m->humanControls(1 - s.offense) && !m->cpuCallsFourthDownKick())
        return {{CallGroup::Defense, "Short", "S"}, {CallGroup::Defense, "Run-Key RB1", "K"},
                {CallGroup::Defense, "Run", "N"}, {CallGroup::Defense, "Read", "R"},
                {CallGroup::Defense, "Dog", "D"}, {CallGroup::Defense, "Blitz", "B"},
                {CallGroup::Defense, "Pass-Man", "M"}, {CallGroup::Defense, "Pass-Zone", "Z"}};
    return {};
}

WorldEditResult<WorldEditCommit> GameTimeModel::next(const WorldActor &actor, std::string_view choice, int decisionSeconds)
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
        const auto calls = playCalls();
        const bool clockCommand = !calls.empty() &&
            ((choice == "TIMEOUT" && canCallTimeout()) || (choice == "DELAY_OF_GAME" && playClockSeconds() > 0));
        if (!clockCommand && (!calls.empty() || !choice.empty()) &&
            std::none_of(calls.begin(), calls.end(), [&](const auto& call) { return call.key == choice; }))
        {
            result.error = WorldEditError::InvalidValue;
            result.message = "Select an available play call";
            return result;
        }
        if (m->interruption)
            return save(actor); // Retry a failed automatic save without another play.
        // All current reduced-provider plays consume at most 40 seconds. Stop
        // before computing a crossing play; do not guess a decision or expose a
        // speculative outcome. A timing-capable provider can later split steps.
        decisionSeconds = !choice.empty() && playClockSeconds() > 0 && m->gameClockRunning
            ? std::clamp(decisionSeconds, 0, m->s.remaining - decisionClockBoundary()) : 0;
        const int through = m->s.elapsed + std::min(40 + decisionSeconds, m->s.remaining);
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
        const bool previousClockRunning = m->gameClockRunning;
        const bool previousExtendedPlayClock = m->extendedPlayClock;
        const int previousKickoffSpot = m->kickoffSpot;
        const auto previousTryKickoffPenalty = m->tryKickoffPenalty;
        const bool previousOnsideRekickUsed = m->onsideRekickUsed;
        const auto previousTimeouts = m->timeouts;
        const auto random = m->random;
        const auto plan = m->planTier;
        const auto replayMark = m->pbp.mark();
        const auto lineCount = m->lines.size(), stepCount = m->pending.size();
        try
        {
            auto p = m->advance(choice, decisionSeconds);
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
            m->gameClockRunning = previousClockRunning;
            m->extendedPlayClock = previousExtendedPlayClock;
            m->kickoffSpot = previousKickoffSpot;
            m->tryKickoffPenalty = previousTryKickoffPenalty;
            m->onsideRekickUsed = previousOnsideRekickUsed;
            m->timeouts = previousTimeouts;
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
WorldEditResult<WorldEditCommit> GameTimeModel::save(const WorldActor &actor)
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
WorldEditResult<WorldTestMatchup> GameTimeModel::resume(const WorldActor &actor)
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
const std::optional<WorldTestQueuedEvent> &GameTimeModel::interruption() const
{
    return m->interruption;
}
const State &GameTimeModel::state() const
{
    return m->s;
}
const WorldTestMatchup &GameTimeModel::matchup() const
{
    return m->game;
}
const std::vector<std::string> &GameTimeModel::log() const
{
    return m->lines;
}
std::string_view GameTimeModel::taggedHistory() const
{
    if (m->s.final && m->game.saved && m->game.saved->game.final)
        return m->game.saved->replayHistory;
    return m->pbp.history();
}
std::string GameTimeModel::statistics() const
{
    return m->stats();
}
} // namespace PffTest
