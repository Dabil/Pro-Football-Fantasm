#pragma once

#include <string>
#include <vector>

#include "Rendering/Objects/XpArtLoader.h"

namespace XpDiagnosticsFormatting
{
    struct LayerInspection
    {
        int layerIndex = -1;
        bool documentVisible = false;
        bool resolvedVisible = false;
        bool visibleForFlattening = false;
        bool matchedCanvasSize = false;
        bool clippingOccurred = false;
        bool hasTransparentBackgroundCells = false;
        bool hasVisibleGlyphsOnTransparentBackground = false;
        XpArtLoader::XpCompositeMode compositeMode = XpArtLoader::XpCompositeMode::Phase45BCompatible;
        int width = 0;
        int height = 0;
    };

    std::string formatXpDocumentSummary(const XpArtLoader::XpDocument& document);

    std::string formatXpVisibilityCompositeSummary(
        const XpArtLoader::XpFrame& frame,
        const XpArtLoader::XpSequenceMetadata& sequenceMetadata);

    std::string formatXpLayerDetails(
        const XpArtLoader::XpDocument& document,
        const XpArtLoader::XpFrame& frame,
        const XpArtLoader::XpSequenceMetadata& sequenceMetadata,
        int layerIndex);

    std::vector<LayerInspection> inspectLayers(
        const XpArtLoader::XpDocument& document,
        const XpArtLoader::XpFrame& frame,
        const XpArtLoader::XpSequenceMetadata& sequenceMetadata);
}
