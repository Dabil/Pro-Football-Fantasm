#pragma once

#include "Animation/TickEvent.h"
#include "Core/Rect.h"

class Surface;

class UIElement
{
public:
    UIElement();
    explicit UIElement(const Rect& bounds);
    virtual ~UIElement();

    const Rect& bounds() const;
    void setBounds(const Rect& bounds);

    Point position() const;
    void setPosition(Point position);
    void setPosition(int x, int y);

    Size size() const;
    void setSize(Size size);
    void setSize(int width, int height);

    int x() const;
    int y() const;
    int width() const;
    int height() const;

    bool isVisible() const;
    void setVisible(bool visible);
    void show();
    void hide();

    bool isEnabled() const;
    void setEnabled(bool enabled);
    void enable();
    void disable();

    virtual void update(const Animation::TickEvent& event);

    virtual void draw(Surface& surface) = 0;

private:
    Rect m_bounds;
    bool m_visible = true;
    bool m_enabled = true;
};