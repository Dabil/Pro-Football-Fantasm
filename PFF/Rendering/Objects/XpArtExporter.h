#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Rendering/Objects/TextObject.h"
#include "Rendering/Objects/TextObjectExporter.h"

namespace XpArtExporter
{
    struct XpColor
    {
        std::uint8_t red = 0;
        std::uint8_t green = 0;
        std::uint8_t blue = 0;
    };

    struct XpCell
    {
        std::uint32_t glyph = 0;
        XpColor foreground;
        XpColor background;
    };

    struct XpLayer
    {
        int width = 0;
        int height = 0;
        std::vector<XpCell> cells;

        bool isValid() const;
    };

    struct XpDocument
    {
        int formatVersion = 1;
        int canvasWidth = 0;
        int canvasHeight = 0;
        std::vector<XpLayer> layers;

        bool isValid() const;
    };

    bool buildDocument(
        const TextObject& object,
        const TextObjectExporter::SaveOptions& options,
        TextObjectExporter::SaveResult& ioResult,
        XpDocument& outDocument);

    bool serializeDocument(
        const XpDocument& document,
        TextObjectExporter::SaveResult& ioResult,
        std::string& outPayloadBytes);

    bool compressSerializedDocument(
        std::string_view payloadBytes,
        TextObjectExporter::SaveResult& ioResult,
        std::string& outCompressedBytes);

    bool exportToBytes(
        const TextObject& object,
        const TextObjectExporter::SaveOptions& options,
        TextObjectExporter::SaveResult& ioResult);
}
