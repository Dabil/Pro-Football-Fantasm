#pragma once

#include "UI/Widgets/Widget.h"
#include "Input/Event.h"
#include "Rendering/Surface.h"
#include "Styles/PFF_Theme.h"
#include <functional>
#include <algorithm>
#include <vector>

// The normal name is already in the page cache. Only active underlines are
// drawn here; a wrapped name is still one keyboard stop.
class TeamLink final : public Widget
{
public:
    struct Span {Rect bounds;std::string text;};
    TeamLink(std::vector<Span> spans,std::function<void()> activate)
        : m_spans(std::move(spans)),m_activate(std::move(activate))
    {
        setFocusable(true);
        if(!m_spans.empty())
        {
            Rect area=m_spans.front().bounds;
            for(const auto& span:m_spans)
            {
                area.size.width=std::max(area.size.width,span.bounds.size.width);
                area.size.height=span.bounds.position.y-area.position.y+1;
            }
            setBounds(area);
        }
    }
    bool canReceivePointerFocus(Point point) const override
    {
        if(!canReceiveFocus()) return false;
        for(const auto& span:m_spans) if(span.bounds.contains(point.x,point.y)) return true;
        return false;
    }
    void draw(Surface& surface) override
    {
        const Style linkFocusStyle=PFF_Colors::TextHighlightColor.withUnderline(true);
        if(!isVisible() || !isFocused()) return;
        for(const auto& span:m_spans)
            surface.buffer().writeString(span.bounds.position.x,span.bounds.position.y,span.text,linkFocusStyle);
    }
    bool handleEvent(const Input::Event& event) override
    {
        if(!canReceiveFocus()) return false;
        bool activate=false;
        if(const auto* mouse=event.asMouse())
        {
            const bool inside=canReceivePointerFocus(mouse->position);
            if(mouse->isMove() || mouse->isDrag()) return inside;
            if(mouse->button!=Input::MouseButton::Left) return false;
            if(mouse->isPress()) {m_pressed=inside;return inside;}
            if(mouse->isRelease()) {activate=m_pressed && inside;m_pressed=false;}
            else return inside;
        }
        else if(const auto* command=event.asCommand())
            activate=isFocused() && command->command.code==Input::CommandCode::Confirm;
        if(!activate) return false;
        const auto callback=m_activate; // Navigation may destroy this widget.
        if(callback) callback();
        return true;
    }
    void clearPointerState() override {m_pressed=false;}
private:
    std::vector<Span> m_spans;
    std::function<void()> m_activate;
    bool m_pressed=false;
};
