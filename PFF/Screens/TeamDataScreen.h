#pragma once
#include "Screens/Screen.h"
#include "Screens/FormCheckBox.h"
#include "Assets/AssetLibrary.h"
#include "App/LoadedWorldContext.h"
#include "GCE/Database/WorldTeamData.h"
#include "UI/Widgets/ContainerWidget.h"
#include "UI/Widgets/TextInput.h"
#include "UI/Widgets/ListBox.h"
#include <functional>
#include <array>
#include <map>

class TeamDataScreen final : public Screen
{
  public:
    using CommitCallback = std::function<void(const WorldEditCommit&)>;
    TeamDataScreen(Assets::AssetLibrary&, std::function<bool(std::string_view)>,
                   std::function<LoadedWorldContext()>, LoadedWorldContext selectedWorld,
                   std::int64_t teamSeasonId, CommitCallback);
    void onEnter() override;
    void onExit() override;
    bool handleEvent(const Input::Event&) override;
    void update(const Animation::TickEvent&) override;
    void draw(Surface&) override;

  private:
    enum class Modal
    {
        None,
        Staff,
        NewPerson,
        History,
        Leave
    };
    enum class Action
    {
        None,
        Return,
        Save,
        Cancel,
        UsePerson,
        RemovePerson,
        KeepPerson,
        NewPerson,
        CreatePerson,
        CloseModal,
        PreviousPage,
        NextPage,
        SaveLeave,
        DiscardLeave
    };
    void buildWidgets();
    void synchronizeModal();
    void reload();
    bool sameWorld() const;
    void refreshContext();
    bool dirty() const;
    WorldTeamDataDraft draft() const;
    void synchronize();
    void toggleManagement(bool);
    void openStaff(std::size_t);
    void openHistory();
    void loadPeoplePage();
    void closeModal();
    void processAction();
    bool save();
    void leave();
    void layout(const Rect&, const Rect&, const Rect&, const Rect&);
    void scrollForm(int rows);
    bool focusForm(bool previous);
    std::array<Widget*, 9> formFields() const;
    std::array<int, 9> formRows() const;
    Rect m_contentBounds, m_controlBounds, m_footerBounds, m_screenBounds;
    int m_formOffset = 0;
    bool m_borderedForms = true;
    Assets::AssetLibrary& m_assets;
    std::function<bool(std::string_view)> m_dispatch;
    std::function<LoadedWorldContext()> m_context;
    CommitCallback m_commit;
    LoadedWorldContext m_selectedWorld, m_displayContext;
    std::int64_t m_teamSeasonId;
    WorldActor m_actor;
    WorldTeamData m_saved, m_permissions;
    bool m_available = false, m_filling = false, m_stale = false, m_management = false;
    std::string m_message;
    TextObject m_banner;
    ContainerWidget m_root, m_modalRoot;
    std::array<TextInput*, 4> m_inputs{}; // City, team, stadium, surface.
    std::vector<Button*> m_staffButtons;
    std::vector<WorldStaffAction> m_staffDraft;
    FormCheckBox* m_manage = nullptr;
    Button* m_save = nullptr;
    Button* m_cancel = nullptr;
    Button* m_history = nullptr;
    Button* m_return = nullptr;
    Modal m_modal = Modal::None;
    Action m_action = Action::None;
    std::size_t m_role = 0, m_page = 0;
    std::vector<WorldPersonChoice> m_people;
    std::map<std::int64_t, std::string> m_draftNames;
    ListBox* m_choices = nullptr;
    std::array<TextInput*, 3> m_personInputs{};
    Button* m_keep = nullptr;
    Button* m_use = nullptr;
    Button* m_remove = nullptr;
    Button* m_new = nullptr;
    Button* m_modalCancel = nullptr;
    Button* m_previous = nullptr;
    Button* m_next = nullptr;
    Button* m_create = nullptr;
    Button* m_saveLeave = nullptr;
    Button* m_discardLeave = nullptr;
    Button* m_stay = nullptr;
    std::vector<std::string> m_historyLines;
    int m_historyWrapWidth = -1;
};
