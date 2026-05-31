#pragma once

#include <functional>

#include "Animation/TickEvent.h"
#include "Core/Rect.h"
#include "UI/Content/IWindowContent.h"

class Surface;

namespace Input
{
    class Event;
}

namespace UI
{
    class ImmediateModeWindowContent final : public IWindowContent
    {
    public:
        using DrawCallback = std::function<void(Surface& surface, const Rect& bounds)>;
        using UpdateCallback = std::function<void(const Animation::TickEvent& event)>;
        using EventCallback = std::function<bool(const Input::Event& event)>;
        using BoundsChangedCallback = std::function<void(const Rect& bounds)>;

        explicit ImmediateModeWindowContent(DrawCallback draw);

        ImmediateModeWindowContent(
            DrawCallback draw,
            UpdateCallback update,
            EventCallback handleEvent = nullptr,
            BoundsChangedCallback onBoundsChanged = nullptr);

        ImmediateModeWindowContent(const ImmediateModeWindowContent&) = delete;
        ImmediateModeWindowContent& operator=(const ImmediateModeWindowContent&) = delete;

        ImmediateModeWindowContent(ImmediateModeWindowContent&&) noexcept = default;
        ImmediateModeWindowContent& operator=(ImmediateModeWindowContent&&) noexcept = default;

        ~ImmediateModeWindowContent() override;

        std::string_view contentTypeName() const;

        void setDrawCallback(DrawCallback draw);
        void setUpdateCallback(UpdateCallback update);
        void setEventCallback(EventCallback handleEvent);
        void setBoundsChangedCallback(BoundsChangedCallback onBoundsChanged);

        void update(const Animation::TickEvent& event) override;
        bool handleEvent(const Input::Event& event) override;
        void draw(Surface& surface, const Rect& bounds) override;
        void onBoundsChanged(const Rect& bounds) override;

    private:
        DrawCallback m_draw;
        UpdateCallback m_update;
        EventCallback m_handleEvent;
        BoundsChangedCallback m_onBoundsChanged;
    };
}