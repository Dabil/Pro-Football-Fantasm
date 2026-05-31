#pragma once

#include "Animation/TickEvent.h"

class Surface;

namespace Input
{
    class Event;
}

class Screen
{
public:
    virtual ~Screen() = default;

    virtual void onEnter() {}
    virtual void onExit() {}

    virtual bool handleEvent(const Input::Event& event)
    {
        (void)event;
        return false;
    }

    virtual void update(const Animation::TickEvent& event)
    {
        (void)event;
    }

    virtual void draw(Surface& surface) = 0;
};