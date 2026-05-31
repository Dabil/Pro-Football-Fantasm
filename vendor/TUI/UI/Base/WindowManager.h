#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Core/Point.h"
#include "Core/Rect.h"
#include "Rendering/LayerInstance.h"
#include "Rendering/Surface.h"
#include "UI/Layout/DockTarget.h"
#include "UI/Interaction/WindowInteraction.h"
#include "UI/Layout/DockDragPreview.h"
#include "UI/Layout/DockTree.h"
#include "Input/MouseEvent.h"
#include "Animation/TickEvent.h"

namespace Input
{
    class Event;
    struct MouseEvent;
}

class Window;

namespace UI
{
    class TabbedWindow;
    class TabbedWindowPage;
    struct TabbedWindowPageMetadata;
}

class WindowManager
{
public:
    WindowManager();
    ~WindowManager();

    void addWindow(Window& window);
    void removeWindow(Window& window);
    void clear();

    bool contains(const Window& window) const;
    std::size_t count() const;
    bool empty() const;

    void show(Window& window);
    void hide(Window& window);

    void bringToFront(Window& window);
    void sendToBack(Window& window);

    void setZOrder(Window& window, int zOrder);
    int zOrderOf(const Window& window) const;

    Window* topWindow();
    const Window* topWindow() const;

    Window* topModalWindow();
    const Window* topModalWindow() const;

    Window* hoveredWindow();
    const Window* hoveredWindow() const;

    Window* focusedWindow();
    const Window* focusedWindow() const;

    bool hasModalWindow() const;
    bool canRouteTo(const Window& window) const;
    bool handleEvent(const Input::Event& event);
    bool handleMouseEvent(const Input::MouseEvent& mouseEvent);

    UI::WindowHitTestResult hitTest(Point screenPosition);
    UI::WindowHitTestResult hitTest(Point screenPosition) const;

    void capturePointer(Window& window, UI::PointerButton button, Point screenPosition);
    void releasePointer(Window& window);
    void releasePointer();
    bool hasPointerCapture() const;
    const UI::PointerCaptureState& pointerCapture() const;
    Window* capturedWindow();
    const Window* capturedWindow() const;

    bool beginDrag(Window& window, Point screenPosition, UI::PointerButton button = UI::PointerButton::Primary);
    bool beginDragAt(Point screenPosition, UI::PointerButton button = UI::PointerButton::Primary);
    void endDrag();
    bool isDragging() const;
    const UI::WindowDragState& dragState() const;

    bool beginResize(Window& window, UI::CursorRegion region, Point screenPosition, UI::PointerButton button = UI::PointerButton::Primary);
    bool beginResizeAt(Point screenPosition, UI::PointerButton button = UI::PointerButton::Primary);
    bool updateResize(Point screenPosition);
    void endResize();
    bool isResizing() const;
    const UI::WindowResizeState& resizeState() const;

    void update(const Animation::TickEvent& event);
    void draw(Surface& surface);

    std::vector<LayerInstance> buildLayers();

    void setDockTree(UI::DockTree* dockTree);
    UI::DockTree* dockTree();
    const UI::DockTree* dockTree() const;

    bool isDockPreviewActive() const;
    const UI::DockDragPreviewState& dockPreviewState() const;
    void cancelDockPreview();

    bool isTabDetachPreviewActive() const;
    Rect tabDetachPreviewBounds() const;
    void cancelTabDetachPreview();

    std::vector<UI::DockTarget> dockTargets() const;
    UI::DockTarget dockTargetAt(Point screenPosition) const;
    std::vector<UI::DockSnapZone> dockSnapZonesForTargets() const;

private:
    struct ManagedWindow
    {
        Window* window = nullptr;
        int zOrder = 0;
        std::size_t insertionOrder = 0;
    };

    struct TabDetachDragState
    {
        UI::TabbedWindow* sourceWindow = nullptr;
        std::size_t tabIndex = 0;
        std::string contentId;
        Point pointerOrigin{};
        Point currentPointer{};
        Rect sourceBounds{};
        Rect previewBounds{};
        bool active = false;

        void clear()
        {
            sourceWindow = nullptr;
            tabIndex = 0;
            contentId.clear();
            pointerOrigin = Point{};
            currentPointer = Point{};
            sourceBounds = Rect{};
            previewBounds = Rect{};
            active = false;
        }
    };

private:
    ManagedWindow* findEntry(Window& window);
    const ManagedWindow* findEntry(const Window& window) const;

    void sortWindows();
    int nextFrontZOrder() const;
    int nextBackZOrder() const;

    void setHoveredWindow(Window* window);
    void setFocusedWindow(Window* window);

    bool updateDrag(Point screenPosition, const Input::KeyModifiers& modifiers = Input::KeyModifiers{});

    bool beginTabDetachDrag(
        UI::TabbedWindow& sourceWindow,
        std::size_t tabIndex,
        Point screenPosition,
        UI::PointerButton button = UI::PointerButton::Primary);
    bool updateTabDetachDrag(
        Point screenPosition,
        const Input::KeyModifiers& modifiers = Input::KeyModifiers{});
    void endTabDetachDrag();
    bool completeTabDetachDrag();
    bool isTabDetachDragging() const;
    Rect makeTabDetachPreviewBounds(
        const UI::TabbedWindow& sourceWindow,
        Point screenPosition) const;
    void drawTabDetachPreview(Surface& surface) const;

    bool isDockingModifierHeld(const Input::KeyModifiers& modifiers) const;
    void updateDockPreview(Point screenPosition, const Input::KeyModifiers& modifiers);

    bool isDockTargetEligible(const Window& window) const;
    std::string dockContentIdForWindow(const Window& window) const;
    std::vector<UI::DockSnapZone> createDockSnapZonesForTarget(
        const UI::DockTarget& target) const;

    bool tryApplyDockPreviewDrop();
    bool tryApplyCenterDockWindowTabs(Window& draggedWindow, Window& targetWindow);
    bool transferStandaloneWindowToTabbedWindow(
        Window& sourceWindow,
        UI::TabbedWindow& tabbedWindow,
        bool selectNewPage);
    bool transferPagesFromTabbedWindow(
        UI::TabbedWindow& sourceWindow,
        UI::TabbedWindow& targetWindow,
        bool selectTransferredPages);
    UI::TabbedWindow* createOwnedTabbedWindow(
        const Rect& bounds,
        const std::string& title);
    UI::TabbedWindowPage makeTabPageFromWindow(Window& window);
    UI::TabbedWindowPageMetadata makeTabPageMetadataFromWindow(const Window& window) const;
    std::unique_ptr<Window> makeStandaloneWindowFromTabPage(
        UI::TabbedWindowPage page,
        const Rect& bounds);
    Window* collapseOneTabTabbedWindowToStandalone(UI::TabbedWindow& sourceWindow);
    void applyMetadataToWindow(
        Window& window,
        const UI::TabbedWindowPageMetadata& metadata);
    std::string tabTitleForWindow(const Window& window) const;
    bool isSideDockZone(UI::DockSnapZoneType type) const;
    Window* findDockWindowByContentId(const std::string& contentId);
    const Window* findDockWindowByContentId(const std::string& contentId) const;
    bool buildSideDockTreeForWindows(
        const Window& draggedWindow,
        const Window& targetWindow,
        UI::DockSnapZoneType type);
    void applyDockTreeLayoutToWindows();
    void applySideDockWindowBounds(
        Window& draggedWindow,
        Window& targetWindow,
        UI::DockSnapZoneType type);

private:
    std::vector<ManagedWindow> m_windows;
    std::vector<std::unique_ptr<Window>> m_ownedWindows;
    std::vector<std::unique_ptr<UI::TabbedWindow>> m_ownedTabbedWindows;
    std::size_t m_nextInsertionOrder = 0;

    UI::PointerCaptureState m_pointerCapture;
    UI::WindowDragState m_dragState;
    TabDetachDragState m_tabDetachDragState;
    UI::WindowResizeState m_resizeState;
    UI::DockTree* m_dockTree = nullptr;
    UI::DockDragPreview m_dockPreview;

    Window* m_hoveredWindow = nullptr;
    Window* m_focusedWindow = nullptr;
};