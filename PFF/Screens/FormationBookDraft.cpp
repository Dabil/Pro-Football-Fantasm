#include "Screens/FormationBookDraft.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace PffFormation
{
namespace
{
constexpr std::size_t offensiveLineSize = 9;

bool isSkill(Role role)
{
    return role == Role::RB || role == Role::WR || role == Role::TE ||
        role == Role::RC || role == Role::RN;
}

bool isLine(Role role)
{
    return role == Role::T || role == Role::G || role == Role::C || role == Role::OL;
}

bool isDefender(Role role)
{
    return role == Role::DE || role == Role::DT || role == Role::DL || role == Role::LB ||
        role == Role::CB || role == Role::S || role == Role::DB;
}

bool isFixedLineSpot(std::string_view label)
{
    return label == "LT" || label == "LG" || label == "OC" || label == "RG" || label == "RT";
}
}

const std::vector<Spot>& stockBoard(Side side)
{
    static const std::vector<Spot> offense{
        {1,"LSE"}, {1,"LTE"}, {1,"LT"}, {1,"LG"}, {1,"OC"},
        {1,"RG"}, {1,"RT"}, {1,"RTE"}, {1,"RSE"},
        {2,"LFL"}, {2,"LSB"}, {2,"QB"}, {2,"RSB"}, {2,"RFL"},
        {3,"FB"}, {4,"LRB"}, {4,"HB"}, {4,"RRB"}
    };
    static const std::vector<Spot> defense{
        {1,"LCB"}, {1,"LOLB"}, {1,"LDE"}, {1,"LDT"}, {1,"NT"},
        {1,"RDT"}, {1,"RDE"}, {1,"ROLB"}, {1,"RCB"},
        {2,"LCB"}, {2,"LOLB"}, {2,"LLB"}, {2,"LILB"}, {2,"MLB"},
        {2,"RILB"}, {2,"RLB"}, {2,"ROLB"}, {2,"RCB"},
        {3,"LCB"}, {3,"LS"}, {3,"MS"}, {3,"RS"}, {3,"RCB"}
    };
    return side == Side::Offense ? offense : defense;
}

Placement placement(Side side, std::size_t spot, Role role)
{
    const auto& board = stockBoard(side);
    if (spot >= board.size()) return Placement::Illegal;
    const auto label = board[spot].label;
    bool preferred = false;
    if (side == Side::Defense)
    {
        if (!isDefender(role)) return Placement::Illegal;
        const bool end = label == "LDE" || label == "RDE";
        const bool tackle = label == "LDT" || label == "NT" || label == "RDT";
        const bool corner = label == "LCB" || label == "RCB";
        const bool safety = label == "LS" || label == "MS" || label == "RS";
        switch (role)
        {
        case Role::DE: preferred = end; break;
        case Role::DT: preferred = tackle; break;
        case Role::DL: preferred = end || tackle; break;
        case Role::LB: preferred = label.find("LB") != std::string_view::npos; break;
        case Role::CB: preferred = corner; break;
        case Role::S: preferred = safety; break;
        case Role::DB: preferred = corner || safety; break;
        default: break;
        }
    }
    else if (role == Role::QB)
    {
        if (label == "QB") return Placement::Preferred;
        return label == "FB" || label == "HB" ? Placement::Allowed : Placement::Illegal;
    }
    else if (isLine(role))
    {
        if (!isFixedLineSpot(label)) return Placement::Illegal;
        preferred = role == Role::OL ||
            (role == Role::T && (label == "LT" || label == "RT")) ||
            (role == Role::G && (label == "LG" || label == "RG")) ||
            (role == Role::C && label == "OC");
    }
    else
    {
        if (!isSkill(role) || isFixedLineSpot(label) || label == "QB")
            return Placement::Illegal;
        const bool back = label == "FB" || label == "HB" || label == "LRB" || label == "RRB";
        const bool tight = label == "LTE" || label == "RTE";
        switch (role)
        {
        case Role::RB:
        case Role::RN: preferred = back; break;
        case Role::WR: preferred = !back && !tight; break;
        case Role::TE: preferred = tight; break;
        case Role::RC: preferred = !back; break;
        default: break;
        }
    }
    return preferred ? Placement::Preferred : Placement::Allowed;
}

bool Draft::place(RoleReference role, std::size_t spot)
{
    if (!role.rank || placement(m_side, spot, role.type) == Placement::Illegal)
        return false;
    const auto destination = m_assignments.find(spot);
    if (destination != m_assignments.end()) return destination->second == role;

    const auto previous = std::find_if(m_assignments.begin(), m_assignments.end(),
        [role](const auto& assignment) { return assignment.second == role; });
    if (previous == m_assignments.end() && m_assignments.size() >= 11) return false;
    m_assignments.emplace(spot, role);
    if (previous != m_assignments.end()) m_assignments.erase(previous);
    return true;
}

bool Draft::swap(std::size_t first, std::size_t second)
{
    const auto a = m_assignments.find(first);
    const auto b = m_assignments.find(second);
    if (a == m_assignments.end() || b == m_assignments.end() ||
        placement(m_side, first, b->second.type) == Placement::Illegal ||
        placement(m_side, second, a->second.type) == Placement::Illegal)
        return false;
    std::swap(a->second, b->second);
    return true;
}

bool Draft::remove(std::size_t spot)
{
    return m_assignments.erase(spot) != 0;
}

QuarterbackAlignment Draft::quarterbackAlignment() const
{
    if (m_side != Side::Offense) return QuarterbackAlignment::Missing;
    for (const auto& assignment : m_assignments)
    {
        if (assignment.second.type != Role::QB) continue;
        const auto label = stockBoard(m_side)[assignment.first].label;
        if (label == "FB") return QuarterbackAlignment::ShotgunTwo;
        if (label == "HB") return QuarterbackAlignment::ShotgunThree;
        return QuarterbackAlignment::UnderCenter;
    }
    return QuarterbackAlignment::Missing;
}

std::vector<std::string> Draft::structuralIssues() const
{
    std::vector<std::string> issues;
    if (m_assignments.size() != 11) issues.emplace_back("Place 11 unique roles.");
    if (m_side == Side::Defense) return issues;

    const auto& board = stockBoard(m_side);
    for (std::size_t i = 0; i < offensiveLineSize; ++i)
        if (isFixedLineSpot(board[i].label) && m_assignments.count(i) == 0)
            issues.emplace_back("Fill the " + std::string(board[i].label) + " line spot.");

    const auto qbCount = std::count_if(m_assignments.begin(), m_assignments.end(),
        [](const auto& assignment) { return assignment.second.type == Role::QB; });
    if (qbCount != 1) issues.emplace_back("Place exactly one quarterback.");

    const auto end = m_assignments.lower_bound(offensiveLineSize);
    if (std::distance(m_assignments.begin(), end) < 7)
        issues.emplace_back("Place at least seven roles on the line.");
    else if (!isSkill(m_assignments.begin()->second.type) || !isSkill(std::prev(end)->second.type))
        issues.emplace_back("Place an eligible receiver at both ends of the line.");
    return issues;
}
}
