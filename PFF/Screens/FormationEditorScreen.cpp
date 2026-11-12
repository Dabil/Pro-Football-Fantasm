#include "Screens/FormationEditorScreen.h"
#include "Screens/GameTimeFormatting.h"
#include "Styles/PFF_Theme.h"
#include "Input/Command.h"
#include "Input/Event.h"
#include "Screens/ContentControlFrameView.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Surface.h"
#include "Rendering/Objects/BannerFactory.h"
#include "Utilities/Text/TextClip.h"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cctype>
#include <utility>

namespace
{
using namespace PffFormation;

class FormationList final : public ListBox
{
public:
    std::function<void(std::size_t, bool)> open;
    std::function<void(std::size_t)> remove;

    bool handleEvent(const Input::Event& event) override
    {
        const bool handled = ListBox::handleEvent(event);
        const auto* mouse = event.asMouse();
        const auto* command = event.asCommand();
        const bool activate = mouse && handled && mouse->button == Input::MouseButton::Left &&
            (mouse->isPress() || mouse->isClick());
        const bool confirm = command && isFocused() && command->command.code == Input::CommandCode::Confirm;
        const auto* key = event.asKey();
        if (key && key->pressed && key->code == Input::KeyCode::Delete && isFocused() && remove)
        {
            if (const auto index = selectedIndex()) remove(*index);
            return true;
        }
        if ((activate || confirm) && open)
        {
            if (const auto index = selectedIndex())
            {
                bool rename = false;
                if (activate)
                {
                    const auto now = std::chrono::steady_clock::now();
                    rename = mouse->clickCount >= 2 || (m_lastClick == index &&
                        now - m_lastClickTime <= std::chrono::milliseconds(GetDoubleClickTime()));
                    m_lastClick = rename ? std::nullopt : index;
                    m_lastClickTime = now;
                }
                open(*index, rename);
            }
            return true;
        }
        return handled;
    }
private:
    std::optional<std::size_t> m_lastClick;
    std::chrono::steady_clock::time_point m_lastClickTime{};
};
constexpr std::pair<std::string_view, Role> roleNames[] = {
    {"QB", Role::QB}, {"RB", Role::RB}, {"WR", Role::WR}, {"TE", Role::TE},
    {"RC", Role::RC}, {"RN", Role::RN}, {"T", Role::T}, {"G", Role::G},
    {"C", Role::C}, {"OL", Role::OL}, {"DE", Role::DE}, {"DT", Role::DT},
    {"DL", Role::DL}, {"LB", Role::LB}, {"CB", Role::CB}, {"S", Role::S}, {"DB", Role::DB}
};

std::string trim(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

std::string uppercase(std::string value)
{
    for (auto& ch : value) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return value;
}

std::optional<RoleReference> parseRole(const std::string& value)
{
    const auto text = uppercase(trim(value));
    for (const auto& [name, type] : roleNames)
    {
        if (text.compare(0, name.size(), name) != 0 || text.size() == name.size()) continue;
        unsigned rank = 0;
        const auto end = text.data() + text.size();
        const auto parsed = std::from_chars(text.data() + name.size(), end, rank);
        if (parsed.ec == std::errc{} && parsed.ptr == end && rank) return RoleReference{type, rank};
    }
    return std::nullopt;
}

std::string roleLabel(RoleReference role)
{
    for (const auto& [name, type] : roleNames)
        if (role.type == type) return std::string(name) + std::to_string(role.rank);
    return {};
}

int lateralPosition(Side side, std::size_t index)
{
    // Half-column units preserve staggered spots and the center spine.
    constexpr int offense[] = {0,2,4,6,8,10,12,14,16, 1,3,8,13,15, 8,4,8,12};
    constexpr int defense[] = {0,2,4,6,8,10,12,14,16, 0,2,4,6,8,10,12,14,16, 1,5,8,11,15};
    return side == Side::Offense ? offense[index] : defense[index];
}
}

FormationEditorScreen::FormationEditorScreen(Assets::AssetLibrary& assets, LoadedWorldContext world,
    std::function<LoadedWorldContext()> context, std::int64_t teamSeasonId, std::function<void()> close)
    : m_world(std::move(world)), m_context(std::move(context)), m_teamSeasonId(teamSeasonId), m_close(std::move(close))
{
    const auto font = assets.loadBannerFontAsset("banner.ansi_shadow");
    if (font.success && font.hasFont())
        m_banner = BannerFactory::makeBanner(*font.asset.font, "Formations", PFF_Colors::ProFootballTitleColor);

    m_actions.push_back(addButton(m_root, "Offensive Alignment", Action::Offense));
    m_actions.push_back(addButton(m_root, "Defensive Alignment", Action::Defense));
    auto list = std::make_unique<FormationList>();
    m_list = list.get();
    list->open = [this](std::size_t index, bool rename) {
        if (index < m_listEntries.size())
        {
            m_openEntry = m_listEntries[index];
            m_action = rename ? Action::Rename : Action::Open;
        }
    };
    list->remove = [this](std::size_t index) {
        if (index < m_listEntries.size())
        {
            m_openEntry = m_listEntries[index];
            m_action = Action::Delete;
        }
    };
    m_root.addChild(std::move(list));
    m_actions.push_back(addButton(m_root, "New", Action::New));
    m_actions.push_back(addButton(m_root, "Save", Action::Save));
    m_actions.push_back(addButton(m_root, "Save As", Action::SaveAs));

    auto name = std::make_unique<TextInput>();
    m_name = name.get();
    name->setPlaceholder("Formation name");
    m_namePrompt.addChild(std::move(name));
    m_confirmName = addButton(m_namePrompt, "Save", Action::ConfirmName);
    m_cancelName = addButton(m_namePrompt, "Cancel", Action::CancelName);
    m_confirmDelete = addButton(m_deletePrompt, "Yes", Action::ConfirmDelete);
    m_cancelDelete = addButton(m_deletePrompt, "No", Action::CancelDelete);
    auto role = std::make_unique<TextInput>();
    m_role = role.get();
    role->setPlaceholder("Role, e.g. WR1 or RC3");
    m_root.addChild(std::move(role));

    for (std::size_t i = 0; i < stockBoard(Side::Defense).size(); ++i)
    {
        auto button = std::make_unique<Button>("");
        m_spots.push_back(button.get());
        button->setActivationCallback([this, i](const ButtonActivationResult& result) {
            if (result.activated) m_pendingSpot = i;
        });
        m_root.addChild(std::move(button));
    }
    m_return = addButton(m_root, "Return", Action::Return);
    m_promptActions = {addButton(m_prompt, "Save as Draft", Action::Save),
        addButton(m_prompt, "Discard", Action::Discard),
        addButton(m_prompt, "Keep Editing", Action::Keep)};
    refreshList();
}

Button* FormationEditorScreen::addButton(ContainerWidget& root, const char* label, Action action)
{
    auto button = std::make_unique<Button>(label);
    auto* result = button.get();
    button->setActivationCallback([this, action](const ButtonActivationResult& event) {
        if (event.activated) m_action = action;
    });
    root.addChild(std::move(button));
    return result;
}

void FormationEditorScreen::onEnter()
{
    m_root.resetFocus();
    if (m_bookLoaded) return;
    try
    {
        m_book = loadWorldFormationBook(m_world, m_teamSeasonId);
        m_bookLoaded = true;
        refreshList();
    }
    catch (const std::exception& error)
    {
        m_message = std::string("Unable to load formations: ") + error.what();
    }
}

void FormationEditorScreen::refreshList()
{
    std::vector<ListBoxItem> items;
    m_listEntries.clear();
    for (std::size_t i = 0; i < m_book.entries.size(); ++i)
    {
        if (m_book.entries[i].draft.side() != m_draft.side()) continue;
        items.push_back({m_book.entries[i].name});
        m_listEntries.push_back(i);
    }
    m_list->setItems(std::move(items));
    if (m_entry)
    {
        const auto found = std::find(m_listEntries.begin(), m_listEntries.end(), *m_entry);
        if (found != m_listEntries.end())
            m_list->selectIndex(static_cast<std::size_t>(found - m_listEntries.begin()));
    }
}

void FormationEditorScreen::load(std::optional<std::size_t> entry, Side side)
{
    m_entry = entry;
    m_draft = entry ? m_book.entries[*entry].draft : Draft(side);
    m_role->clear();
    m_message.clear();
    refreshList();
}

bool FormationEditorScreen::dirty() const
{
    if (!m_entry) return !m_draft.assignments().empty();
    const auto& saved = m_book.entries[*m_entry];
    return m_draft.assignments() != saved.draft.assignments();
}

bool FormationEditorScreen::save(std::string name, const Draft& draft, std::optional<std::size_t> entry)
{
    name = trim(std::move(name));
    if (name.empty())
    {
        m_message = "Enter a formation name.";
        return false;
    }
    const auto key = uppercase(name);
    for (std::size_t i = 0; i < m_book.entries.size(); ++i)
    {
        if (entry == i || m_book.entries[i].draft.side() != draft.side()) continue;
        if (uppercase(m_book.entries[i].name) == key)
        {
            m_message = "That formation name is already in this book.";
            return false;
        }
    }
    auto updated = m_book;
    const auto index = entry.value_or(updated.entries.size());
    if (entry) updated.entries[index] = {name, draft};
    else updated.entries.push_back({name, draft});
    if (!persist(std::move(updated))) return false;
    load(index, m_book.entries[index].draft.side());
    m_message = "Draft saved to the league.";
    return true;
}

bool FormationEditorScreen::persist(WorldFormationBook updated)
{
    const auto world = m_context ? m_context() : m_world;
    if (!m_bookLoaded || !world.hasActiveCompetitionSeason() ||
        world.databasePath != m_world.databasePath || world.worldKey != m_world.worldKey ||
        world.activeCompetitionSeasonId != m_world.activeCompetitionSeasonId)
    {
        m_message = "Reopen the editor for the active team before saving.";
        return false;
    }
    try
    {
        saveWorldFormationBook(m_world, m_teamSeasonId, updated);
    }
    catch (const std::exception& error)
    {
        m_message = std::string("Unable to update formations: ") + error.what();
        return false;
    }
    m_book = std::move(updated);
    return true;
}

void FormationEditorScreen::deleteSelected()
{
    if (!m_deleteEntry || *m_deleteEntry >= m_book.entries.size()) return;
    const auto index = *m_deleteEntry;
    auto updated = m_book;
    updated.entries.erase(updated.entries.begin() + static_cast<std::ptrdiff_t>(index));
    if (!persist(std::move(updated))) return;

    m_deleteEntry.reset();
    if (m_entry == index) load({}, m_draft.side());
    else
    {
        if (m_entry && *m_entry > index) --*m_entry;
        refreshList();
    }
    m_message = "Formation deleted.";
}

bool FormationEditorScreen::saveExisting()
{
    return m_entry && dirty() && save(m_book.entries[*m_entry].name, m_draft, m_entry);
}

void FormationEditorScreen::promptForName(Action action)
{
    m_naming = action;
    m_name->setText(action == Action::Rename && m_entry ? m_book.entries[*m_entry].name : "");
    m_message.clear();
    m_namePrompt.resetFocus();
    m_namePrompt.setFocusedChild(*m_name);
}

void FormationEditorScreen::activateSpot(std::size_t spot)
{
    if (m_draft.remove(spot))
    {
        m_message = "Role removed.";
        return;
    }
    const auto role = parseRole(m_role->text());
    if (role && m_draft.place(*role, spot)) m_message = "Role placed.";
    else m_message = role ? "That placement is unavailable." : "Enter a role such as WR1 or LB2.";
}

void FormationEditorScreen::navigate(Action action)
{
    if (action == Action::Return)
    {
        const auto close = m_close;
        if (close) close();
    }
    else if (action == Action::Offense) load({}, Side::Offense);
    else if (action == Action::Defense) load({}, Side::Defense);
    else if (action == Action::New) promptForName(Action::New);
    else if (action == Action::Delete && m_openEntry)
    {
        m_deleteEntry = std::exchange(m_openEntry, std::nullopt);
        m_message.clear();
        m_deletePrompt.resetFocus();
        m_deletePrompt.setFocusedChild(*m_cancelDelete);
    }
    else if ((action == Action::Open || action == Action::Rename) && m_openEntry)
    {
        load(std::exchange(m_openEntry, std::nullopt), m_draft.side());
        if (action == Action::Rename) promptForName(action);
    }
}

void FormationEditorScreen::process(Action action)
{
    if (m_deleteEntry)
    {
        if (action == Action::ConfirmDelete) deleteSelected();
        else if (action == Action::CancelDelete)
        {
            m_deleteEntry.reset();
            refreshList();
        }
        return;
    }
    if (m_naming != Action::None)
    {
        if (action == Action::CancelName)
        {
            m_naming = Action::None;
            m_navigation = Action::None;
            m_message.clear();
        }
        else if (action == Action::ConfirmName)
        {
            const Draft draft = m_naming == Action::New ? Draft(m_draft.side()) : m_draft;
            if (save(m_name->text(), draft, m_naming == Action::Rename ? m_entry : std::nullopt))
            {
                m_namePrompt.clearChildFocus();
                m_naming = Action::None;
                navigate(std::exchange(m_navigation, Action::None));
            }
        }
        return;
    }
    if (m_navigation != Action::None)
    {
        if (action == Action::Keep)
        {
            m_navigation = Action::None;
            refreshList();
        }
        else if (action == Action::Save && !m_entry) promptForName(Action::SaveAs);
        else if (action == Action::Discard || (action == Action::Save && saveExisting()))
            navigate(std::exchange(m_navigation, Action::None));
        return;
    }

    if (action == Action::Save) saveExisting();
    else if (action == Action::SaveAs) promptForName(Action::SaveAs);
    else if (action != Action::None)
    {
        if ((action == Action::Offense && m_draft.side() == Side::Offense) ||
            (action == Action::Defense && m_draft.side() == Side::Defense) ||
            (action == Action::Open && m_openEntry == m_entry)) return;
        if (dirty())
        {
            m_navigation = action;
            m_prompt.resetFocus();
        }
        else navigate(action);
    }
}

bool FormationEditorScreen::handleEvent(const Input::Event& event)
{
    if (m_naming != Action::None && m_name->isFocused())
    {
        const auto* command = event.asCommand();
        if (command && command->command.code == Input::CommandCode::Confirm)
        {
            process(Action::ConfirmName);
            return true;
        }
    }
    auto& root = m_deleteEntry ? m_deletePrompt : m_naming != Action::None ? m_namePrompt :
        m_navigation == Action::None ? m_root : m_prompt;
    const bool handled = root.handleEvent(event);
    if (m_pendingSpot)
    {
        const auto spot = *m_pendingSpot;
        m_pendingSpot.reset();
        if (spot < stockBoard(m_draft.side()).size()) activateSpot(spot);
    }
    const auto action = std::exchange(m_action, Action::None);
    if (action != Action::None) process(action);
    return handled || action != Action::None;
}

void FormationEditorScreen::update(const Animation::TickEvent& event)
{
    (m_deleteEntry ? m_deletePrompt : m_naming != Action::None ? m_namePrompt :
        m_navigation == Action::None ? m_root : m_prompt).update(event);
}

void FormationEditorScreen::draw(Surface& surface)
{
    auto& buffer = surface.buffer();
    Composition::PageComposer page(buffer);
    const auto frame = ContentControlFrameView::drawFrame(
        page, m_banner, "Formations", ContentControlFrameView::TitleFallback::CenteredInTitleArea,
        ContentControlView::ControlPaneWidth{0, 30});
    const auto& screen = frame.screen;
    const auto& footer = frame.footer;
    const auto& content = frame.content;
    const auto& controls = frame.controls;
    const auto& text = PFF_Colors::TextHighlightColor;
    const auto line = [&](const Rect& area, int row, std::string_view value) {
        if (row >= 0 && row < area.size.height)
            page.writeLine(area, row, TextClip::clipUtf8Text(value, area.size.width), text);
    };
    const auto bounds = [](const Rect& area, int x, int y, int width, int height) {
        return Rect{Point{area.position.x + x, area.position.y + y},
            Size{std::max(0, width), std::max(0, height)}};
    };
    const bool offense = m_draft.side() == Side::Offense;
    const auto& boardStyle = offense ? PFF_Colors::FormationOffenseColor : PFF_Colors::FormationDefenseColor;
    const auto boardArea = bounds(content, 0, 5, content.size.width,
        std::min(offense ? 13 : 11, content.size.height - 5));
    if (boardArea.size.height > 0)
    {
        buffer.fillRect(boardArea, U' ', boardStyle.withUnderline(false));
        page.writeLine(boardArea, 0, TextClip::clipUtf8Text("Line of scrimmage", boardArea.size.width),
            boardStyle.withUnderline(false));
    }
    const auto styleButton = [](Button& button, const Style& base) {
        const auto plain = base.withUnderline(false);
        button.setNormalStyle(plain);
        button.setFocusedStyle(plain.withBold(true));
        button.setHoverStyle(plain.withBold(true));
        button.setPressedStyle(plain.withBold(true));
        button.setDisabledStyle(plain.withDim(true));
    };
    line(content, 0, m_draft.side() == Side::Offense ? "Offensive Alignment" : "Defensive Alignment");
    if (m_entry) line(content, 1, m_book.entries[*m_entry].name);
    m_role->setBounds(bounds(content, 1, 3, content.size.width - 2, 1));
    line(content, 4, "Green: preferred. Yellow: allowed. Select a role and spot.");


    const auto& board = stockBoard(m_draft.side());
    const auto selected = parseRole(m_role->text());
    const int cell = std::max(1, content.size.width / 9);
    for (std::size_t i = 0; i < m_spots.size(); ++i)
    {
        auto& button = *m_spots[i];
        if (i >= board.size())
        {
            button.setEnabled(false);
            button.setBounds(bounds(content, 0, 0, 0, 0));
            continue;
        }
        const auto& spot = board[i];
        const auto assigned = m_draft.assignments().find(i);
        if (assigned != m_draft.assignments().end())
            button.setLabel(roleLabel(assigned->second));
        else if (m_draft.assignments().size() == 11)
            button.clearLabel();
        else
            button.setLabel(spot.label);
        const int x = lateralPosition(m_draft.side(), i) * cell / 2;
        const int y = 6 + (spot.row - 1) * 3;
        button.setBounds(bounds(content, x, y, cell, 3));
        button.setEnabled(y + 3 <= content.size.height);
        auto style = boardStyle;
        if (selected)
        {
            const auto preference = placement(m_draft.side(), i, selected->type);
            if (preference == Placement::Preferred) style = PFF_Colors::FormationPreferredColor;
            else if (preference == Placement::Allowed) style = PFF_Colors::FormationAllowedColor;
        }
        styleButton(button, style.withDim(assigned == m_draft.assignments().end()));
    }
    if (!offense && boardArea.size.height > 10)
        page.writeLine(boardArea, 10,
            TextClip::clipUtf8Text("LD / RD: assigned in defensive plays", boardArea.size.width),
            boardStyle.withUnderline(false));
    for (std::size_t i = 0; i < m_actions.size(); ++i)
    {
        const auto& controlStyle = i == 0 && !offense ? PFF_Colors::FormationInactiveOffenseColor :
            i == 1 && offense ? PFF_Colors::FormationInactiveDefenseColor : boardStyle;
        styleButton(*m_actions[i], controlStyle);
    }
    line(content, 18, m_message);
    int issueRow = 19;
    for (const auto& issue : m_draft.structuralIssues()) line(content, issueRow++, issue);
    line(content, issueRow, "Drafts save to the league. Callable saves require personnel packages.");

    const int controlWidth = controls.size.width - 2;
    m_actions[0]->setBounds(bounds(controls, 1, 0, controlWidth, 3));
    m_actions[1]->setBounds(bounds(controls, 1, 3, controlWidth, 3));
    m_actions[3]->setEnabled(m_bookLoaded && m_entry.has_value() && dirty());
    const int listHeight = std::max(2, controls.size.height - 15);
    m_list->setBounds(bounds(controls, 1, 6, controlWidth, listHeight));
    for (std::size_t i = 2; i < m_actions.size(); ++i)
        m_actions[i]->setBounds(bounds(controls, 1, 6 + listHeight + static_cast<int>(i - 2) * 3, controlWidth, 3));
    const auto world = m_context ? m_context() : m_world;
    ContentControlFrameView::drawFooter(page, footer, world.worldName,
        PffGameTime::formatGameTimeEastern(world.currentTimeUtc));
    m_return->setBounds(PffLeagueFooter::returnBounds(footer));
    m_return->setNormalStyle(PFF_Colors::ButtonNormalColor);
    m_return->setFocusedStyle(PFF_Colors::ButtonHoverColor);
    m_return->setHoverStyle(PFF_Colors::ButtonHoverColor);
    m_return->setPressedStyle(PFF_Colors::ButtonPressedColor);
    m_return->setDisabledStyle(PFF_Colors::ButtonNormalColor.withDim(true));
    m_root.setBounds(screen);
    m_root.draw(surface);
    if (m_deleteEntry)
    {
        const int width = std::min(screen.size.width, 66);
        const auto modal = bounds(screen, (screen.size.width - width) / 2,
            std::max(0, (screen.size.height - 8) / 2), width, 8);
        buffer.fillRect(modal, U' ', PFF_Colors::HeaderInnerColor);
        line(modal, 1, "Delete formation " + m_book.entries[*m_deleteEntry].name + "?");
        line(modal, 2, m_message);
        m_confirmDelete->setBounds(bounds(modal, 2, 4, width / 2 - 3, 3));
        m_cancelDelete->setBounds(bounds(modal, width / 2 + 1, 4, width / 2 - 3, 3));
        m_deletePrompt.setBounds(modal);
        m_deletePrompt.draw(surface);
    }
    else if (m_naming != Action::None)
    {
        const int width = std::min(screen.size.width, 66);
        const auto modal = bounds(screen, (screen.size.width - width) / 2,
            std::max(0, (screen.size.height - 9) / 2), width, 9);
        buffer.fillRect(modal, U' ', PFF_Colors::HeaderInnerColor);
        line(modal, 1, m_naming == Action::New ? "New Formation" :
            m_naming == Action::Rename ? "Rename Formation" : "Save Formation As");
        line(modal, 2, m_message);
        m_name->setBounds(bounds(modal, 2, 3, width - 4, 1));
        m_confirmName->setBounds(bounds(modal, 2, 5, width / 2 - 3, 3));
        m_cancelName->setBounds(bounds(modal, width / 2 + 1, 5, width / 2 - 3, 3));
        m_namePrompt.setBounds(modal);
        m_namePrompt.draw(surface);
    }
    else if (m_navigation != Action::None)
    {
        const int width = std::min(screen.size.width, 66);
        const auto modal = bounds(screen, (screen.size.width - width) / 2,
            std::max(0, (screen.size.height - 8) / 2), width, 8);
        buffer.fillRect(modal, U' ', PFF_Colors::HeaderInnerColor);
        line(modal, 1, "Save changes before leaving this formation?");
        line(modal, 2, m_message);
        const int buttonWidth = width / 3;
        for (std::size_t i = 0; i < m_promptActions.size(); ++i)
            m_promptActions[i]->setBounds(bounds(modal, static_cast<int>(i) * buttonWidth, 4, buttonWidth, 3));
        m_prompt.setBounds(modal);
        m_prompt.draw(surface);
    }
}
