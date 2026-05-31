#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Rendering/Composition/WritePresets.h"
#include "Rendering/Objects/TextObject.h"
#include "Rendering/Styles/Style.h"

namespace Rendering
{
    // A single composition layer that places one TextObject into a larger layered asset.
    // The TextObject remains layer-local content; placement is expressed only through
    // offsetX/offsetY plus zIndex/visibility metadata.
    struct TextObjectLayer
    {
        std::string name;
        int zIndex = 0;
        bool visible = true;
        int offsetX = 0;
        int offsetY = 0;
        TextObject object;

        bool empty() const
        {
            return object.isEmpty();
        }
    };

    // LayeredTextObject is a composition container around TextObject primitives.
    // It exists for authored/multi-pass assets that need preserved layer identity until
    // a later flatten/composition step. It is not a second kind of render surface.
    class LayeredTextObject
    {
    public:
        struct FlattenOptions
        {
            // Controls whether invisible layers are excluded from flattening.
            // This is layer visibility, not glyph visibility.
            bool visibleOnly = true;

            std::optional<Style> overrideStyle;

            // Flattening is policy-driven. The default preserves the historic
            // "flatten the retained layered asset into a complete surface" behavior.
            //
            // Use:
            // - visibleObject()   for visual non-space glyph overlays
            // - authoredObject()  for authored Glyph cells, including U' ', while skipping Empty
            // - solidObject()   for full-footprint writes where Empty clears to authored space
            Composition::WritePolicy writePolicy = Composition::WritePresets::solidObject();
        };

    public:
        LayeredTextObject() = default;
        LayeredTextObject(int width, int height);

        void clear();

        bool isLoaded() const;
        bool isEmpty() const;

        int getWidth() const;
        int getHeight() const;
        void setDimensions(int width, int height);

        std::size_t getLayerCount() const;
        bool hasVisibleLayers() const;
        bool hasVisibleNonEmptyLayers() const;

        const std::vector<TextObjectLayer>& getLayers() const;
        std::vector<TextObjectLayer>& getLayers();

        const TextObjectLayer* findLayer(std::string_view name) const;
        TextObjectLayer* findLayer(std::string_view name);

        const TextObject* findLayerTextObjectByName(std::string_view name) const;

        bool hasLayer(std::string_view name) const;
        bool addLayer(const TextObjectLayer& layer);
        bool addLayer(TextObjectLayer&& layer);
        bool removeLayer(std::string_view name);

        void setLayerVisibility(std::string_view name, bool visible);
        bool getLayerVisibility(std::string_view name, bool defaultValue = false) const;

        void setLayerOffset(std::string_view name, int offsetX, int offsetY);

        TextObject flattenVisibleOnly() const;
        TextObject flattenAllLayers() const;
        TextObject flatten(const FlattenOptions& options) const;

    private:
        static std::vector<const TextObjectLayer*> collectFlattenLayers(
            const std::vector<TextObjectLayer>& layers,
            const FlattenOptions& options);

        static TextObject flattenInternal(
            const std::vector<TextObjectLayer>& layers,
            int width,
            int height,
            const FlattenOptions& options);

    private:
        int m_width = 0;
        int m_height = 0;
        bool m_loaded = false;
        std::vector<TextObjectLayer> m_layers;
    };
}