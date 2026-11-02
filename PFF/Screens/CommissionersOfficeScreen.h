#pragma once

#include "Screens/Screen.h"
#include "Assets/AssetLibrary.h"
#include "App/LoadedWorldContext.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ContainerWidget.h"
#include "UI/Widgets/ListBox.h"
#include "UI/Widgets/TextInput.h"
#include "Database/CoreManifest.h"
#include "Database/PFFDatabase.h"
#include "Database/LeagueSetup.h"

#include <filesystem>
#include <functional>
#include <future>
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
        std::function<bool(const std::filesystem::path&)> loadLeagueCallback);

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
    void positionButtons(const Rect& controlPane, const Rect& footer);
    void positionLoadLeagueList(const Rect& contentRect);

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

    void beginLoadLeagueSelection();
    void cancelLoadLeagueSelection();
    void refreshLeagueSaveList();
    bool loadSelectedLeague();

    void drawLeagueDetails(
        Composition::PageComposer& page,
        const LoadedWorldContext& worldContext) const;

    void drawLoadLeagueSelection(
        Composition::PageComposer& page);

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
    std::vector<LeagueSaveEntry> m_availableLeagues;
    std::string m_loadLeagueMessage;

    // Widgets
    ContainerWidget m_root;
    ListBox* m_leagueListBox = nullptr;

    Button* m_buildCoreDbButton = nullptr;
    Button* m_newLeagueButton = nullptr;
    Button* m_loadLeagueButton = nullptr;
    Button* m_mainButton = nullptr;

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

