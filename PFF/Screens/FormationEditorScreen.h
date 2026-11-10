#pragma once

#include "App/LoadedWorldContext.h"
#include "Assets/AssetLibrary.h"
#include "Database/WorldFormationBook.h"
#include "Screens/Screen.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ContainerWidget.h"
#include "UI/Widgets/ListBox.h"
#include "UI/Widgets/TextInput.h"
#include <functional>
#include <optional>

class FormationEditorScreen final : public Screen
{
public:
    FormationEditorScreen(Assets::AssetLibrary& assets, LoadedWorldContext world, std::function<LoadedWorldContext()> context,
        std::int64_t teamSeasonId, std::function<void()> close);
    void onEnter() override;
    bool handleEvent(const Input::Event& event) override;
    void update(const Animation::TickEvent& event) override;
    void draw(Surface& surface) override;

private:
    enum class Action { None, Offense, Defense, New, Open, Rename, Delete, ConfirmDelete, CancelDelete, SaveAs, Save, ConfirmName, CancelName, Return, Discard, Keep };
    Button* addButton(ContainerWidget& root, const char* label, Action action);
    void refreshList();
    void load(std::optional<std::size_t> entry, PffFormation::Side side);
    void activateSpot(std::size_t spot);
    bool dirty() const;
    bool save(std::string name, const PffFormation::Draft& draft, std::optional<std::size_t> entry);
    bool saveExisting();
    bool persist(WorldFormationBook updated);
    void deleteSelected();
    void promptForName(Action action);
    void navigate(Action action);
    void process(Action action);

    LoadedWorldContext m_world;
    std::function<LoadedWorldContext()> m_context;
    std::int64_t m_teamSeasonId;
    WorldFormationBook m_book;
    bool m_bookLoaded = false;
    std::function<void()> m_close;
    PffFormation::Draft m_draft{PffFormation::Side::Offense};
    std::optional<std::size_t> m_entry, m_openEntry, m_pendingSpot, m_deleteEntry;
    std::vector<std::size_t> m_listEntries;
    TextObject m_banner;
    ContainerWidget m_root, m_prompt, m_namePrompt, m_deletePrompt;
    ListBox* m_list = nullptr;
    TextInput* m_name = nullptr;
    TextInput* m_role = nullptr;
    std::vector<Button*> m_spots;
    std::vector<Button*> m_actions;
    std::vector<Button*> m_promptActions;
    Button* m_return = nullptr;
    Button* m_confirmName = nullptr;
    Button* m_cancelName = nullptr;
    Button* m_confirmDelete = nullptr;
    Button* m_cancelDelete = nullptr;
    Action m_action = Action::None;
    Action m_navigation = Action::None;
    Action m_naming = Action::None;
    std::string m_message;
};
