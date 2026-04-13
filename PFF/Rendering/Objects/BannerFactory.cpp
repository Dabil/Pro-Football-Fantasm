#include "Rendering/Objects/BannerFactory.h"

#include <algorithm>

#include "Rendering/Objects/TextObjectBuilder.h"
#include "Utilities/Unicode/UnicodeConversion.h"

namespace BannerFactory
{
    namespace
    {
        std::string toUtf8(std::u32string_view text)
        {
            return UnicodeConversion::u32ToUtf8(text);
        }

        void blitObject(
            TextObjectBuilder& builder,
            const TextObject& source,
            const int offsetX,
            const int offsetY)
        {
            for (int y = 0; y < source.getHeight(); ++y)
            {
                for (int x = 0; x < source.getWidth(); ++x)
                {
                    const TextObjectCell* cell = source.tryGetCell(x, y);
                    if (cell == nullptr)
                    {
                        continue;
                    }

                    if (cell->kind == CellKind::Empty)
                    {
                        continue;
                    }

                    const int destX = offsetX + x;
                    const int destY = offsetY + y;

                    if (!builder.inBounds(destX, destY))
                    {
                        continue;
                    }

                    switch (cell->kind)
                    {
                    case CellKind::Glyph:
                        if (cell->width == CellWidth::Two)
                        {
                            builder.setWideGlyph(destX, destY, cell->glyph, cell->style);
                        }
                        else
                        {
                            builder.setGlyph(destX, destY, cell->glyph, cell->style);
                        }
                        break;

                    case CellKind::WideTrailing:
                    case CellKind::CombiningContinuation:
                        builder.setCell(destX, destY, cell->glyph, cell->kind, cell->width, cell->style);
                        break;

                    case CellKind::Empty:
                    default:
                        break;
                    }
                }
            }
        }

        TextObject compositeTwo(
            const TextObject& back,
            const int backX,
            const int backY,
            const TextObject& front,
            const int frontX,
            const int frontY)
        {
            if (back.isEmpty() && front.isEmpty())
            {
                return TextObject();
            }

            const int backMinX = backX;
            const int backMinY = backY;
            const int backMaxX = backX + back.getWidth();
            const int backMaxY = backY + back.getHeight();

            const int frontMinX = frontX;
            const int frontMinY = frontY;
            const int frontMaxX = frontX + front.getWidth();
            const int frontMaxY = frontY + front.getHeight();

            const int minX = std::min(backMinX, frontMinX);
            const int minY = std::min(backMinY, frontMinY);
            const int maxX = std::max(backMaxX, frontMaxX);
            const int maxY = std::max(backMaxY, frontMaxY);

            const int width = std::max(0, maxX - minX);
            const int height = std::max(0, maxY - minY);

            if (width <= 0 || height <= 0)
            {
                return TextObject();
            }

            TextObjectBuilder builder(width, height);

            blitObject(builder, back, backX - minX, backY - minY);
            blitObject(builder, front, frontX - minX, frontY - minY);

            return builder.build();
        }
    }

    // -------------------------------------------------------------------------
    // Render option presets
    // -------------------------------------------------------------------------

    AsciiBanner::RenderOptions defaultOptions()
    {
        return AsciiBanner::RenderOptions{};
    }

    AsciiBanner::RenderOptions kernOptions()
    {
        AsciiBanner::RenderOptions options;
        options.composeMode = AsciiBanner::ComposeMode::Kern;
        return options;
    }

    AsciiBanner::RenderOptions fullWidthOptions()
    {
        AsciiBanner::RenderOptions options;
        options.composeMode = AsciiBanner::ComposeMode::FullWidth;
        return options;
    }

    AsciiBanner::RenderOptions centeredOptions(const std::size_t targetWidth)
    {
        AsciiBanner::RenderOptions options;
        options.alignment = AsciiBanner::Alignment::Center;
        options.targetWidth = targetWidth;
        return options;
    }

    AsciiBanner::RenderOptions centeredKernOptions(const std::size_t targetWidth)
    {
        AsciiBanner::RenderOptions options;
        options.composeMode = AsciiBanner::ComposeMode::Kern;
        options.alignment = AsciiBanner::Alignment::Center;
        options.targetWidth = targetWidth;
        return options;
    }

    // -------------------------------------------------------------------------
    // Preferred API: makeBanner(...)
    // UTF-8 overloads
    // -------------------------------------------------------------------------

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::string_view text)
    {
        return AsciiBanner::generateTextObject(font, text);
    }

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const AsciiBanner::RenderOptions& options)
    {
        return AsciiBanner::generateTextObject(font, text, options);
    }

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& style)
    {
        return AsciiBanner::generateTextObject(font, text, style);
    }

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& style,
        const AsciiBanner::RenderOptions& options)
    {
        return AsciiBanner::generateTextObject(font, text, style, options);
    }

    // -------------------------------------------------------------------------
    // Preferred API: makeBanner(...)
    // UTF-32 overloads
    // -------------------------------------------------------------------------

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::u32string_view text)
    {
        return makeBanner(font, toUtf8(text));
    }

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const AsciiBanner::RenderOptions& options)
    {
        return makeBanner(font, toUtf8(text), options);
    }

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& style)
    {
        return makeBanner(font, toUtf8(text), style);
    }

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& style,
        const AsciiBanner::RenderOptions& options)
    {
        return makeBanner(font, toUtf8(text), style, options);
    }

    // -------------------------------------------------------------------------
    // Generic layered helpers
    // -------------------------------------------------------------------------

    LayeredBanner makeLayeredBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle)
    {
        return makeLayeredBanner(font, text, mainStyle, shadowStyle, defaultOptions());
    }

    LayeredBanner makeLayeredBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const AsciiBanner::RenderOptions& options)
    {
        LayeredBanner layered;
        layered.main = makeBanner(font, text, mainStyle, options);
        layered.shadow = makeBanner(font, text, shadowStyle, options);
        layered.shadowOffsetX = 0;
        layered.shadowOffsetY = 0;
        return layered;
    }

    LayeredBanner makeLayeredBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle)
    {
        return makeLayeredBanner(font, text, mainStyle, shadowStyle, defaultOptions());
    }

    LayeredBanner makeLayeredBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const AsciiBanner::RenderOptions& options)
    {
        return makeLayeredBanner(font, toUtf8(text), mainStyle, shadowStyle, options);
    }

    // -------------------------------------------------------------------------
    // Shadow effects
    // -------------------------------------------------------------------------

    TextObject makeShadowBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        int shadowOffsetX,
        int shadowOffsetY)
    {
        return makeShadowBanner(
            font,
            text,
            mainStyle,
            shadowStyle,
            defaultOptions(),
            shadowOffsetX,
            shadowOffsetY);
    }

    TextObject makeShadowBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const AsciiBanner::RenderOptions& options,
        int shadowOffsetX,
        int shadowOffsetY)
    {
        const LayeredBanner layered = makeLayeredShadowBanner(
            font,
            text,
            mainStyle,
            shadowStyle,
            options,
            shadowOffsetX,
            shadowOffsetY);

        return compositeTwo(
            layered.shadow,
            layered.shadowOffsetX,
            layered.shadowOffsetY,
            layered.main,
            0,
            0);
    }

    TextObject makeShadowBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        int shadowOffsetX,
        int shadowOffsetY)
    {
        return makeShadowBanner(
            font,
            text,
            mainStyle,
            shadowStyle,
            defaultOptions(),
            shadowOffsetX,
            shadowOffsetY);
    }

    TextObject makeShadowBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const AsciiBanner::RenderOptions& options,
        int shadowOffsetX,
        int shadowOffsetY)
    {
        return makeShadowBanner(
            font,
            toUtf8(text),
            mainStyle,
            shadowStyle,
            options,
            shadowOffsetX,
            shadowOffsetY);
    }

    LayeredBanner makeLayeredShadowBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        int shadowOffsetX,
        int shadowOffsetY)
    {
        return makeLayeredShadowBanner(
            font,
            text,
            mainStyle,
            shadowStyle,
            defaultOptions(),
            shadowOffsetX,
            shadowOffsetY);
    }

    LayeredBanner makeLayeredShadowBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const AsciiBanner::RenderOptions& options,
        int shadowOffsetX,
        int shadowOffsetY)
    {
        LayeredBanner layered;
        layered.main = makeBanner(font, text, mainStyle, options);
        layered.shadow = makeBanner(font, text, shadowStyle, options);
        layered.shadowOffsetX = shadowOffsetX;
        layered.shadowOffsetY = shadowOffsetY;
        return layered;
    }

    LayeredBanner makeLayeredShadowBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        int shadowOffsetX,
        int shadowOffsetY)
    {
        return makeLayeredShadowBanner(
            font,
            text,
            mainStyle,
            shadowStyle,
            defaultOptions(),
            shadowOffsetX,
            shadowOffsetY);
    }

    LayeredBanner makeLayeredShadowBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const AsciiBanner::RenderOptions& options,
        int shadowOffsetX,
        int shadowOffsetY)
    {
        return makeLayeredShadowBanner(
            font,
            toUtf8(text),
            mainStyle,
            shadowStyle,
            options,
            shadowOffsetX,
            shadowOffsetY);
    }
}