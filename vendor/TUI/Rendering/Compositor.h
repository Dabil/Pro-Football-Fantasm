#pragma once

#include <vector>

#include "Rendering/Composition/WritePolicy.h"
#include "Rendering/LayerInstance.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Surface.h"

class Compositor
{
public:
    static void compose(
        const std::vector<LayerInstance>& layers,
        Surface& destination);

    static void compose(
        const std::vector<LayerInstance>& layers,
        ScreenBuffer& destination);

    static void compose(
        const std::vector<LayerInstance>& layers,
        Surface& destination,
        const Composition::WritePolicy& writePolicy);

    static void compose(
        const std::vector<LayerInstance>& layers,
        ScreenBuffer& destination,
        const Composition::WritePolicy& writePolicy);

private:
    static void composeLayer(
        const LayerInstance& layer,
        ScreenBuffer& destination,
        const Composition::WritePolicy& writePolicy);
};