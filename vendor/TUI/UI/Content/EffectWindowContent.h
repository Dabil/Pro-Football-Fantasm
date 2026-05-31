#pragma once

#include <memory>

#include "UI/Content/IWindowContent.h"

class IEffect;

namespace UI
{
    class EffectWindowContent final : public IWindowContent
    {
    public:
        explicit EffectWindowContent(std::unique_ptr<IEffect> effect);

        EffectWindowContent(const EffectWindowContent&) = delete;
        EffectWindowContent& operator=(const EffectWindowContent&) = delete;

        EffectWindowContent(EffectWindowContent&&) noexcept = default;
        EffectWindowContent& operator=(EffectWindowContent&&) noexcept = default;

        ~EffectWindowContent() override;

        std::string_view contentTypeName() const;

        bool hasEffect() const;

        IEffect* effect();
        const IEffect* effect() const;

        std::unique_ptr<IEffect> releaseEffect();

        void update(const Animation::TickEvent& event) override;
        void draw(Surface& surface, const Rect& bounds) override;
        void onAttached() override;
        void onDetached() override;
        void onBoundsChanged(const Rect& bounds) override;

    private:
        std::unique_ptr<IEffect> m_effect;
    };
}