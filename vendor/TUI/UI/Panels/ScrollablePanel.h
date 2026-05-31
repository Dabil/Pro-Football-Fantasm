#pragma once

#include "Core/Rect.h"
#include "Core/Size.h"
#include "Rendering/Surface.h"
#include "UI/Base/Viewport.h"
#include "UI/Panels/Panel.h"

class ScrollablePanel : public Panel
{
public:
    ScrollablePanel();
    explicit ScrollablePanel(const Rect& bounds);
    ScrollablePanel(const Rect& bounds, std::string title);

    Viewport& viewport();
    const Viewport& viewport() const;

    void setContentSize(Size size);
    void setContentSize(int width, int height);

    void setViewSize(Size size);
    void setViewSize(int width, int height);

    Rect viewportBounds() const;
    Rect visibleContentRect() const;

    bool scrollUp(int lines = 1);
    bool scrollDown(int lines = 1);
    bool scrollLeft(int columns = 1);
    bool scrollRight(int columns = 1);

    bool pageUp();
    bool pageDown();

    bool home();
    bool end();

    void draw(Surface& surface) override;

protected:
    virtual void drawScrollableContent(
        Surface& surface,
        const Rect& visibleContentRect);

    Point contentToViewport(Point contentPoint) const;
    Point viewportToContent(Point viewportPoint) const;

private:
    void syncViewportToPanel();
    bool scrollToAndReportChange(int x, int y);
    void blitViewportSurface(Surface& target, const Surface& source, const Rect& targetRect) const;

private:
    Viewport m_viewport;
};