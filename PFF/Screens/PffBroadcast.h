#pragma once

#include "Database/PbpRecorder.h"
#include <algorithm>
#include <cstdlib>

namespace PffBroadcast
{
enum class View { Broadcast, Detail, Pbp };

struct Play
{
    std::vector<PffPbp::Tag> tags;

    std::string value(std::string_view block, std::string_view field) const
    {
        const auto begin = std::string(block) + "_BEGIN";
        const auto end = std::string(block) + "_END";
        bool inside = false;
        for (const auto &tag : tags)
        {
            if (tag.name == begin)
                inside = true;
            else if (tag.name == end)
                break;
            else if (inside && tag.name == field)
                return tag.value;
        }
        return {};
    }
};

struct Teams
{
    std::string awayKey, homeKey, away, home;

    std::string name(const std::string &key) const
    {
        if (key == awayKey && !key.empty())
            return away;
        if (key == homeKey && !key.empty())
            return home;
        return key;
    }

    const std::string &opponent(const std::string &key) const { return key == awayKey ? home : away; }
};

inline std::vector<Play> readPlays(std::istream &input)
{
    std::vector<Play> plays;
    Play record;
    bool isPlay = false;
    for (auto &tag : PffPbp::read(input))
    {
        if (tag.name == "RECORD_BEGIN" && tag.depth == 1)
        {
            record.tags.clear();
            isPlay = false;
        }
        if (tag.depth == 2 && tag.name == "RecordKind")
            isPlay = tag.value == "PLAY" || tag.value == "PENALTY";
        if (!record.tags.empty() || (tag.depth == 1 && tag.name == "RECORD_BEGIN"))
            record.tags.push_back(std::move(tag));
        if (!record.tags.empty() && record.tags.back().name == "RECORD_END" && record.tags.back().depth == 1)
        {
            if (isPlay)
                plays.push_back(std::move(record));
            record.tags.clear();
        }
    }
    return plays;
}

inline std::vector<Play> latestPlay(std::string_view history)
{
    const auto playTag = history.rfind("\n[PLAY_ID]");
    const auto penaltyTag = history.rfind("\n[RecordKind]PENALTY\n");
    if (playTag == std::string_view::npos && penaltyTag == std::string_view::npos)
        return {};
    const auto play = playTag == std::string_view::npos ? penaltyTag :
        penaltyTag == std::string_view::npos ? playTag : std::max(playTag, penaltyTag);
    const auto begin = history.rfind("[RECORD_BEGIN]\n", play);
    const auto end = history.find("\n[RECORD_END]\n", play);
    if (begin == std::string_view::npos || end == std::string_view::npos)
        return {};
    std::istringstream input("[PBP_BEGIN]\n[FORMAT_VERSION]2\n[DATA_VERSION]1\n" +
                             std::string(history.substr(begin, end + 14 - begin)) + "[PBP_END]\n");
    return readPlays(input);
}

inline int number(const std::string &value)
{
    return value.empty() ? 0 : std::stoi(value);
}

inline std::string possessive(const std::string &team)
{
    return team + (!team.empty() && team.back() == 's' ? "'" : "'s");
}

inline std::string yardLine(const std::string &team, const std::string &opponent, int spot)
{
    if (spot == 50)
        return "midfield";
    return (spot < 50 ? possessive(team) + " " + std::to_string(spot) : possessive(opponent) + " " + std::to_string(100 - spot)) +
           "-yard line";
}

inline std::vector<std::string> narrate(const Play &play, const Teams &teams)
{
    const auto state = [&](std::string_view field) { return play.value("START_STATE", field); };
    const auto context = [&](std::string_view field) { return play.value("PLAY_CONTEXT", field); };
    const auto key = state("POSSESSION_TEAM_KEY");
    const auto offense = teams.name(key);
    const auto defense = teams.opponent(key);
    const int yard = number(state("BALL_AT_YARDLINE"));
    const int spot = state("BALL_AT_TEAM_KEY") == key ? yard : 100 - yard;
    const auto kind = context("PlayKind");
    const bool standalone = std::any_of(play.tags.begin(), play.tags.end(), [](const auto& tag) {
        return tag.name == "RecordKind" && tag.value == "PENALTY";
    });
    const auto quarter = state("QUARTER");
    std::vector<std::string> lines;
    if (!quarter.empty())
        lines.push_back((quarter == "5" ? "Overtime" : "Quarter " + quarter) + " | " + state("CLOCK"));
    if (!standalone && kind == "SCRIMMAGE")
    {
        const auto down = number(state("DOWN"));
        const std::string ordinal = down == 1 ? "first" : down == 2 ? "second" : down == 3 ? "third" : "fourth";
        const auto call = play.value("MAIN_CARD_LOOKUP", "OffensePlayName");
        lines.push_back("On " + ordinal + " and " + state("TOGO") + " at the " + yardLine(offense, defense, spot) +
                        (call.empty() ? "." : ", the " + offense + " call " + call +
                        (play.value("PLAY_CALLS", "OffenseCall") == "A" ? " with play action" : "") +
                        " against the " + possessive(defense) + " " + play.value("MAIN_CARD_LOOKUP", "DefensePlayName") + "."));
    }
    else if (!standalone && kind == "KICKOFF")
    {
        const auto kicking = teams.name(context("KickingTeamKey"));
        const auto receiving = teams.name(context("ReceivingTeamKey"));
        const bool onside = std::any_of(play.tags.begin(), play.tags.end(), [](const auto &tag) {
            return tag.name == "LookupPurpose" && tag.value.rfind("Onsides Kick:", 0) == 0;
        });
        lines.push_back("The " + kicking + (onside ? " attempt an onside kick to the " : " kick off to the ") + receiving + ".");
        const auto carrySpot = play.value("TRY_KICKOFF_CARRYOVER", "AdjustedSpot");
        if (!carrySpot.empty())
        {
            const auto kickKey = play.value("TRY_KICKOFF_CARRYOVER", "KickingTeamKey");
            const auto foulKey = play.value("TRY_KICKOFF_CARRYOVER", "FoulingTeamKey");
            lines.push_back("The " + teams.name(kickKey) + " kick from " +
                yardLine(teams.name(kickKey), teams.opponent(kickKey), number(carrySpot)) + " after the " +
                play.value("TRY_KICKOFF_CARRYOVER", "Yards") + "-yard extra-point penalty on the " +
                teams.name(foulKey) + ".");
        }
    }
    else if (!standalone)
        lines.push_back("The " + offense + (kind == "PUNT" ? " punt." : " attempt a field goal."));

    std::string action, team, detail;
    int value = 0;
    int returnStart = -1;
    if (kind == "KICKOFF")
    {
        const auto distance = play.value("KICKOFF_DISTANCE", "LookupResult");
        if (!distance.empty() && distance.find_first_not_of("0123456789") == std::string::npos)
            returnStart = 100 - yard - number(distance);
    }
    bool inAction = false;
    bool inTryPenalty = false;
    std::string tryFoul, tryTeam, tryYards, tryDisposition, trySpot;
    for (const auto &tag : play.tags)
    {
        if (tag.name == "TRY_PENALTY_BEGIN")
            inTryPenalty = true;
        if (inTryPenalty)
        {
            if (tag.name == "PenaltyKey") tryFoul = tag.value;
            if (tag.name == "TeamKey") tryTeam = tag.value;
            if (tag.name == "Yards") tryYards = tag.value;
            if (tag.name == "Disposition") tryDisposition = tag.value;
            if (tag.name == "AdjustedSpot") trySpot = tag.value;
            if (tag.name == "TRY_PENALTY_END")
            {
                inTryPenalty = false;
                std::replace(tryFoul.begin(), tryFoul.end(), '_', ' ');
                const bool carry = tryDisposition == "KICKOFF_CARRYOVER";
                const bool noKickoff = carry &&
                    play.value("TRY_KICKOFF_UNENFORCED", "Disposition") == "NO_ENSUING_KICKOFF";
                const int yard = number(trySpot);
                const auto kickingKey = carry ? (tryTeam == teams.awayKey ? teams.homeKey : teams.awayKey) :
                    play.value("TRY_START", "POSSESSION_TEAM_KEY");
                const auto adjusted = yardLine(teams.name(kickingKey), teams.opponent(kickingKey), yard);
                lines.push_back(tryFoul + " on the " + teams.name(tryTeam) + ". " +
                    (noKickoff ? "The extra point stands. " + tryYards + " yards unenforced because there is no ensuing kickoff." :
                    carry && trySpot.empty() ? "The extra point stands. " + tryYards + " yards carry to the kickoff." :
                    tryYards + " yard penalty. " +
                    (carry ? "The extra point stands. Kickoff from " : "Replay the try from ") + adjusted + "."));
                trySpot.clear();
                if (!carry)
                    lines.push_back("The try is untimed.");
            }
            continue;
        }
        if (tag.name == "KICK_CHECK_BEGIN")
        {
            action.clear();
            detail.clear();
            continue;
        }
        if (tag.name == "PlayKind")
            action = tag.value;
        if (tag.name == "CheckOutcome" && tag.value == "MISSED")
            lines.push_back(action == "EXTRA_POINT" ? "The extra point is no good." : "The field goal is no good.");
        if (tag.name == "ACTION_BEGIN")
        {
            action.clear();
            team.clear();
            detail.clear();
            value = 0;
            inAction = true;
        }
        if (!inAction)
            continue;
        if (tag.name == "ActionKind") action = tag.value;
        if (tag.name == "TeamKey") team = teams.name(tag.value);
        if (tag.name == "ActionValue") value = number(tag.value);
        if (tag.name == "ActionDetail") detail = tag.value;
        if (tag.name != "ACTION_END")
            continue;
        inAction = false;
        if (action == "RUSH" || action == "COMPLETE_PASS" || action == "SACK")
        {
            const int gain = std::clamp(value, -spot, 100 - spot);
            std::string outcome = action == "RUSH" ? "The ball carrier" :
                                  action == "SACK" ? "The quarterback is sacked and" : "Pass complete. The receiver";
            outcome += gain < 0 ? " loses " : " gains ";
            outcome += std::to_string(std::abs(gain)) + (std::abs(gain) == 1 ? " yard" : " yards");
            if (spot + gain == 100)
                outcome += " and takes the ball into the end zone";
            else if (spot + gain == 0)
                outcome += " back to the goal line";
            else
                outcome += " to the " + yardLine(offense, defense, spot + gain);
            lines.push_back(outcome + ".");
        }
        else if (action == "INCOMPLETE_PASS") lines.push_back("Incomplete pass.");
        else if (action == "FIRST_DOWN") lines.push_back("First down.");
        else if (action == "SCORE" && value == 6)
            lines.push_back("Touchdown " + team + "!");
        else if (action == "SCORE" && value == 3)
            lines.push_back("The " + team + " make the field goal.");
        else if (action == "SAFETY") lines.push_back("Safety for the " + team + ".");
        else if (action == "EXTRA_POINT") lines.push_back("The extra point is good.");
        else if (action == "FIELD_GOAL_BLOCKED" || action == "EXTRA_POINT_BLOCKED")
            lines.push_back(action == "FIELD_GOAL_BLOCKED" ? "The field goal is blocked." : "The extra point is blocked.");
        else if (action == "PUNT")
        {
            const int distance = std::min(value, 100 - spot);
            lines.push_back("The punt travels " + std::to_string(distance) + " yards.");
            returnStart = 100 - spot - distance;
        }
        else if (action == "PUNT_PARTIALLY_BLOCKED") lines.push_back("The punt is partially blocked.");
        else if (action == "PUNT_BLOCKED" || (action == "KICK_BLOCKED" && kind == "PUNT"))
            lines.push_back("The punt is blocked.");
        else if (action == "FUMBLE_RETAINED") lines.push_back("Fumble. The offense recovers.");
        else if (action == "POSSESSION")
        {
            if (detail.find("touchback") != std::string::npos) lines.push_back("The ball goes into the end zone for a touchback.");
            else if (detail == "kickoff out of bounds") lines.push_back("The kickoff goes out of bounds.");
            else if (detail == "interception" || detail == "fumble recovery")
            {
                returnStart = value;
                lines.push_back(detail == "interception" ? "Intercepted by the " + team + "."
                                                        : "Fumble. The " + team + " recover.");
            }
            else if (detail == "blocked kick recovery") lines.push_back("The " + team + " recover the blocked kick.");
        }
        else if (action == "KICKOFF_RETURN" || action == "PUNT_RETURN" ||
                 action == "INTERCEPTION_RETURN" || action == "FUMBLE_RETURN" || action == "ONSIDE_RECOVERY")
        {
            if (action == "PUNT_RETURN" && detail == "FC")
                lines.push_back("The " + team + " call for a fair catch.");
            else if (action == "PUNT_RETURN" && detail == "OB")
                lines.push_back("The punt goes out of bounds.");
            else if (action == "ONSIDE_RECOVERY")
            {
                lines.push_back("The " + team + " recover the onside kick.");
                if (value > 0)
                    lines.push_back("They advance it " + std::to_string(value) + " yards.");
            }
            else if (value != 0)
            {
                const int yards = returnStart >= 0 ? std::clamp(value, -returnStart, 100 - returnStart) : value;
                lines.push_back("The " + team + (yards < 0 ? " lose " : " return it for ") +
                                std::to_string(std::abs(yards)) + " yards.");
            }
        }
    }
    const auto penalty = play.value("PENALTY_ENFORCEMENT", "PenaltyKey");
    if (!penalty.empty())
    {
        auto name = penalty;
        std::replace(name.begin(), name.end(), '_', ' ');
        const auto foul = play.value("PENALTY_ENFORCEMENT", "FoulSpot");
        const auto teamKey = play.value("PENALTY_ENFORCEMENT", "TeamKey");
        const auto spotTeamKey = play.value("PENALTY_ENFORCEMENT", "SpotTeamKey");
        const auto spot = [&](const std::string& yard) {
            const auto& key = spotTeamKey.empty() ? teamKey : spotTeamKey;
            return yardLine(teams.name(key), teams.opponent(key), number(yard));
        };
        const auto disposition = play.value("PENALTY_ENFORCEMENT", "Disposition");
        const bool declined = disposition == "DECLINED";
        const bool limitPending = disposition == "ONSIDE_LIMIT_PENDING";
        const bool onsideAward = disposition == "ONSIDE_LIMIT_AWARD" || disposition == "ONSIDE_RECEIVER_KEEPS";
        const bool receiverAward = play.value("PENALTY_ENFORCEMENT", "EnforcementSpot") == "RECEIVER_AWARD";
        const auto rekickTiming = play.value("KICKOFF_REKICK_TIMING", "Disposition");
        const auto possessionTiming = play.value("KICKOFF_POSSESSION_TIMING", "Disposition");
        const auto receiverSpot = [&] {
            return yardLine(teams.opponent(teamKey), teams.name(teamKey),
                number(play.value("PENALTY_ENFORCEMENT", "FinalSpot")));
        };
        lines.push_back(name + " on the " + teams.name(teamKey) + (foul.empty() ? "" : " at the " + spot(foul)) +
            (limitPending ? ". The onside re-kick limit was reached. Second-foul enforcement is pending." :
            disposition == "ONSIDE_LIMIT_AWARD" ? ". No further re-kick. The receiving team takes over at " + receiverSpot() + "." :
            disposition == "ONSIDE_RECEIVER_KEEPS" ? ". " + play.value("PENALTY_ENFORCEMENT", "Yards") +
                " yard penalty. The receiving team keeps the ball at " + receiverSpot() + "." :
            declined ? " declined. The field goal stands." :
            receiverAward ? ". Five-yard penalty. The receiving team takes over at " + receiverSpot() + "." :
            rekickTiming == "CANCELED" ? ". " + play.value("PENALTY_ENFORCEMENT", "Yards") + "-yard penalty." :
            ". " + play.value("PENALTY_ENFORCEMENT", "Yards") + " yard penalty.") +
            (play.value("PENALTY_ENFORCEMENT", "ApparentFieldGoalGood") == "1" ? " The field goal is erased." :
            !standalone && !onsideAward && play.value("PENALTY_ENFORCEMENT", "PlayResultStands") == "0" ? " The play result is erased." : ""));
        if (!foul.empty())
        {
            const auto finalSpot = play.value("PENALTY_ENFORCEMENT", "FinalSpot");
            lines.push_back(std::string(!possessionTiming.empty() ? "The enforced spot is " :
                number(finalSpot) >= 90 ? "First and goal at " : "First and ten at ") +
                spot(finalSpot) + ".");
        }
        const auto rekick = play.value("PENALTY_ENFORCEMENT", "RekickSpot");
        if (rekickTiming == "CANCELED")
            lines.push_back("The period expired. The re-kick is canceled.");
        else if (rekickTiming == "NEXT_QUARTER")
            lines.push_back("The " + teams.name(teamKey) + " re-kick from " + spot(rekick) +
                " to start quarter " + play.value("KICKOFF_REKICK_TIMING", "NextQuarter") + ".");
        else if (!rekick.empty())
        {
            lines.push_back("Re-kick from " + spot(rekick) + ".");
            if (play.value("PENALTY_ENFORCEMENT", "OnsideRekick") == "1")
                lines.push_back("The kicking team has used its one onside re-kick.");
        }
        else if (play.value("PENALTY_ENFORCEMENT", "FirstDownAwarded") == "1")
            lines.push_back("First down by penalty.");
        else if (play.value("PENALTY_ENFORCEMENT", "EnforcementSpot") == "PREVIOUS_SPOT")
            lines.push_back("Repeat the down.");
        if (possessionTiming == "NEXT_QUARTER")
        {
            const auto key = play.value("KICKOFF_POSSESSION_TIMING", "ReceivingTeamKey");
            const auto award = play.value("KICKOFF_POSSESSION_TIMING", "AwardedSpot");
            lines.push_back("The receiving team opens quarter " +
                play.value("KICKOFF_POSSESSION_TIMING", "NextQuarter") + " with first down at " +
                yardLine(teams.name(key), teams.opponent(key), number(award)) + ".");
        }
        else if (possessionTiming == "HALF_ENDED")
            lines.push_back("The half ends before the receiving team can snap.");
    }
    else if (standalone)
    {
        std::string team;
        for (const auto& tag : play.tags)
        {
            if (tag.name == "TeamKey") team = teams.name(tag.value);
            if (tag.name == "ActionValue")
                lines.push_back("Delay of game on the " + team + ". " + tag.value + " yard penalty.");
        }
    }
    const auto clockAction = play.value("PENALTY_CLOCK", "Action");
    if (clockAction == "ACTION_PREVENTED")
        lines.push_back("Dead ball foul. No time was charged.");
    else if (clockAction == "LIVE_KICK_ERASED")
        lines.push_back("The kick used " + play.value("PENALTY_CLOCK", "GameClockSeconds") +
            " seconds on the game clock.");
    const auto firstTouchSpot = play.value("PUNT_FIRST_TOUCH", "Spot");
    if (!firstTouchSpot.empty())
    {
        const auto kickingTeam = teams.name(play.value("PUNT_FIRST_TOUCH", "KickingTeamKey"));
        const auto receivingTeam = teams.name(play.value("PUNT_FIRST_TOUCH", "ReceivingTeamKey"));
        lines.push_back("First touching by the " + kickingTeam + ". The " + receivingTeam +
            " take over at " + yardLine(receivingTeam, kickingTeam, number(firstTouchSpot)) + ".");
    }
    if (play.value("UNTIMED_DOWN", "Played") == "1")
        lines.push_back("That play was untimed.");
    if (play.value("UNTIMED_DOWN", "Awarded") == "1")
    {
        const auto team = play.value("UNTIMED_DOWN", "TeamKey");
        lines.push_back("The period is extended for an untimed down from " +
            yardLine(teams.name(team), teams.opponent(team),
                number(play.value("UNTIMED_DOWN", "EnforcedSpot"))) + ".");
    }
    return lines;
}

inline std::vector<std::string> render(const Play &play, const Teams &teams, View view)
{
    if (view == View::Broadcast)
        return narrate(play, teams);
    std::vector<std::string> lines;
    bool detailBlock = false;
    for (const auto &tag : play.tags)
    {
        const bool end = tag.depth == 2 && tag.name.size() >= 4 &&
                         tag.name.compare(tag.name.size() - 4, 4, "_END") == 0;
        if (tag.depth == 2 && !end)
        {
            const auto &name = tag.name;
            detailBlock = name == "ROLL_BEGIN" || name == "MAIN_CARD_LOOKUP_BEGIN" || name == "LOOKUP_BEGIN" ||
                          name == "ADJUSTMENT_BEGIN" || name == "PLAY_CALLS_BEGIN" || name == "RATING_BEGIN" ||
                          name == "CHECK_BEGIN" || name == "BLOCK_CHECK_BEGIN" || name == "KICK_CHECK_BEGIN" ||
                          name == "KICKOFF_DISTANCE_BEGIN" || name == "PENALTY_CHECK_BEGIN" ||
                          name == "PENALTY_ENFORCEMENT_BEGIN" || name == "TRY_PENALTY_BEGIN" ||
                          name == "TRY_KICKOFF_CARRYOVER_BEGIN" || name == "TRY_KICKOFF_UNENFORCED_BEGIN" ||
                          name == "PENALTY_CLOCK_BEGIN" || name == "UNTIMED_DOWN_BEGIN" ||
                          name == "PUNT_FIRST_TOUCH_BEGIN" || name == "KICKOFF_REKICK_TIMING_BEGIN" ||
                          name == "KICKOFF_POSSESSION_TIMING_BEGIN";
        }
        if (view == View::Pbp || detailBlock)
        {
            const auto base = view == View::Pbp ? 1u : 2u;
            lines.push_back(std::string((tag.depth - base) * 5, ' ') + tag.source);
        }
        if (end)
            detailBlock = false;
    }
    return lines;
}
} // namespace PffBroadcast
