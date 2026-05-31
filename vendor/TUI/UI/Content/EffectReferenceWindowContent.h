#pragma once

#include "UI/Content/IWindowContent.h"

class IEffect;

namespace UI
{
    class EffectReferenceWindowContent final : public IWindowContent
    {
    public:
        explicit EffectReferenceWindowContent(IEffect& effect);

        EffectReferenceWindowContent(const EffectReferenceWindowContent&) = delete;
        EffectReferenceWindowContent& operator=(const EffectReferenceWindowContent&) = delete;

        EffectReferenceWindowContent(EffectReferenceWindowContent&&) noexcept = default;
        EffectReferenceWindowContent& operator=(EffectReferenceWindowContent&&) noexcept = default;

        ~EffectReferenceWindowContent() override;

        std::string_view contentTypeName() const;

        IEffect& effect();
        const IEffect& effect() const;

        void update(const Animation::TickEvent& event) override;
        void draw(Surface& surface, const Rect& bounds) override;
        void onAttached() override;
        void onDetached() override;
        void onBoundsChanged(const Rect& bounds) override;

    private:
        IEffect* m_effect = nullptr;
    };
}

