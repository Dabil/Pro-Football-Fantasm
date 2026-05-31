#pragma once

#include <memory>
#include <string>

#include "Core/Rect.h"
#include "UI/Panels/Window.h"

class IEffect;

namespace UI
{
    class EffectReferenceWindowContent;
}

class EffectWindow : public Window
{
public:
    // Compatibility wrapper for older screens that still construct EffectWindow directly.
    //
    // New code should prefer:
    // ContentWindow(bounds, title, std::make_unique<UI::EffectReferenceWindowContent>(effect))
    //
    // EffectWindow borrows the effect. It does not own, reset, or destroy it.
    EffectWindow(Rect bounds, std::string title, IEffect& effect);
    ~EffectWindow() override;

    bool hasTransferableContent() const override;
    std::unique_ptr<UI::IWindowContent> releaseContent() override;

    void update(const Animation::TickEvent& event) override;
    void draw(Surface& surface) override;

private:
    void detachContent();
    void notifyContentBoundsChanged();

private:
    std::unique_ptr<UI::EffectReferenceWindowContent> m_content;
    Rect m_lastContentBounds{};
    bool m_hasLastContentBounds = false;
};