#pragma once

#include <string_view>

#include "Animation/TickEvent.h"
#include "Core/Rect.h"

class Surface;

namespace Input
{
    class Event;
}

namespace UI
{
    class IWindowContent
    {
    public:
        IWindowContent() = default;

        IWindowContent(const IWindowContent&) = delete;
        IWindowContent& operator=(const IWindowContent&) = delete;

        IWindowContent(IWindowContent&&) noexcept = default;
        IWindowContent& operator=(IWindowContent&&) noexcept = default;

        virtual ~IWindowContent() = default;

        virtual std::string_view contentTypeName() const
        {
            return "UI::IWindowContent";
        }

        virtual void update(const Animation::TickEvent& event) = 0;

        virtual bool handleEvent(const Input::Event& event)
        {
            return false;
        }

        virtual void draw(Surface& surface, const Rect& bounds) = 0;

        virtual void onAttached()
        {
        }

        virtual void onDetached()
        {
        }

        virtual void onBoundsChanged(const Rect& bounds)
        {
        }
    };
}