#pragma once

#include "Core/Point.h"
#include "Core/Rect.h"
#include "Rendering/Surface.h"

class LayerInstance
{
public:
    LayerInstance();
    LayerInstance(int width, int height);
    LayerInstance(int width, int height, Point position, int zOrder = 0, bool visible = true);
    LayerInstance(Surface surface, Point position = {}, int zOrder = 0, bool visible = true);

    Surface& surface();
    const Surface& surface() const;

    ScreenBuffer& buffer();
    const ScreenBuffer& buffer() const;

    int width() const;
    int height() const;
    bool isEmpty() const;

    Point position() const;
    void setPosition(Point position);
    void setPosition(int x, int y);

    int x() const;
    int y() const;
    void setX(int x);
    void setY(int y);
    void moveBy(int dx, int dy);

    int zOrder() const;
    void setZOrder(int zOrder);

    bool isVisible() const;
    void setVisible(bool visible);
    void show();
    void hide();

    Rect bounds() const;
    void resize(int width, int height);

private:
    Surface m_surface;
    Point m_position;
    int m_zOrder = 0;
    bool m_visible = true;
};

