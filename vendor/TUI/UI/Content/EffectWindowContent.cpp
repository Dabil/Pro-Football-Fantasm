#include "UI/Content/EffectWindowContent.h"

#include <utility>

#include "Rendering/Effects/IEffects.h"

namespace UI
{
    EffectWindowContent::EffectWindowContent(std::unique_ptr<IEffect> effect)
        : m_effect(std::move(effect))
    {
    }

    EffectWindowContent::~EffectWindowContent() = default;

    std::string_view EffectWindowContent::contentTypeName() const
    {
        return "UI::EffectWindowContent";
    }

    bool EffectWindowContent::hasEffect() const
    {
        return m_effect != nullptr;
    }

    IEffect* EffectWindowContent::effect()
    {
        return m_effect.get();
    }

    const IEffect* EffectWindowContent::effect() const
    {
        return m_effect.get();
    }

    std::unique_ptr<IEffect> EffectWindowContent::releaseEffect()
    {
        return std::move(m_effect);
    }

    void EffectWindowContent::update(const Animation::TickEvent& event)
    {
        if (m_effect == nullptr)
        {
            return;
        }

        m_effect->update(event);
    }

    void EffectWindowContent::draw(Surface& surface, const Rect& bounds)
    {
        if (m_effect == nullptr)
        {
            return;
        }

        m_effect->draw(surface, bounds);
    }

    void EffectWindowContent::onAttached()
    {
        if (m_effect == nullptr)
        {
            return;
        }

        m_effect->onEnter();
    }

    void EffectWindowContent::onDetached()
    {
        if (m_effect == nullptr)
        {
            return;
        }

        m_effect->onExit();
    }

    void EffectWindowContent::onBoundsChanged(const Rect& bounds)
    {
        if (m_effect == nullptr)
        {
            return;
        }

        m_effect->onResize(bounds);
    }
}