#pragma once

class Surface;
struct Rect;

namespace Animation
{
    struct TickEvent;
}

/*
    Concrete Rule:

    IEffect implementations must NOT:
        - depend on Screen
        - depend on Window
        - depend on PageComposer
        - assume full-screen

    Only: Surface + Rect + internal state
*/

class IEffect
{
public:
    virtual ~IEffect() = default;

    IEffect() = default;
    IEffect(const IEffect&) = delete;
    IEffect& operator=(const IEffect&) = delete;

    virtual void onEnter() {}
    virtual void onExit() {}
    virtual void update(const Animation::TickEvent& event) = 0;
    virtual void draw(Surface& surface, const Rect& viewport) = 0;

    virtual void onResize(const Rect& viewport) {}
};