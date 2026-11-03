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
#include <filesystem>
#include <optional>
#include <utility>

namespace
{
    using Composition::PageComposer;

    constexpr std::string_view BuildCoreDbCommand = "action:pff:build_core_db";
    constexpr std::string_view NewLeagueCommand = "action:pff:new_league";
    constexpr std::string_view LoadLeagueCommand = "action:pff:load_league";

    const std::filesystem::path WorldRoot =
        std::filesystem::path("Data") / "World";
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
}

bool CommissionersOfficeScreen::handleEvent(const Input::Event& event)
{
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
    auto god=std::make_unique<FormCheckBox>("God mode");
    god->setNormalStyle(checkboxNormal);god->setFocusedStyle(checkboxActive);god->setHoverStyle(checkboxActive);god->setPressedStyle(checkboxPressed);god->setDisabledStyle(checkboxDisabled);
    god->setChangeCallback([this](bool enabled){m_requestedGod=enabled;});m_godMode=god.get();m_root.addChild(std::move(god));
    m_root.addChild(std::move(mainButton));
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

    if (m_buildCoreDbButton)
    {
        m_buildCoreDbButton->setBounds(Rect{
            Point{ controlX, firstButtonY },
            controlButtonSize
            });

        m_buildCoreDbButton->setNormalStyle(ButtonNormalStyle);
        m_buildCoreDbButton->setHoverStyle(ButtonHoverStyle);
        m_buildCoreDbButton->setPressedStyle(ButtonPressedStyle);
        m_buildCoreDbButton->setFocusedStyle(ButtonFocusedStyle);
    }

    if (m_newLeagueButton)
    {
        m_newLeagueButton->setBounds(Rect{
            Point{ controlX, firstButtonY + buttonSpacing },
            controlButtonSize
            });

        m_newLeagueButton->setNormalStyle(ButtonNormalStyle);
        m_newLeagueButton->setHoverStyle(ButtonHoverStyle);
        m_newLeagueButton->setPressedStyle(ButtonPressedStyle);
        m_newLeagueButton->setFocusedStyle(ButtonFocusedStyle);
    }

    if (m_loadLeagueButton)
    {
        m_loadLeagueButton->setBounds(Rect{
            Point{ controlX, firstButtonY + (buttonSpacing * 2) },
            controlButtonSize
            });

        m_loadLeagueButton->setNormalStyle(ButtonNormalStyle);
        m_loadLeagueButton->setHoverStyle(ButtonHoverStyle);
        m_loadLeagueButton->setPressedStyle(ButtonPressedStyle);
        m_loadLeagueButton->setFocusedStyle(ButtonFocusedStyle);
    }

    if(m_godMode)m_godMode->setBounds(Rect{Point{controlX,firstButtonY+buttonSpacing*3},controlButtonSize});

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
