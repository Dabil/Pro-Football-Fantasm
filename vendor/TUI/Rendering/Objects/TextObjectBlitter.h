#pragma once

#include <optional>

#include "Rendering/Composition/WritePresets.h"
#include "Rendering/Objects/TextObject.h"
#include "Rendering/Objects/TextObjectBuilder.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/Style.h"

namespace TextObjectBlitter
{
    struct BlitOptions
    {
        std::optional<Style> overrideStyle;
        Composition::WritePolicy writePolicy = Composition::WritePresets::authoredObject();
        bool skipStructuralContinuationCells = true;
    };

    void blitToBuilder(
        TextObjectBuilder& builder,
        const TextObject& source,
        int offsetX,
        int offsetY,
        const BlitOptions& options = {});

    void blitToScreenBuffer(
        ScreenBuffer& target,
        const TextObject& source,
        int offsetX,
        int offsetY,
        const BlitOptions& options = {});
}