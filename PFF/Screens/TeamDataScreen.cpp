#include "Screens/TeamDataScreen.h"
#include "Database/CoreManifest.h"
#include "Screens/GameTimeFormatting.h"
#include "Styles/PFF_Theme.h"
#include "Input/Event.h"
#include "Rendering/Surface.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Objects/BannerFactory.h"
#include "Utilities/Text/TextClip.h"
#include "Utilities/Text/TextWrap.h"
#include <algorithm>

namespace
{
    const std::array<const char*, 5> RoleKeys = {"OWNER", "GENERAL_MANAGER", "HEAD_COACH",
                                                 "OFFENSIVE_COORDINATOR", "DEFENSIVE_COORDINATOR"};
    const std::array<const char*, 5> RoleLabels = {"Owner", "General Manager", "Head Coach",
                                                   "Offensive Coordinator", "Defensive Coordinator"};
    void orderStaff(WorldTeamData& model)
    {
        const auto original = model.staff;
        model.staff.clear();
        for (std::size_t i = 0; i < RoleKeys.size(); ++i)
        {
            auto p = std::find_if(original.begin(), original.end(),
                                  [&](const auto& s) { return s.roleKey == RoleKeys[i]; });
            if (p != original.end())
                model.staff.push_back(*p);
            else
            {
                WorldStaffPosition empty;
                empty.roleKey = RoleKeys[i];
                empty.displayName = RoleLabels[i];
                model.staff.push_back(empty);
            }
        }
    }
} // namespace
TeamDataScreen::TeamDataScreen(Assets::AssetLibrary& assets, std::function<bool(std::string_view)> dispatch,
                               std::function<LoadedWorldContext()> context, LoadedWorldContext selected,
                               std::int64_t id, CommitCallback commit)
    : m_assets(assets), m_dispatch(std::move(dispatch)), m_context(std::move(context)),
      m_commit(std::move(commit)), m_selectedWorld(std::move(selected)), m_teamSeasonId(id)
{
    buildWidgets();
}
void TeamDataScreen::buildWidgets()
{
    // Text inputs: normal, focus, mouse hover, hint and read-only appearance.
    const Style& input = PFF_Colors::InputFieldNormalColor;
    const Style& inputFocus = PFF_Colors::InputFieldFocusedColor;
    const Style& inputHover = PFF_Colors::InputFieldFocusedColor;
    const Style& hint = PFF_Colors::InputFieldNormalHintColor;
    const Style& hintFocus = PFF_Colors::InputFieldFocusedHintColor;
    const Style inputDisabled = input.withDim(true);
    // Action buttons share one appearance throughout the form and dialogs.
    const Style& button = PFF_Colors::ButtonNormalColor;
    const Style& buttonFocus = PFF_Colors::ButtonHoverColor;
    const Style& buttonHover = PFF_Colors::ButtonHoverColor;
    const Style& buttonPress = PFF_Colors::ButtonPressedColor;
    const Style buttonDisabled = button.withDim(true);
    // Staff selectors are a separate component type from action buttons.
    const Style& selector = PFF_Colors::InputFieldNormalColor;
    const Style& selectorFocus = PFF_Colors::InputFieldFocusedColor;
    const Style& selectorPress = PFF_Colors::ButtonPressedColor;
    // Checkbox uses its own mappings, independent of other buttons.
    const Style& checkbox = PFF_Colors::InputFieldNormalColor;
    const Style& checkboxFocus = PFF_Colors::InputFieldFocusedColor;
    const Style& checkboxPress = PFF_Colors::ButtonPressedColor;
    // Lists and scrollbar colors for people and venue history.
    const Style& list = PFF_Colors::InputFieldNormalColor;
    const Style& selected = PFF_Colors::InputFieldFocusedColor;
    const Style& thumb = PFF_Colors::PanelSliderColor;
    const Style& track = PFF_Colors::PanelTrackColor;
    auto inputWidget = [&](ContainerWidget& root, const char* placeholder) {
        auto p = std::make_unique<TextInput>();
        p->setPlaceholder(placeholder);
        p->setNormalStyle(input);
        p->setFocusedStyle(inputFocus);
        p->setHoverStyle(inputHover);
        p->setDisabledStyle(inputDisabled);
        p->setPlaceholderNormalStyle(hint);
        p->setPlaceholderFocusedStyle(hintFocus);
        auto* out = p.get();
        root.addChild(std::move(p));
        return out;
    };
    auto actionButton = [&](ContainerWidget& root, const char* label, Action action) {
        auto p = std::make_unique<Button>(label);
        p->setNormalStyle(button);
        p->setFocusedStyle(buttonFocus);
        p->setHoverStyle(buttonHover);
        p->setPressedStyle(buttonPress);
        p->setDisabledStyle(buttonDisabled);
        p->setActivationCallback([this, action](const ButtonActivationResult& r) {
            if (r.activated)
                m_action = action;
        });
        auto* out = p.get();
        root.addChild(std::move(p));
        return out;
    };
    m_inputs[0] = inputWidget(m_root, "City");
    m_inputs[1] = inputWidget(m_root, "Team name");
    for (std::size_t i = 0; i < 5; ++i)
    {
        auto p = std::make_unique<Button>("Unassigned v");
        p->setNormalStyle(selector);
        p->setFocusedStyle(selectorFocus);
        p->setHoverStyle(selectorFocus);
        p->setPressedStyle(selectorPress);
        p->setDisabledStyle(selector.withDim(true));
        // Opening a selector does not remove any widgets during event dispatch.
        p->setActivationCallback([this, i](const ButtonActivationResult& r) {
            if (r.activated)
                openStaff(i);
        });
        m_staffButtons.push_back(p.get());
        m_root.addChild(std::move(p));
    }
    m_inputs[2] = inputWidget(m_root, "Unknown stadium");
    m_inputs[3] = inputWidget(m_root, "Unknown surface");
    for (auto* p : m_inputs)
        p->setChangeCallback([this](const std::string&) {
            if (!m_filling)
                synchronize();
        });
    auto management = std::make_unique<FormCheckBox>("Manage Team");
    management->setNormalStyle(checkbox);
    management->setFocusedStyle(checkboxFocus);
    management->setHoverStyle(checkboxFocus);
    management->setPressedStyle(checkboxPress);
    management->setDisabledStyle(checkbox.withDim(true));
    management->setChangeCallback([this](bool enabled) { toggleManagement(enabled); });
    m_manage = management.get();
    m_root.addChild(std::move(management));
    m_save = actionButton(m_root, "Save Changes", Action::Save);
    m_cancel = actionButton(m_root, "Cancel Changes", Action::Cancel);
    m_history = actionButton(m_root, "Venue History", Action::None);
    m_history->setActivationCallback([this](const ButtonActivationResult& r) {
        if (r.activated)
            openHistory();
    });
    auto listWidget = [&](ContainerWidget& root) {
        auto p = std::make_unique<ListBox>();
        p->setNormalStyle(list);
        p->setFocusedStyle(list);
        p->setSelectedStyle(list);
        p->setSelectedFocusedStyle(selected);
        p->setVerticalScrollbarVisible(true);
        auto bar = p->verticalScrollbarStyle();
        bar.thumbStyle = thumb;
        bar.trackStyle = track;
        p->setVerticalScrollbarStyle(bar);
        auto* out = p.get();
        root.addChild(std::move(p));
        return out;
    };
    m_return = actionButton(m_root, "Return", Action::Return);
    m_choices = listWidget(m_modalRoot);
    m_use = actionButton(m_modalRoot, "Use Selected Person", Action::UsePerson);
    m_remove = actionButton(m_modalRoot, "Remove from Role", Action::RemovePerson);
    m_new = actionButton(m_modalRoot, "Create New Person", Action::NewPerson);
    m_keep = actionButton(m_modalRoot, "Keep Current Person", Action::KeepPerson);
    m_previous = actionButton(m_modalRoot, "Previous Page", Action::PreviousPage);
    m_next = actionButton(m_modalRoot, "Next Page", Action::NextPage);
    for (std::size_t i = 0; i < 3; ++i)
        m_personInputs[i] = inputWidget(m_modalRoot, i == 0   ? "Person display name"
                                                     : i == 1 ? "First name (optional)"
                                                              : "Last name (optional)");
    m_create = actionButton(m_modalRoot, "Use New Person", Action::CreatePerson);
    m_modalCancel = actionButton(m_modalRoot, "Cancel", Action::CloseModal);
    m_saveLeave = actionButton(m_modalRoot, "Save and Return", Action::SaveLeave);
    m_discardLeave = actionButton(m_modalRoot, "Discard and Return", Action::DiscardLeave);
    m_stay = actionButton(m_modalRoot, "Stay Here", Action::CloseModal);
}
void TeamDataScreen::onEnter()
{
    // Header title uses the established banner style.
    const Style& title = PFF_Colors::ProFootballTitleColor;
    const auto font = m_assets.loadBannerFontAsset("banner.ansi_shadow");
    if (font.success && font.hasFont())
        m_banner = BannerFactory::makeBanner(*font.asset.font, "Team Data", title);
    reload();
    m_root.resetFocus();
    m_modalRoot.resetFocus();
}
void TeamDataScreen::onExit()
{
    m_root.resetFocus();
    m_modalRoot.resetFocus();
}
bool TeamDataScreen::sameWorld() const
{
    const auto c = m_context ? m_context() : LoadedWorldContext{};
    return c.hasActiveCompetitionSeason() && c.worldKey == m_selectedWorld.worldKey &&
           c.databasePath == m_selectedWorld.databasePath &&
           c.activeCompetitionSeasonId == m_selectedWorld.activeCompetitionSeasonId;
}
void TeamDataScreen::reload()
{
    closeModal();
    m_filling = true;
    m_available = false;
    m_stale = false;
    m_message.clear();
    m_staffDraft.clear();
    m_draftNames.clear();
    m_saved = {};
    m_permissions = {};
    m_management = false;
    for (auto* p : m_inputs)
        p->clear();
    m_displayContext = m_context ? m_context() : LoadedWorldContext{};
    if (!sameWorld())
        m_message = "This team is not in the active league and season.";
    else
    {
        const auto actor = openLocalWorldActor(m_displayContext.databasePath);
        if (!actor.success)
            m_message = actor.message;
        else
        {
            m_actor = actor.value;
            auto loaded = loadTeamData(m_actor, m_teamSeasonId);
            if (!loaded.success)
                m_message = loaded.message;
            else
            {
                m_saved = loaded.value;
                orderStaff(m_saved);
                m_permissions = m_saved;
                m_available = true;
                m_management = m_saved.managed;
                m_inputs[0]->setText(m_saved.city);
                m_inputs[1]->setText(m_saved.nickname);
                m_inputs[2]->setText(m_saved.venueName);
                m_inputs[3]->setText(m_saved.surface.value_or(""));
            }
        }
    }
    m_filling = false;
    synchronize();
    m_root.resetFocus();
}
WorldTeamDataDraft TeamDataScreen::draft() const
{
    WorldTeamDataDraft d;
    d.worldId = m_saved.worldId;
    d.organizationId = m_saved.organizationId;
    d.teamSeasonId = m_saved.teamSeasonId;
    d.expectedRevision = m_saved.revision;
    if (m_inputs[0]->text() != m_saved.city)
        d.city = m_inputs[0]->text();
    if (m_inputs[1]->text() != m_saved.nickname)
        d.nickname = m_inputs[1]->text();
    if (m_inputs[2]->text() != m_saved.venueName)
        d.venueName = m_inputs[2]->text();
    if (m_inputs[3]->text() != m_saved.surface.value_or(""))
        d.surface.emplace(m_inputs[3]->text().empty() ? std::nullopt
                                                      : std::optional<std::string>(m_inputs[3]->text()));
    if (m_management != m_saved.managed)
        d.managed = m_management;
    d.staff = m_staffDraft;
    return d;
}
bool TeamDataScreen::dirty() const
{
    if (!m_available)
        return false;
    const auto d = draft();
    return d.city || d.nickname || d.venueName || d.surface || d.managed || !d.staff.empty();
}
void TeamDataScreen::synchronize()
{
    const bool active = m_available && sameWorld() && !m_stale;
    m_manage->setChecked(m_management);
    m_manage->setEnabled(active && m_saved.canManage);
    m_inputs[0]->setEnabled(active && m_permissions.canEditIdentity);
    m_inputs[1]->setEnabled(active && m_permissions.canEditIdentity);
    m_inputs[2]->setEnabled(active && m_permissions.canEditVenue);
    m_inputs[3]->setEnabled(active && m_permissions.canEditVenue);
    for (std::size_t i = 0; i < m_staffButtons.size(); ++i)
    {
        std::string name = "Unassigned";
        if (m_available)
        {
            const auto& p = m_saved.staff[i];
            if (p.assignmentId)
                name = p.personName;
            const auto changed = std::find_if(m_staffDraft.begin(), m_staffDraft.end(),
                                              [&](const auto& a) { return a.positionId == p.positionId; });
            if (changed != m_staffDraft.end())
            {
                if (changed->kind == WorldStaffActionKind::Remove)
                    name = "Unassigned (pending)";
                else if (changed->newPerson)
                    name = changed->newPerson->displayName + " (new)";
                else
                {
                    auto n = m_draftNames.find(p.positionId);
                    name = n == m_draftNames.end() ? "Selected person (pending)" : n->second + " (pending)";
                }
            }
        }
        m_staffButtons[i]->setLabel(name + " v");
        m_staffButtons[i]->setEnabled(active && m_permissions.staff[i].editable &&
                                      m_saved.staff[i].positionId != 0);
    }
    const bool modified = dirty();
    m_save->setVisible(modified);
    m_save->setEnabled(active && modified);
    m_cancel->setVisible(modified);
    m_cancel->setEnabled(modified);
    m_history->setEnabled(active && m_saved.venueId != 0);
    if (auto* p = m_root.focusedChild())
        if (!p->canReceiveFocus())
            m_root.clearChildFocus();
}
void TeamDataScreen::toggleManagement(bool enabled)
{
    if (!m_available)
        return;
    const auto projected = previewTeamManagement(m_actor, m_teamSeasonId, enabled);
    if (!projected.success)
    {
        m_manage->setChecked(m_management);
        m_message = projected.message;
        return;
    }
    if (projected.value.revision != m_saved.revision)
    {
        m_manage->setChecked(m_management);
        m_stale = true;
        m_message = "The league changed. Cancel Changes to reload before saving.";
        return;
    }
    m_management = enabled;
    m_permissions = projected.value;
    orderStaff(m_permissions);
    m_message.clear();
    synchronize();
}
void TeamDataScreen::refreshContext()
{
    const auto c = m_context ? m_context() : LoadedWorldContext{};
    if (c.loaded != m_displayContext.loaded || c.worldKey != m_displayContext.worldKey ||
        c.databasePath != m_displayContext.databasePath ||
        c.activeCompetitionSeasonId != m_displayContext.activeCompetitionSeasonId ||
        c.dataRevision != m_displayContext.dataRevision ||
        c.currentTimeUtc != m_displayContext.currentTimeUtc)
    {
        if (!dirty())
            reload();
        else
        {
            m_stale = true;
            m_message = "The league changed. Your draft is retained. Cancel Changes to reload.";
            m_displayContext = c;
            synchronize();
        }
    }
}
void TeamDataScreen::openStaff(std::size_t role)
{
    if (!m_available || !m_permissions.staff[role].editable)
        return;
    m_role = role;
    m_page = 0;
    m_modal = Modal::Staff;
    m_message.clear();
    loadPeoplePage();
    m_root.clearPointerState();
    m_root.clearChildFocus();
    m_modalRoot.resetFocus();
}
void TeamDataScreen::loadPeoplePage()
{
    const auto result = listAvailablePeople(m_actor, m_saved.organizationId, m_page * 100, 101);
    m_people.clear();
    m_choices->clearItems();
    if (!result.success)
    {
        m_message = result.message;
        return;
    }
    m_people = result.value;
    const bool more = m_people.size() > 100;
    if (more)
        m_people.pop_back();
    for (const auto& p : m_people)
        m_choices->addItem(p.displayName);
    m_use->setEnabled(!m_people.empty());
    m_previous->setEnabled(m_page > 0);
    m_next->setEnabled(more);
    if (!m_people.empty())
        m_choices->selectIndex(0);
    else
        m_choices->clearSelection();
}
void TeamDataScreen::openHistory()
{
    m_historyLines.clear();
    m_historyWrapWidth = -1;
    std::size_t offset = 0;
    for (;;)
    {
        auto history = listVenueHistory(m_actor, m_saved.venueId, offset, 100);
        if (!history.success)
        {
            m_message = history.message;
            return;
        }
        std::size_t weeks = 0;
        for (const auto& e : history.value)
        {
            if (!e.baseline)
                ++weeks;
            const auto label = e.baseline ? std::string("Original venue")
                                          : std::string("Game week: ") +
                                                PffGameTime::formatGameTimeEastern(e.startsUtc) + " to " +
                                                PffGameTime::formatGameTimeEastern(e.endsUtc) +
                                                (e.closed ? " (closed)" : " (current)");
            m_historyLines.push_back(label);
            m_historyLines.push_back(e.openingName + " / " + e.openingSurface.value_or("Unknown surface") +
                                     " -> " + e.finalName + " / " +
                                     e.finalSurface.value_or("Unknown surface"));
            if (!e.baseline && !e.hasNetChange)
                m_historyLines.push_back("No net change this game week.");
        }
        if (weeks < 100)
            break;
        offset += weeks;
    }
    m_choices->clearItems();
    for (const auto& line : m_historyLines)
        m_choices->addItem(line);
    m_modal = Modal::History;
    m_root.clearPointerState();
    m_root.clearChildFocus();
    m_modalRoot.resetFocus();
}
void TeamDataScreen::closeModal()
{
    m_modal = Modal::None;
    m_modalRoot.resetFocus();
    m_root.resetFocus();
}
bool TeamDataScreen::save()
{
    if (!sameWorld())
    {
        m_message = "The active league changed. Your draft was not saved.";
        return false;
    }
    if (m_stale)
    {
        m_message = "The league changed. Cancel Changes to reload; your draft is retained.";
        return false;
    }
    const auto saved = saveTeamData(m_actor, draft());
    if (!saved.success)
    {
        m_message = saved.message;
        return false;
    }
    if (m_commit)
        m_commit(saved.value);
    reload();
    m_message = "Changes saved.";
    return true;
}
void TeamDataScreen::leave()
{
    const auto dispatch = m_dispatch;
    if (dispatch)
        dispatch("nav:back");
}
void TeamDataScreen::processAction()
{
    const auto action = m_action;
    m_action = Action::None;
    if (action == Action::Return)
    {
        if (dirty())
        {
            m_modal = Modal::Leave;
            m_modalRoot.resetFocus();
            m_root.resetFocus();
        }
        else
            leave();
        return;
    }
    if (action == Action::Save)
    {
        save();
        return;
    }
    if (action == Action::Cancel)
    {
        reload();
        return;
    }
    if (action == Action::SaveLeave)
    {
        if (save())
            leave();
        return;
    }
    if (action == Action::DiscardLeave)
    {
        leave();
        return;
    }
    if (action == Action::CloseModal)
    {
        closeModal();
        return;
    }
    if (action == Action::PreviousPage)
    {
        if (m_page)
            --m_page;
        loadPeoplePage();
        return;
    }
    if (action == Action::NextPage)
    {
        ++m_page;
        loadPeoplePage();
        return;
    }
    if (action == Action::NewPerson)
    {
        m_modal = Modal::NewPerson;
        for (auto* p : m_personInputs)
            p->clear();
        const auto pending = std::find_if(m_staffDraft.begin(), m_staffDraft.end(), [&](const auto& a) {
            return a.positionId == m_saved.staff[m_role].positionId;
        });
        if (pending != m_staffDraft.end() && pending->newPerson)
        {
            const auto& n = *pending->newPerson;
            m_personInputs[0]->setText(n.displayName);
            m_personInputs[1]->setText(n.firstName.value_or(""));
            m_personInputs[2]->setText(n.lastName.value_or(""));
        }
        m_modalRoot.resetFocus();
        return;
    }
    if (action == Action::KeepPerson)
    {
        const auto pos = m_saved.staff[m_role].positionId;
        m_staffDraft.erase(std::remove_if(m_staffDraft.begin(), m_staffDraft.end(),
                                          [&](const auto& a) { return a.positionId == pos; }),
                           m_staffDraft.end());
        m_draftNames.erase(pos);
        closeModal();
        synchronize();
        return;
    }
    if (action == Action::UsePerson || action == Action::RemovePerson || action == Action::CreatePerson)
    {
        WorldStaffAction choice;
        choice.positionId = m_saved.staff[m_role].positionId;
        choice.expectedAssignmentId = m_saved.staff[m_role].assignmentId;
        if (action == Action::RemovePerson)
            choice.kind = WorldStaffActionKind::Remove;
        else
        {
            choice.kind = WorldStaffActionKind::Replace;
            if (action == Action::UsePerson)
            {
                const auto index = m_choices->selectedIndex();
                if (!index || *index >= m_people.size())
                {
                    m_message = "Select an available person first.";
                    return;
                }
                choice.personId = m_people[*index].personId;
                m_draftNames[choice.positionId] = m_people[*index].displayName;
            }
            else
            {
                if (CoreManifest::trim(m_personInputs[0]->text()).empty())
                {
                    m_message = "Enter a person display name.";
                    return;
                }
                WorldNewPerson person;
                person.displayName = m_personInputs[0]->text();
                if (!m_personInputs[1]->empty())
                    person.firstName = m_personInputs[1]->text();
                if (!m_personInputs[2]->empty())
                    person.lastName = m_personInputs[2]->text();
                choice.newPerson = person;
            }
        }
        m_staffDraft.erase(std::remove_if(m_staffDraft.begin(), m_staffDraft.end(),
                                          [&](const auto& a) { return a.positionId == choice.positionId; }),
                           m_staffDraft.end());
        if (choice.kind != WorldStaffActionKind::Remove || choice.expectedAssignmentId)
            m_staffDraft.push_back(choice);
        closeModal();
        synchronize();
    }
}
bool TeamDataScreen::handleEvent(const Input::Event& event)
{
    refreshContext();
    if (const auto* c = event.asCommand())
    {
        if (c->command.code == Input::CommandCode::Back || c->command.code == Input::CommandCode::Cancel ||
            c->command.code == Input::CommandCode::Close)
        {
            m_action = m_modal == Modal::None ? Action::Return : Action::CloseModal;
            processAction();
            return true;
        }
        if (c->command.code == Input::CommandCode::Quit && dirty())
        {
            m_modal = Modal::Leave;
            m_modalRoot.resetFocus();
            return true;
        }
        if (c->command.code == Input::CommandCode::Refresh)
        {
            if (!dirty())
                reload();
            else
                m_message = "Cancel Changes to reload; your draft is retained.";
            return true;
        }
    }
    if (m_modal == Modal::None && m_contentBounds.size.height > 0 && m_contentBounds.size.height < 12)
    {
        if (const auto* c = event.asCommand())
        {
            if (c->command.code == Input::CommandCode::PageUp ||
                c->command.code == Input::CommandCode::PageDown)
            {
                scrollForm(c->command.code == Input::CommandCode::PageUp
                               ? -std::max(1, m_contentBounds.size.height - 1)
                               : std::max(1, m_contentBounds.size.height - 1));
                return true;
            }
            if ((c->command.code == Input::CommandCode::NextFocus ||
                 c->command.code == Input::CommandCode::PreviousFocus) &&
                focusForm(c->command.code == Input::CommandCode::PreviousFocus))
                return true;
        }
        if (const auto* mouse = event.asMouse())
            if (mouse->isWheel() && m_contentBounds.contains(mouse->position.x, mouse->position.y))
            {
                scrollForm(mouse->button == Input::MouseButton::WheelUp || mouse->wheelDelta > 0 ? -3 : 3);
                return true;
            }
    }
    synchronizeModal();
    if (m_modal == Modal::Staff && m_modalRoot.focusedChild() == m_choices)
        if (const auto* c = event.asCommand())
            if (c->command.code == Input::CommandCode::Confirm)
            {
                m_action = Action::UsePerson;
                processAction();
                return true;
            }
    auto& root = m_modal == Modal::None ? m_root : m_modalRoot;
    const bool handled = root.handleEvent(event);
    processAction();
    return handled;
}
void TeamDataScreen::update(const Animation::TickEvent& tick)
{
    refreshContext();
    if (m_modal == Modal::None)
        m_root.update(tick);
    else
        m_modalRoot.update(tick);
}
std::array<Widget*, 9> TeamDataScreen::formFields() const
{
    return {m_inputs[0],       m_inputs[1],       m_staffButtons[0], m_staffButtons[1], m_staffButtons[2],
            m_staffButtons[3], m_staffButtons[4], m_inputs[2],       m_inputs[3]};
}
std::array<int, 9> TeamDataScreen::formRows() const
{
    const int gap = m_borderedForms || m_contentBounds.size.height >= 14 ? 1 : 0;
    const int height = m_borderedForms ? 17 : 12 + 2 * gap;
    const int top = std::max(0, (m_contentBounds.size.height - height) / 2);
    auto rows = m_borderedForms ? std::array<int, 9>{1, 2, 6, 7, 8, 9, 10, 14, 15}
                               : std::array<int, 9>{1, 2, 4 + gap, 5 + gap, 6 + gap,
                                                    7 + gap, 8 + gap, 10 + 2 * gap, 11 + 2 * gap};
    for (auto& row : rows)
        row += top;
    return rows;
}
void TeamDataScreen::scrollForm(int rows)
{
    m_formOffset = std::clamp(m_formOffset + rows, 0, std::max(0, 12 - m_contentBounds.size.height));
    m_root.clearPointerState();
    layout(m_screenBounds, m_contentBounds, m_controlBounds, m_footerBounds);
    if (auto* focused = m_root.focusedChild())
        if (!focused->canReceiveFocus())
            m_root.clearChildFocus();
}
bool TeamDataScreen::focusForm(bool previous)
{
    if (m_contentBounds.size.height <= 0 || m_contentBounds.size.height >= 12 || !m_root.focusedChild())
        return false;
    const auto fields = formFields();
    const auto rows = formRows();
    const int count = static_cast<int>(m_root.childCount()),
              start = m_root.indexOfChild(*m_root.focusedChild());
    for (int step = 1; step <= count; ++step)
    {
        auto* candidate = m_root.childAt((start + (previous ? -step : step) + count) % count);
        if (!candidate->isEnabled() || !candidate->isFocusable())
            continue;
        const auto field = std::find(fields.begin(), fields.end(), candidate);
        if (field != fields.end())
        {
            const int row = rows[std::distance(fields.begin(), field)];
            int offset = m_formOffset;
            if (row < offset)
                offset = row;
            else if (row >= offset + m_contentBounds.size.height)
                offset = row - m_contentBounds.size.height + 1;
            scrollForm(offset - m_formOffset);
        }
        else if (!candidate->isVisible())
            continue;
        m_root.clearPointerState();
        return m_root.setFocusedChild(*candidate);
    }
    return false;
}
void TeamDataScreen::layout(const Rect& screen, const Rect& content, const Rect& controls, const Rect& footer)
{
    m_screenBounds = screen;
    m_contentBounds = content;
    m_controlBounds = controls;
    m_footerBounds = footer;
    m_borderedForms = content.size.height >= 17;
    m_formOffset = std::clamp(m_formOffset, 0, std::max(0, 12 - content.size.height));
    m_root.setBounds(screen);
    const int labelWidth = std::min(24, std::max(1, content.size.width - 12)),
              fieldX = content.position.x + 4 + labelWidth;
    const int fieldWidth = std::max(1, content.size.width - labelWidth - 8);
    const auto fields = formFields();
    const auto rows = formRows();
    for (std::size_t i = 0; i < fields.size(); ++i)
    {
        const int row = rows[i] - m_formOffset;
        fields[i]->setVisible(row >= 0 && row < content.size.height);
        fields[i]->setBounds(Rect{Point{fieldX, content.position.y + row}, Size{fieldWidth, 1}});
    }
    // Keep the established controls at their original positions when space allows.
    // Short panes use the same controls with tighter vertical spacing.
    const int buttonHeight = controls.size.height >= 17 ? 3 : 1;
    const std::array<int, 4> controlRows = controls.size.height >= 22 ? std::array<int, 4>{2, 7, 11, 15}
                                           : buttonHeight == 3        ? std::array<int, 4>{1, 4, 7, 10}
                                                                      : std::array<int, 4>{1, 2, 3, 4};
    const std::array<Widget*, 4> buttons = {m_manage, m_save, m_cancel, m_history};
    for (std::size_t i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds(Rect{Point{controls.position.x + 2, controls.position.y + controlRows[i]},
                                   Size{std::max(1, controls.size.width - 4), buttonHeight}});
    m_return->setBounds(Rect{
        Point{footer.position.x + std::max(0, footer.size.width - 38), footer.position.y + 1}, Size{28, 3}});

}
void TeamDataScreen::synchronizeModal()
{
    const bool staff = m_modal == Modal::Staff, person = m_modal == Modal::NewPerson,
               history = m_modal == Modal::History, leave = m_modal == Modal::Leave;
    m_choices->setVisible(staff || history);
    m_choices->setEnabled(staff || history);
    for (auto* p : {m_use, m_remove, m_new, m_keep, m_previous, m_next})
        p->setVisible(staff);
    m_remove->setEnabled(staff && m_available && m_saved.staff[m_role].assignmentId != 0);
    for (auto* p : m_personInputs)
        p->setVisible(person);
    m_create->setVisible(person);
    m_modalCancel->setVisible(staff || person || history);
    m_modalCancel->setLabel(history ? "Close" : "Cancel");
    for (auto* p : {m_saveLeave, m_discardLeave, m_stay})
        p->setVisible(leave);
    if (auto* focused = m_modalRoot.focusedChild())
        if (!focused->canReceiveFocus())
            m_modalRoot.clearChildFocus();
}
void TeamDataScreen::draw(Surface& surface)
{
    // Page background, banner and normal footer/instruction text.
    const Style& headerOuter = PFF_Colors::HeaderOuterColor;
    const Style& headerInner = PFF_Colors::HeaderInnerColor;
    const Style& background = PFF_Colors::MainBackgroundColor;
    const Style& title = PFF_Colors::ProFootballTitleColor;
    const Style& text = PFF_Colors::TextHighlightColor;
    // Form panels, borders, headings and field labels.
    const Style& panel = PFF_Colors::FormPanelColor;
    const Style& border = PFF_Colors::FormBorderColor;
    const Style& heading = PFF_Colors::FormPanelHeadingColor;
    const Style& label = PFF_Colors::FormPanelHeadingColor;
    // Modal panels and their messages are separate from normal field styling.
    const Style& modalPanel = PFF_Colors::FormPanelColor;
    const Style& modalBorder = PFF_Colors::FormBorderColor;
    const Style& message = PFF_Colors::TextHighlightColor;
    refreshContext();
    auto& buffer = surface.buffer();
    buffer.clear(background);
    Composition::PageComposer page(buffer);
    page.createFullScreenRegion("Screen");
    const auto screen = page.resolveRegion("Screen");
    const auto [header, rest] = page.splitTop(screen, 12);
    const auto [footer, body] = page.splitBottom(rest, 5);
    const auto [controls, content] = page.splitRight(body, std::min(34, std::max(28, body.size.width / 3)));
    page.createRegion("Header", header);
    page.createInsetRegion("HeaderInner", "Header", 2, 1, 2, 1);
    page.createInsetRegion("Title", "HeaderInner", 4, 2, 2, 1);
    page.drawPanel("Header", headerOuter, headerOuter, ObjectFactory::doubleLineBorder());
    buffer.fillRect(page.resolveRegion("HeaderInner"), U' ', headerInner);
    const auto titleArea = page.resolveRegion("Title");
    if (!m_banner.isEmpty() && m_banner.getWidth() <= titleArea.size.width &&
        m_banner.getHeight() <= titleArea.size.height)
        page.writeObjectInRegion(m_banner, "Title", Composition::Align::center());
    else
        page.writeLine(titleArea, 0,
                       std::string(std::max(0, (titleArea.size.width - 9) / 2), ' ') + "Team Data", title);
    synchronize();
    layout(screen, content, controls, footer);
    const Rect footerText{Point{footer.position.x + 2, footer.position.y + 1},
                          Size{std::max(0, footer.size.width - 42), 2}};
    page.writeLine(footerText, 0, TextClip::clipUtf8Text(m_selectedWorld.worldName, footerText.size.width),
                   text);
    page.writeLine(
        footerText, 1,
        TextClip::clipUtf8Text(PffGameTime::formatGameTimeEastern(
                                   m_available ? m_saved.currentTimeUtc : m_selectedWorld.currentTimeUtc),
                               footerText.size.width),
        text);
    {
        const auto group = [&](const char* key, const char* caption, int y, int height) {
            page.createRegion(key, Rect{Point{content.position.x + 2, content.position.y + y},
                                        Size{std::max(0, content.size.width - 4), height}});
            page.drawPanel(key, panel, border, ObjectFactory::singleLineBorder());
            page.writePanelTitle(key, caption, heading, 3, 0);
        };
        const int top = formRows()[0] - 1;
        if (m_borderedForms)
        {
            group("Location", " Location ", top, 4);
            group("Leadership", " Leadership ", top + 5, 7);
            group("Venue", " Venue ", top + 13, 4);
        }
        else
        {
            const std::array<const char*, 3> captions = {"Location", "Leadership", "Venue"};
            const int gap = content.size.height >= 14 ? 1 : 0;
            const std::array<int, 3> rows = {top, top + 3 + gap, top + 9 + 2 * gap};
            for (std::size_t i = 0; i < captions.size(); ++i)
                page.writeLine(content, rows[i] - m_formOffset, "  " + std::string(captions[i]), heading);
        }
        const int labelWidth = std::min(24, std::max(1, content.size.width - 12));
        const Rect labels{Point{content.position.x + 4, content.position.y},
                          Size{labelWidth, content.size.height}};
        const std::array<const char*, 9> captions = {"City",        "Team",        RoleLabels[0],
                                                     RoleLabels[1], RoleLabels[2], RoleLabels[3],
                                                     RoleLabels[4], "Stadium",     "Surface"};
        const auto rows = formRows();
        for (std::size_t i = 0; i < rows.size(); ++i)
            page.writeLine(labels, rows[i] - m_formOffset, TextClip::clipUtf8Text(captions[i], labelWidth),
                           label);
        page.writeLine(
            controls, 0,
            m_available ? (m_saved.godMode ? "God mode: ON" : "") : "Team unavailable", heading);
        std::string status = m_message;
        if (status.empty())
        {
            if (m_available && !m_permissions.calendarReady)
                status = "Venue edits need an installed league calendar.";
            else if (m_available && m_permissions.otherVenueUsers && !m_saved.godMode)
                status = "Shared Stadium and Surface are read-only.";
            else if (dirty() && m_management && !m_permissions.canEditIdentity && !m_saved.godMode)
                status = "No controlled owner authority. Use Commissioner God mode to assign staff.";
            else if (!dirty())
                status = "Changes are saved to this league.";
            if (dirty())
                status += " Unsaved changes.";
        }
        int row = controls.size.height >= 22 ? 20 : controls.size.height >= 17 ? 14 : 6;
        if (content.size.height < 12)
            status = "PgUp/PgDn scrolls fields. " + status;
        for (const auto& line : wrapText(status, std::max(1, controls.size.width - 4)))
            if (row < controls.size.height)
                page.writeLine(controls, row++, "  " + line, message);
        m_root.draw(surface);
    }
    if (m_modal == Modal::None)
        return;
    synchronizeModal();
    const Rect dialogArea = (body.size.height < 35 || body.size.width < 70) ? screen : body;
    const int w = std::max(1, std::min(100, dialogArea.size.width - 6)),
              h = std::max(1, std::min(33, dialogArea.size.height - 2));
    const Rect modal{Point{dialogArea.position.x + (dialogArea.size.width - w) / 2,
                           dialogArea.position.y + (dialogArea.size.height - h) / 2},
                     Size{w, h}};
    page.createRegion("Dialog", modal);
    page.drawPanel("Dialog", modalPanel, modalBorder, ObjectFactory::doubleLineBorder());
    std::string caption = m_modal == Modal::Staff       ? std::string(" Change ") + RoleLabels[m_role] + " "
                          : m_modal == Modal::NewPerson ? " Create Person "
                          : m_modal == Modal::History   ? " Venue History "
                                                        : " Unsaved Changes ";
    page.writePanelTitle("Dialog", caption, heading, 3, 0);
    m_modalRoot.setBounds(screen);
    const auto rect = [&](int x, int y, int width, int height) {
        return Rect{Point{modal.position.x + x, modal.position.y + y},
                    Size{std::max(1, width), std::max(1, height)}};
    };
    if (m_modal == Modal::Staff || m_modal == Modal::History)
    {
        m_choices->setBounds(rect(3, 4, w - 6, m_modal == Modal::Staff ? h - 22 : h - 10));
        if (m_modal == Modal::History && m_historyWrapWidth != m_choices->width())
        {
            m_historyWrapWidth = m_choices->width();
            m_choices->clearItems();
            for (const auto& line : m_historyLines)
                for (const auto& wrapped : wrapText(line, std::max(1, m_choices->width() - 1)))
                    m_choices->addItem(wrapped);
        }
        if (m_modal == Modal::Staff)
        {
            page.writeLine(modal, 2,
                           TextClip::clipUtf8Text("   Current: " + (m_saved.staff[m_role].assignmentId
                                                                        ? m_saved.staff[m_role].personName
                                                                        : std::string("Unassigned")),
                                                  w - 2),
                           text);
            page.writeLine(modal, 3, "   Available people without roles; Enter selects into the draft.",
                           text);
            m_use->setBounds(rect(3, h - 14, (w - 9) / 2, 3));
            m_remove->setBounds(rect(w / 2, h - 14, w / 2 - 3, 3));
            m_new->setBounds(rect(3, h - 10, (w - 9) / 2, 3));
            m_keep->setBounds(rect(w / 2, h - 10, w / 2 - 3, 3));
            m_previous->setBounds(rect(3, h - 6, 25, 3));
            m_next->setBounds(rect(30, h - 6, 25, 3));
            m_modalCancel->setBounds(rect(w - 29, h - 6, 26, 3));
            page.writeLine(modal, h - 2,
                           "   Page " + std::to_string(m_page + 1) + ". Save Changes commits the draft.",
                           text);
        }
        else
        {
            page.writeLine(modal, 2,
                           "   Original venue and weekly summaries; unknown surface values are preserved.",
                           text);
            m_modalCancel->setBounds(rect(w - 29, h - 5, 26, 3));
        }
    }
    else if (m_modal == Modal::NewPerson)
    {
        const std::array<const char*, 3> names = {"Display name", "First name (optional)",
                                                  "Last name (optional)"};
        for (std::size_t i = 0; i < 3; ++i)
        {
            page.writeLine(modal, 3 + static_cast<int>(i) * 4, "   " + std::string(names[i]), label);
            m_personInputs[i]->setBounds(rect(3, 4 + static_cast<int>(i) * 4, w - 6, 1));
        }
        page.writeLine(modal, 17, "   This creates a new person only when Save Changes succeeds.", text);
        m_create->setBounds(rect(3, h - 5, 28, 3));
        m_modalCancel->setBounds(rect(w - 31, h - 5, 28, 3));
    }
    else
    {
        page.writeLine(modal, 3, "   You have unsaved changes. Save them, discard them, or stay here.", text);
        m_saveLeave->setBounds(rect(3, 7, w - 6, 3));
        m_discardLeave->setBounds(rect(3, 11, w - 6, 3));
        m_stay->setBounds(rect(3, 15, w - 6, 3));
    }
    if (!m_message.empty())
    {
        const auto lines = wrapText(m_message, std::max(1, w - 6));
        for (std::size_t i = 0; i < lines.size() && i < 3; ++i)
            page.writeLine(modal,
                           (m_modal == Modal::Staff   ? h - 18
                            : m_modal == Modal::Leave ? 20
                                                      : 20) +
                               static_cast<int>(i),
                           "   " + lines[i], message);
    }
    m_modalRoot.draw(surface);
}
