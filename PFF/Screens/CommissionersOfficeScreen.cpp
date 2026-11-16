// PFF includes
#include "Screens/CommissionersOfficeScreen.h"
#include "Database/PFFDatabase.h"
#include "Styles/PFF_Theme.h"

// TUI includes
#include "Input/Command.h"
#include "Input/Event.h"
#include "Rendering/Surface.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Objects/BannerFactory.h"
#include "Utilities/Text/TextClip.h"
#include "Utilities/Text/TextWrap.h"

// C++ library includes
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <optional>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace
{
    using Composition::PageComposer;

    constexpr std::string_view BuildCoreDbCommand = "action:pff:build_core_db";
    constexpr std::string_view NewLeagueCommand = "action:pff:new_league";
    constexpr std::string_view LoadLeagueCommand = "action:pff:load_league";
    constexpr std::string_view DeleteLeagueCommand = "action:pff:delete_league";
    constexpr std::string_view AddonsCommand = "action:pff:addons";

    const std::filesystem::path WorldRoot =
        std::filesystem::path("Data") / "World";

    bool ordinarySavePath(const std::filesystem::path& path, bool directory)
    {
        const auto attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
            !(attributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
            bool(attributes & FILE_ATTRIBUTE_DIRECTORY) == directory;
    }

    std::filesystem::path checkedLeagueFolder(const std::filesystem::path& database)
    {
        const auto root = std::filesystem::absolute(WorldRoot).lexically_normal();
        const auto folder = std::filesystem::absolute(database.parent_path()).lexically_normal();
        if (folder == root || folder.parent_path() != root ||
            !ordinarySavePath(root.parent_path(), true) || !ordinarySavePath(root, true) ||
            !ordinarySavePath(folder, true) || !ordinarySavePath(folder / "World.db", false))
            throw std::runtime_error("This save is missing or is not a regular league folder in Data/World.");

        const auto resolvedRoot = std::filesystem::canonical(root);
        const auto resolvedFolder = std::filesystem::canonical(folder);
        if (resolvedFolder.parent_path() != resolvedRoot)
            throw std::runtime_error("The selected league folder is outside Data/World.");

        for (const auto& entry : std::filesystem::recursive_directory_iterator(resolvedFolder))
        {
            const auto attributes = GetFileAttributesW(entry.path().c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
                throw std::runtime_error("The selected league contains a linked or unreadable entry.");
        }
        return resolvedFolder;
    }
}

CommissionersOfficeScreen::CommissionersOfficeScreen(
    Assets::AssetLibrary& assetLibrary,
    std::function<bool(std::string_view)> commandDispatcher,
    std::function<LoadedWorldContext()> loadedWorldContextProvider,
    std::function<bool(const std::filesystem::path&)> loadLeagueCallback,
    std::function<void(const WorldEditCommit&)> commitCallback)
    : m_assetLibrary(assetLibrary)
    , m_commandDispatcher(std::move(commandDispatcher))
    , m_loadedWorldContextProvider(std::move(loadedWorldContextProvider))
    , m_loadLeagueCallback(std::move(loadLeagueCallback))
    , m_commitCallback(std::move(commitCallback))
{
    buildWidgets();
}

CommissionersOfficeScreen::~CommissionersOfficeScreen()
{
}

void CommissionersOfficeScreen::onEnter()
{
    // Styles used by this method. Change these theme mappings to restyle it.
    const Style& titleStyle = PFF_Colors::ProFootballTitleColor; // Commissioner banner lettering.

    m_root.resetFocus();
    refreshEditState(true);
    m_fontResult = m_assetLibrary.loadBannerFontAsset(m_TitleBannerFontKey);

    if (!m_fontResult.success || !m_fontResult.hasFont())
    {
        return;
    }

    m_CommissionersOfficeTitleObject = BannerFactory::makeBanner(
        *m_fontResult.asset.font,
        "COMMISSIONER'S OFFICE",
        titleStyle);
}

void CommissionersOfficeScreen::onExit()
{
    closeLeagueSetup();
    closeCoreBuild();
    m_CommissionersOfficeTitleObject.clear();
    cancelLoadLeagueSelection();
    cancelDeleteLeague();
    closeAddons();
    m_groundRulesActive = false;
}

bool CommissionersOfficeScreen::handleEvent(const Input::Event& event)
{
    if (m_groundRulesActive)
    {
        if (const auto* command = event.asCommand())
        {
            const auto code = command->command.code;
            if (code == Input::CommandCode::Cancel || code == Input::CommandCode::Back || code == Input::CommandCode::Close)
            {
                m_groundRulesActive = false;
                return true;
            }
        }
        m_groundRulesRoot.handleEvent(event);
        return true;
    }
    if (m_addonsActive)
        return handleAddonsEvent(event);
    if (m_deleteLeagueActive)
        return handleDeleteEvent(event);
    if (m_leagueStage != LeagueStage::Closed)
    {
        if (const auto* command=event.asCommand())
        {
            const auto code=command->command.code;
            if(m_coreChoicesOpen && (code==Input::CommandCode::NextFocus || code==Input::CommandCode::PreviousFocus))
            {if(m_leagueRoot.focusedChild()) selectLeagueCore();return m_leagueRoot.handleEvent(event);}
            if(code==Input::CommandCode::Cancel || code==Input::CommandCode::Back || code==Input::CommandCode::Close)
            {
                if(m_coreChoicesOpen) {m_coreChoicesOpen=false; m_coreChoices->setVisible(false); m_coreChoices->setEnabled(false); m_leagueRoot.setFocusedChild(*m_coreSelector);}
                else closeLeagueSetup();
                return true;
            }
            if(m_coreChoicesOpen && code==Input::CommandCode::Confirm) {if(m_leagueRoot.focusedChild()==m_coreChoices) selectLeagueCore();return true;}
            if(!m_coreChoicesOpen && code==Input::CommandCode::Confirm)
            {if(auto* focus=m_leagueRoot.focusedChild()) focus->handleEvent(event);return true;}
        }
        if(m_coreChoicesOpen)
        {
            const bool handled=m_leagueRoot.handleEvent(event);
            if(const auto* mouse=event.asMouse())
                if(handled && m_leagueRoot.focusedChild()==m_coreChoices && mouse->button==Input::MouseButton::Left && (mouse->isPress()||mouse->isClick())) selectLeagueCore();
            return handled;
        }
        return m_leagueRoot.handleEvent(event);
    }
    if (m_coreStage != CoreStage::Closed)
    {
        if (const auto* command = event.asCommand())
        {
            const auto code = command->command.code;
            if(m_openDropdown && (code==Input::CommandCode::NextFocus || code==Input::CommandCode::PreviousFocus))
            {if(m_coreRoot.focusedChild()) closePackageDropdown(true);return m_coreRoot.handleEvent(event);}
            if (code == Input::CommandCode::Cancel || code == Input::CommandCode::Close || code == Input::CommandCode::Back)
            {
                if (m_openDropdown) closePackageDropdown(false);
                else if (m_coreStage == CoreStage::Configure || m_coreStage == CoreStage::Result) closeCoreBuild();
                return true;
            }
            if (m_openDropdown && code == Input::CommandCode::Confirm)
            { if(m_coreRoot.focusedChild()==m_packageList) closePackageDropdown(true); return true; }
            if (code == Input::CommandCode::Refresh && m_coreStage == CoreStage::Configure)
            { reloadCorePackages(); return true; }
        }
        if (m_openDropdown)
        {
            const bool handled = m_coreRoot.handleEvent(event);
            if (const auto* mouse = event.asMouse())
                if (m_coreRoot.focusedChild()==m_packageList && mouse->button == Input::MouseButton::Left && (mouse->isPress() || mouse->isClick()) && handled)
                    closePackageDropdown(true);
            return handled;
        }
        // The form consumes Confirm after offering it only to its active
        // control, including when a results list has nothing to activate.
        if (const auto* command = event.asCommand())
            if (command->command.code == Input::CommandCode::Confirm)
            {
                if (auto* focused = m_coreRoot.focusedChild()) focused->handleEvent(event);
                return true;
            }
        return m_coreRoot.handleEvent(event);
    }
    if (m_loadLeagueSelectionActive)
    {
        if (const auto* command=event.asCommand())
        {
            const auto code=command->command.code;
            if(code==Input::CommandCode::Cancel || code==Input::CommandCode::Close || code==Input::CommandCode::Back)
            {
                if(m_savedLeaguesOpen) closeSavedLeagueDropdown(false);
                else cancelLoadLeagueSelection();
                return true;
            }
            if(m_savedLeaguesOpen && (code==Input::CommandCode::NextFocus || code==Input::CommandCode::PreviousFocus))
            {
                if(m_loadRoot.focusedChild()) closeSavedLeagueDropdown(true);
                return m_loadRoot.handleEvent(event);
            }
            if(m_savedLeaguesOpen && code==Input::CommandCode::Confirm)
            {
                if(m_loadRoot.focusedChild()==m_leagueListBox) closeSavedLeagueDropdown(true);
                return true;
            }
        }
        const bool handled=m_loadRoot.handleEvent(event);
        if(m_savedLeaguesOpen)
            if(const auto* mouse=event.asMouse())
                if(handled && m_loadRoot.focusedChild()==m_leagueListBox &&
                    mouse->button==Input::MouseButton::Left && (mouse->isPress() || mouse->isClick()))
                    closeSavedLeagueDropdown(true);
        return handled;
    }

    const bool handled=m_root.handleEvent(event);
    const auto navigation=std::move(m_pendingNavigation);m_pendingNavigation.reset();
    const auto dispatcher=m_commandDispatcher;
    if(m_requestedGod){const bool enabled=*m_requestedGod;m_requestedGod.reset();toggleGodMode(enabled);}
    if(navigation&&dispatcher)dispatcher(*navigation); // May destroy this screen; use only locals afterward.
    return handled;
}

void CommissionersOfficeScreen::update(const Animation::TickEvent& event)
{
    if(m_leagueCreation.valid() && m_leagueCreation.wait_for(std::chrono::seconds(0))==std::future_status::ready)
    {
        m_leagueMessages.clear();
        try
        {
            const auto result=m_leagueCreation.get();
            m_leagueMessages.push_back(result.success?"SUCCESS: League created at "+result.databasePath.u8string():"League creation failed; existing saves are unchanged.");
            for(const auto& diagnostic:result.diagnostics) m_leagueMessages.push_back(diagnostic.message);
            if(result.success)
            {
                m_leagueMessages.push_back("Preserved Core: "+result.coreSnapshotPath.u8string());
                m_leagueMessages.push_back("Snapshot SHA-256: "+result.coreSnapshotFileHash);
                if(!m_loadLeagueCallback || !m_loadLeagueCallback(result.databasePath))
                    m_leagueMessages.push_back("The league was saved but could not be loaded. Use Load League to retry.");
                else m_leagueMessages.push_back("League loaded.");
            }
        }
        catch(const std::exception& ex){m_leagueMessages.push_back(ex.what());}
        m_leagueStage=LeagueStage::Result; m_leagueDetailsKey.clear();
        m_cancelLeague->setVisible(true); m_cancelLeague->setEnabled(true); m_cancelLeague->setLabel("Continue");
        m_leagueRoot.setFocusedChild(*m_cancelLeague);
    }
    if(m_leagueStage!=LeagueStage::Closed) {m_leagueRoot.update(event);return;}
    if (m_coreBuildTask.valid() && m_coreBuildTask.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        try { finishCoreBuild(m_coreBuildTask.get()); }
        catch (const std::exception& ex)
        {
            CoreDatabaseBuildResult result;
            result.diagnostics.push_back({DatabaseDiagnosticSeverity::Error, ex.what(), {}, 0});
            finishCoreBuild(std::move(result));
        }
    }
    if (m_coreStage != CoreStage::Closed) m_coreRoot.update(event);
    else if(m_loadLeagueSelectionActive) m_loadRoot.update(event);
    else if (m_groundRulesActive) m_groundRulesRoot.update(event);
    else if (m_addonsActive) m_addonsRoot.update(event);
    else if (m_deleteLeagueActive) m_deleteRoot.update(event);
    else m_root.update(event);
}

void CommissionersOfficeScreen::draw(Surface& surface)
{
    const Style& officeModeStyle=PFF_Colors::TextHighlightColor; // God-mode state and errors.
    // Styles used by this method. Change these theme mappings to restyle it.
    const Style& backgroundStyle = PFF_Colors::MainBackgroundColor; // Screen background.
    const Style& headerFillStyle = PFF_Colors::HeaderOuterColor; // Outer header fill.
    const Style& headerBorderStyle = PFF_Colors::HeaderOuterColor; // Outer header border.
    const Style& headerInnerStyle = PFF_Colors::HeaderInnerColor; // Inner banner background.

    ScreenBuffer& buffer = surface.buffer();
    buffer.clear(backgroundStyle);

    PageComposer page(buffer);
    page.clearRegions();
    page.createFullScreenRegion("Screen");

    const Rect screen = page.resolveRegion("Screen");
    const auto [header, afterHeader] = page.splitTop(screen, 12);
    const auto [footer, body] = page.splitBottom(afterHeader, 5);

    const int controlWidth =
        std::min(34, std::max(28, body.size.width / 3));

    // PageComposer split helpers return { slice, remainder }.
    // splitRight() therefore returns the RIGHT control slice first and the
    // remaining LEFT content pane second.
    const auto [controlPane, contentPane] =
        page.splitRight(body, controlWidth);

    m_root.setBounds(body);
    refreshEditState();
    positionButtons(controlPane, footer);

    page.createRegion("Header", header);
    page.createInsetRegion("InnerPanel", "Header", 2, 1, 2, 1);
    page.createInsetRegion("TitleArea", "InnerPanel", 4, 2, 2, 1);

    page.createRegion("Footer", footer);
    page.createRegion("Body", body);
    page.createRegion("ContentPane", contentPane);
    page.createRegion("ControlPane", controlPane);

    page.createInsetRegion(
        "LeaguePanel",
        "ContentPane",
        2, 2, 2, 2);

    page.createInsetRegion(
        "LeagueContent",
        "LeaguePanel",
        3, 3, 3, 2);

    page.drawPanel(
        "Header",
        headerFillStyle,
        headerBorderStyle,
        ObjectFactory::doubleLineBorder());

    buffer.fillRect(
        page.resolveRegion("InnerPanel"),
        U' ',
        headerInnerStyle);

    page.writeObjectInRegion(
        m_CommissionersOfficeTitleObject,
        "TitleArea",
        Composition::Align::center());

    if (m_leagueStage != LeagueStage::Closed)
    {
        drawLeagueSetup(page,controlPane);
        m_leagueRoot.draw(surface);
        return;
    }

    if (m_coreStage != CoreStage::Closed)
    {
        drawCoreBuild(page, controlPane);
        m_coreRoot.draw(surface);
        return;
    }


    if (m_loadLeagueSelectionActive)
    {
        drawLoadLeagueSelection(page,controlPane);
        m_loadRoot.draw(surface);
        return;
    }
    else
    {
        if (m_leagueListBox)
        {
            m_leagueListBox->setVisible(false);
            m_leagueListBox->setEnabled(false);
        }

        if (m_loadedWorldContextProvider)
        {
            const LoadedWorldContext worldContext =
                m_loadedWorldContextProvider();

            if (worldContext.isLoaded())
            {
                drawLeagueDetails(page, worldContext);
            }
        }
    }

    if(m_godMode->isVisible()){
        const auto lines=wrapText(m_editMessage.empty()?std::string(m_editState.godMode?"God mode: ON. Supported league edits are unlocked.":"God mode: OFF. Controlled-role permissions apply."):m_editMessage,std::max(1,controlPane.size.width-4));
        for(std::size_t i=0;i<lines.size()&&i<6;++i)page.writeLine(controlPane,25+static_cast<int>(i),"  "+lines[i],officeModeStyle);
    }
    m_root.draw(surface);
    if (m_groundRulesActive)
    {
        drawGroundRules(page);
        m_groundRulesRoot.draw(surface);
        return;
    }
    if (m_deleteLeagueActive)
    {
        drawDeleteLeague(page);
        m_deleteRoot.draw(surface);
    }
    else if (m_addonsActive)
    {
        drawAddons(page);
        m_addonsRoot.draw(surface);
    }
}

void CommissionersOfficeScreen::buildWidgets()
{
    // Checkbox: independent appearance for World God-mode state.
    const Style& checkboxNormal=PFF_Colors::InputFieldNormalColor;
    const Style& checkboxActive=PFF_Colors::InputFieldFocusedColor;
    const Style& checkboxPressed=PFF_Colors::ButtonPressedColor;
    const Style checkboxDisabled=checkboxNormal.withDim(true);
    // Styles by COMPONENT TYPE. Instances of the same type share these mappings.

    // Action buttons: Create, Build, Open, Reload, Cancel and Continue.
    const Style& ButtonNormalStyle = PFF_Colors::ButtonNormalColor; // Idle button.
    const Style& ButtonFocusedStyle = PFF_Colors::ButtonHoverColor; // Keyboard-focused button.
    const Style& ButtonHoverStyle = PFF_Colors::ButtonHoverColor; // Mouse over a button.
    const Style& ButtonPressedStyle = PFF_Colors::ButtonPressedColor; // Pressed button.

    // Text inputs: League Name and Core Database Name.
    const Style& InputNormalStyle = PFF_Colors::InputFieldNormalColor; // Entered text, not focused.
    const Style& InputFocusedStyle = PFF_Colors::InputFieldFocusedColor; // Entered text while focused.
    const Style& InputHoverStyle = PFF_Colors::InputFieldFocusedColor; // Mouse over an unfocused input.

    const Style& InputPlaceholderNormalStyle = PFF_Colors::InputFieldNormalHintColor; // Hint when the empty input is not focused.
    const Style& InputPlaceholderFocusedStyle = PFF_Colors::InputFieldFocusedHintColor; // Hint when the empty input has keyboard focus.

    // Closed dropdown controls: Core Database, Ruleset, Season and saved-league selectors.
    const Style& DropdownNormalStyle = PFF_Colors::InputFieldNormalColor; // Closed selector, idle.
    const Style& DropdownFocusedStyle = PFF_Colors::InputFieldFocusedColor; // Closed selector, keyboard focus.
    const Style& DropdownHoverStyle = PFF_Colors::ButtonHoverColor; // Mouse over the closed selector.
    const Style& DropdownPressedStyle = PFF_Colors::ButtonPressedColor; // Pressed selector.

    // Open dropdown option lists: the choices shown after opening a selector.
    const Style& DropdownListNormalStyle = PFF_Colors::InputFieldNormalColor; // Unselected option, not focused.
    const Style& DropdownListFocusedStyle = PFF_Colors::InputFieldNormalColor; // Unselected option, list focused.
    const Style& DropdownListSelectedStyle = PFF_Colors::InputFieldFocusedColor; // Selected option, not focused.
    const Style& DropdownListSelectedFocusedStyle = PFF_Colors::ButtonHoverColor; // Selected option, list focused.

    // General lists: Core Details, existing saves and build results.
    const Style& ListNormalStyle = PFF_Colors::InputFieldNormalColor; // Unselected row, not focused.
    const Style& ListFocusedStyle = PFF_Colors::InputFieldNormalColor; // Unselected row, list focused.
    const Style& ListSelectedStyle = PFF_Colors::InputFieldNormalColor; // Selected row, not focused.
    const Style& ListSelectedFocusedStyle = PFF_Colors::InputFieldFocusedColor; // Selected row, list focused.

    auto leagueName=std::make_unique<TextInput>();
    leagueName->setMaxLength(64); leagueName->setPlaceholder("<Enter league name>");
    leagueName->setNormalStyle(InputNormalStyle); leagueName->setFocusedStyle(InputFocusedStyle);
    leagueName->setHoverStyle(InputHoverStyle);
    leagueName->setPlaceholderNormalStyle(InputPlaceholderNormalStyle);
    leagueName->setPlaceholderFocusedStyle(InputPlaceholderFocusedStyle);
    m_leagueName=leagueName.get(); m_leagueRoot.addChild(std::move(leagueName));
    auto leagueButton=[&](const std::string& label,auto callback) {
        auto button=std::make_unique<Button>(label);
        button->setActivationCallback([callback](const ButtonActivationResult& result){if(result.activated) callback();});
        auto* pointer=button.get();m_leagueRoot.addChild(std::move(button));return pointer;
    };
    m_coreSelector=leagueButton("Select Core Database v",[this] {
        if(m_installedCores.empty()) return;
        m_coreChoicesOpen=true; m_coreChoices->setVisible(true);m_coreChoices->setEnabled(true);
        m_coreChoices->selectIndex(m_chosenCore);m_leagueRoot.setFocusedChild(*m_coreChoices);
    });
    m_createLeague=leagueButton("Create League",[this]{createSelectedLeague();});
    m_cancelLeague=leagueButton("Cancel",[this]{closeLeagueSetup();});
    auto leagueList=[&] {
        auto list=std::make_unique<ListBox>();
        list->setVerticalScrollbarVisible(true);auto* ptr=list.get();m_leagueRoot.addChild(std::move(list));return ptr;
    };
    m_leagueDetails=leagueList();m_existingSaves=leagueList();m_coreChoices=leagueList();
    auto name = std::make_unique<TextInput>();
    name->setPlaceholder("<Enter database name>");
    name->setMaxLength(64);
    m_coreName = name.get();
    m_coreRoot.addChild(std::move(name));
    auto addButton = [&](const std::string& label, auto callback) {
        auto button = std::make_unique<Button>(label);
        button->setActivationCallback([callback](const ButtonActivationResult& result) {
            if (result.activated) callback();
        });
        auto* ptr = button.get();
        m_coreRoot.addChild(std::move(button));
        return ptr;
    };
    m_rulesetDropdown = addButton("Select ruleset v", [this] { openPackageDropdown(true); });
    m_seasonDropdown = addButton("Select season v", [this] { openPackageDropdown(false); });
    m_coreBuildButton = addButton("Build Core Database", [this] { buildCoreDatabase(); });
    m_reloadPackages = addButton("Reload Packages", [this] { reloadCorePackages(); });
    m_coreBack = addButton("Cancel", [this] {
        if (m_coreStage == CoreStage::Configure || m_coreStage == CoreStage::Result) closeCoreBuild();
    });
    auto messages = std::make_unique<ListBox>();
    m_buildMessages = messages.get();
    m_coreRoot.addChild(std::move(messages));
    auto packageList = std::make_unique<ListBox>();
    m_packageList = packageList.get();
    m_coreRoot.addChild(std::move(packageList));
    m_coreName->setNormalStyle(InputNormalStyle);
    m_coreName->setFocusedStyle(InputFocusedStyle);
    m_coreName->setHoverStyle(InputHoverStyle);
    m_coreName->setPlaceholderNormalStyle(InputPlaceholderNormalStyle);
    m_coreName->setPlaceholderFocusedStyle(InputPlaceholderFocusedStyle);
    // Apply the component-type mappings declared above.
    auto styleButton = [](Button& button, const Style& normal, const Style& focused,
                          const Style& hover, const Style& pressed) {
        button.setNormalStyle(normal);
        button.setFocusedStyle(focused);
        button.setHoverStyle(hover);
        button.setPressedStyle(pressed);
    };

    auto styleList = [](ListBox& list, const Style& normal, const Style& focused,
        const Style& selected, const Style& selectedFocused) {
            list.setNormalStyle(normal);
            list.setFocusedStyle(focused);
            list.setSelectedStyle(selected);
            list.setSelectedFocusedStyle(selectedFocused);
            list.setVerticalScrollbarVisible(true);

            auto scrollbar = list.verticalScrollbarStyle();
            scrollbar.thumbStyle = PFF_Colors::PanelSliderColor;
            scrollbar.trackStyle = PFF_Colors::PanelTrackColor;
            list.setVerticalScrollbarStyle(scrollbar);
        };

    styleButton(*m_coreSelector, DropdownNormalStyle, DropdownFocusedStyle, DropdownHoverStyle, DropdownPressedStyle);
    styleButton(*m_createLeague, ButtonNormalStyle, ButtonFocusedStyle, ButtonHoverStyle, ButtonPressedStyle);
    styleButton(*m_cancelLeague, ButtonNormalStyle, ButtonFocusedStyle, ButtonHoverStyle, ButtonPressedStyle);
    styleButton(*m_rulesetDropdown, DropdownNormalStyle, DropdownFocusedStyle, DropdownHoverStyle, DropdownPressedStyle);
    styleButton(*m_seasonDropdown, DropdownNormalStyle, DropdownFocusedStyle, DropdownHoverStyle, DropdownPressedStyle);
    styleButton(*m_coreBuildButton, ButtonNormalStyle, ButtonFocusedStyle, ButtonHoverStyle, ButtonPressedStyle);
    styleButton(*m_reloadPackages, ButtonNormalStyle, ButtonFocusedStyle, ButtonHoverStyle, ButtonPressedStyle);
    styleButton(*m_coreBack, ButtonNormalStyle, ButtonFocusedStyle, ButtonHoverStyle, ButtonPressedStyle);
    styleList(*m_coreChoices, DropdownListNormalStyle, DropdownListFocusedStyle, DropdownListSelectedStyle, DropdownListSelectedFocusedStyle);
    styleList(*m_packageList, DropdownListNormalStyle, DropdownListFocusedStyle, DropdownListSelectedStyle, DropdownListSelectedFocusedStyle);
    styleList(*m_leagueDetails, ListNormalStyle, ListFocusedStyle, ListSelectedStyle, ListSelectedFocusedStyle);
    styleList(*m_existingSaves, ListNormalStyle, ListFocusedStyle, ListSelectedStyle, ListSelectedFocusedStyle);
    styleList(*m_buildMessages, ListNormalStyle, ListFocusedStyle, ListSelectedStyle, ListSelectedFocusedStyle);

    auto buildCoreDbButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 28, 3 } },
        "Build Core DB",
        std::string(BuildCoreDbCommand));

    auto newLeagueButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 28, 3 } },
        "New League",
        std::string(NewLeagueCommand));

    auto loadLeagueButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 28, 3 } },
        "Load League",
        std::string(LoadLeagueCommand));

    auto mainButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 28, 3 } },
        "Return",
        "nav:back");

    auto loadButton=[&](const std::string& label,auto callback) {
        auto button=std::make_unique<Button>(label);
        button->setActivationCallback([callback](const ButtonActivationResult& result){if(result.activated) callback();});
        auto* ptr=button.get();m_loadRoot.addChild(std::move(button));return ptr;
    };
    m_savedLeagueSelector=loadButton("Select saved league v",[this]{openSavedLeagueDropdown();});
    m_openLeague=loadButton("Open League",[this]{loadSelectedLeague();});
    m_cancelLoadLeague=loadButton("Cancel",[this]{cancelLoadLeagueSelection();});
    styleButton(*m_savedLeagueSelector,DropdownNormalStyle,DropdownFocusedStyle,DropdownHoverStyle,DropdownPressedStyle);
    styleButton(*m_openLeague,ButtonNormalStyle,ButtonFocusedStyle,ButtonHoverStyle,ButtonPressedStyle);
    styleButton(*m_cancelLoadLeague,ButtonNormalStyle,ButtonFocusedStyle,ButtonHoverStyle,ButtonPressedStyle);
    auto leagueListBox = std::make_unique<ListBox>();
    styleList(*leagueListBox, DropdownListNormalStyle, DropdownListFocusedStyle, DropdownListSelectedStyle, DropdownListSelectedFocusedStyle);
    leagueListBox->setVisible(false);
    leagueListBox->setEnabled(false);

    buildCoreDbButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    newLeagueButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    loadLeagueButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    mainButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    m_buildCoreDbButton = buildCoreDbButton.get();
    m_newLeagueButton = newLeagueButton.get();
    m_loadLeagueButton = loadLeagueButton.get();
    m_mainButton = mainButton.get();
    m_leagueListBox = leagueListBox.get();

    m_root.addChild(std::move(buildCoreDbButton));
    m_root.addChild(std::move(newLeagueButton));
    m_root.addChild(std::move(loadLeagueButton));
    const auto addOfficeButton = [&](const char* label, const char* command = "") {
        auto button = std::make_unique<Button>(Rect{}, label, command);
        button->setActivationCallback(
            [this](const ButtonActivationResult& result) { handleButtonActivation(result); });
        button->setDisabledStyle(ButtonNormalStyle.withDim(true));
        auto* result = button.get();
        m_root.addChild(std::move(button));
        return result;
    };
    m_deleteLeagueButton = addOfficeButton("Delete League", DeleteLeagueCommand.data());
    m_closeLeagueButton = addOfficeButton("Close League", "action:pff:close_league");
    m_addonsButton = addOfficeButton("Addons", AddonsCommand.data());
    m_groundRulesButton = addOfficeButton("Ground Rules");
    m_groundRulesButton->setActivationCallback([this](const ButtonActivationResult& result) {
        if (result.activated) openGroundRules();
    });
    for (std::size_t index = 0; index < m_groundRuleChecks.size(); ++index)
    {
        auto check = std::make_unique<FormCheckBox>(GroundRules::labels[index]);
        styleButton(*check, checkboxNormal, checkboxActive, checkboxActive, checkboxPressed);
        check->setChangeCallback([this](bool) { refreshGroundRulesToggle(); });
        m_groundRuleChecks[index] = check.get();
        m_groundRulesRoot.addChild(std::move(check));
    }
    auto seconds = std::make_unique<TextInput>();
    seconds->setNormalStyle(InputNormalStyle);
    seconds->setFocusedStyle(InputFocusedStyle);
    seconds->setHoverStyle(InputHoverStyle);
    seconds->setMaxLength(10);
    m_playClockSeconds = seconds.get();
    m_groundRulesRoot.addChild(std::move(seconds));
    const auto addGroundRulesButton = [&](const char* label, auto callback) {
        auto button = std::make_unique<Button>(label);
        styleButton(*button, ButtonNormalStyle, ButtonFocusedStyle, ButtonHoverStyle, ButtonPressedStyle);
        button->setActivationCallback([callback](const ButtonActivationResult& result) {
            if (result.activated) callback();
        });
        auto* widget = button.get();
        m_groundRulesRoot.addChild(std::move(button));
        return widget;
    };
    m_groundRulesToggle = addGroundRulesButton("Check All", [this] {
        const bool checked = std::any_of(m_groundRuleChecks.begin(), m_groundRuleChecks.end(),
            [](const auto* check) { return check->isVisible() && !check->checked(); });
        for (auto* check : m_groundRuleChecks)
            if (check->isVisible()) check->setChecked(checked);
        refreshGroundRulesToggle();
    });
    m_groundRulesSave = addGroundRulesButton("Save & Close", [this] { saveGroundRules(); });
    m_groundRulesCancel = addGroundRulesButton("Cancel", [this] { m_groundRulesActive = false; });
    auto god=std::make_unique<FormCheckBox>("God Mode");
    god->setNormalStyle(checkboxNormal);god->setFocusedStyle(checkboxActive);god->setHoverStyle(checkboxActive);god->setPressedStyle(checkboxPressed);god->setDisabledStyle(checkboxDisabled);
    god->setChangeCallback([this](bool enabled){m_requestedGod=enabled;});m_godMode=god.get();m_root.addChild(std::move(god));
    m_root.addChild(std::move(mainButton));
    for (std::size_t index = 0; index < m_addonChecks.size(); ++index)
    {
        auto checkbox = std::make_unique<FormCheckBox>(LeagueSetup::AddonNames[index]);
        styleButton(*checkbox, checkboxNormal, checkboxActive, checkboxActive, checkboxPressed);
        checkbox->setDisabledStyle(checkboxDisabled);
        checkbox->setEnabled(false);
        m_addonChecks[index] = checkbox.get();
        m_leagueRoot.addChild(std::move(checkbox));
    }
    auto addonsClose = std::make_unique<Button>("Close");
    styleButton(*addonsClose, ButtonNormalStyle, ButtonFocusedStyle, ButtonHoverStyle, ButtonPressedStyle);
    addonsClose->setActivationCallback([this](const ButtonActivationResult& result) {
        m_closeAddonsRequested = result.activated;
    });
    m_addonsClose = addonsClose.get();
    m_addonsRoot.addChild(std::move(addonsClose));

    auto deleteList = std::make_unique<ScrollBox>();
    m_deleteList = deleteList.get();
    m_deleteRoot.addChild(std::move(deleteList));
    const auto deleteButton = [&](const char* label, auto callback) {
        auto button = std::make_unique<Button>(label);
        styleButton(*button, ButtonNormalStyle, ButtonFocusedStyle, ButtonHoverStyle, ButtonPressedStyle);
        button->setDisabledStyle(ButtonNormalStyle.withDim(true));
        button->setActivationCallback([callback](const ButtonActivationResult& result) {
            if (result.activated)
                callback();
        });
        auto* result = button.get();
        m_deleteRoot.addChild(std::move(button));
        return result;
    };
    m_confirmDelete = deleteButton("Confirm Delete", [this] { m_deleteConfirmed = true; });
    m_cancelDelete = deleteButton("Cancel", [this] { m_deleteCanceled = true; });
    m_loadRoot.addChild(std::move(leagueListBox));
}

void CommissionersOfficeScreen::positionButtons(
    const Rect& controlPane,
    const Rect& footer)
{
    // Main office action buttons: one shared mapping for all four buttons.
    const Style& ButtonNormalStyle = PFF_Colors::ButtonNormalColor; // Idle button.
    const Style& ButtonFocusedStyle = PFF_Colors::ButtonHoverColor; // Keyboard-focused button.
    const Style& ButtonHoverStyle = PFF_Colors::ButtonHoverColor; // Mouse over a button.
    const Style& ButtonPressedStyle = PFF_Colors::ButtonPressedColor; // Pressed button.

    const Size controlButtonSize{ 24, 3 };
    const int controlX =
        controlPane.position.x +
        ((controlPane.size.width - controlButtonSize.width) / 2);

    const int firstButtonY = controlPane.position.y + 5;
    constexpr int buttonSpacing = 5;

    const auto position = [&](Button& button, int slot) {
        button.setBounds(Rect{Point{controlX, firstButtonY + buttonSpacing * slot}, controlButtonSize});
        button.setNormalStyle(ButtonNormalStyle);
        button.setHoverStyle(ButtonHoverStyle);
        button.setPressedStyle(ButtonPressedStyle);
        button.setFocusedStyle(ButtonFocusedStyle);
    };
    position(*m_buildCoreDbButton, 0);
    position(*m_newLeagueButton, 1);
    position(*m_loadLeagueButton, 2);
    position(*m_deleteLeagueButton, 3);
    position(*m_closeLeagueButton, 0);
    position(*m_addonsButton, 1);
    position(*m_groundRulesButton, 2);
    m_godMode->setBounds(Rect{Point{controlX, firstButtonY + buttonSpacing * 3}, controlButtonSize});

    if (m_mainButton)
    {
        const Size buttonSize{ 28, 3 };

        m_mainButton->setBounds(Rect{
            Point{
                footer.position.x + ((footer.size.width - buttonSize.width) - 10),
                footer.position.y + ((footer.size.height - buttonSize.height) / 2)
            },
            buttonSize
            });

        m_mainButton->setNormalStyle(ButtonNormalStyle);
        m_mainButton->setHoverStyle(ButtonHoverStyle);
        m_mainButton->setPressedStyle(ButtonPressedStyle);
        m_mainButton->setFocusedStyle(ButtonFocusedStyle);
    }
}

void CommissionersOfficeScreen::handleButtonActivation(
    const ButtonActivationResult& result)
{
    if (!result.activated || result.commandId.empty())
    {
        return;
    }

    if (result.commandId == BuildCoreDbCommand)
    {
        beginCoreBuild();
        return;
    }

    if (result.commandId == NewLeagueCommand)
    {
        beginLeagueSetup();
        return;
    }

    if (result.commandId == LoadLeagueCommand)
    {
        beginLoadLeagueSelection();
        return;
    }

    if (result.commandId == DeleteLeagueCommand)
    {
        beginDeleteLeague();
        return;
    }

    if (result.commandId == AddonsCommand)
    {
        beginAddons();
        return;
    }

    // Defer navigation until widget dispatch and pending edit-mode handling end.
    m_pendingNavigation=result.commandId;
}

void CommissionersOfficeScreen::buildCoreDatabase()
{
    if (m_coreStage != CoreStage::Configure || m_coreBuildTask.valid()) return;
    clearCoreMessages();
    m_coreStage = CoreStage::Building;
    // Hide actions immediately, including before the next frame is drawn.
    for (auto* widget : std::vector<Widget*>{m_coreName, m_rulesetDropdown, m_seasonDropdown,
        m_coreBuildButton, m_reloadPackages, m_coreBack, m_packageList})
    { widget->setVisible(false); widget->setEnabled(false); }
    m_buildMessages->setVisible(true);
    m_buildMessages->setEnabled(true);
    m_coreRoot.setFocusedChild(*m_buildMessages);
    try
    {
        const auto error = CoreManifest::basenameError(m_coreName->text());
        if (!error.empty()) throw std::runtime_error(error);
        if (m_rulesets.empty() || m_seasons.empty())
            throw std::runtime_error("Install a valid ruleset and season, then reopen Build Core DB.");
        CoreDatabaseBuildRequest request;
        request.sourceRoot = std::filesystem::path("Data") / "Source";
        request.rulesetManifest = m_rulesets.at(m_selectedRuleset).path;
        request.seasonManifest = m_seasons.at(m_selectedSeason).path;
        request.outputDatabase = std::filesystem::path("Data") / (m_coreName->text() + ".db");
        addCoreMessage("Database: " + request.outputDatabase.u8string());
        addCoreMessage("Ruleset: " + m_rulesets.at(m_selectedRuleset).displayName);
        addCoreMessage("Season: " + m_seasons.at(m_selectedSeason).displayName);
        addCoreMessage("Building Core database. Please wait...");
        for (const auto& message : m_discoveryMessages) addCoreMessage("WARNING: " + message);
        // The worker owns only a request copy. Widgets and presentation state
        // stay on the UI thread; the shared builder validates and publishes once.
        m_coreBuildTask = std::async(std::launch::async, [request] {
            return createCoreDatabaseFromSource(request);
        });
    }
    catch (const std::exception& ex)
    {
        CoreDatabaseBuildResult result;
        result.diagnostics.push_back({DatabaseDiagnosticSeverity::Error, ex.what(), {}, 0});
        finishCoreBuild(std::move(result));
    }
}

void CommissionersOfficeScreen::finishCoreBuild(CoreDatabaseBuildResult result)
{
    clearCoreMessages();
    addCoreMessage(result.success ? "SUCCESS: " + result.databasePath.u8string() : "BUILD FAILED: existing Core preserved.");
    addCoreMessage("Imported files: " + std::to_string(result.filesProcessed) +
        "; inserted records: " + std::to_string(result.recordsInserted));
    for (const auto& diagnostic : result.diagnostics)
    {
        const auto prefix = diagnostic.severity == DatabaseDiagnosticSeverity::Error ? "ERROR: " :
            diagnostic.severity == DatabaseDiagnosticSeverity::Warning ? "WARNING: " : "OK: ";
        addCoreMessage(prefix + diagnostic.message + " Source: " + diagnostic.sourceFile.u8string() + ":" +
            std::to_string(diagnostic.sourceLine));
    }
    m_coreStage = CoreStage::Result;
    m_coreBack->setLabel("Continue");
    m_coreBack->setVisible(true);
    m_coreBack->setEnabled(true);
    m_coreRoot.setFocusedChild(*m_coreBack);
}

void CommissionersOfficeScreen::beginCoreBuild()
{
    cancelLoadLeagueSelection();
    m_coreStage = CoreStage::Configure;
    reloadCorePackages();
    m_coreRoot.resetFocus();
}

void CommissionersOfficeScreen::closeCoreBuild()
{
    if (m_coreBuildTask.valid()) return;
    m_coreStage = CoreStage::Closed;
    m_openDropdown = 0;
    m_coreName->clear();
    m_rulesets.clear();
    m_seasons.clear();
    m_discoveryMessages.clear();
    clearCoreMessages();
    m_coreRoot.clearChildFocus();
    if (m_buildCoreDbButton) m_root.setFocusedChild(*m_buildCoreDbButton);
}

void CommissionersOfficeScreen::reloadCorePackages()
{
    m_openDropdown = 0;
    m_discoveryMessages.clear();
    const auto root = std::filesystem::path("Data") / "Source";
    m_rulesets = CoreManifest::discover(root / "RuleSets", ".pffruleset", "RULESET", m_discoveryMessages);
    m_seasons = CoreManifest::discover(root / "Seasons", ".pffseason", "SEASON", m_discoveryMessages);
    // v0.1 defines no add-on compatibility grammar or importers. Do not
    // infer compatibility or offer checkboxes for unsupported packages.
    try {
        if (std::filesystem::exists(root / "AddOns"))
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root / "AddOns"))
                if (entry.is_regular_file() && entry.path().extension() == ".pffaddon")
                    m_discoveryMessages.push_back("Unsupported add-on (not selectable): " + entry.path().u8string());
    }
    catch (const std::exception& ex) { m_discoveryMessages.push_back(ex.what()); }
    m_selectedRuleset = m_selectedSeason = 0;
    refreshCoreLabels();
}

void CommissionersOfficeScreen::refreshCoreLabels()
{
    m_rulesetDropdown->setLabel(m_rulesets.empty() ? "No valid rulesets installed" : m_rulesets[m_selectedRuleset].displayName + " v");
    m_seasonDropdown->setLabel(m_seasons.empty() ? "No valid seasons installed" : m_seasons[m_selectedSeason].displayName + " v");
}

void CommissionersOfficeScreen::openPackageDropdown(bool ruleset)
{
    const auto& packages = ruleset ? m_rulesets : m_seasons;
    if (packages.empty()) return;
    m_openDropdown = ruleset ? 1 : 2;
    m_packageList->clearItems();
    for (const auto& package : packages)
        m_packageList->addItem(package.displayName + " [" + package.key + " @ " + package.version + "]");
    m_packageList->selectIndex(ruleset ? m_selectedRuleset : m_selectedSeason);
    m_packageList->setVisible(true);
    m_packageList->setEnabled(true);
    m_coreRoot.setFocusedChild(*m_packageList);
}

void CommissionersOfficeScreen::closePackageDropdown(bool accept)
{
    const bool ruleset = m_openDropdown == 1;
    if (accept && m_packageList->selectedIndex())
    {
        (ruleset ? m_selectedRuleset : m_selectedSeason) = *m_packageList->selectedIndex();
    }
    m_openDropdown = 0;
    m_packageList->setVisible(false);
    m_packageList->setEnabled(false);
    refreshCoreLabels();
    m_coreRoot.setFocusedChild(ruleset ? *m_rulesetDropdown : *m_seasonDropdown);
}

void CommissionersOfficeScreen::clearCoreMessages()
{
    m_coreMessages.clear();
    m_buildMessages->clearItems();
    m_coreMessageWidth = -1;
}

void CommissionersOfficeScreen::addCoreMessage(const std::string& message)
{
    m_coreMessages.push_back(message);
    m_coreMessageWidth = -1;
}

void CommissionersOfficeScreen::wrapCoreMessages(int width)
{
    width = std::max(1, width - 1); // reserve the list scrollbar column
    if (width == m_coreMessageWidth) return;
    m_coreMessageWidth = width;
    m_buildMessages->clearItems();
    for (const auto& message : m_coreMessages)
    {
        std::u32string line;
        int used = 0;
        for (const auto& cluster : GraphemeSegmentation::segment(UnicodeConversion::utf8ToU32(message)))
        {
            const int size = std::max(0, static_cast<int>(cluster.displayWidth));
            if (used + size > width && !line.empty())
            { m_buildMessages->addItem(UnicodeConversion::u32ToUtf8(line)); line.clear(); used = 0; }
            line += cluster.codePoints;
            used += size;
        }
        m_buildMessages->addItem(UnicodeConversion::u32ToUtf8(line));
    }
}

void CommissionersOfficeScreen::drawCoreBuild(PageComposer& page, const Rect& controls)
{
    // Styles used by this method. Change these theme mappings to restyle it.
    const Style& panelFillStyle = PFF_Colors::FormPanelColor; // Form panel background/fill.
    const Style& panelBorderStyle = PFF_Colors::FormBorderColor; // Form panel border.
    const Style& panelTitleStyle = PFF_Colors::FormPanelHeadingColor; // Form panel heading.
    const Style& labelStyle = PFF_Colors::FormPanelColor; // Field labels, previews, instructions and status messages.

    const bool configure = m_coreStage == CoreStage::Configure;
    page.drawPanel("LeaguePanel", panelFillStyle, panelBorderStyle, ObjectFactory::singleLineBorder());
    page.writePanelTitle("LeaguePanel", configure ? " BUILD CORE DATABASE " : " REVIEW CORE DATABASE BUILD ", panelTitleStyle, 3, 1);
    const auto content = page.resolveRegion("LeagueContent");
    m_coreRoot.setBounds(page.resolveRegion("Body"));
    auto show = [](Widget* widget, bool visible) { widget->setVisible(visible); widget->setEnabled(visible); };
    for (auto* widget : std::vector<Widget*>{m_coreName, m_rulesetDropdown, m_seasonDropdown, m_coreBuildButton, m_reloadPackages})
        show(widget, configure);
    show(m_coreBack, m_coreStage != CoreStage::Building);
    m_coreBack->setLabel(configure ? "Cancel" : "Continue");
    show(m_buildMessages, !configure);
    show(m_packageList, configure && m_openDropdown != 0);
    if (m_coreRoot.focusedChild() && !m_coreRoot.focusedChild()->canReceiveFocus())
        m_coreRoot.setFocusedChild(configure ? static_cast<Widget&>(*m_coreName) : static_cast<Widget&>(*m_buildMessages));
    auto field = [&](Widget* widget, int row, int height) {
        widget->setBounds(Rect{Point{content.position.x, content.position.y + row},
            Size{std::max(0, content.size.width), std::max(0, std::min(height, content.size.height - row))}});
    };
    field(m_coreName, 1, 1);
    field(m_rulesetDropdown, 5, 3);
    field(m_seasonDropdown, 9, 3);
    field(m_buildMessages, 0, content.size.height);
    if (!configure) wrapCoreMessages(content.size.width);
    field(m_packageList, m_openDropdown == 1 ? 5 : 9, 8);
    auto action = [&](Button* button, int row) {
        button->setBounds(Rect{Point{controls.position.x + 1, controls.position.y + row},
            Size{std::max(0, controls.size.width - 2), 3}});
    };
    action(m_coreBuildButton, 5);
    action(m_reloadPackages, 10); action(m_coreBack, m_coreStage == CoreStage::Result ? 5 : 15);
    if (configure)
    {
        auto line = [&](int row, const std::string& text) {
            if (row < content.size.height) page.writeLine("LeagueContent", row,
                TextClip::clipUtf8Text(text, content.size.width), labelStyle);
        };
        line(0, "Core Database Name");
        line(2, "Output: Data/" + m_coreName->text() + ".db");
        line(4, "Ruleset"); line(8, "Season"); line(12, "Optional Add-ons");
        line(13, "No compatible add-ons installed.");
        line(15, "Selected packages: " + std::to_string((m_rulesets.empty() ? 0 : 1) + (m_seasons.empty() ? 0 : 1)));
        line(17, "Enter opens/selects; Esc cancels; Tab changes focus.");
        if (!m_discoveryMessages.empty())
            line(18, "Discovery warnings: " + std::to_string(m_discoveryMessages.size()) + ". See review.");
    }
}

void CommissionersOfficeScreen::beginLeagueSetup()
{
    cancelLoadLeagueSelection();
    m_leagueStage=LeagueStage::Configure;
    m_coreChoicesOpen=false;m_chosenCore=0;m_leagueMessages.clear();m_leagueDetailsKey.clear();
    m_installedCores=LeagueSetup::discover("Data",m_leagueMessages);
    m_coreChoices->clearItems();
    for(const auto& core:m_installedCores) m_coreChoices->addItem(core.path.filename().u8string());
    m_existingSaves->clearItems();
    try
    {
        std::vector<std::string> saves;
        if(std::filesystem::exists(WorldRoot)) for(const auto& entry:std::filesystem::directory_iterator(WorldRoot))
            if(entry.is_directory()) saves.push_back(entry.path().filename().u8string()+
                (std::filesystem::is_regular_file(entry.path()/"World.db")?" (World.db)":" (folder already exists)"));
        std::sort(saves.begin(),saves.end());
        for(const auto& save:saves) m_existingSaves->addItem(save);
        if(saves.empty()) m_existingSaves->addItem("No existing league/save folders.");
    }
    catch(const std::exception& ex){m_existingSaves->addItem(ex.what());}
    m_leagueName->setVisible(true);m_leagueName->setEnabled(true);
    m_leagueRoot.resetFocus();
}

void CommissionersOfficeScreen::closeLeagueSetup()
{
    if(m_leagueCreation.valid()) return;
    m_leagueStage=LeagueStage::Closed;m_coreChoicesOpen=false;
    m_leagueName->clear();m_installedCores.clear();m_leagueMessages.clear();m_leagueDetailsKey.clear();
    m_leagueRoot.clearChildFocus();
    if(m_newLeagueButton) m_root.setFocusedChild(*m_newLeagueButton);
}

void CommissionersOfficeScreen::selectLeagueCore()
{
    if(m_coreChoices->selectedIndex()) m_chosenCore=*m_coreChoices->selectedIndex();
    m_coreChoicesOpen=false;m_coreChoices->setVisible(false);m_coreChoices->setEnabled(false);
    m_leagueDetailsKey.clear();m_leagueRoot.setFocusedChild(*m_coreSelector);
}

void CommissionersOfficeScreen::createSelectedLeague()
{
    if(m_leagueStage!=LeagueStage::Configure || m_leagueCreation.valid()) return;
    try
    {
        const auto error=LeagueSetup::nameError(WorldRoot,m_leagueName->text());
        if(!error.empty()) throw std::runtime_error(error);
        if(m_installedCores.empty()) throw std::runtime_error("No Core database is available.");
        const auto core=m_installedCores.at(m_chosenCore);
        if(!core.error.empty()) throw std::runtime_error(core.error);
        const auto name=m_leagueName->text();
        m_leagueStage=LeagueStage::Creating;
        m_leagueMessages={"Creating league: "+name,"Saving: "+LeagueSetup::destination(WorldRoot,name).u8string(),
            "Preserving and verifying the selected Core snapshot. Please wait..."};
        for(auto* widget:std::vector<Widget*>{m_leagueName,m_coreSelector,m_createLeague,m_cancelLeague,m_coreChoices,m_existingSaves})
        {widget->setVisible(false);widget->setEnabled(false);}
        m_leagueDetails->setVisible(true);m_leagueDetails->setEnabled(true);m_leagueRoot.setFocusedChild(*m_leagueDetails);
        m_leagueCreation=std::async(std::launch::async,[core,name]{return LeagueSetup::create(core,WorldRoot,name);});
    }
    catch(const std::exception& ex)
    {m_leagueMessages={ex.what()};m_leagueStage=LeagueStage::Result;}
    m_leagueDetailsKey.clear();
}

void CommissionersOfficeScreen::drawLeagueSetup(PageComposer& page,const Rect& controls)
{
    // Styles used by this method. Change these theme mappings to restyle it.
    const Style& panelFillStyle = PFF_Colors::FormPanelColor; // Form panel background/fill.
    const Style& panelBorderStyle = PFF_Colors::FormBorderColor; // Form panel border.
    const Style& panelTitleStyle = PFF_Colors::FormPanelHeadingColor; // Form panel heading.
    const Style& labelStyle = PFF_Colors::FormPanelColor; // Field labels, previews, instructions and status messages.

    const bool configure=m_leagueStage==LeagueStage::Configure;
    page.drawPanel("LeaguePanel",panelFillStyle, panelBorderStyle,ObjectFactory::singleLineBorder());
    page.writePanelTitle("LeaguePanel",configure?" NEW LEAGUE ":" LEAGUE CREATION ",panelTitleStyle,3,1);
    const auto content=page.resolveRegion("LeagueContent");
    m_leagueRoot.setBounds(page.resolveRegion("Body"));
    auto show=[](Widget* widget,bool visible){widget->setVisible(visible);widget->setEnabled(visible);};
    for(auto* widget:std::vector<Widget*>{m_leagueName,m_coreSelector,m_createLeague,m_existingSaves}) show(widget,configure);
    show(m_cancelLeague,m_leagueStage!=LeagueStage::Creating);
    m_cancelLeague->setLabel(configure?"Cancel":"Continue");
    show(m_leagueDetails,true);show(m_coreChoices,configure&&m_coreChoicesOpen);
    auto field=[&](Widget* widget,int row,int height) {
        widget->setBounds(Rect{Point{content.position.x,content.position.y+row},
            Size{std::max(0,content.size.width),std::max(0,std::min(height,content.size.height-row))}});
    };
    const int saveRows=std::min(5,std::max(1,content.size.height/5));
    const int saveStart=std::max(12,content.size.height-saveRows);
    field(m_leagueName,1,1);field(m_coreSelector,5,3);
    field(m_leagueDetails,configure?9:0,configure?std::max(1,saveStart-10):content.size.height);
    field(m_existingSaves,saveStart,saveRows);field(m_coreChoices,5,std::max(1,std::min(10,content.size.height-5)));
    auto action=[&](Button* button,int row){button->setBounds(Rect{Point{controls.position.x+1,controls.position.y+row},Size{std::max(0,controls.size.width-2),3}});};
    action(m_createLeague,5);action(m_cancelLeague,configure?10:5);
    for (std::size_t index = 0; index < m_addonChecks.size(); ++index)
    {
        const int row = 15 + static_cast<int>(index);
        auto& checkbox = *m_addonChecks[index];
        checkbox.setVisible(configure && row < controls.size.height);
        checkbox.setBounds(Rect{Point{controls.position.x + 1, controls.position.y + row},
                                Size{std::max(0, controls.size.width - 2), 1}});
    }
    std::vector<std::string> details=m_leagueMessages;
    const auto key=std::to_string(static_cast<int>(m_leagueStage))+":"+std::to_string(m_chosenCore)+":"+std::to_string(content.size.width);
    if(configure)
    {
        const bool available=!m_installedCores.empty();
        m_coreSelector->setLabel(available?m_installedCores.at(m_chosenCore).path.filename().u8string()+" v":"No Core databases found in Data");
        const auto error=LeagueSetup::nameError(WorldRoot,m_leagueName->text());
        m_createLeague->setEnabled(error.empty()&&available&&m_installedCores.at(m_chosenCore).error.empty());
        auto line=[&](int row,const std::string& text){if(row<content.size.height) page.writeLine("LeagueContent",row,TextClip::clipUtf8Text(text,content.size.width),labelStyle);};
        line(0,"League Name");
        line(2,"Save: "+LeagueSetup::destination(WorldRoot,m_leagueName->text()).u8string());
        line(3,error.empty()?"Save name is available.":error);
        line(4,"Core Database");line(8,"Core Details (Up and Down Arrows Scroll)");line(saveStart-1,"Existing league/save folders");
        if(available)
        {
            details=m_installedCores.at(m_chosenCore).details;
            if(!m_installedCores.at(m_chosenCore).error.empty()) details.push_back(m_installedCores.at(m_chosenCore).error);
            details.insert(details.end(),m_leagueMessages.begin(),m_leagueMessages.end());
        }
        else details.insert(details.begin(),"Install a Core database in Data, then reopen New League.");
    }
    if(key!=m_leagueDetailsKey)
    {
        m_leagueDetailsKey=key;m_leagueDetails->clearItems();
        const int width=std::max(1,content.size.width-1);
        for(const auto& text:details)
        {
            std::u32string line;int used=0;
            for(const auto& cluster:GraphemeSegmentation::segment(UnicodeConversion::utf8ToU32(text)))
            {
                const int size=std::max(0,static_cast<int>(cluster.displayWidth));
                if(used+size>width&&!line.empty()){m_leagueDetails->addItem(UnicodeConversion::u32ToUtf8(line));line.clear();used=0;}
                line+=cluster.codePoints;used+=size;
            }
            m_leagueDetails->addItem(UnicodeConversion::u32ToUtf8(line));
        }
    }
    if(m_leagueRoot.focusedChild() && !m_leagueRoot.focusedChild()->canReceiveFocus())
        m_leagueRoot.setFocusedChild(configure?static_cast<Widget&>(*m_leagueName):m_leagueStage==LeagueStage::Result?static_cast<Widget&>(*m_cancelLeague):static_cast<Widget&>(*m_leagueDetails));
}

void CommissionersOfficeScreen::beginAddons()
{
    if (!m_loadedWorldContextProvider || !m_loadedWorldContextProvider().isLoaded())
        return;
    m_addonsActive = true;
    m_addonsRoot.resetFocus();
    m_addonsRoot.setFocusedChild(*m_addonsClose);
}

void CommissionersOfficeScreen::openGroundRules()
{
    refreshEditState(true);
    if (!m_editReady || !m_editContext.hasActiveCompetitionSeason())
        return;
    const auto options = loadWorldLeagueOptions(m_editActor, m_editContext.activeCompetitionSeasonId);
    if (!options.success)
    {
        m_editMessage = options.message;
        return;
    }
    m_groundRulesContext = m_editContext;
    m_groundRulesRevision = options.value.revision;
    m_groundRules = GroundRules::resolve(options.value);
    m_groundRulesError.clear();
    bool supported = false;
    for (std::size_t i = 0; i < m_groundRuleChecks.size(); ++i)
    {
        auto* check = m_groundRuleChecks[i];
        check->setVisible(m_groundRules.supported[i]);
        check->setEnabled(m_groundRules.supported[i]);
        check->setChecked(m_groundRules.enabled[i]);
        supported = supported || m_groundRules.supported[i];
    }
    m_playClockSeconds->setVisible(m_groundRules.supported[GroundRules::PlayClock]);
    m_playClockSeconds->setEnabled(m_groundRules.supported[GroundRules::PlayClock]);
    m_playClockSeconds->setText(std::to_string(m_groundRules.clockSeconds));
    m_groundRulesToggle->setVisible(supported);
    m_groundRulesSave->setVisible(supported);
    refreshGroundRulesToggle();
    m_groundRulesRoot.resetFocus();
    m_groundRulesRoot.setFocusedChild(*m_groundRulesCancel);
    m_groundRulesActive = true;
}

void CommissionersOfficeScreen::refreshGroundRulesToggle()
{
    const bool allChecked = std::none_of(m_groundRuleChecks.begin(), m_groundRuleChecks.end(),
        [](const auto* check) { return check->isVisible() && !check->checked(); });
    m_groundRulesToggle->setLabel(allChecked ? "Uncheck All" : "Check All");
}

void CommissionersOfficeScreen::saveGroundRules()
{
    const auto context = m_loadedWorldContextProvider();
    if (context.databasePath != m_groundRulesContext.databasePath || context.worldKey != m_groundRulesContext.worldKey ||
        context.activeCompetitionSeasonId != m_groundRulesContext.activeCompetitionSeasonId)
    {
        m_groundRulesError = "League changed. Reopen Ground Rules.";
        return;
    }
    auto draft = m_groundRules;
    for (std::size_t i = 0; i < m_groundRuleChecks.size(); ++i)
        draft.enabled[i] = m_groundRuleChecks[i]->checked();
    if (m_groundRules.supported[GroundRules::PlayClock])
    {
        const auto text = m_playClockSeconds->text();
        const auto end = text.data() + text.size();
        int seconds = 0;
        const auto parsed = std::from_chars(text.data(), end, seconds);
        if (parsed.ec != std::errc{} || parsed.ptr != end || seconds <= 0)
        {
            m_groundRulesError = "Enter a positive whole number of seconds.";
            m_groundRulesRoot.setFocusedChild(*m_playClockSeconds);
            return;
        }
        draft.clockSeconds = seconds;
    }
    const auto result = saveWorldLeagueOptions(m_editActor, context.activeCompetitionSeasonId,
        GroundRules::values(draft), m_groundRulesRevision);
    if (!result.success)
    {
        m_groundRulesError = result.message;
        return;
    }
    if (m_commitCallback) m_commitCallback(result.value);
    m_groundRulesActive = false;
}

void CommissionersOfficeScreen::drawGroundRules(PageComposer& page)
{
    const auto pane = page.resolveRegion("ContentPane");
    const int width = std::min(54, pane.size.width);
    const int rows = static_cast<int>(std::count(m_groundRules.supported.begin(), m_groundRules.supported.end(), true));
    const int height = rows + 10;
    const Rect popup{Point{pane.position.x + std::max(0, (pane.size.width - width) / 2),
                           pane.position.y + std::max(0, (pane.size.height - height) / 2)}, Size{width, height}};
    page.createRegion("GroundRulesPopup", popup);
    page.drawPanel("GroundRulesPopup", PFF_Colors::FormPanelColor, PFF_Colors::MainBorderColor,
                   ObjectFactory::singleLineBorder());
    page.writePanelTitle("GroundRulesPopup", " GROUND RULES ", PFF_Colors::FormPanelHeadingColor, 2, 1);
    m_groundRulesRoot.setBounds(popup);
    const auto box = [&](int x, int y, int w, int h = 1) {
        return Rect{Point{popup.position.x + x, popup.position.y + y}, Size{w, h}};
    };
    int row = 2;
    for (auto* check : m_groundRuleChecks)
        if (check->isVisible()) check->setBounds(box(2, row++, 22));
    if (m_playClockSeconds->isVisible())
    {
        page.writeLine(box(2, row, 25), 0, "Play clock (seconds)", PFF_Colors::FormPanelHeadingColor);
        m_playClockSeconds->setBounds(box(26, row, 12));
    }
    if (!rows)
        page.writeLine(box(2, 2, width - 4), 0, "This ruleset has no supported Ground Rules.", PFF_Colors::FormPanelHeadingColor);
    const auto errors = wrapText(m_groundRulesError, std::max(1, width - 4));
    for (std::size_t i = 0; i < errors.size() && i < 2; ++i)
        page.writeLine(box(2, height - 6 + static_cast<int>(i), width - 4), 0, errors[i], PFF_Colors::FormPanelHeadingColor);
    m_groundRulesToggle->setBounds(box(2, height - 4, 14, 3));
    m_groundRulesSave->setBounds(box(17, height - 4, 16, 3));
    m_groundRulesCancel->setBounds(box(34, height - 4, 12, 3));
}

void CommissionersOfficeScreen::closeAddons()
{
    if (!m_addonsActive)
        return;
    m_addonsActive = false;
    m_closeAddonsRequested = false;
    m_addonsRoot.resetFocus();
    m_root.clearPointerState();
    m_root.setFocusedChild(*m_addonsButton);
}

bool CommissionersOfficeScreen::handleAddonsEvent(const Input::Event& event)
{
    if (const auto* command = event.asCommand())
    {
        const auto code = command->command.code;
        if (code == Input::CommandCode::Cancel || code == Input::CommandCode::Back || code == Input::CommandCode::Close)
        {
            closeAddons();
            return true;
        }
    }
    m_addonsRoot.handleEvent(event);
    if (m_closeAddonsRequested)
        closeAddons();
    return true;
}

void CommissionersOfficeScreen::drawAddons(PageComposer& page)
{
    const auto pane = page.resolveRegion("ContentPane");
    if (pane.size.width < 6 || pane.size.height < 6)
    {
        m_addonsClose->setBounds(Rect{});
        return;
    }
    constexpr std::string_view message = "No add-ons were incorporated when this league was built.";
    const int width = std::min(62, pane.size.width);
    const auto lines = wrapText(message, width - 4);
    const int height = std::min(pane.size.height, static_cast<int>(lines.size()) + 6);
    const Rect popup{Point{pane.position.x + (pane.size.width - width) / 2,
                           pane.position.y + (pane.size.height - height) / 2}, Size{width, height}};
    page.createRegion("AddonsPopup", popup);
    page.drawPanel("AddonsPopup", PFF_Colors::FormPanelColor, PFF_Colors::MainBorderColor,
                   ObjectFactory::singleLineBorder());
    page.writePanelTitle("AddonsPopup", " ADDONS ", PFF_Colors::FormPanelHeadingColor, 2, 1);
    const Rect content{Point{popup.position.x + 2, popup.position.y + 2}, Size{width - 4, height - 6}};
    for (int row = 0; row < content.size.height; ++row)
        page.writeLine(content, row, lines[row], PFF_Colors::FormPanelHeadingColor);
    m_addonsRoot.setBounds(popup);
    const int buttonWidth = std::min(16, width - 2);
    m_addonsClose->setBounds(Rect{Point{popup.position.x + (width - buttonWidth) / 2,
                                      popup.position.y + height - 4}, Size{buttonWidth, 3}});
}


void CommissionersOfficeScreen::beginDeleteLeague()
{
    if (m_loadedWorldContextProvider && m_loadedWorldContextProvider().isLoaded())
        return;
    m_deleteLeagueActive = true;
    refreshDeleteList();
}

void CommissionersOfficeScreen::cancelDeleteLeague()
{
    if (!m_deleteLeagueActive)
        return;
    m_deleteLeagueActive = false;
    m_deleteConfirmed = m_deleteCanceled = false;
    m_leagueToDelete.reset();
    m_deleteRoot.resetFocus();
    m_deleteList->clearChildren();
    m_availableLeagues.clear();
    m_root.clearPointerState();
    m_root.setFocusedChild(*m_deleteLeagueButton);
}

void CommissionersOfficeScreen::refreshDeleteList()
{
    refreshLeagueSaveList();
    m_deleteMessage = m_loadLeagueMessage;
    m_leagueToDelete.reset();
    m_confirmDelete->setEnabled(false);
    m_deleteRoot.resetFocus();
    m_deleteList->clearChildren();
    m_deleteList->viewport().scrollTo(0, 0);
    for (std::size_t index = 0; index < m_availableLeagues.size(); ++index)
    {
        auto button = std::make_unique<Button>(m_availableLeagues[index].displayName);
        button->setHoverStyle(PFF_Colors::ButtonHoverColor);
        button->setFocusedStyle(PFF_Colors::ButtonHoverColor);
        button->setPressedStyle(PFF_Colors::ButtonPressedColor);
        button->setNormalStyle(PFF_Colors::ButtonNormalColor);
        button->setActivationCallback([this, index](const ButtonActivationResult& result) {
            if (result.activated)
                selectLeagueToDelete(index);
        });
        m_deleteList->addChild(std::move(button));
    }
}

void CommissionersOfficeScreen::selectLeagueToDelete(std::size_t index)
{
    if (m_leagueToDelete)
        static_cast<Button*>(m_deleteList->childAt(*m_leagueToDelete))->setNormalStyle(PFF_Colors::ButtonNormalColor);
    m_leagueToDelete = index;
    static_cast<Button*>(m_deleteList->childAt(index))->setNormalStyle(PFF_Colors::ButtonHoverColor);
    m_confirmDelete->setEnabled(true);
    m_deleteMessage.clear();
}

void CommissionersOfficeScreen::confirmDeleteLeague()
{
    if (!m_leagueToDelete || (m_loadedWorldContextProvider && m_loadedWorldContextProvider().isLoaded()))
        return;
    const auto save = m_availableLeagues[*m_leagueToDelete];
    try
    {
        const auto folder = checkedLeagueFolder(save.databasePath);
        if (std::filesystem::remove_all(folder) == 0)
            throw std::runtime_error("The selected league folder no longer exists.");
        refreshDeleteList();
        m_deleteMessage = "Deleted " + save.displayName + ".";
    }
    catch (const std::exception& error)
    {
        m_deleteMessage = "Unable to delete " + save.displayName + ": " + error.what();
    }
}

bool CommissionersOfficeScreen::handleDeleteEvent(const Input::Event& event)
{
    const int count = static_cast<int>(m_deleteList->childCount());
    if (const auto* command = event.asCommand())
    {
        const auto code = command->command.code;
        if (code == Input::CommandCode::Cancel || code == Input::CommandCode::Back ||
            code == Input::CommandCode::Close)
        {
            cancelDeleteLeague();
            return true;
        }
        if (code == Input::CommandCode::NextFocus || code == Input::CommandCode::PreviousFocus)
        {
            const bool forward = code == Input::CommandCode::NextFocus;
            const bool canConfirm = m_confirmDelete->isEnabled();
            const int slots = count + 1 + (canConfirm ? 1 : 0);
            int focused = forward ? -1 : 0;
            const auto* rootFocus = m_deleteRoot.focusedChild();
            if (rootFocus == m_cancelDelete)
                focused = slots - 1;
            else if (rootFocus == m_confirmDelete)
                focused = count;
            else if (rootFocus == m_deleteList)
            {
                for (int i = 0; i < count; ++i)
                    if (m_deleteList->childAt(i) == m_deleteList->focusedChild())
                    {
                        focused = i;
                        break;
                    }
            }
            const int next = (focused + slots + (forward ? 1 : -1)) % slots;
            m_deleteRoot.clearPointerState();
            if (next < count)
            {
                m_deleteRoot.setFocusedChild(*m_deleteList);
                auto& entry = *m_deleteList->childAt(next);
                m_deleteList->setFocusedChild(entry);
                auto& viewport = m_deleteList->viewport();
                const int first = entry.bounds().position.y;
                const int last = first + entry.bounds().size.height;
                const int earliest = std::clamp(last - viewport.viewSize().height, 0, first);
                viewport.scrollTo(0, std::clamp(viewport.scrollY(), earliest, first));
            }
            else
                m_deleteRoot.setFocusedChild(*(canConfirm && next == count ? m_confirmDelete : m_cancelDelete));
            return true;
        }
    }
    if (const auto* key = event.asKey())
    {
        if (key->pressed && !key->modifiers.ctrl && !key->modifiers.alt &&
            (key->code == Input::KeyCode::Space || key->character == U' ') &&
            m_deleteRoot.focusedChild() == m_deleteList && m_deleteList->focusedChild())
        {
            m_deleteList->focusedChild()->handleEvent(Input::Event::command(Input::Command{Input::CommandCode::Confirm}));
            return true;
        }
    }

    m_deleteRoot.handleEvent(event);
    if (std::exchange(m_deleteCanceled, false))
        cancelDeleteLeague();
    else if (std::exchange(m_deleteConfirmed, false))
        confirmDeleteLeague();
    return true;
}

void CommissionersOfficeScreen::drawDeleteLeague(PageComposer& page)
{
    page.drawPanel("LeaguePanel", PFF_Colors::FormPanelColor, PFF_Colors::MainBorderColor,
                   ObjectFactory::singleLineBorder());
    page.writePanelTitle("LeaguePanel", " DELETE LEAGUE ", PFF_Colors::FormPanelHeadingColor, 3, 1);
    const auto content = page.resolveRegion("LeagueContent");
    const auto line = [&](int row, std::string_view text) {
        page.writeLine(content, row, TextClip::clipUtf8Text(text, content.size.width), PFF_Colors::FormPanelHeadingColor);
    };
    line(0, "Choose a saved league. Confirm Delete permanently removes its folder.");
    const int listHeight = std::max(0, content.size.height - 7);
    const int entryWidth = std::max(0, content.size.width - 1);
    m_deleteRoot.setBounds(page.resolveRegion("Body"));
    m_deleteList->setBounds(Rect{Point{content.position.x, content.position.y + 1},
                                 Size{content.size.width, listHeight}});
    for (std::size_t index = 0; index < m_deleteList->childCount(); ++index)
        m_deleteList->childAt(index)->setBounds(Rect{Point{0, static_cast<int>(index) * 3}, Size{entryWidth, 3}});
    m_deleteList->setContentSize(entryWidth, static_cast<int>(m_deleteList->childCount()) * 3);
    line(listHeight + 1, m_leagueToDelete ? "Selected: " + m_availableLeagues[*m_leagueToDelete].displayName : "Select a league to enable Confirm Delete.");
    line(listHeight + 2, m_deleteMessage);
    const int y = content.position.y + listHeight + 4;
    const int buttonWidth = std::min(24, std::max(0, (content.size.width - 2) / 2));
    m_confirmDelete->setBounds(Rect{Point{content.position.x, y}, Size{buttonWidth, 3}});
    m_cancelDelete->setBounds(Rect{Point{content.position.x + buttonWidth + 2, y}, Size{buttonWidth, 3}});
}


void CommissionersOfficeScreen::beginLoadLeagueSelection()
{
    refreshLeagueSaveList();
    m_loadLeagueSelectionActive=true;
    m_savedLeaguesOpen=false;m_selectedLeague=0;
    m_leagueListBox->setVisible(false);m_leagueListBox->setEnabled(false);
    m_loadRoot.resetFocus();
}

void CommissionersOfficeScreen::openSavedLeagueDropdown()
{
    if(m_availableLeagues.empty()) return;
    m_savedLeaguesOpen=true;
    m_leagueListBox->setVisible(true);m_leagueListBox->setEnabled(true);
    m_leagueListBox->selectIndex(m_selectedLeague);
    // The popup owns focus until the pointer moves to a new target.
    m_loadRoot.clearPointerState();
    m_loadRoot.setFocusedChild(*m_leagueListBox);
}

void CommissionersOfficeScreen::closeSavedLeagueDropdown(bool accept)
{
    if(accept)
        if(const auto selected=m_leagueListBox->selectedIndex();selected && *selected<m_availableLeagues.size())
            m_selectedLeague=*selected;
    m_savedLeaguesOpen=false;
    m_leagueListBox->setVisible(false);m_leagueListBox->setEnabled(false);
    m_loadRoot.setFocusedChild(*m_savedLeagueSelector);
}

void CommissionersOfficeScreen::cancelLoadLeagueSelection()
{
    m_loadLeagueSelectionActive=false;m_savedLeaguesOpen=false;m_selectedLeague=0;
    m_loadLeagueMessage.clear();m_availableLeagues.clear();
    m_loadRoot.resetFocus();
    if(m_leagueListBox)
    {
        m_leagueListBox->clearItems();
        m_leagueListBox->setVisible(false);m_leagueListBox->setEnabled(false);
    }
    if(m_loadLeagueButton) m_root.setFocusedChild(*m_loadLeagueButton);
}

void CommissionersOfficeScreen::refreshLeagueSaveList()
{
    m_availableLeagues.clear();
    m_loadLeagueMessage.clear();

    if (m_leagueListBox)
    {
        m_leagueListBox->clearItems();
    }

    std::error_code ec;

    if (!std::filesystem::exists(WorldRoot, ec))
    {
        if (ec)
        {
            m_loadLeagueMessage =
                "Unable to inspect Data/World: " + ec.message();
        }
        else
        {
            m_loadLeagueMessage =
                "No saved leagues were found in Data/World.";
        }

        return;
    }

    std::filesystem::directory_iterator iterator(WorldRoot, ec);
    const std::filesystem::directory_iterator end;

    if (ec)
    {
        m_loadLeagueMessage =
            "Unable to inspect Data/World: " + ec.message();
        return;
    }

    for (; iterator != end; iterator.increment(ec))
    {
        if (ec)
        {
            m_loadLeagueMessage =
                "Unable to finish scanning Data/World: " + ec.message();
            break;
        }

        const std::filesystem::directory_entry& entry = *iterator;

        if (!entry.is_directory(ec))
        {
            ec.clear();
            continue;
        }

        const std::filesystem::path worldDatabase =
            entry.path() / "World.db";

        const bool hasWorldDatabase =
            std::filesystem::is_regular_file(worldDatabase, ec);

        if (ec)
        {
            ec.clear();
            continue;
        }

        if (!hasWorldDatabase)
        {
            continue;
        }

        LeagueSaveEntry save;
        save.displayName = entry.path().filename().string();
        std::replace(
            save.displayName.begin(),
            save.displayName.end(),
            '_',
            ' ');

        save.databasePath = worldDatabase;
        m_availableLeagues.push_back(std::move(save));
    }

    std::sort(
        m_availableLeagues.begin(),
        m_availableLeagues.end(),
        [](const LeagueSaveEntry& left, const LeagueSaveEntry& right)
        {
            return left.displayName < right.displayName;
        });

    if (m_leagueListBox)
    {
        for (const LeagueSaveEntry& save : m_availableLeagues)
        {
            m_leagueListBox->addItem(save.displayName);
        }
    }

    if (m_availableLeagues.empty() && m_loadLeagueMessage.empty())
    {
        m_loadLeagueMessage =
            "No World.db save files were found in Data/World.";
    }
}

bool CommissionersOfficeScreen::loadSelectedLeague()
{
    if (!m_leagueListBox || !m_loadLeagueCallback)
    {
        m_loadLeagueMessage =
            "Load League is not connected to the application.";
        return false;
    }

    if(m_selectedLeague>=m_availableLeagues.size())
    {
        m_loadLeagueMessage="Select a league first.";
        return false;
    }
    const LeagueSaveEntry& selected=m_availableLeagues[m_selectedLeague];

    if (!m_loadLeagueCallback(selected.databasePath))
    {
        m_loadLeagueMessage =
            "The selected World.db could not be validated or loaded.";
        return false;
    }

    cancelLoadLeagueSelection();
    return true;
}

void CommissionersOfficeScreen::drawLeagueDetails(
    Composition::PageComposer& page,
    const LoadedWorldContext& worldContext) const
{
    // Styles used by this method. Change these theme mappings to restyle it.
    const Style& panelFillStyle = PFF_Colors::MainBackgroundColor; // Loaded-league panel fill.
    const Style& panelBorderStyle = PFF_Colors::MainBorderColor; // Loaded-league panel border.
    const Style& panelTitleStyle = PFF_Colors::TextHighlightColor; // Loaded-league heading.
    const Style& detailLabelStyle = PFF_Colors::TextHighlightColor; // Detail names (League, Status, etc.).
    const Style& detailValueStyle = PFF_Colors::TextHighlightColor; // Recorded values beside detail names.

    page.drawPanel(
        "LeaguePanel",
        panelFillStyle,
        panelBorderStyle,
        ObjectFactory::singleLineBorder());

    page.writePanelTitle(
        "LeaguePanel",
        " LEAGUE DETAILS ",
        panelTitleStyle,
        3,
        1);

    const Rect details =
        page.resolveRegion("LeagueContent");

    auto drawDetail =
        [&](int row,
            std::string_view label,
            const std::string& value)
        {
            const int valueWidth = std::max(
                0,
                details.size.width -
                static_cast<int>(label.size()) -
                3);

            const std::string clippedValue =
                TextClip::clipUtf8Text(value, valueWidth);

            page.drawNameValue(
                details,
                row,
                label,
                clippedValue,
                detailLabelStyle,
                detailValueStyle,
                3);
        };

    drawDetail(0, "League", worldContext.worldName);
    drawDetail(2, "Status", "Loaded");
    drawDetail(4, "World ID", std::to_string(worldContext.worldId));
    drawDetail(6, "World Key", worldContext.worldKey);
    drawDetail(
        8,
        "Competition Season ID",
        std::to_string(worldContext.activeCompetitionSeasonId));
    drawDetail(10, "World Time (UTC)", worldContext.currentTimeUtc);
    drawDetail(
        12,
        "World Database",
        worldContext.databasePath.u8string());
}

void CommissionersOfficeScreen::drawLoadLeagueSelection(
    Composition::PageComposer& page, const Rect& controls)
{
    // Styles used by this method. Change these theme mappings to restyle it.
    const Style& panelFillStyle = PFF_Colors::FormPanelColor; // Load League chooser panel fill.
    const Style& panelBorderStyle = PFF_Colors::FormBorderColor; // Chooser panel border.
    const Style& panelTitleStyle = PFF_Colors::FormPanelHeadingColor; // Load League heading.
    const Style& instructionStyle = PFF_Colors::FormPanelHeadingColor; // Selection/keyboard instructions.
    const Style& messageStyle = PFF_Colors::InputFieldNormalColor; // Empty-list and load-error messages.

    page.drawPanel(
        "LeaguePanel",
        panelFillStyle,
        panelBorderStyle,
        ObjectFactory::singleLineBorder());

    page.writePanelTitle(
        "LeaguePanel",
        " LOAD LEAGUE ",
        panelTitleStyle,
        3,
        1);

    const Rect content=page.resolveRegion("LeagueContent");
    m_loadRoot.setBounds(page.resolveRegion("Body"));
    const bool available=m_selectedLeague<m_availableLeagues.size();
    m_savedLeagueSelector->setLabel(available?m_availableLeagues[m_selectedLeague].displayName+" v":"No saved leagues found");
    m_savedLeagueSelector->setEnabled(available);m_openLeague->setEnabled(available);
    m_savedLeagueSelector->setBounds(Rect{Point{content.position.x,content.position.y+1},Size{std::max(0,content.size.width),3}});
    m_leagueListBox->setBounds(Rect{Point{content.position.x,content.position.y+4},
        Size{std::max(0,content.size.width),std::max(0,std::min(8,content.size.height-5))}});
    m_leagueListBox->setVisible(m_savedLeaguesOpen);m_leagueListBox->setEnabled(m_savedLeaguesOpen);
    auto action=[&](Button* button,int row) {
        button->setBounds(Rect{Point{controls.position.x+1,controls.position.y+row},Size{std::max(0,controls.size.width-2),3}});
    };
    action(m_openLeague,5);action(m_cancelLoadLeague,10);
    auto line=[&](int row,const std::string& text,const Style& style) {
        if(row>=0 && row<content.size.height)
            page.writeLine(content,row,TextClip::clipUtf8Text(text,content.size.width),style);
    };
    line(0,"Available Leagues",instructionStyle);
    if(!m_savedLeaguesOpen)
    {
        if(available) line(6,"World: "+m_availableLeagues[m_selectedLeague].databasePath.u8string(),messageStyle);
        line(8,"Choose a league from the dropdown, then select Open League. Esc cancels.",instructionStyle);
        if(!m_loadLeagueMessage.empty()) line(10,m_loadLeagueMessage,messageStyle);
    }
}

void CommissionersOfficeScreen::refreshEditState(bool force)
{
    const auto c=m_loadedWorldContextProvider?m_loadedWorldContextProvider():LoadedWorldContext{};
    if(!force&&m_editChecked&&c.loaded==m_editContext.loaded&&c.databasePath==m_editContext.databasePath&&c.worldKey==m_editContext.worldKey&&c.dataRevision==m_editContext.dataRevision)return;
    m_editChecked=true;m_editContext=c;m_editReady=false;m_editMessage.clear();m_editState={};
    if(c.isLoaded()){const auto actor=openLocalWorldActor(c.databasePath);if(!actor.success)m_editMessage=actor.message;else{m_editActor=actor.value;auto state=loadWorldEditState(m_editActor);if(state.success){m_editState=state.value;m_editReady=true;}else m_editMessage=state.message;}}
    const bool loaded = c.isLoaded();
    for (auto* button : {m_buildCoreDbButton, m_newLeagueButton, m_loadLeagueButton})
    {
        button->setVisible(!loaded);
        button->setEnabled(!loaded);
    }
    m_deleteLeagueButton->setVisible(!loaded);
    m_deleteLeagueButton->setEnabled(!loaded);
    m_closeLeagueButton->setVisible(loaded);
    m_closeLeagueButton->setEnabled(loaded);
    m_addonsButton->setVisible(loaded);
    m_addonsButton->setEnabled(loaded);
    m_groundRulesButton->setVisible(loaded);
    m_groundRulesButton->setEnabled(loaded);
    m_godMode->setVisible(c.isLoaded());m_godMode->setEnabled(m_editReady&&m_editState.canEnableGodMode&&static_cast<bool>(m_commitCallback));m_godMode->setChecked(m_editState.godMode);
}
void CommissionersOfficeScreen::toggleGodMode(bool enabled)
{
    const auto c=m_loadedWorldContextProvider?m_loadedWorldContextProvider():LoadedWorldContext{};
    if(!m_editReady||!c.isLoaded()||c.databasePath!=m_editContext.databasePath||c.worldKey!=m_editContext.worldKey){refreshEditState(true);return;}
    const auto result=setWorldEditMode(m_editActor,enabled,m_editState.revision);
    if(!result.success){m_godMode->setChecked(m_editState.godMode);m_editMessage=result.message;return;}
    if(m_commitCallback)m_commitCallback(result.value);refreshEditState(true);
}
