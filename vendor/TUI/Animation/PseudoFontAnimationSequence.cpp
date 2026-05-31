#include "Animation/PseudoFontAnimationSequence.h"

#include <sstream>
#include <utility>

#include "Rendering/Objects/TextObjectBuilder.h"

namespace
{
    constexpr const char* kPseudoFontSourceFormat = "pfont";

    Animation::AnimatedTextAssetFrameTiming makeTiming(const double durationSeconds)
    {
        return Animation::AnimatedTextAssetFrameTiming::fromSeconds(durationSeconds);
    }

    Animation::AnimatedTextAssetFrameInfo makeFrameInfo(
        const Animation::PseudoFontAnimationFrame& frame,
        const int sourceFrameIndex)
    {
        Animation::AnimatedTextAssetFrameInfo info;
        info.name = frame.frameName;
        info.sourceFormat = kPseudoFontSourceFormat;
        info.sourceFrameIndex = sourceFrameIndex;
        return info;
    }

    Animation::PseudoFontAnimationFrameDiagnostic makeDiagnostic(
        std::string frameName,
        const Animation::PseudoFontAnimationFrameBuildKind kind,
        const bool success,
        std::string message)
    {
        Animation::PseudoFontAnimationFrameDiagnostic diagnostic;
        diagnostic.frameName = std::move(frameName);
        diagnostic.kind = kind;
        diagnostic.success = success;
        diagnostic.message = std::move(message);
        return diagnostic;
    }

    TextObject makeHiddenTextObject(const Rendering::LayeredTextObject& source)
    {
        const int width = source.getWidth();
        const int height = source.getHeight();

        if (width <= 0 || height <= 0)
        {
            return TextObject{};
        }

        TextObjectBuilder builder(width, height);
        builder.fillTransparent();
        return builder.build();
    }

    bool canUseLayerObjectDirectly(
        const Rendering::LayeredTextObject& source,
        const Rendering::TextObjectLayer& layer)
    {
        return layer.offsetX == 0 &&
            layer.offsetY == 0 &&
            layer.object.isLoaded() &&
            layer.object.getWidth() == source.getWidth() &&
            layer.object.getHeight() == source.getHeight();
    }

    TextObject composeLayersIntoFrame(
        const Rendering::LayeredTextObject& source,
        const std::vector<const Rendering::TextObjectLayer*>& layers)
    {
        if (!source.isLoaded() || source.getWidth() <= 0 || source.getHeight() <= 0)
        {
            return TextObject{};
        }

        Rendering::LayeredTextObject composed(source.getWidth(), source.getHeight());

        for (const Rendering::TextObjectLayer* sourceLayer : layers)
        {
            if (sourceLayer == nullptr)
            {
                continue;
            }

            Rendering::TextObjectLayer layer = *sourceLayer;
            layer.visible = true;
            composed.addLayer(std::move(layer));
        }

        return composed.flattenVisibleOnly();
    }

    bool addTextObjectFrame(
        Animation::AnimatedTextAssetSequence& sequence,
        TextObject object,
        const Animation::PseudoFontAnimationFrame& frame,
        const int sourceFrameIndex)
    {
        if (!object.isLoaded())
        {
            return false;
        }

        sequence.addFrame(Animation::AnimatedTextAssetFrame::fromTextObject(
            std::move(object),
            makeTiming(frame.durationSeconds),
            makeFrameInfo(frame, sourceFrameIndex)));

        return true;
    }

    bool addHiddenFrameFromSource(
        Animation::AnimatedTextAssetSequence& sequence,
        const Rendering::LayeredTextObject& source,
        const Animation::PseudoFontAnimationFrame& frame,
        const int sourceFrameIndex)
    {
        return addTextObjectFrame(
            sequence,
            makeHiddenTextObject(source),
            frame,
            sourceFrameIndex);
    }

    bool addSingleLayerFrame(
        Animation::AnimatedTextAssetSequence& sequence,
        const Rendering::LayeredTextObject& source,
        const Rendering::TextObjectLayer& layer,
        const Animation::PseudoFontAnimationFrame& frame,
        const int sourceFrameIndex)
    {
        if (!layer.object.isLoaded())
        {
            return false;
        }

        if (canUseLayerObjectDirectly(source, layer))
        {
            return addTextObjectFrame(
                sequence,
                layer.object,
                frame,
                sourceFrameIndex);
        }

        std::vector<const Rendering::TextObjectLayer*> layers;
        layers.push_back(&layer);

        return addTextObjectFrame(
            sequence,
            composeLayersIntoFrame(source, layers),
            frame,
            sourceFrameIndex);
    }

    bool addCompositedFrame(
        Animation::AnimatedTextAssetSequence& sequence,
        const Rendering::LayeredTextObject& source,
        const std::vector<const Rendering::TextObjectLayer*>& layers,
        const Animation::PseudoFontAnimationFrame& frame,
        const int sourceFrameIndex)
    {
        return addTextObjectFrame(
            sequence,
            composeLayersIntoFrame(source, layers),
            frame,
            sourceFrameIndex);
    }

    std::string makeMissingLayerMessage(
        const Animation::PseudoFontAnimationFrame& frame,
        const std::string& layerName)
    {
        std::ostringstream stream;
        stream << "pFont animation frame";

        if (!frame.frameName.empty())
        {
            stream << " '" << frame.frameName << "'";
        }

        stream << " references missing layer '" << layerName << "'.";
        return stream.str();
    }

    std::string makeSingleLayerMessage(
        const Rendering::LayeredTextObject& source,
        const Rendering::TextObjectLayer& layer)
    {
        std::ostringstream stream;

        if (canUseLayerObjectDirectly(source, layer))
        {
            stream << "Added direct single-layer pFont animation frame.";
        }
        else
        {
            stream << "Added positioned single-layer pFont animation frame using composition.";
        }

        return stream.str();
    }
}

namespace Animation
{
    bool PseudoFontAnimationSequenceBuildResult::hasDiagnostics() const
    {
        return !diagnostics.empty();
    }

    PseudoFontAnimationSequenceBuildResult buildPseudoFontAnimationSequenceWithDiagnostics(
        const Rendering::LayeredTextObject& source,
        const std::vector<PseudoFontAnimationFrame>& frames,
        std::string sequenceName)
    {
        PseudoFontAnimationSequenceBuildResult result;
        result.sequence = AnimatedTextAssetSequence(std::move(sequenceName));
        result.requestedFrameCount = frames.size();

        if (!source.isLoaded() || source.getWidth() <= 0 || source.getHeight() <= 0)
        {
            result.diagnostics.push_back(makeDiagnostic(
                {},
                PseudoFontAnimationFrameBuildKind::Invalid,
                false,
                "Cannot build a pFont animation sequence from an unloaded LayeredTextObject."));

            result.success = false;
            return result;
        }

        for (std::size_t index = 0; index < frames.size(); ++index)
        {
            const PseudoFontAnimationFrame& frame = frames[index];
            const int sourceFrameIndex = static_cast<int>(index);

            if (frame.visibleLayerNames.empty())
            {
                const bool added = addHiddenFrameFromSource(
                    result.sequence,
                    source,
                    frame,
                    sourceFrameIndex);

                result.diagnostics.push_back(makeDiagnostic(
                    frame.frameName,
                    PseudoFontAnimationFrameBuildKind::Hidden,
                    added,
                    added
                    ? "Added hidden pFont animation frame."
                    : "Failed to add hidden pFont animation frame."));

                continue;
            }

            std::vector<const Rendering::TextObjectLayer*> resolvedLayers;
            resolvedLayers.reserve(frame.visibleLayerNames.size());

            bool missingLayer = false;

            for (const std::string& layerName : frame.visibleLayerNames)
            {
                const Rendering::TextObjectLayer* layer = source.findLayer(layerName);
                if (layer == nullptr)
                {
                    missingLayer = true;

                    result.diagnostics.push_back(makeDiagnostic(
                        frame.frameName,
                        PseudoFontAnimationFrameBuildKind::Invalid,
                        false,
                        makeMissingLayerMessage(frame, layerName)));

                    break;
                }

                resolvedLayers.push_back(layer);
            }

            if (missingLayer)
            {
                continue;
            }

            if (resolvedLayers.size() == 1)
            {
                const Rendering::TextObjectLayer* layer = resolvedLayers.front();

                const bool added = layer != nullptr &&
                    addSingleLayerFrame(
                        result.sequence,
                        source,
                        *layer,
                        frame,
                        sourceFrameIndex);

                result.diagnostics.push_back(makeDiagnostic(
                    frame.frameName,
                    PseudoFontAnimationFrameBuildKind::SingleLayer,
                    added,
                    added && layer != nullptr
                    ? makeSingleLayerMessage(source, *layer)
                    : "Failed to add single-layer pFont animation frame."));

                continue;
            }

            const bool added = addCompositedFrame(
                result.sequence,
                source,
                resolvedLayers,
                frame,
                sourceFrameIndex);

            result.diagnostics.push_back(makeDiagnostic(
                frame.frameName,
                PseudoFontAnimationFrameBuildKind::Composited,
                added,
                added
                ? "Added composited pFont animation frame."
                : "Failed to add composited pFont animation frame."));
        }

        result.builtFrameCount = result.sequence.frameCount();
        result.success = result.builtFrameCount == result.requestedFrameCount;

        return result;
    }

    AnimatedTextAssetSequence buildPseudoFontAnimationSequence(
        const Rendering::LayeredTextObject& source,
        const std::vector<PseudoFontAnimationFrame>& frames,
        std::string sequenceName)
    {
        return buildPseudoFontAnimationSequenceWithDiagnostics(
            source,
            frames,
            std::move(sequenceName)).sequence;
    }

    const char* toString(const PseudoFontAnimationFrameBuildKind kind)
    {
        switch (kind)
        {
        case PseudoFontAnimationFrameBuildKind::Hidden:
            return "Hidden";
        case PseudoFontAnimationFrameBuildKind::SingleLayer:
            return "SingleLayer";
        case PseudoFontAnimationFrameBuildKind::Composited:
            return "Composited";
        case PseudoFontAnimationFrameBuildKind::Invalid:
            return "Invalid";
        }

        return "Unknown";
    }
}