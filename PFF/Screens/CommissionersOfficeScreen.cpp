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
    std::function<bool(const std::filesystem::path&)> loadLeagueCallback)
    : m_assetLibrary(assetLibrary)
    , m_commandDispatcher(std::move(commandDispatcher))
    , m_loadedWorldContextProvider(std::move(loadedWorldContextProvider))
    , m_loadLeagueCallback(std::move(loadLeagueCallback))
{
    buildWidgets();
}

CommissionersOfficeScreen::~CommissionersOfficeScreen()
{
}

void CommissionersOfficeScreen::onEnter()
{
    m_fontResult = m_assetLibrary.loadBannerFontAsset(m_TitleBannerFontKey);

    if (!m_fontResult.success || !m_fontResult.hasFont())
    {
        return;
    }

    m_CommissionersOfficeTitleObject = BannerFactory::makeBanner(
        *m_fontResult.asset.font,
        "COMMISSIONER'S OFFICE",
        PFF_Colors::ProFootballTitleColor);
}

void CommissionersOfficeScreen::onExit()
{
    m_CommissionersOfficeTitleObject.clear();
    cancelLoadLeagueSelection();
}

bool CommissionersOfficeScreen::handleEvent(const Input::Event& event)
{
    // While the league list owns focus, Enter/Space means "load the selected
    // league" rather than merely changing ListBox selection.
    if (m_loadLeagueSelectionActive)
    {
        if (const Input::CommandEvent* commandEvent = event.asCommand())
        {
            if (commandEvent->command.code == Input::CommandCode::Confirm &&
                m_root.focusedChild() == m_leagueListBox)
            {
                return loadSelectedLeague();
            }

            if (commandEvent->command.code == Input::CommandCode::Cancel ||
                commandEvent->command.code == Input::CommandCode::Close)
            {
                cancelLoadLeagueSelection();
                return true;
            }
        }
    }

    return m_root.handleEvent(event);
}

void CommissionersOfficeScreen::update(const Animation::TickEvent& event)
{
    m_root.update(event);
}

void CommissionersOfficeScreen::draw(Surface& surface)
{
    ScreenBuffer& buffer = surface.buffer();
    buffer.clear(PFF_Colors::MainBackgroundColor);

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
        PFF_Colors::HeaderOuterColor,
        PFF_Colors::HeaderOuterColor,
        ObjectFactory::doubleLineBorder());

    buffer.fillRect(
        page.resolveRegion("InnerPanel"),
        U' ',
        PFF_Colors::HeaderInnerColor);

    page.writeObjectInRegion(
        m_CommissionersOfficeTitleObject,
        "TitleArea",
        Composition::Align::center());

    if (m_loadLeagueSelectionActive)
    {
        drawLoadLeagueSelection(page);
        positionLoadLeagueList(page.resolveRegion("LeagueContent"));
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

    m_root.draw(surface);
}

void CommissionersOfficeScreen::buildWidgets()
{
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

    auto leagueListBox = std::make_unique<ListBox>();
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
    m_root.addChild(std::move(mainButton));
    m_root.addChild(std::move(leagueListBox));
}

void CommissionersOfficeScreen::positionButtons(
    const Rect& controlPane,
    const Rect& footer)
{
    const Style& normal = PFF_Colors::ButtonNormalColor;
    const Style& hover = PFF_Colors::ButtonHoverColor;
    const Style& pressed = PFF_Colors::ButtonPressedColor;

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

        m_buildCoreDbButton->setNormalStyle(normal);
        m_buildCoreDbButton->setHoverStyle(hover);
        m_buildCoreDbButton->setPressedStyle(pressed);
        m_buildCoreDbButton->setFocusedStyle(normal);
    }

    if (m_newLeagueButton)
    {
        m_newLeagueButton->setBounds(Rect{
            Point{ controlX, firstButtonY + buttonSpacing },
            controlButtonSize
            });

        m_newLeagueButton->setNormalStyle(normal);
        m_newLeagueButton->setHoverStyle(hover);
        m_newLeagueButton->setPressedStyle(pressed);
        m_newLeagueButton->setFocusedStyle(normal);
    }

    if (m_loadLeagueButton)
    {
        m_loadLeagueButton->setBounds(Rect{
            Point{ controlX, firstButtonY + (buttonSpacing * 2) },
            controlButtonSize
            });

        m_loadLeagueButton->setNormalStyle(normal);
        m_loadLeagueButton->setHoverStyle(hover);
        m_loadLeagueButton->setPressedStyle(pressed);
        m_loadLeagueButton->setFocusedStyle(normal);
    }

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

        m_mainButton->setNormalStyle(normal);
        m_mainButton->setHoverStyle(hover);
        m_mainButton->setPressedStyle(pressed);
        m_mainButton->setFocusedStyle(normal);
    }
}

void CommissionersOfficeScreen::positionLoadLeagueList(
    const Rect& contentRect)
{
    if (!m_leagueListBox || !m_loadLeagueSelectionActive)
    {
        return;
    }

    const int topOffset = 4;

    m_leagueListBox->setBounds(Rect{
        Point{
            contentRect.position.x,
            contentRect.position.y + topOffset
        },
        Size{
            std::max(0, contentRect.size.width),
            std::max(0, contentRect.size.height - topOffset)
        }
        });
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
        buildCoreDatabase();
        return;
    }

    if (result.commandId == NewLeagueCommand)
    {
        // If the user starts creating a new league while the load chooser is
        // open, leave selection mode first.
        if (m_loadLeagueSelectionActive)
        {
            cancelLoadLeagueSelection();
        }

        if (m_commandDispatcher)
        {
            m_commandDispatcher(result.commandId);
        }

        return;
    }

    if (result.commandId == LoadLeagueCommand)
    {
        if (m_loadLeagueSelectionActive)
        {
            (void)loadSelectedLeague();
        }
        else
        {
            beginLoadLeagueSelection();
        }

        return;
    }

    if (m_commandDispatcher)
    {
        m_commandDispatcher(result.commandId);
    }
}

void CommissionersOfficeScreen::buildCoreDatabase()
{
    CoreDatabaseBuildRequest request;
    request.sourceRoot = std::filesystem::path("Data") / "Source";
    request.ruleSet = "FirstEdition";
    request.season = 1989;
    request.outputDatabase = std::filesystem::path("Data") / "1989_PFF_Core.db";
    request.overwriteExisting = true;

    (void)createCoreDatabaseFromSource(request);
}

void CommissionersOfficeScreen::beginLoadLeagueSelection()
{
    refreshLeagueSaveList();
    m_loadLeagueSelectionActive = true;

    if (m_loadLeagueButton)
    {
        m_loadLeagueButton->setLabel("Open League");
    }

    if (m_leagueListBox)
    {
        m_leagueListBox->setVisible(true);
        m_leagueListBox->setEnabled(true);

        if (!m_leagueListBox->empty())
        {
            m_root.setFocusedChild(*m_leagueListBox);
        }
    }
}

void CommissionersOfficeScreen::cancelLoadLeagueSelection()
{
    m_loadLeagueSelectionActive = false;
    m_loadLeagueMessage.clear();
    m_availableLeagues.clear();

    if (m_leagueListBox)
    {
        m_leagueListBox->clearItems();
        m_leagueListBox->setVisible(false);
        m_leagueListBox->setEnabled(false);
    }

    if (m_loadLeagueButton)
    {
        m_loadLeagueButton->setLabel("Load League");

        if (m_root.containsChild(*m_loadLeagueButton))
        {
            m_root.setFocusedChild(*m_loadLeagueButton);
        }
    }
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

    const std::optional<std::size_t> selectedIndex =
        m_leagueListBox->selectedIndex();

    if (!selectedIndex.has_value() ||
        *selectedIndex >= m_availableLeagues.size())
    {
        m_loadLeagueMessage = "Select a league first.";
        return false;
    }

    const LeagueSaveEntry& selected =
        m_availableLeagues[*selectedIndex];

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
    page.drawPanel(
        "LeaguePanel",
        PFF_Colors::MainBackgroundColor,
        PFF_Colors::MainBorderColor,
        ObjectFactory::singleLineBorder());

    page.writePanelTitle(
        "LeaguePanel",
        " LEAGUE DETAILS ",
        PFF_Colors::TextHighlightColor,
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
                PFF_Colors::TextHighlightColor,
                PFF_Colors::TextHighlightColor,
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
    Composition::PageComposer& page)
{
    page.drawPanel(
        "LeaguePanel",
        PFF_Colors::MainBackgroundColor,
        PFF_Colors::MainBorderColor,
        ObjectFactory::singleLineBorder());

    page.writePanelTitle(
        "LeaguePanel",
        " LOAD LEAGUE ",
        PFF_Colors::TextHighlightColor,
        3,
        1);

    page.writeLine(
        "LeagueContent",
        0,
        "Select a saved league. Enter/Space opens it; Esc cancels.",
        PFF_Colors::TextHighlightColor);

    if (!m_loadLeagueMessage.empty())
    {
        page.writeLine(
            "LeagueContent",
            2,
            m_loadLeagueMessage,
            PFF_Colors::TextHighlightColor);
    }
}

