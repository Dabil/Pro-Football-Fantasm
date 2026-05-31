#pragma once

#include <cstdint>
#include <variant>

#include "Animation/TickEvent.h"
#include "Core/Size.h"
#include "Input/Command.h"
#include "Input/KeyEvent.h"
#include "Input/MouseEvent.h"

namespace Input
{
    using TickEvent = Animation::TickEvent;

    enum class EventType
    {
        None,
        Key,
        Command,
        Resize,
        Tick,
        Mouse
    };

    struct CommandEvent
    {
        Command command;
    };

    struct ResizeEvent
    {
        Size previousSize;
        Size currentSize;

        bool sizeChanged() const
        {
            return previousSize.width != currentSize.width
                || previousSize.height != currentSize.height;
        }
    };

    using EventPayload = std::variant<
        std::monostate,
        KeyEvent,
        CommandEvent,
        ResizeEvent,
        TickEvent,
        MouseEvent>;

    class Event
    {
    public:
        Event() = default;

        explicit Event(const KeyEvent& keyEvent)
            : m_payload(keyEvent)
        {
        }

        explicit Event(const CommandEvent& commandEvent)
            : m_payload(commandEvent)
        {
        }

        explicit Event(const ResizeEvent& resizeEvent)
            : m_payload(resizeEvent)
        {
        }

        explicit Event(const TickEvent& tickEvent)
            : m_payload(tickEvent)
        {
        }

        explicit Event(const MouseEvent& mouseEvent)
            : m_payload(mouseEvent)
        {
        }

        EventType type() const
        {
            if (std::holds_alternative<KeyEvent>(m_payload)) return EventType::Key;
            if (std::holds_alternative<CommandEvent>(m_payload)) return EventType::Command;
            if (std::holds_alternative<ResizeEvent>(m_payload)) return EventType::Resize;
            if (std::holds_alternative<TickEvent>(m_payload)) return EventType::Tick;
            if (std::holds_alternative<MouseEvent>(m_payload)) return EventType::Mouse;

            return EventType::None;
        }

        const KeyEvent* asKey() const { return std::get_if<KeyEvent>(&m_payload); }
        const CommandEvent* asCommand() const { return std::get_if<CommandEvent>(&m_payload); }
        const ResizeEvent* asResize() const { return std::get_if<ResizeEvent>(&m_payload); }
        const TickEvent* asTick() const { return std::get_if<TickEvent>(&m_payload); }
        const MouseEvent* asMouse() const { return std::get_if<MouseEvent>(&m_payload); }

        static Event key(const KeyEvent& keyEvent)
        {
            return Event(keyEvent);
        }

        static Event command(const Command& command)
        {
            CommandEvent event;
            event.command = command;
            return Event(event);
        }

        static Event resize(const Size& previousSize, const Size& currentSize)
        {
            ResizeEvent event;
            event.previousSize = previousSize;
            event.currentSize = currentSize;
            return Event(event);
        }

        static Event tick(const TickEvent& tickEvent)
        {
            return Event(tickEvent);
        }

        static Event tick(double deltaSeconds, std::uint64_t frameIndex)
        {
            TickEvent event;
            event.deltaSeconds = deltaSeconds;
            event.frameIndex = frameIndex;
            return Event(event);
        }

        static Event mouse(const MouseEvent& mouseEvent)
        {
            return Event(mouseEvent);
        }

    private:
        EventPayload m_payload;
    };
}