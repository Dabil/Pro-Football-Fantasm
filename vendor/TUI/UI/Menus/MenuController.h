#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>

#include "UI/Menus/MenuModel.h"
#include "UI/Menus/MenuView.h"

namespace UI::Menus
{
    enum class MenuControllerAction
    {
        None,
        SelectionChanged,
        Confirmed,
        Cancelled
    };

    struct MenuControllerResult
    {
        MenuControllerAction action = MenuControllerAction::None;
        std::optional<std::size_t> selectedIndex;
        std::string commandId;

        bool handled() const;
        bool selectionChanged() const;
        bool confirmed() const;
        bool cancelled() const;
    };

    class MenuController
    {
    public:
        using SelectionChangedCallback = std::function<void(std::size_t, const MenuItem&)>;

        MenuController() = default;
        explicit MenuController(MenuModel& model);
        MenuController(MenuModel& model, MenuView& view);

        MenuModel* model();
        const MenuModel* model() const;
        void setModel(MenuModel* model);
        void setModel(MenuModel& model);
        void clearModel();

        MenuView* view();
        const MenuView* view() const;
        void setView(MenuView* view);
        void setView(MenuView& view);
        void clearView();

        MenuOrientation orientation() const;
        void setOrientation(MenuOrientation orientation);

        MenuNavigationMode navigationMode() const;
        void setNavigationMode(MenuNavigationMode mode);

        std::optional<std::size_t> selectedIndex() const;
        const MenuItem* selectedItem() const;
        std::string selectedCommandId() const;

        void setOnSelectionChanged(SelectionChangedCallback callback);
        void clearOnSelectionChanged();

        MenuControllerResult selectFirstEnabled();
        MenuControllerResult selectLastEnabled();
        MenuControllerResult setSelectedIndex(std::size_t index);
        MenuControllerResult clearSelection();

        MenuControllerResult next();
        MenuControllerResult previous();
        MenuControllerResult up();
        MenuControllerResult down();
        MenuControllerResult left();
        MenuControllerResult right();
        MenuControllerResult confirm() const;
        MenuControllerResult cancel() const;

    private:
        MenuControllerResult moveSelection(int direction);
        MenuControllerResult setSelectionInternal(
            std::optional<std::size_t> index,
            bool notifySelectionChanged);

        bool hasValidModel() const;
        bool isValidEnabledIndex(std::size_t index) const;
        std::optional<std::size_t> normalizedSelection() const;
        void syncView() const;
        void syncModelSelectionFlags() const;
        MenuControllerResult makeSelectionChangedResult() const;

    private:
        MenuModel* m_model = nullptr;
        MenuView* m_view = nullptr;
        MenuOrientation m_orientation = MenuOrientation::Vertical;
        MenuNavigationMode m_navigationMode = MenuNavigationMode::Wrap;
        std::optional<std::size_t> m_selectedIndex;
        SelectionChangedCallback m_onSelectionChanged;
    };
}