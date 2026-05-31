#include "UI/Menus/MenuController.h"

#include <utility>

namespace UI::Menus
{
    bool MenuControllerResult::handled() const
    {
        return action != MenuControllerAction::None;
    }

    bool MenuControllerResult::selectionChanged() const
    {
        return action == MenuControllerAction::SelectionChanged;
    }

    bool MenuControllerResult::confirmed() const
    {
        return action == MenuControllerAction::Confirmed;
    }

    bool MenuControllerResult::cancelled() const
    {
        return action == MenuControllerAction::Cancelled;
    }

    MenuController::MenuController(MenuModel& model)
    {
        setModel(model);
    }

    MenuController::MenuController(MenuModel& model, MenuView& view)
    {
        setView(view);
        setModel(model);
    }

    MenuModel* MenuController::model()
    {
        return m_model;
    }

    const MenuModel* MenuController::model() const
    {
        return m_model;
    }

    void MenuController::setModel(MenuModel* model)
    {
        m_model = model;

        if (m_view != nullptr)
        {
            m_view->setModel(m_model);
        }

        if (!hasValidModel())
        {
            m_selectedIndex.reset();
            syncView();
            syncModelSelectionFlags();
            return;
        }

        const std::optional<std::size_t> normalized = normalizedSelection();
        if (normalized.has_value())
        {
            m_selectedIndex = normalized;
        }
        else
        {
            m_selectedIndex = m_model->firstEnabledIndex();
        }

        syncView();
        syncModelSelectionFlags();
    }

    void MenuController::setModel(MenuModel& model)
    {
        setModel(&model);
    }

    void MenuController::clearModel()
    {
        m_model = nullptr;
        m_selectedIndex.reset();

        if (m_view != nullptr)
        {
            m_view->clearModel();
        }
    }

    MenuView* MenuController::view()
    {
        return m_view;
    }

    const MenuView* MenuController::view() const
    {
        return m_view;
    }

    void MenuController::setView(MenuView* view)
    {
        m_view = view;

        if (m_view == nullptr)
        {
            return;
        }

        m_view->setOrientation(m_orientation);
        m_view->setModel(m_model);
        syncView();
    }

    void MenuController::setView(MenuView& view)
    {
        setView(&view);
    }

    void MenuController::clearView()
    {
        if (m_view != nullptr)
        {
            m_view->clearSelectedIndex();
        }

        m_view = nullptr;
    }

    MenuOrientation MenuController::orientation() const
    {
        return m_orientation;
    }

    void MenuController::setOrientation(MenuOrientation orientation)
    {
        m_orientation = orientation;

        if (m_view != nullptr)
        {
            m_view->setOrientation(orientation);
        }
    }

    MenuNavigationMode MenuController::navigationMode() const
    {
        return m_navigationMode;
    }

    void MenuController::setNavigationMode(MenuNavigationMode mode)
    {
        m_navigationMode = mode;
    }

    std::optional<std::size_t> MenuController::selectedIndex() const
    {
        return m_selectedIndex;
    }

    const MenuItem* MenuController::selectedItem() const
    {
        if (!hasValidModel() || !m_selectedIndex.has_value())
        {
            return nullptr;
        }

        return m_model->itemAt(m_selectedIndex.value());
    }

    std::string MenuController::selectedCommandId() const
    {
        const MenuItem* item = selectedItem();
        return item != nullptr ? item->commandId() : std::string();
    }

    void MenuController::setOnSelectionChanged(SelectionChangedCallback callback)
    {
        m_onSelectionChanged = std::move(callback);
    }

    void MenuController::clearOnSelectionChanged()
    {
        m_onSelectionChanged = nullptr;
    }

    MenuControllerResult MenuController::selectFirstEnabled()
    {
        if (!hasValidModel())
        {
            return clearSelection();
        }

        return setSelectionInternal(m_model->firstEnabledIndex(), true);
    }

    MenuControllerResult MenuController::selectLastEnabled()
    {
        if (!hasValidModel())
        {
            return clearSelection();
        }

        return setSelectionInternal(m_model->lastEnabledIndex(), true);
    }

    MenuControllerResult MenuController::setSelectedIndex(std::size_t index)
    {
        if (!isValidEnabledIndex(index))
        {
            return MenuControllerResult{};
        }

        return setSelectionInternal(index, true);
    }

    MenuControllerResult MenuController::clearSelection()
    {
        return setSelectionInternal(std::nullopt, true);
    }

    MenuControllerResult MenuController::next()
    {
        return moveSelection(1);
    }

    MenuControllerResult MenuController::previous()
    {
        return moveSelection(-1);
    }

    MenuControllerResult MenuController::up()
    {
        if (m_orientation != MenuOrientation::Vertical)
        {
            return MenuControllerResult{};
        }

        return previous();
    }

    MenuControllerResult MenuController::down()
    {
        if (m_orientation != MenuOrientation::Vertical)
        {
            return MenuControllerResult{};
        }

        return next();
    }

    MenuControllerResult MenuController::left()
    {
        if (m_orientation != MenuOrientation::Horizontal)
        {
            return MenuControllerResult{};
        }

        return previous();
    }

    MenuControllerResult MenuController::right()
    {
        if (m_orientation != MenuOrientation::Horizontal)
        {
            return MenuControllerResult{};
        }

        return next();
    }

    MenuControllerResult MenuController::confirm() const
    {
        const MenuItem* item = selectedItem();

        if (item == nullptr || !item->isEnabled())
        {
            return MenuControllerResult{};
        }

        return MenuControllerResult{
            MenuControllerAction::Confirmed,
            m_selectedIndex,
            item->commandId()
        };
    }

    MenuControllerResult MenuController::cancel() const
    {
        return MenuControllerResult{
            MenuControllerAction::Cancelled,
            m_selectedIndex,
            std::string()
        };
    }

    MenuControllerResult MenuController::moveSelection(int direction)
    {
        if (!hasValidModel())
        {
            return clearSelection();
        }

        if (!m_model->hasEnabledItems())
        {
            return clearSelection();
        }

        if (!m_selectedIndex.has_value() || !isValidEnabledIndex(m_selectedIndex.value()))
        {
            return setSelectionInternal(m_model->firstEnabledIndex(), true);
        }

        std::optional<std::size_t> nextIndex;

        if (direction > 0)
        {
            nextIndex = m_model->nextEnabledIndex(m_selectedIndex.value(), m_navigationMode);
        }
        else if (direction < 0)
        {
            nextIndex = m_model->previousEnabledIndex(m_selectedIndex.value(), m_navigationMode);
        }
        else
        {
            nextIndex = m_selectedIndex;
        }

        if (!nextIndex.has_value())
        {
            return MenuControllerResult{};
        }

        return setSelectionInternal(nextIndex, true);
    }

    MenuControllerResult MenuController::setSelectionInternal(
        std::optional<std::size_t> index,
        bool notifySelectionChanged)
    {
        if (index.has_value() && !isValidEnabledIndex(index.value()))
        {
            return MenuControllerResult{};
        }

        if (m_selectedIndex == index)
        {
            syncView();
            syncModelSelectionFlags();
            return MenuControllerResult{};
        }

        m_selectedIndex = index;
        syncView();
        syncModelSelectionFlags();

        if (notifySelectionChanged && m_onSelectionChanged && m_selectedIndex.has_value())
        {
            const MenuItem* item = selectedItem();
            if (item != nullptr)
            {
                m_onSelectionChanged(m_selectedIndex.value(), *item);
            }
        }

        return makeSelectionChangedResult();
    }

    bool MenuController::hasValidModel() const
    {
        return m_model != nullptr && !m_model->isEmpty();
    }

    bool MenuController::isValidEnabledIndex(std::size_t index) const
    {
        if (!hasValidModel())
        {
            return false;
        }

        const MenuItem* item = m_model->itemAt(index);
        return item != nullptr && item->isEnabled();
    }

    std::optional<std::size_t> MenuController::normalizedSelection() const
    {
        if (!hasValidModel() || !m_selectedIndex.has_value())
        {
            return std::nullopt;
        }

        if (isValidEnabledIndex(m_selectedIndex.value()))
        {
            return m_selectedIndex;
        }

        return std::nullopt;
    }

    void MenuController::syncView() const
    {
        if (m_view == nullptr)
        {
            return;
        }

        m_view->setSelectedIndex(m_selectedIndex);
    }

    void MenuController::syncModelSelectionFlags() const
    {
        if (m_model == nullptr)
        {
            return;
        }

        for (std::size_t index = 0; index < m_model->itemCount(); ++index)
        {
            MenuItem* item = m_model->itemAt(index);
            if (item != nullptr)
            {
                item->setSelected(m_selectedIndex.has_value() && m_selectedIndex.value() == index);
            }
        }
    }

    MenuControllerResult MenuController::makeSelectionChangedResult() const
    {
        return MenuControllerResult{
            MenuControllerAction::SelectionChanged,
            m_selectedIndex,
            selectedCommandId()
        };
    }
}