#pragma once

#include "Screens/Screen.h"
#include "Assets/AssetLibrary.h"
#include "App/LoadedWorldContext.h"
#include "UI/Widgets/Button.h"
#include "Screens/FormCheckBox.h"
#include "UI/Widgets/ContainerWidget.h"
#include "UI/Widgets/ListBox.h"
#include "UI/Widgets/ScrollBox.h"
#include "UI/Widgets/TextInput.h"
#include "Database/CoreManifest.h"
#include "Database/PFFDatabase.h"
#include "Database/LeagueSetup.h"
#include "Database/GroundRules.h"

#include <filesystem>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class Surface;

namespace Composition
{
    class PageComposer;
}

class CommissionersOfficeScreen : public Screen
{
public:
    explicit CommissionersOfficeScreen(
        Assets::AssetLibrary& assetLibrary,
        std::function<bool(std::string_view)> commandDispatcher,
        std::function<LoadedWorldContext()> loadedWorldContextProvider,
        std::function<bool(const std::filesystem::path&)> loadLeagueCallback,
        std::function<void(const WorldEditCommit&)> commitCallback = {});

    ~CommissionersOfficeScreen();

    void onEnter() override;
    void onExit() override;
    bool handleEvent(const Input::Event& event) override;
    void update(const Animation::TickEvent& event) override;
    void draw(Surface& surface) override;

private:
    struct LeagueSaveEntry
    {
        std::string displayName;
        std::filesystem::path databasePath;
    };

    void buildWidgets();
    void refreshEditState(bool force=false);
    void toggleGodMode(bool enabled);
    void positionButtons(const Rect& controlPane, const Rect& footer);

    void handleButtonActivation(const ButtonActivationResult& result);
    void buildCoreDatabase();
    void finishCoreBuild(CoreDatabaseBuildResult result);
    void beginCoreBuild();
    void closeCoreBuild();
    void reloadCorePackages();
    void openPackageDropdown(bool ruleset);
    void closePackageDropdown(bool accept);
    void drawCoreBuild(Composition::PageComposer& page, const Rect& controls);
    void refreshCoreLabels();
    void addCoreMessage(const std::string& message);
    void clearCoreMessages();
    void wrapCoreMessages(int width);
    void beginLeagueSetup();
    void closeLeagueSetup();
    void selectLeagueCore();
    void createSelectedLeague();
    void drawLeagueSetup(Composition::PageComposer& page, const Rect& controls);

    void beginAddons();
    void closeAddons();
    bool handleAddonsEvent(const Input::Event& event);
    void drawAddons(Composition::PageComposer& page);
    void openGroundRules();
    void drawGroundRules(Composition::PageComposer& page);
    void refreshGroundRulesToggle();
    void saveGroundRules();

    void beginDeleteLeague();
    void cancelDeleteLeague();
    void refreshDeleteList();
    void selectLeagueToDelete(std::size_t index);
    void confirmDeleteLeague();
    bool handleDeleteEvent(const Input::Event& event);
    void drawDeleteLeague(Composition::PageComposer& page);

    void beginLoadLeagueSelection();
    void openSavedLeagueDropdown();
    void closeSavedLeagueDropdown(bool accept);
    void cancelLoadLeagueSelection();
    void refreshLeagueSaveList();
    bool loadSelectedLeague();

    void drawLeagueDetails(
        Composition::PageComposer& page,
        const LoadedWorldContext& worldContext) const;

    void drawLoadLeagueSelection(
        Composition::PageComposer& page, const Rect& controls);

private:
    Assets::AssetLibrary& m_assetLibrary;

    // Title Text
    std::string m_TitleBannerFontKey = "banner.ansi_shadow";
    TextObject m_CommissionersOfficeTitleObject;
    Assets::LoadBannerFontResult m_fontResult;

    // Application callbacks
    std::function<bool(std::string_view)> m_commandDispatcher;

    // Returns a snapshot of the application's currently loaded World context.
    // The screen does not own or mutate World selection state.
    std::function<LoadedWorldContext()> m_loadedWorldContextProvider;

    // Asks PFFApplication to validate/load the chosen World.db and publish it
    // into the application's LoadedWorldContext.
    std::function<bool(const std::filesystem::path&)> m_loadLeagueCallback;

    // Load League selection state
    bool m_loadLeagueSelectionActive = false;
    bool m_savedLeaguesOpen = false;
    std::size_t m_selectedLeague = 0;
    ContainerWidget m_loadRoot;
    Button* m_savedLeagueSelector = nullptr;
    Button* m_openLeague = nullptr;
    Button* m_cancelLoadLeague = nullptr;
    std::vector<LeagueSaveEntry> m_availableLeagues;
    std::string m_loadLeagueMessage;

    bool m_addonsActive = false;
    bool m_closeAddonsRequested = false;
    ContainerWidget m_addonsRoot;
    std::array<FormCheckBox*, LeagueSetup::AddonNames.size()> m_addonChecks{};
    Button* m_addonsClose = nullptr;

    bool m_groundRulesActive = false;
    ContainerWidget m_groundRulesRoot;
    std::array<FormCheckBox*, GroundRules::keys.size()> m_groundRuleChecks{};
    TextInput* m_playClockSeconds = nullptr;
    Button* m_groundRulesToggle = nullptr;
    Button* m_groundRulesSave = nullptr;
    Button* m_groundRulesCancel = nullptr;
    GroundRules::Settings m_groundRules;
    LoadedWorldContext m_groundRulesContext;
    std::int64_t m_groundRulesRevision = 0;
    std::string m_groundRulesError;

    bool m_deleteLeagueActive = false;
    bool m_deleteConfirmed = false, m_deleteCanceled = false;
    std::optional<std::size_t> m_leagueToDelete;
    ContainerWidget m_deleteRoot;
    ScrollBox* m_deleteList = nullptr;
    Button* m_confirmDelete = nullptr;
    Button* m_cancelDelete = nullptr;
    std::string m_deleteMessage;

    // Widgets
    ContainerWidget m_root;
    ListBox* m_leagueListBox = nullptr;

    Button* m_buildCoreDbButton = nullptr;
    Button* m_newLeagueButton = nullptr;
    Button* m_loadLeagueButton = nullptr;
    Button* m_deleteLeagueButton = nullptr;
    Button* m_closeLeagueButton = nullptr;
    Button* m_addonsButton = nullptr;
    Button* m_groundRulesButton = nullptr;
    Button* m_mainButton = nullptr;
    FormCheckBox* m_godMode=nullptr;
    std::function<void(const WorldEditCommit&)> m_commitCallback;
    WorldActor m_editActor;
    WorldEditState m_editState;
    LoadedWorldContext m_editContext;
    bool m_editChecked=false,m_editReady=false;
    std::optional<bool> m_requestedGod;
    std::optional<std::string> m_pendingNavigation;
    std::string m_editMessage;

    enum class CoreStage { Closed, Configure, Building, Result };
    CoreStage m_coreStage = CoreStage::Closed;
    ContainerWidget m_coreRoot;
    TextInput* m_coreName = nullptr;
    Button* m_rulesetDropdown = nullptr;
    Button* m_seasonDropdown = nullptr;
    Button* m_coreBuildButton = nullptr;
    Button* m_coreBack = nullptr;
    Button* m_reloadPackages = nullptr;
    ListBox* m_packageList = nullptr;
    ListBox* m_buildMessages = nullptr;
    int m_openDropdown = 0;
    std::vector<CoreManifest::Package> m_rulesets, m_seasons;
    std::size_t m_selectedRuleset = 0, m_selectedSeason = 0;
    std::vector<std::string> m_discoveryMessages;
    std::future<CoreDatabaseBuildResult> m_coreBuildTask;
    std::vector<std::string> m_coreMessages;
    int m_coreMessageWidth = -1;

    enum class LeagueStage { Closed, Configure, Creating, Result };
    LeagueStage m_leagueStage = LeagueStage::Closed;
    ContainerWidget m_leagueRoot;
    TextInput* m_leagueName = nullptr;
    Button* m_coreSelector = nullptr;
    Button* m_createLeague = nullptr;
    Button* m_cancelLeague = nullptr;
    ListBox* m_coreChoices = nullptr;
    ListBox* m_leagueDetails = nullptr;
    ListBox* m_existingSaves = nullptr;
    bool m_coreChoicesOpen = false;
    std::vector<LeagueSetup::Core> m_installedCores;
    std::size_t m_chosenCore = 0;
    std::vector<std::string> m_leagueMessages;
    std::future<WorldDatabaseCreateResult> m_leagueCreation;
    std::string m_leagueDetailsKey;
};

