#pragma once
#include "App/LoadedWorldContext.h"
#include "Screens/FormationBookDraft.h"
#include <optional>

struct FormationBookEntry
{
    std::string name;
    PffFormation::Draft draft;
};

struct WorldFormationBook
{
    std::vector<FormationBookEntry> entries;
    std::optional<std::string> snapshot;
};

WorldFormationBook loadWorldFormationBook(const LoadedWorldContext&, std::int64_t team);
void saveWorldFormationBook(const LoadedWorldContext&, std::int64_t team, WorldFormationBook&);
