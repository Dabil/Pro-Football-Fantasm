#include "Rendering/LayerInstance.h"

#include <utility>

LayerInstance::LayerInstance() = default;

LayerInstance::LayerInstance(int width, int height)
    : m_surface(width, height)
{
}

LayerInstance::LayerInstance(int width, int height, Point position, int zOrder, bool visible)
    : m_surface(width, height)
    , m_position(position)
    , m_zOrder(zOrder)
    , m_visible(visible)
{
}

LayerInstance::LayerInstance(Surface surface, Point position, int zOrder, bool visible)
    : m_surface(std::move(surface))
    , m_position(position)
    , m_zOrder(zOrder)
    , m_visible(visible)
{
}

Surface& LayerInstance::surface()
{
    return m_surface;
}

const Surface& LayerInstance::surface() const
{
    return m_surface;
}

ScreenBuffer& LayerInstance::buffer()
{
    return m_surface.buffer();
}

const ScreenBuffer& LayerInstance::buffer() const
{
    return m_surface.buffer();
}

int LayerInstance::width() const
{
    return m_surface.buffer().getWidth();
}

int LayerInstance::height() const
{
    return m_surface.buffer().getHeight();
}

bool LayerInstance::isEmpty() const
{
    return width() <= 0 || height() <= 0;
}

Point LayerInstance::position() const
{
    return m_position;
}

void LayerInstance::setPosition(Point position)
{
    m_position = position;
}

void LayerInstance::setPosition(int x, int y)
{
    m_position = { x, y };
}

int LayerInstance::x() const
{
    return m_position.x;
}

int LayerInstance::y() const
{
    return m_position.y;
}

void LayerInstance::setX(int x)
{
    m_position.x = x;
}

void LayerInstance::setY(int y)
{
    m_position.y = y;
}

void LayerInstance::moveBy(int dx, int dy)
{
    m_position.x += dx;
    m_position.y += dy;
}

int LayerInstance::zOrder() const
{
    return m_zOrder;
}

void LayerInstance::setZOrder(int zOrder)
{
    m_zOrder = zOrder;
}

bool LayerInstance::isVisible() const
{
    return m_visible;
}

void LayerInstance::setVisible(bool visible)
{
    m_visible = visible;
}

void LayerInstance::show()
{
    m_visible = true;
}

void LayerInstance::hide()
{
    m_visible = false;
}

Rect LayerInstance::bounds() const
{
    return { m_position, { width(), height() } };
}

void LayerInstance::resize(int width, int height)
{
    m_surface.resize(width, height);
}

