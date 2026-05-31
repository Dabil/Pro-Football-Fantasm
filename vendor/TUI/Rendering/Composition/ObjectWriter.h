#pragma once

#include <optional>

#include "Rendering/Composition/WritePolicy.h"
#include "Rendering/Composition/WritePresets.h"
#include "Rendering/Objects/TextObject.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/Style.h"

namespace Composition
{
    class ObjectWriter
    {
    public:
        ObjectWriter(ScreenBuffer& target, int offsetX = 0, int offsetY = 0);

        void setOffset(int offsetX, int offsetY);

        int offsetX() const;
        int offsetY() const;

        void writeObject(const TextObject& source, const WritePolicy& policy);
        void writeObject(
            const TextObject& source,
            const WritePolicy& policy,
            const std::optional<Style>& sourceStyleOverride);

        void writeVisibleOnly(
            const TextObject& source,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);
        void writeVisibleOnly(
            const TextObject& source,
            const Style& sourceStyleOverride,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);
        void writeVisibleOnly(
            const TextObject& source,
            const std::optional<Style>& sourceStyleOverride,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);

        void writeAuthoredOnly(
            const TextObject& source,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);
        void writeAuthoredOnly(
            const TextObject& source,
            const Style& sourceStyleOverride,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);
        void writeAuthoredOnly(
            const TextObject& source,
            const std::optional<Style>& sourceStyleOverride,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);

        void writeSolidObject(
            const TextObject& source,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);
        void writeSolidObject(
            const TextObject& source,
            const Style& sourceStyleOverride,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);
        void writeSolidObject(
            const TextObject& source,
            const std::optional<Style>& sourceStyleOverride,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);

        void writeGlyphsOnly(
            const TextObject& source,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);
        void writeGlyphsOnly(
            const TextObject& source,
            const Style& sourceStyleOverride,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);
        void writeGlyphsOnly(
            const TextObject& source,
            const std::optional<Style>& sourceStyleOverride,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);

        void writeStyleMask(
            const TextObject& source,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);
        void writeStyleMask(
            const TextObject& source,
            const Style& sourceStyleOverride,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);
        void writeStyleMask(
            const TextObject& source,
            const std::optional<Style>& sourceStyleOverride,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);

        void writeStyleBlock(
            const TextObject& source,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);
        void writeStyleBlock(
            const TextObject& source,
            const Style& sourceStyleOverride,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);
        void writeStyleBlock(
            const TextObject& source,
            const std::optional<Style>& sourceStyleOverride,
            DepthPolicy depthPolicy = DepthPolicy::Ignore);

    private:
        void writeObjectInternal(
            const TextObject& source,
            const WritePolicy& policy,
            const std::optional<Style>& sourceStyleOverride);

    private:
        ScreenBuffer& m_target;
        int m_offsetX = 0;
        int m_offsetY = 0;
    };
}