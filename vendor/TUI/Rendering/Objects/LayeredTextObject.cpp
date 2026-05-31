#include "Rendering/Objects/LayeredTextObject.h"

#include <algorithm>
#include <utility>

#include "Rendering/Objects/TextObjectBlitter.h"
#include "Rendering/Objects/TextObjectBuilder.h"

namespace Rendering
{
    LayeredTextObject::LayeredTextObject(int width, int height)
        : m_width(width > 0 ? width : 0)
        , m_height(height > 0 ? height : 0)
        , m_loaded(m_width > 0 && m_height > 0)
    {
    }

    void LayeredTextObject::clear()
    {
        m_width = 0;
        m_height = 0;
        m_loaded = false;
        m_layers.clear();
    }

    bool LayeredTextObject::isLoaded() const
    {
        return m_loaded;
    }

    bool LayeredTextObject::isEmpty() const
    {
        return m_layers.empty();
    }

    int LayeredTextObject::getWidth() const
    {
        return m_width;
    }

    int LayeredTextObject::getHeight() const
    {
        return m_height;
    }

    void LayeredTextObject::setDimensions(int width, int height)
    {
        m_width = width > 0 ? width : 0;
        m_height = height > 0 ? height : 0;
        m_loaded = (m_width > 0 && m_height > 0);
    }

    std::size_t LayeredTextObject::getLayerCount() const
    {
        return m_layers.size();
    }

    bool LayeredTextObject::hasVisibleLayers() const
    {
        return std::any_of(
            m_layers.begin(),
            m_layers.end(),
            [](const TextObjectLayer& layer)
            {
                return layer.visible;
            });
    }

    bool LayeredTextObject::hasVisibleNonEmptyLayers() const
    {
        return std::any_of(
            m_layers.begin(),
            m_layers.end(),
            [](const TextObjectLayer& layer)
            {
                return layer.visible && !layer.object.isEmpty();
            });
    }

    const std::vector<TextObjectLayer>& LayeredTextObject::getLayers() const
    {
        return m_layers;
    }

    std::vector<TextObjectLayer>& LayeredTextObject::getLayers()
    {
        return m_layers;
    }

    const TextObjectLayer* LayeredTextObject::findLayer(std::string_view name) const
    {
        const auto it = std::find_if(
            m_layers.begin(),
            m_layers.end(),
            [&](const TextObjectLayer& layer)
            {
                return layer.name == name;
            });

        return (it != m_layers.end()) ? &(*it) : nullptr;
    }

    TextObjectLayer* LayeredTextObject::findLayer(std::string_view name)
    {
        const auto it = std::find_if(
            m_layers.begin(),
            m_layers.end(),
            [&](const TextObjectLayer& layer)
            {
                return layer.name == name;
            });

        return (it != m_layers.end()) ? &(*it) : nullptr;
    }

    const TextObject* LayeredTextObject::findLayerTextObjectByName(std::string_view name) const
    {
        const TextObjectLayer* layer = findLayer(name);
        return (layer != nullptr) ? &layer->object : nullptr;
    }

    bool LayeredTextObject::hasLayer(std::string_view name) const
    {
        return findLayer(name) != nullptr;
    }

    bool LayeredTextObject::addLayer(const TextObjectLayer& layer)
    {
        if (layer.name.empty() || hasLayer(layer.name))
        {
            return false;
        }

        m_layers.push_back(layer);
        return true;
    }

    bool LayeredTextObject::addLayer(TextObjectLayer&& layer)
    {
        if (layer.name.empty() || hasLayer(layer.name))
        {
            return false;
        }

        m_layers.push_back(std::move(layer));
        return true;
    }

    bool LayeredTextObject::removeLayer(std::string_view name)
    {
        const auto it = std::find_if(
            m_layers.begin(),
            m_layers.end(),
            [&](const TextObjectLayer& layer)
            {
                return layer.name == name;
            });

        if (it == m_layers.end())
        {
            return false;
        }

        m_layers.erase(it);
        return true;
    }

    void LayeredTextObject::setLayerVisibility(std::string_view name, bool visible)
    {
        TextObjectLayer* layer = findLayer(name);
        if (layer != nullptr)
        {
            layer->visible = visible;
        }
    }

    bool LayeredTextObject::getLayerVisibility(std::string_view name, bool defaultValue) const
    {
        const TextObjectLayer* layer = findLayer(name);
        return (layer != nullptr) ? layer->visible : defaultValue;
    }

    void LayeredTextObject::setLayerOffset(std::string_view name, int offsetX, int offsetY)
    {
        TextObjectLayer* layer = findLayer(name);
        if (layer != nullptr)
        {
            layer->offsetX = offsetX;
            layer->offsetY = offsetY;
        }
    }

    TextObject LayeredTextObject::flattenVisibleOnly() const
    {
        FlattenOptions options;
        options.visibleOnly = true;
        options.writePolicy = Composition::WritePresets::solidObject();
        return flatten(options);
    }

    TextObject LayeredTextObject::flattenAllLayers() const
    {
        FlattenOptions options;
        options.visibleOnly = false;
        options.writePolicy = Composition::WritePresets::solidObject();
        return flatten(options);
    }

    TextObject LayeredTextObject::flatten(const FlattenOptions& options) const
    {
        return flattenInternal(m_layers, m_width, m_height, options);
    }

    std::vector<const TextObjectLayer*> LayeredTextObject::collectFlattenLayers(
        const std::vector<TextObjectLayer>& layers,
        const FlattenOptions& options)
    {
        std::vector<const TextObjectLayer*> sortedLayers;
        sortedLayers.reserve(layers.size());

        for (const TextObjectLayer& layer : layers)
        {
            if (options.visibleOnly && !layer.visible)
            {
                continue;
            }

            if (!layer.object.isLoaded() || layer.object.getWidth() <= 0 || layer.object.getHeight() <= 0)
            {
                continue;
            }

            sortedLayers.push_back(&layer);
        }

        std::stable_sort(
            sortedLayers.begin(),
            sortedLayers.end(),
            [](const TextObjectLayer* a, const TextObjectLayer* b)
            {
                return a->zIndex < b->zIndex;
            });

        return sortedLayers;
    }

    TextObject LayeredTextObject::flattenInternal(
        const std::vector<TextObjectLayer>& layers,
        const int width,
        const int height,
        const FlattenOptions& options)
    {
        if (width <= 0 || height <= 0)
        {
            return TextObject{};
        }

        TextObjectBuilder builder(width, height);
        builder.fillTransparent();

        const std::vector<const TextObjectLayer*> sortedLayers =
            collectFlattenLayers(layers, options);

        for (const TextObjectLayer* layer : sortedLayers)
        {
            if (layer == nullptr)
            {
                continue;
            }

            TextObjectBlitter::BlitOptions blitOptions;
            blitOptions.overrideStyle = options.overrideStyle;
            blitOptions.writePolicy = options.writePolicy;
            blitOptions.skipStructuralContinuationCells = true;

            TextObjectBlitter::blitToBuilder(
                builder,
                layer->object,
                layer->offsetX,
                layer->offsetY,
                blitOptions);
        }

        return builder.build();
    }
}