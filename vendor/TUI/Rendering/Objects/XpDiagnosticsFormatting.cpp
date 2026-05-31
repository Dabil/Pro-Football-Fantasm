#include "Rendering/Objects/XpDiagnosticsFormatting.h"

#include <algorithm>
#include <sstream>

namespace
{
    bool containsLayerIndex(const std::vector<int>& values, const int layerIndex)
    {
        return std::find(values.begin(), values.end(), layerIndex) != values.end();
    }

    std::string joinIntegers(const std::vector<int>& values)
    {
        if (values.empty())
        {
            return "<none>";
        }

        std::ostringstream stream;
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            if (index > 0)
            {
                stream << ',';
            }

            stream << values[index];
        }

        return stream.str();
    }

    bool resolveLayerVisibility(
        const XpArtLoader::XpDocument& document,
        const XpArtLoader::XpFrame& frame,
        const XpArtLoader::XpSequenceMetadata& sequenceMetadata,
        const int layerIndex)
    {
        const XpArtLoader::XpVisibleLayerMode visibleLayerMode =
            frame.resolveVisibleLayerMode(sequenceMetadata);

        if (visibleLayerMode == XpArtLoader::XpVisibleLayerMode::ForceAllLayersVisible)
        {
            return true;
        }

        if (visibleLayerMode == XpArtLoader::XpVisibleLayerMode::UseExplicitVisibleLayerList)
        {
            return containsLayerIndex(
                frame.resolveExplicitVisibleLayerIndices(sequenceMetadata),
                layerIndex);
        }

        const XpArtLoader::XpLayer* layer = XpArtLoader::tryGetLayer(document, layerIndex);
        return layer != nullptr && layer->visible;
    }
}

namespace XpDiagnosticsFormatting
{
    std::string formatXpDocumentSummary(const XpArtLoader::XpDocument& document)
    {
        if (!document.isValid())
        {
            return "XP retained document: unavailable";
        }

        const XpArtLoader::XpDocumentMetadata& metadata = document.metadata;

        std::ostringstream stream;
        stream << "XP retained document: "
            << document.width << 'x' << document.height
            << ", layers=" << document.getLayerCount()
            << ", version=" << document.formatVersion
            << ", legacyLayerHeader=" << (document.usesLegacyLayerCountHeader ? "true" : "false")
            << ", retained=" << (metadata.retainedPathAvailable ? "true" : "false")
            << ", flattened=" << (metadata.flattenPathUsed ? "true" : "false")
            << ", inputCompressed=" << (metadata.inputWasCompressed ? "true" : "false")
            << ", inputAlreadyDecompressed=" << (metadata.inputWasAlreadyDecompressed ? "true" : "false")
            << ", compositeMode=" << XpArtLoader::toString(metadata.compositeModeUsed);

        return stream.str();
    }

    std::vector<LayerInspection> inspectLayers(
        const XpArtLoader::XpDocument& document,
        const XpArtLoader::XpFrame& frame,
        const XpArtLoader::XpSequenceMetadata& sequenceMetadata)
    {
        std::vector<LayerInspection> inspections;
        if (!document.isValid())
        {
            return inspections;
        }

        inspections.reserve(document.layers.size());
        for (std::size_t index = 0; index < document.layers.size(); ++index)
        {
            const XpArtLoader::XpLayer& layer = document.layers[index];

            LayerInspection inspection;
            inspection.layerIndex = static_cast<int>(index);
            inspection.documentVisible = layer.visible;
            inspection.resolvedVisible = resolveLayerVisibility(
                document,
                frame,
                sequenceMetadata,
                inspection.layerIndex);
            inspection.visibleForFlattening = layer.metadata.visibilityUsedForFlattening;
            inspection.matchedCanvasSize = layer.metadata.matchedCanvasSize;
            inspection.clippingOccurred = layer.metadata.clippingOccurredDuringFlatten;
            inspection.hasTransparentBackgroundCells =
                layer.metadata.encounteredTransparentBackgroundCells;
            inspection.hasVisibleGlyphsOnTransparentBackground =
                layer.metadata.encounteredVisibleGlyphsOnTransparentBackground;
            inspection.compositeMode = layer.metadata.compositeModeUsed;
            inspection.width = layer.width;
            inspection.height = layer.height;

            inspections.push_back(inspection);
        }

        return inspections;
    }

    std::string formatXpVisibilityCompositeSummary(
        const XpArtLoader::XpFrame& frame,
        const XpArtLoader::XpSequenceMetadata& sequenceMetadata)
    {
        std::ostringstream stream;

        const XpArtLoader::XpVisibleLayerMode visibleLayerMode =
            frame.resolveVisibleLayerMode(sequenceMetadata);
        const std::vector<int> explicitVisibleLayerIndices =
            frame.resolveExplicitVisibleLayerIndices(sequenceMetadata);

        stream << "XP visibility/composite: compositeMode="
            << XpArtLoader::toString(frame.resolveCompositeMode(sequenceMetadata))
            << ", visibleLayerMode="
            << XpArtLoader::toString(visibleLayerMode);

        if (visibleLayerMode == XpArtLoader::XpVisibleLayerMode::UseExplicitVisibleLayerList)
        {
            stream << ", explicitVisibleLayers="
                << joinIntegers(explicitVisibleLayerIndices);
        }

        if (const std::optional<int> durationMs =
            frame.resolveDurationMilliseconds(sequenceMetadata);
            durationMs.has_value())
        {
            stream << ", durationMs=" << *durationMs;
        }

        return stream.str();
    }

    std::string formatXpLayerDetails(
        const XpArtLoader::XpDocument& document,
        const XpArtLoader::XpFrame& frame,
        const XpArtLoader::XpSequenceMetadata& sequenceMetadata,
        const int layerIndex)
    {
        const XpArtLoader::XpLayer* layer = XpArtLoader::tryGetLayer(document, layerIndex);
        if (layer == nullptr)
        {
            return "XP layer: unavailable";
        }

        const LayerInspection inspection = inspectLayers(document, frame, sequenceMetadata)[static_cast<std::size_t>(layerIndex)];

        std::ostringstream stream;
        stream << "XP layer " << inspection.layerIndex
            << ": " << inspection.width << 'x' << inspection.height
            << ", documentVisible=" << (inspection.documentVisible ? "true" : "false")
            << ", resolvedVisible=" << (inspection.resolvedVisible ? "true" : "false")
            << ", flattenVisible=" << (inspection.visibleForFlattening ? "true" : "false")
            << ", matchesCanvas=" << (inspection.matchedCanvasSize ? "true" : "false")
            << ", clipped=" << (inspection.clippingOccurred ? "true" : "false")
            << ", transparentCells=" << (inspection.hasTransparentBackgroundCells ? "true" : "false")
            << ", transparentVisibleGlyphs=" << (inspection.hasVisibleGlyphsOnTransparentBackground ? "true" : "false")
            << ", compositeMode=" << XpArtLoader::toString(inspection.compositeMode);

        return stream.str();
    }
}
