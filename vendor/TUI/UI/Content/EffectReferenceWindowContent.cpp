#include "UI/Content/EffectReferenceWindowContent.h"

#include "Rendering/Effects/IEffects.h"

namespace UI
{
    EffectReferenceWindowContent::EffectReferenceWindowContent(IEffect& effect)
        : m_effect(&effect)
    {
    }

    EffectReferenceWindowContent::~EffectReferenceWindowContent() = default;

    std::string_view EffectReferenceWindowContent::contentTypeName() const
    {
        return "UI::EffectReferenceWindowContent";
    }

    IEffect& EffectReferenceWindowContent::effect()
    {
        return *m_effect;
    }

    const IEffect& EffectReferenceWindowContent::effect() const
    {
        return *m_effect;
    }

    void EffectReferenceWindowContent::update(const Animation::TickEvent& event)
    {
        if (m_effect == nullptr)
        {
            return;
        }

        m_effect->update(event);
    }

    void EffectReferenceWindowContent::draw(Surface& surface, const Rect& bounds)
    {
        if (m_effect == nullptr)
        {
            return;
        }

        m_effect->draw(surface, bounds);
    }

    void EffectReferenceWindowContent::onAttached()
    {
        // This content adapter borrows an effect owned elsewhere.
        // The owning screen/window controls the effect lifecycle.
        // Do not call onEnter() here, because attaching/detaching tabs
        // should not reset or reinitialize borrowed effect state.
    }

    void EffectReferenceWindowContent::onDetached()
    {
        // This content adapter borrows an effect owned elsewhere.
        // Do not call onExit() here; ownership is not ending.
    }

    void EffectReferenceWindowContent::onBoundsChanged(const Rect& bounds)
    {
        if (m_effect == nullptr)
        {
            return;
        }

        m_effect->onResize(bounds);
    }
}