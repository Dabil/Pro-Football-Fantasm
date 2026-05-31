#pragma once

#include <memory>
#include <string>

#include "Core/Point.h"
#include "Core/Rect.h"
#include "Rendering/Styles/Style.h"
#include "UI/Content/IWindowContent.h"
#include "UI/Interaction/WindowInteraction.h"
#include "UI/Panels/Panel.h"

class Window : public Panel
{
public:
    Window();
    explicit Window(const Rect& bounds);
    Window(const Rect& bounds, std::string title);
    Window(const Rect& bounds, std::string title, bool modal);

    bool isModal() const;
    void setModal(bool modal);

    bool isDraggable() const;
    void setDraggable(bool draggable);

    bool isResizable() const;
    void setResizable(bool resizable);

    Size minimumSize() const;
    void setMinimumSize(Size size);
    void setMinimumSize(int width, int height);

    int resizeBorderThickness() const;
    void setResizeBorderThickness(int thickness);

    int titleBarHeight() const;
    void setTitleBarHeight(int height);

    const Style& hoveredBorderStyle() const;
    void setHoveredBorderStyle(const Style& style);

    const Style& hoveredTitleStyle() const;
    void setHoveredTitleStyle(const Style& style);

    virtual bool hasTransferableContent() const;
    virtual std::unique_ptr<UI::IWindowContent> releaseContent();

    UI::CursorRegion hitTest(Point screenPosition) const;
    bool containsScreenPoint(Point screenPosition) const;

    bool isHovered() const;
    void setHovered(bool hovered);

    void draw(Surface& surface) override;

private:
    bool m_modal = false;
    bool m_draggable = true;
    bool m_resizable = true;
    bool m_hovered = false;
    Size m_minimumSize{ 4, 3 };
    int m_resizeBorderThickness = 1;
    int m_titleBarHeight = 1;

    Style m_hoveredBorderStyle;
    Style m_hoveredTitleStyle;
};