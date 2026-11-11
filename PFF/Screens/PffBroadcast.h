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
            isPlay = tag.value == "PLAY";
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
    const auto play = history.rfind("\n[PLAY_ID]");
    if (play == std::string_view::npos)
        return {};
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
    const auto quarter = state("QUARTER");
    std::vector<std::string> lines{
        (quarter == "5" ? "Overtime" : "Quarter " + quarter) + " | " + state("CLOCK")};
    if (kind == "SCRIMMAGE")
    {
        const auto down = number(state("DOWN"));
        const std::string ordinal = down == 1 ? "first" : down == 2 ? "second" : down == 3 ? "third" : "fourth";
        lines.push_back("On " + ordinal + " and " + state("TOGO") + " at " + yardLine(offense, defense, spot) +
                        ", the " + offense + " call " + play.value("MAIN_CARD_LOOKUP", "OffensePlayName") +
                        (play.value("PLAY_CALLS", "OffenseCall") == "A" ? " with play action" : "") +
                        " against the " + possessive(defense) + " " + play.value("MAIN_CARD_LOOKUP", "DefensePlayName") + ".");
    }
    else if (kind == "KICKOFF")
    {
        const auto kicking = teams.name(context("KickingTeamKey"));
        const auto receiving = teams.name(context("ReceivingTeamKey"));
        const bool onside = std::any_of(play.tags.begin(), play.tags.end(), [](const auto &tag) {
            return tag.name == "LookupPurpose" && tag.value.rfind("Onsides Kick:", 0) == 0;
        });
        lines.push_back("The " + kicking + (onside ? " attempt an onside kick to the " : " kick off to the ") + receiving + ".");
    }
    else
        lines.push_back("The " + offense + (kind == "PUNT" ? " punt." : " attempt a field goal."));

    std::string action, team, detail;
    int value = 0;
    int returnStart = -1;
    if (kind == "KICKOFF")
    {
        const auto distance = play.value("KICKOFF_DISTANCE", "LookupResult");
        if (!distance.empty() && distance.find_first_not_of("0123456789") == std::string::npos)
            returnStart = 65 - number(distance);
    }
    bool inAction = false;
    for (const auto &tag : play.tags)
    {
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
                outcome += " to the end zone";
            else if (spot + gain == 0)
                outcome += " back to the goal line";
            else
                outcome += " to " + yardLine(offense, defense, spot + gain);
            lines.push_back(outcome + ".");
        }
        else if (action == "INCOMPLETE_PASS") lines.push_back("Incomplete pass.");
        else if (action == "FIRST_DOWN") lines.push_back("First down.");
        else if (action == "SCORE")
            lines.push_back("The " + team + (value == 6 ? " score a touchdown!" : " make the field goal."));
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
        else if (action == "KICK_BLOCKED" && kind == "PUNT") lines.push_back("The punt is blocked.");
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
                          name == "KICKOFF_DISTANCE_BEGIN";
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
