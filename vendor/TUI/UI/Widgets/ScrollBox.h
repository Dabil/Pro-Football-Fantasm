#pragma once

#include "Core/Rect.h"
#include "Rendering/Styles/Style.h"
#include "UI/Base/Viewport.h"
#include "UI/Scrolling/Scrollbar.h"
#include "UI/Widgets/ContainerWidget.h"
#include "UI/Widgets/WidgetStyle.h"

class Surface;

namespace Input
{
    class Event;
    struct MouseEvent;
}

class ScrollBox : public ContainerWidget
{
public:
    ScrollBox();
    explicit ScrollBox(const Rect& bounds);

    Viewport& viewport();
    const Viewport& viewport() const;

    void setContentSize(Size size);
    void setContentSize(int width, int height);

    bool autoSizesToChildren() const;
    void setAutoSizesToChildren(bool enabled);

    bool isVerticalScrollbarVisible() const;
    void setVerticalScrollbarVisible(bool visible);

    bool reservesVerticalScrollbarColumn() const;
    void setReserveVerticalScrollbarColumn(bool reserveColumn);

    const UI::Scrolling::VerticalScrollbarStyle& verticalScrollbarStyle() const;
    void setVerticalScrollbarStyle(const UI::Scrolling::VerticalScrollbarStyle& style);

    const WidgetStyles::StyleSet& styleSet() const;
    void setStyleSet(const WidgetStyles::StyleSet& styleSet);

    void draw(Surface& surface) override;
    bool handleEvent(const Input::Event& event) override;

private:
    void updateViewport();
    Size calculatedContentSize() const;

    Rect viewportBounds() const;
    Rect scrollbarBounds() const;

    bool shouldDrawVerticalScrollbar() const;
    void drawScrollbarIfNeeded(Surface& surface);

    bool handleMouseEvent(const Input::Event& event);
    bool dispatchTranslatedMouseEvent(const Input::MouseEvent& mouseEvent);

    Point screenToContent(Point screenPoint) const;
    Widget* hitTestContent(Point contentPoint);

    bool canDispatchToChild(const Widget& child) const;

private:
    Viewport m_viewport;
    Size m_minimumContentSize{ 0, 0 };
    bool m_autoSizeToChildren = true;

    bool m_verticalScrollbarVisible = true;
    bool m_reserveVerticalScrollbarColumn = true;
    UI::Scrolling::VerticalScrollbarStyle m_verticalScrollbarStyle;

    WidgetStyles::StyleSet m_styleSet;

    Widget* m_mouseCaptureChild = nullptr;
};