#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace PffFormation
{
enum class Side { Offense, Defense };
enum class Role { QB, RB, WR, TE, RC, RN, T, G, C, OL, DE, DT, DL, LB, CB, S, DB };
enum class Placement { Illegal, Allowed, Preferred };
enum class QuarterbackAlignment { Missing, UnderCenter, ShotgunTwo, ShotgunThree };

struct Spot
{
    int row;
    std::string_view label;
};

struct RoleReference
{
    Role type;
    unsigned rank;
    bool operator==(const RoleReference& other) const
    {
        return type == other.type && rank == other.rank;
    }
};

const std::vector<Spot>& stockBoard(Side side);
Placement placement(Side side, std::size_t spot, Role role);

class Draft
{
public:
    explicit Draft(Side side) : m_side(side) {}
    Side side() const { return m_side; }
    const std::map<std::size_t, RoleReference>& assignments() const { return m_assignments; }
    bool place(RoleReference role, std::size_t spot);
    bool swap(std::size_t first, std::size_t second);
    bool remove(std::size_t spot);
    QuarterbackAlignment quarterbackAlignment() const;
    std::vector<std::string> structuralIssues() const;

private:
    Side m_side;
    std::map<std::size_t, RoleReference> m_assignments;
};
}
