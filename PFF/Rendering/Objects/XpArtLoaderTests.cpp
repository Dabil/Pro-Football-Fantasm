#include "Rendering/Objects/XpArtLoaderTests.h"

#include <cstdint>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Rendering/Objects/TextObject.h"
#include "Rendering/Objects/XpArtLoader.h"
#include "Rendering/Objects/XpSequenceLoader.h"
#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/Style.h"

#if defined(__has_include)
#   if __has_include(<zlib.h>)
#       include <zlib.h>
#       define TUI_XP_ART_LOADER_TESTS_HAS_ZLIB 1
#   else
#       define TUI_XP_ART_LOADER_TESTS_HAS_ZLIB 0
#   endif
#else
#   define TUI_XP_ART_LOADER_TESTS_HAS_ZLIB 0
#endif

namespace
{
    using namespace XpArtLoader;

    struct TestContext
    {
        std::string testName;
        std::vector<XpArtLoaderTests::Failure> failures;

        void fail(const std::string& message)
        {
            failures.push_back({ testName, message });
        }
    };

    using TestFunction = void(*)(TestContext&);

    struct TestCase
    {
        const char* name = "";
        TestFunction function = nullptr;
    };

    RgbColor rgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue)
    {
        RgbColor color;
        color.red = red;
        color.green = green;
        color.blue = blue;
        return color;
    }

    XpLayerCell makeLayerCell(
        std::uint32_t glyph,
        const RgbColor& foreground,
        const RgbColor& background)
    {
        XpLayerCell cell;
        cell.glyph = glyph;
        cell.foreground = foreground;
        cell.background = background;
        return cell;
    }

    XpLayer makeLayer(
        int width,
        int height,
        const std::vector<XpLayerCell>& cells,
        bool visible = true)
    {
        XpLayer layer;
        layer.width = width;
        layer.height = height;
        layer.visible = visible;
        layer.cells = cells;
        return layer;
    }

    XpDocument makeDocument(
        int width,
        int height,
        const std::vector<XpLayer>& layers,
        int formatVersion = 1)
    {
        XpDocument document;
        document.width = width;
        document.height = height;
        document.formatVersion = formatVersion;
        document.layers = layers;

        document.metadata.canvasWidth = width;
        document.metadata.canvasHeight = height;
        document.metadata.layerCount = static_cast<int>(layers.size());
        document.metadata.parsedFormatVersion = formatVersion;
        document.metadata.retainedPathAvailable = true;
        document.metadata.flattenPathUsed = false;
        document.metadata.inputWasCompressed = false;
        document.metadata.inputWasAlreadyDecompressed = false;
        document.metadata.compositeModeUsed = XpCompositeMode::Phase45BCompatible;

        for (std::size_t index = 0; index < document.layers.size(); ++index)
        {
            XpLayer& layer = document.layers[index];
            layer.metadata.sourceLayerIndex = static_cast<int>(index);
            layer.metadata.sourceWidth = layer.width;
            layer.metadata.sourceHeight = layer.height;
            layer.metadata.matchedCanvasSize =
                (layer.width == width && layer.height == height);
            layer.metadata.visibilityUsedForFlattening = layer.visible;
            layer.metadata.compositeModeUsed = XpCompositeMode::Phase45BCompatible;

            for (const XpLayerCell& cell : layer.cells)
            {
                if (cell.background == rgb(255, 0, 255))
                {
                    layer.metadata.encounteredTransparentBackgroundCells = true;

                    if (cell.glyph != 0 && cell.glyph != static_cast<std::uint32_t>(U' '))
                    {
                        layer.metadata.encounteredVisibleGlyphsOnTransparentBackground = true;
                    }
                }
            }
        }

        return document;
    }

    void appendLe32(std::string& bytes, std::uint32_t value)
    {
        bytes.push_back(static_cast<char>(value & 0xFF));
        bytes.push_back(static_cast<char>((value >> 8) & 0xFF));
        bytes.push_back(static_cast<char>((value >> 16) & 0xFF));
        bytes.push_back(static_cast<char>((value >> 24) & 0xFF));
    }

    void appendPayloadCell(
        std::string& bytes,
        std::uint32_t glyph,
        const RgbColor& foreground,
        const RgbColor& background)
    {
        appendLe32(bytes, glyph);
        bytes.push_back(static_cast<char>(foreground.red));
        bytes.push_back(static_cast<char>(foreground.green));
        bytes.push_back(static_cast<char>(foreground.blue));
        bytes.push_back(static_cast<char>(background.red));
        bytes.push_back(static_cast<char>(background.green));
        bytes.push_back(static_cast<char>(background.blue));
    }

    std::string makeXpPayload(
        int formatVersion,
        const std::vector<XpLayer>& layers)
    {
        std::string bytes;

        appendLe32(bytes, static_cast<std::uint32_t>(-formatVersion));
        appendLe32(bytes, static_cast<std::uint32_t>(layers.size()));

        for (const XpLayer& layer : layers)
        {
            appendLe32(bytes, static_cast<std::uint32_t>(layer.width));
            appendLe32(bytes, static_cast<std::uint32_t>(layer.height));

            for (const XpLayerCell& cell : layer.cells)
            {
                appendPayloadCell(bytes, cell.glyph, cell.foreground, cell.background);
            }
        }

        return bytes;
    }

#if TUI_XP_ART_LOADER_TESTS_HAS_ZLIB
    bool tryGzipCompress(std::string_view input, std::string& outBytes)
    {
        outBytes.clear();

        z_stream stream{};
        stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
        stream.avail_in = static_cast<uInt>(input.size());

        if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        {
            return false;
        }

        constexpr std::size_t kChunkSize = 16384;
        char chunk[kChunkSize];

        int rc = Z_OK;
        while (rc == Z_OK)
        {
            stream.next_out = reinterpret_cast<Bytef*>(chunk);
            stream.avail_out = static_cast<uInt>(kChunkSize);

            rc = deflate(&stream, Z_FINISH);
            if (rc != Z_OK && rc != Z_STREAM_END)
            {
                deflateEnd(&stream);
                outBytes.clear();
                return false;
            }

            const std::size_t produced = kChunkSize - static_cast<std::size_t>(stream.avail_out);
            outBytes.append(chunk, produced);
        }

        deflateEnd(&stream);
        return rc == Z_STREAM_END;
    }
#endif

    std::string boolString(bool value)
    {
        return value ? "true" : "false";
    }

    void expectTrue(TestContext& context, bool value, const std::string& label)
    {
        if (!value)
        {
            context.fail(label + " expected true.");
        }
    }

    void expectFalse(TestContext& context, bool value, const std::string& label)
    {
        if (value)
        {
            context.fail(label + " expected false.");
        }
    }

    void expectEqualInt(TestContext& context, int actual, int expected, const std::string& label)
    {
        if (actual != expected)
        {
            std::ostringstream stream;
            stream << label << " expected " << expected << ", got " << actual << '.';
            context.fail(stream.str());
        }
    }

    void expectEqualChar(TestContext& context, char32_t actual, char32_t expected, const std::string& label)
    {
        if (actual != expected)
        {
            std::ostringstream stream;
            stream << label << " expected codepoint " << static_cast<std::uint32_t>(expected)
                << ", got " << static_cast<std::uint32_t>(actual) << '.';
            context.fail(stream.str());
        }
    }

    void expectHasWarning(TestContext& context, const LoadResult& result, LoadWarningCode code)
    {
        if (!hasWarning(result, code))
        {
            std::ostringstream stream;
            stream << "Expected warning " << toString(code) << " to be present.";
            context.fail(stream.str());
        }
    }

    void expectNoWarning(TestContext& context, const LoadResult& result, LoadWarningCode code)
    {
        if (hasWarning(result, code))
        {
            std::ostringstream stream;
            stream << "Did not expect warning " << toString(code) << '.';
            context.fail(stream.str());
        }
    }

    void expectMutationSuccess(
        TestContext& context,
        const MutationResult& result,
        bool expectedChanged,
        const std::string& label)
    {
        if (!result.success)
        {
            context.fail(label + " expected success but failed: " + result.message);
            return;
        }

        if (result.changed != expectedChanged)
        {
            context.fail(label + " changed flag mismatch.");
        }
    }

    void expectRgbForeground(
        TestContext& context,
        const TextObjectCell& cell,
        const RgbColor& expected,
        const std::string& label)
    {
        if (!cell.style.has_value())
        {
            context.fail(label + " expected a style with foreground.");
            return;
        }

        const Style& style = *cell.style;
        if (!style.hasForeground())
        {
            context.fail(label + " expected foreground color.");
            return;
        }

        const Color& color = *style.foreground();
        if (!color.isRgb())
        {
            context.fail(label + " expected RGB foreground.");
            return;
        }

        if (color.red() != expected.red ||
            color.green() != expected.green ||
            color.blue() != expected.blue)
        {
            std::ostringstream stream;
            stream << label << " expected foreground RGB("
                << static_cast<int>(expected.red) << ','
                << static_cast<int>(expected.green) << ','
                << static_cast<int>(expected.blue) << ").";
            context.fail(stream.str());
        }
    }

    void expectRgbBackground(
        TestContext& context,
        const TextObjectCell& cell,
        const RgbColor& expected,
        const std::string& label)
    {
        if (!cell.style.has_value())
        {
            context.fail(label + " expected a style with background.");
            return;
        }

        const Style& style = *cell.style;
        if (!style.hasBackground())
        {
            context.fail(label + " expected background color.");
            return;
        }

        const Color& color = *style.background();
        if (!color.isRgb())
        {
            context.fail(label + " expected RGB background.");
            return;
        }

        if (color.red() != expected.red ||
            color.green() != expected.green ||
            color.blue() != expected.blue)
        {
            std::ostringstream stream;
            stream << label << " expected background RGB("
                << static_cast<int>(expected.red) << ','
                << static_cast<int>(expected.green) << ','
                << static_cast<int>(expected.blue) << ").";
            context.fail(stream.str());
        }
    }

    const TextObjectCell& requireCell(
        TestContext& context,
        const TextObject& object,
        int x,
        int y,
        const std::string& label)
    {
        const TextObjectCell* cell = object.tryGetCell(x, y);
        if (cell == nullptr)
        {
            context.fail(label + " cell lookup returned null.");
            return object.getCell(0, 0);
        }

        return *cell;
    }

    void testTransparentBackgroundVisibleGlyph(TestContext& context)
    {
        const RgbColor blue = rgb(10, 20, 200);
        const RgbColor green = rgb(20, 200, 20);
        const RgbColor red = rgb(200, 20, 20);
        const RgbColor magenta = rgb(255, 0, 255);

        const XpLayer bottom = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U'B'), green, blue)
            });

        const XpLayer top = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U'A'), red, magenta)
            });

        const XpDocument document = makeDocument(1, 1, { bottom, top });

        const LoadResult result = buildTextObject(document);

        expectTrue(context, result.success, "buildTextObject success");
        expectHasWarning(context, result, LoadWarningCode::MultipleLayersFlattened);
        expectNoWarning(context, result, LoadWarningCode::TransparentCellsSkipped);

        const TextObjectCell& cell = requireCell(context, result.object, 0, 0, "transparent visible glyph");
        expectTrue(context, cell.isGlyph(), "cell isGlyph");
        expectEqualChar(context, cell.glyph, U'A', "glyph");
        expectRgbForeground(context, cell, red, "foreground");
        expectRgbBackground(context, cell, blue, "background");
    }

    void testTransparentBackgroundBlankGlyph(TestContext& context)
    {
        const RgbColor blue = rgb(10, 20, 200);
        const RgbColor green = rgb(20, 200, 20);
        const RgbColor yellow = rgb(220, 220, 20);
        const RgbColor magenta = rgb(255, 0, 255);

        const XpLayer bottom = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U'B'), green, blue)
            });

        const XpLayer top = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U' '), yellow, magenta)
            });

        const XpDocument document = makeDocument(1, 1, { bottom, top });

        const LoadResult result = buildTextObject(document);

        expectTrue(context, result.success, "buildTextObject success");
        expectHasWarning(context, result, LoadWarningCode::TransparentCellsSkipped);

        const TextObjectCell& cell = requireCell(context, result.object, 0, 0, "transparent blank glyph");
        expectTrue(context, cell.isGlyph(), "cell isGlyph");
        expectEqualChar(context, cell.glyph, U'B', "glyph");
        expectRgbForeground(context, cell, green, "foreground");
        expectRgbBackground(context, cell, blue, "background");
    }

    void testOpaqueBackgroundVisibleGlyph(TestContext& context)
    {
        const RgbColor blue = rgb(10, 20, 200);
        const RgbColor green = rgb(20, 200, 20);
        const RgbColor red = rgb(200, 20, 20);
        const RgbColor black = rgb(0, 0, 0);

        const XpLayer bottom = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U'B'), green, blue)
            });

        const XpLayer top = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U'A'), red, black)
            });

        const XpDocument document = makeDocument(1, 1, { bottom, top });

        const LoadResult result = buildTextObject(document);

        expectTrue(context, result.success, "buildTextObject success");

        const TextObjectCell& cell = requireCell(context, result.object, 0, 0, "opaque visible glyph");
        expectTrue(context, cell.isGlyph(), "cell isGlyph");
        expectEqualChar(context, cell.glyph, U'A', "glyph");
        expectRgbForeground(context, cell, red, "foreground");
        expectRgbBackground(context, cell, black, "background");
    }

    void testOpaqueBackgroundBlankGlyph(TestContext& context)
    {
        const RgbColor blue = rgb(10, 20, 200);
        const RgbColor green = rgb(20, 200, 20);
        const RgbColor red = rgb(200, 20, 20);
        const RgbColor yellow = rgb(220, 220, 20);

        const XpLayer bottom = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U'B'), green, blue)
            });

        const XpLayer top = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U' '), yellow, red)
            });

        const XpDocument document = makeDocument(1, 1, { bottom, top });

        const LoadResult result = buildTextObject(document);

        expectTrue(context, result.success, "buildTextObject success");

        const TextObjectCell& cell = requireCell(context, result.object, 0, 0, "opaque blank glyph");
        expectTrue(context, cell.isEmpty(), "cell isEmpty");
        expectRgbForeground(context, cell, yellow, "foreground");
        expectRgbBackground(context, cell, red, "background");
    }

    void testBottomToTopLayerOrder(TestContext& context)
    {
        const RgbColor bottomForeground = rgb(10, 100, 220);
        const RgbColor middleForeground = rgb(220, 180, 30);
        const RgbColor topForeground = rgb(220, 30, 100);

        const RgbColor bottomBackground = rgb(10, 10, 10);
        const RgbColor middleBackground = rgb(30, 30, 30);
        const RgbColor topBackground = rgb(50, 50, 50);

        const XpLayer bottom = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U'1'), bottomForeground, bottomBackground)
            });

        const XpLayer middle = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U'2'), middleForeground, middleBackground)
            });

        const XpLayer top = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U'3'), topForeground, topBackground)
            });

        const XpDocument document = makeDocument(1, 1, { bottom, middle, top });

        const LoadResult result = buildTextObject(document);

        expectTrue(context, result.success, "buildTextObject success");
        expectTrue(context, result.usedLayerFlattening, "usedLayerFlattening");
        expectEqualInt(context, result.resolvedLayerCount, 3, "resolvedLayerCount");

        const TextObjectCell& cell = requireCell(context, result.object, 0, 0, "bottom-to-top order");
        expectTrue(context, cell.isGlyph(), "cell isGlyph");
        expectEqualChar(context, cell.glyph, U'3', "glyph");
        expectRgbForeground(context, cell, topForeground, "foreground");
        expectRgbBackground(context, cell, topBackground, "background");
    }

    void testHiddenLayerSkippedDuringFlattening(TestContext& context)
    {
        const RgbColor blue = rgb(10, 20, 200);
        const RgbColor green = rgb(20, 200, 20);
        const RgbColor red = rgb(200, 20, 20);
        const RgbColor black = rgb(0, 0, 0);

        const XpLayer bottom = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U'B'), green, blue)
            });

        const XpLayer hiddenTop = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U'A'), red, black)
            }, false);

        const XpDocument document = makeDocument(1, 1, { bottom, hiddenTop });

        const LoadResult result = buildTextObject(document);

        expectTrue(context, result.success, "buildTextObject success");
        expectHasWarning(context, result, LoadWarningCode::HiddenLayersSkipped);
        expectEqualInt(context, result.visibleLayerCount, 1, "visibleLayerCount");
        expectEqualInt(context, result.hiddenLayerCount, 1, "hiddenLayerCount");

        const TextObjectCell& cell = requireCell(context, result.object, 0, 0, "hidden layer skipped");
        expectTrue(context, cell.isGlyph(), "cell isGlyph");
        expectEqualChar(context, cell.glyph, U'B', "glyph");
        expectRgbForeground(context, cell, green, "foreground");
        expectRgbBackground(context, cell, blue, "background");

        const XpLayerMetadata* metadata = tryGetLayerMetadata(result.retainedDocument, 1);
        if (metadata == nullptr)
        {
            context.fail("Hidden layer metadata should be available.");
        }
        else
        {
            expectFalse(context, metadata->visibilityUsedForFlattening, "hidden layer visibilityUsedForFlattening");
        }
    }

    void testStrictVsNonStrictLayerSizeValidation(TestContext& context)
    {
        const RgbColor fg = rgb(200, 200, 200);
        const RgbColor bg = rgb(50, 50, 50);

        const XpLayer smallLayer = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U'A'), fg, bg)
            });

        const XpDocument document = makeDocument(2, 1, { smallLayer });

        LoadOptions strictOptions;
        strictOptions.strictLayerSizeValidation = true;

        const LoadResult strictResult = buildTextObject(document, strictOptions);
        expectFalse(context, strictResult.success, "strict size validation result.success");

        LoadOptions nonStrictOptions;
        nonStrictOptions.strictLayerSizeValidation = false;

        const LoadResult nonStrictResult = buildTextObject(document, nonStrictOptions);
        expectTrue(context, nonStrictResult.success, "non-strict size validation result.success");
        expectHasWarning(context, nonStrictResult, LoadWarningCode::LayerSizeMismatchClipped);

        const XpLayerMetadata* metadata = tryGetLayerMetadata(nonStrictResult.retainedDocument, 0);
        if (metadata == nullptr)
        {
            context.fail("Non-strict retained layer metadata should be available.");
        }
        else
        {
            expectFalse(context, metadata->matchedCanvasSize, "matchedCanvasSize");
            expectTrue(context, metadata->clippingOccurredDuringFlatten, "clippingOccurredDuringFlatten");
        }

        const TextObjectCell& firstCell = requireCell(context, nonStrictResult.object, 0, 0, "non-strict first cell");
        expectTrue(context, firstCell.isGlyph(), "first cell isGlyph");
        expectEqualChar(context, firstCell.glyph, U'A', "first cell glyph");

        const TextObjectCell& secondCell = requireCell(context, nonStrictResult.object, 1, 0, "non-strict second cell");
        expectTrue(context, secondCell.isEmpty(), "second cell isEmpty");
    }

    void testLoadFromBytesMetadataAndTrailingBytes(TestContext& context)
    {
        const RgbColor blue = rgb(10, 20, 200);
        const RgbColor green = rgb(20, 200, 20);
        const RgbColor red = rgb(200, 20, 20);
        const RgbColor magenta = rgb(255, 0, 255);

        const XpLayer bottom = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U'B'), green, blue)
            });

        const XpLayer top = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U'A'), red, magenta)
            });

        std::string payload = makeXpPayload(1, { bottom, top });
        payload.push_back(static_cast<char>(0x7E));
        payload.push_back(static_cast<char>(0x7F));

        const LoadResult result = loadFromBytes(payload);

        expectTrue(context, result.success, "loadFromBytes success");
        expectEqualInt(context, result.resolvedWidth, 1, "resolvedWidth");
        expectEqualInt(context, result.resolvedHeight, 1, "resolvedHeight");
        expectEqualInt(context, result.resolvedLayerCount, 2, "resolvedLayerCount");
        expectEqualInt(context, result.parsedFormatVersion, 1, "parsedFormatVersion");

        expectFalse(context, result.inputWasCompressed, "inputWasCompressed");
        expectTrue(context, result.inputWasAlreadyDecompressed, "inputWasAlreadyDecompressed");
        expectTrue(context, result.usedLayerFlattening, "usedLayerFlattening");
        expectTrue(context, result.hasRetainedDocument, "hasRetainedDocument");

        expectHasWarning(context, result, LoadWarningCode::MultipleLayersFlattened);
        expectHasWarning(context, result, LoadWarningCode::ExtraTrailingBytesIgnored);
        expectHasWarning(context, result, LoadWarningCode::InputWasAlreadyDecompressed);

        expectEqualInt(context, result.retainedDocument.metadata.canvasWidth, 1, "document metadata canvasWidth");
        expectEqualInt(context, result.retainedDocument.metadata.canvasHeight, 1, "document metadata canvasHeight");
        expectEqualInt(context, result.retainedDocument.metadata.layerCount, 2, "document metadata layerCount");
        expectEqualInt(context, result.retainedDocument.metadata.parsedFormatVersion, 1, "document metadata parsedFormatVersion");
        expectTrue(context, result.retainedDocument.metadata.retainedPathAvailable, "document metadata retainedPathAvailable");
        expectTrue(context, result.retainedDocument.metadata.flattenPathUsed, "document metadata flattenPathUsed");
        expectFalse(context, result.retainedDocument.metadata.inputWasCompressed, "document metadata inputWasCompressed");
        expectTrue(context, result.retainedDocument.metadata.inputWasAlreadyDecompressed, "document metadata inputWasAlreadyDecompressed");

        const XpLayerMetadata* topMetadata = tryGetLayerMetadata(result.retainedDocument, 1);
        if (topMetadata == nullptr)
        {
            context.fail("Top layer metadata should be available.");
        }
        else
        {
            expectEqualInt(context, topMetadata->sourceLayerIndex, 1, "top layer sourceLayerIndex");
            expectEqualInt(context, topMetadata->sourceWidth, 1, "top layer sourceWidth");
            expectEqualInt(context, topMetadata->sourceHeight, 1, "top layer sourceHeight");
            expectTrue(context, topMetadata->encounteredTransparentBackgroundCells, "top layer transparent background metadata");
            expectTrue(context, topMetadata->encounteredVisibleGlyphsOnTransparentBackground, "top layer transparent visible glyph metadata");
            expectTrue(context, topMetadata->visibilityUsedForFlattening, "top layer visibilityUsedForFlattening");
        }
    }

    void testMutableCellEditAndDirtyTracking(TestContext& context)
    {
        const RgbColor fg = rgb(200, 200, 200);
        const RgbColor bg = rgb(10, 10, 10);
        const RgbColor newFg = rgb(10, 240, 60);
        const RgbColor newBg = rgb(0, 0, 120);

        XpDocument document = makeDocument(1, 1, {
            makeLayer(1, 1, { makeLayerCell(static_cast<std::uint32_t>(U'A'), fg, bg) })
            });

        expectFalse(context, document.isDirty(), "document dirty before edit");
        expectEqualInt(context, static_cast<int>(document.getMutationRevision()), 0, "document mutationRevision before edit");

        MutationResult result = document.setCell(
            0,
            0,
            0,
            makeLayerCell(static_cast<std::uint32_t>(U'Z'), newFg, newBg));

        expectMutationSuccess(context, result, true, "setCell result");
        expectTrue(context, document.isDirty(), "document dirty after edit");
        expectEqualInt(context, static_cast<int>(document.getMutationRevision()), 1, "document mutationRevision after edit");

        LoadOptions options;
        options.flattenLayers = true;

        const LoadResult loadResult = buildTextObjectFromXpDocument(document, options);
        expectTrue(context, loadResult.success, "recompose after setCell success");

        const TextObjectCell& cell = requireCell(context, loadResult.object, 0, 0, "mutated cell");
        expectEqualChar(context, cell.glyph, U'Z', "mutated glyph");

        document.clearDirty();
        expectFalse(context, document.isDirty(), "document dirty after clearDirty");

        MutationResult noOpResult = document.setCell(
            0,
            0,
            0,
            makeLayerCell(static_cast<std::uint32_t>(U'Z'), newFg, newBg));
        expectMutationSuccess(context, noOpResult, false, "setCell no-op result");
        expectEqualInt(context, static_cast<int>(document.getMutationRevision()), 1, "document mutationRevision after no-op");
    }

    void testLayerVisibilityToggleAndFrameDirtyPropagation(TestContext& context)
    {
        const RgbColor fg = rgb(220, 220, 220);
        const RgbColor bg = rgb(20, 20, 20);

        XpDocument document = makeDocument(1, 1, {
            makeLayer(1, 1, { makeLayerCell(static_cast<std::uint32_t>(U'B'), fg, bg) }, true),
            makeLayer(1, 1, { makeLayerCell(static_cast<std::uint32_t>(U'T'), fg, bg) }, true)
            });

        XpFrame frame;
        frame.frameIndex = 0;
        frame.document = std::make_shared<XpDocument>(document);

        MutationResult visibilityResult = frame.getDocument()->setLayerVisibility(1, false);
        expectMutationSuccess(context, visibilityResult, true, "setLayerVisibility result");
        expectTrue(context, frame.isDirty(), "frame dirty after visibility edit");

        const LoadResult loadResult = buildTextObjectFromXpFrame(frame);
        expectTrue(context, loadResult.success, "recompose after visibility edit success");
        expectHasWarning(context, loadResult, LoadWarningCode::HiddenLayersSkipped);

        const TextObjectCell& cell = requireCell(context, loadResult.object, 0, 0, "visible toggle cell");
        expectEqualChar(context, cell.glyph, U'B', "visible toggle glyph");

        frame.clearDirty();
        expectFalse(context, frame.isDirty(), "frame dirty after clearDirty");
    }

    void testLayerReorderAndSequenceDirtyPropagation(TestContext& context)
    {
        const RgbColor fg = rgb(255, 255, 255);
        const RgbColor bg = rgb(0, 0, 0);

        XpDocument document = makeDocument(1, 1, {
            makeLayer(1, 1, { makeLayerCell(static_cast<std::uint32_t>(U'B'), fg, bg) }, true),
            makeLayer(1, 1, { makeLayerCell(static_cast<std::uint32_t>(U'T'), fg, bg) }, true)
            });

        XpSequence sequence = buildRetainedSequence(document, 0, "frame0");
        expectFalse(context, sequence.isDirty(), "sequence dirty before reorder");

        XpFrame* frame = sequence.getDefaultFrame();
        if (frame == nullptr || frame->getDocument() == nullptr)
        {
            context.fail("Default frame should be available for reorder test.");
            return;
        }

        MutationResult reorderResult = frame->getDocument()->reorderLayer(1, 0);
        expectMutationSuccess(context, reorderResult, true, "reorderLayer result");
        expectTrue(context, sequence.isDirty(), "sequence dirty after reorder");
        expectEqualInt(context, static_cast<int>(frame->getDocument()->getMutationRevision()), 1, "mutationRevision after reorder");

        const LoadResult loadResult = buildTextObjectFromXpSequence(sequence);
        expectTrue(context, loadResult.success, "recompose after reorder success");

        const TextObjectCell& cell = requireCell(context, loadResult.object, 0, 0, "reordered cell");
        expectEqualChar(context, cell.glyph, U'B', "reordered glyph");

        sequence.clearDirty();
        expectFalse(context, sequence.isDirty(), "sequence dirty after clearDirty");
    }

    void testMutationFailurePaths(TestContext& context)
    {
        const RgbColor fg = rgb(200, 200, 200);
        const RgbColor bg = rgb(10, 10, 10);

        XpDocument document = makeDocument(1, 1, {
            makeLayer(1, 1, { makeLayerCell(static_cast<std::uint32_t>(U'A'), fg, bg) })
            });

        MutationResult badLayerResult = document.setLayerVisibility(5, false);
        expectFalse(context, badLayerResult.success, "bad layer mutation success");
        expectFalse(context, badLayerResult.changed, "bad layer mutation changed");
        expectFalse(context, document.isDirty(), "document dirty after failed layer mutation");

        MutationResult badCellResult = document.setCell(
            0,
            5,
            0,
            makeLayerCell(static_cast<std::uint32_t>(U'Q'), fg, bg));
        expectFalse(context, badCellResult.success, "bad cell mutation success");
        expectFalse(context, badCellResult.changed, "bad cell mutation changed");
        expectFalse(context, document.isDirty(), "document dirty after failed cell mutation");
    }

#if TUI_XP_ART_LOADER_TESTS_HAS_ZLIB
    void testCompressedInputFlagsWhenZlibIsAvailable(TestContext& context)
    {
        const RgbColor fg = rgb(200, 200, 200);
        const RgbColor bg = rgb(10, 10, 10);

        const XpLayer layer = makeLayer(1, 1, {
            makeLayerCell(static_cast<std::uint32_t>(U'Z'), fg, bg)
            });

        const std::string payload = makeXpPayload(1, { layer });

        std::string compressed;
        if (!tryGzipCompress(payload, compressed))
        {
            context.fail("Failed to gzip-compress synthetic XP payload.");
            return;
        }

        const LoadResult result = loadFromBytes(compressed);

        expectTrue(context, result.success, "compressed loadFromBytes success");
        expectTrue(context, result.inputWasCompressed, "compressed inputWasCompressed");
        expectFalse(context, result.inputWasAlreadyDecompressed, "compressed inputWasAlreadyDecompressed");
        expectTrue(context, result.retainedDocument.metadata.inputWasCompressed, "document metadata inputWasCompressed");
        expectFalse(context, result.retainedDocument.metadata.inputWasAlreadyDecompressed, "document metadata inputWasAlreadyDecompressed");
    }
#endif

    void testTransparencyPolicyDefaultsMatchLegacyBehavior(TestContext& context)
    {
        LoadOptions options;
        const XpTransparencyPolicy policy = resolveTransparencyPolicy(options);

        expectTrue(context,
            policy.mode == XpTransparencyMode::RexPaintMagentaBackground,
            "default transparency mode");
        expectTrue(context,
            policy.treatsBackgroundAsTransparent(rgb(255, 0, 255)),
            "magenta treated as transparent");
        expectFalse(context,
            policy.treatsBackgroundAsTransparent(rgb(0, 0, 0)),
            "black treated as opaque");
        expectTrue(context,
            policy.visibleTransparentBaseCellsUseBlackBackground,
            "transparent base cells default black background");
    }

    void testUnknownManifestDirectiveCanBePreserved(TestContext& context)
    {
        const std::string manifest = R"(xpseq 1
future_sequence_flag = "enabled"
frame {
  index = 0
  source = "frame0.xp"
  future_frame_flag = "beta"
}
)";

        XpSequenceLoader::LoadOptions options;
        options.requireContiguousFrameIndices = false;
        options.unknownFieldPolicy = XpSequenceLoader::UnknownFieldPolicy::PreserveInMetadata;
        options.xpLoadOptions.flattenLayers = false;

        const XpSequenceLoader::LoadResult result =
            XpSequenceLoader::loadFromString(manifest, "in_memory.xpseq", options);

        expectFalse(context, result.success, "preserve unknown manifest load success without frame file");

        const XpArtLoader::XpExtensionField* sequenceField =
            result.sequence.metadata.extensionFields.find("manifest.sequence.future_sequence_flag");
        if (sequenceField == nullptr || sequenceField->value != "enabled")
        {
            context.fail("Expected preserved sequence-level unknown manifest field.");
        }

        const XpArtLoader::XpExtensionField* frameField =
            result.sequence.metadata.extensionFields.find("manifest.frame.0.future_frame_flag");
        if (frameField == nullptr || frameField->value != "beta")
        {
            context.fail("Expected preserved frame-level unknown manifest field.");
        }

        const XpSequenceLoader::Diagnostic* preservedSequence =
            XpSequenceLoader::getFirstDiagnosticByCode(
                result,
                XpSequenceLoader::DiagnosticCode::UnknownDirectivePreserved);
        if (preservedSequence == nullptr)
        {
            context.fail("Expected UnknownDirectivePreserved diagnostic.");
        }

        const XpSequenceLoader::Diagnostic* preservedFrame =
            XpSequenceLoader::getFirstDiagnosticByCode(
                result,
                XpSequenceLoader::DiagnosticCode::UnknownFrameDirectivePreserved);
        if (preservedFrame == nullptr)
        {
            context.fail("Expected UnknownFrameDirectivePreserved diagnostic.");
        }
    }

    std::vector<TestCase> buildTestCases()
    {
        std::vector<TestCase> cases =
        {
            { "TransparentBackgroundVisibleGlyph", &testTransparentBackgroundVisibleGlyph },
            { "TransparentBackgroundBlankGlyph", &testTransparentBackgroundBlankGlyph },
            { "OpaqueBackgroundVisibleGlyph", &testOpaqueBackgroundVisibleGlyph },
            { "OpaqueBackgroundBlankGlyph", &testOpaqueBackgroundBlankGlyph },
            { "BottomToTopLayerOrder", &testBottomToTopLayerOrder },
            { "HiddenLayerSkippedDuringFlattening", &testHiddenLayerSkippedDuringFlattening },
            { "StrictVsNonStrictLayerSizeValidation", &testStrictVsNonStrictLayerSizeValidation },
            { "LoadFromBytesMetadataAndTrailingBytes", &testLoadFromBytesMetadataAndTrailingBytes },
            { "MutableCellEditAndDirtyTracking", &testMutableCellEditAndDirtyTracking },
            { "LayerVisibilityToggleAndFrameDirtyPropagation", &testLayerVisibilityToggleAndFrameDirtyPropagation },
            { "LayerReorderAndSequenceDirtyPropagation", &testLayerReorderAndSequenceDirtyPropagation },
            { "MutationFailurePaths", &testMutationFailurePaths },
            { "TransparencyPolicyDefaultsMatchLegacyBehavior", &testTransparencyPolicyDefaultsMatchLegacyBehavior },
            { "UnknownManifestDirectiveCanBePreserved", &testUnknownManifestDirectiveCanBePreserved }
        };

#if TUI_XP_ART_LOADER_TESTS_HAS_ZLIB
        cases.push_back({ "CompressedInputFlagsWhenZlibIsAvailable", &testCompressedInputFlagsWhenZlibIsAvailable });
#endif

        return cases;
    }
}

namespace XpArtLoaderTests
{
    Result runAll()
    {
        Result result;
        const std::vector<TestCase> cases = buildTestCases();

        result.totalCount = static_cast<int>(cases.size());

        for (const TestCase& testCase : cases)
        {
            TestContext context;
            context.testName = testCase.name;

            if (testCase.function != nullptr)
            {
                testCase.function(context);
            }
            else
            {
                context.fail("Test function was null.");
            }

            if (context.failures.empty())
            {
                ++result.passedCount;
            }
            else
            {
                ++result.failedCount;
                result.failures.insert(
                    result.failures.end(),
                    context.failures.begin(),
                    context.failures.end());
            }
        }

        return result;
    }

    std::string formatSummary(const Result& result)
    {
        std::ostringstream stream;
        stream << "XpArtLoaderTests: "
            << result.passedCount << " passed, "
            << result.failedCount << " failed, "
            << result.totalCount << " total";

        if (!result.failures.empty())
        {
            for (const Failure& failure : result.failures)
            {
                stream << "\n - [" << failure.testName << "] " << failure.message;
            }
        }

        return stream.str();
    }
}