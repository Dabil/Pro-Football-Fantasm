#pragma once

#include <cstddef>
#include <string_view>

#include "Assets/Models/AsciiBannerFont.h"
#include "Rendering/Objects/AsciiBanner.h"
#include "Rendering/Objects/LayeredTextObject.h"
#include "Rendering/Objects/TextObject.h"
#include "Rendering/Objects/pFontLoader.h"
#include "Rendering/Styles/Style.h"

namespace BannerFactory
{
    // -------------------------------------------------------------------------
    // Render option presets
    // -------------------------------------------------------------------------

    AsciiBanner::RenderOptions defaultOptions();
    AsciiBanner::RenderOptions kernOptions();
    AsciiBanner::RenderOptions fullWidthOptions();
    AsciiBanner::RenderOptions centeredOptions(std::size_t targetWidth);
    AsciiBanner::RenderOptions centeredKernOptions(std::size_t targetWidth);

    // -------------------------------------------------------------------------
    // Preferred API: makeBanner(...)
    // Returns a single placement-ready TextObject. Any layered source data is
    // flattened before it leaves this API surface.
    // AsciiBannerFont UTF-8 overloads
    // -------------------------------------------------------------------------

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::string_view text);

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const AsciiBanner::RenderOptions& options);

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& style);

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& style,
        const AsciiBanner::RenderOptions& options);

    // -------------------------------------------------------------------------
    // Preferred API: makeBanner(...)
    // AsciiBannerFont UTF-32 overloads
    // -------------------------------------------------------------------------

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::u32string_view text);

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const AsciiBanner::RenderOptions& options);

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& style);

    TextObject makeBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& style,
        const AsciiBanner::RenderOptions& options);

    // -------------------------------------------------------------------------
    // Preferred API: makeBanner(...)
    // pFont UTF-8 overloads
    // -------------------------------------------------------------------------

    TextObject makeBanner(
        const PseudoFont::FontDefinition& font,
        std::string_view text);

    TextObject makeBanner(
        const PseudoFont::FontDefinition& font,
        std::string_view text,
        const PseudoFont::RenderOptions& options);

    TextObject makeBanner(
        const PseudoFont::FontDefinition& font,
        std::string_view text,
        const Style& overrideStyle);

    TextObject makeBanner(
        const PseudoFont::FontDefinition& font,
        std::string_view text,
        const Style& overrideStyle,
        const PseudoFont::RenderOptions& options);

    // -------------------------------------------------------------------------
    // Preferred API: makeBanner(...)
    // pFont UTF-32 overloads
    // -------------------------------------------------------------------------

    TextObject makeBanner(
        const PseudoFont::FontDefinition& font,
        std::u32string_view text);

    TextObject makeBanner(
        const PseudoFont::FontDefinition& font,
        std::u32string_view text,
        const PseudoFont::RenderOptions& options);

    TextObject makeBanner(
        const PseudoFont::FontDefinition& font,
        std::u32string_view text,
        const Style& overrideStyle);

    TextObject makeBanner(
        const PseudoFont::FontDefinition& font,
        std::u32string_view text,
        const Style& overrideStyle,
        const PseudoFont::RenderOptions& options);

    // -------------------------------------------------------------------------
    // Layered helpers
    // Returns LayeredTextObject only when layer identity needs to survive for
    // later composition/effects/animation control.
    // AsciiBanner synthetic layered helpers
    // -------------------------------------------------------------------------

    Rendering::LayeredTextObject makeLayeredBannerObject(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& layerOneStyle,
        const Style& layerTwoStyle);

    Rendering::LayeredTextObject makeLayeredBannerObject(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& layerOneStyle,
        const Style& layerTwoStyle,
        const AsciiBanner::RenderOptions& options);

    Rendering::LayeredTextObject makeLayeredBannerObject(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& layerOneStyle,
        const Style& layerTwoStyle);

    Rendering::LayeredTextObject makeLayeredBannerObject(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& layerOneStyle,
        const Style& layerTwoStyle,
        const AsciiBanner::RenderOptions& options);

    // -------------------------------------------------------------------------
    // Layered helpers
    // pFont native layered helpers
    // -------------------------------------------------------------------------

    Rendering::LayeredTextObject makeLayeredBannerObject(
        const PseudoFont::FontDefinition& font,
        std::string_view text);

    Rendering::LayeredTextObject makeLayeredBannerObject(
        const PseudoFont::FontDefinition& font,
        std::string_view text,
        const PseudoFont::LayeredRenderOptions& options);

    Rendering::LayeredTextObject makeLayeredBannerObject(
        const PseudoFont::FontDefinition& font,
        std::string_view text,
        const Style& overrideStyle);

    Rendering::LayeredTextObject makeLayeredBannerObject(
        const PseudoFont::FontDefinition& font,
        std::string_view text,
        const Style& overrideStyle,
        const PseudoFont::LayeredRenderOptions& options);

    Rendering::LayeredTextObject makeLayeredBannerObject(
        const PseudoFont::FontDefinition& font,
        std::u32string_view text);

    Rendering::LayeredTextObject makeLayeredBannerObject(
        const PseudoFont::FontDefinition& font,
        std::u32string_view text,
        const PseudoFont::LayeredRenderOptions& options);

    Rendering::LayeredTextObject makeLayeredBannerObject(
        const PseudoFont::FontDefinition& font,
        std::u32string_view text,
        const Style& overrideStyle);

    Rendering::LayeredTextObject makeLayeredBannerObject(
        const PseudoFont::FontDefinition& font,
        std::u32string_view text,
        const Style& overrideStyle,
        const PseudoFont::LayeredRenderOptions& options);

    // -------------------------------------------------------------------------
    // Shadow effects
    // AsciiBanner only
    // -------------------------------------------------------------------------

    TextObject makeShadowBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    TextObject makeShadowBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const AsciiBanner::RenderOptions& options,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    TextObject makeShadowBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    TextObject makeShadowBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const AsciiBanner::RenderOptions& options,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    Rendering::LayeredTextObject makeLayeredShadowBannerObject(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    Rendering::LayeredTextObject makeLayeredShadowBannerObject(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const AsciiBanner::RenderOptions& options,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    Rendering::LayeredTextObject makeLayeredShadowBannerObject(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    Rendering::LayeredTextObject makeLayeredShadowBannerObject(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const AsciiBanner::RenderOptions& options,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    // -------------------------------------------------------------------------
    // Shadow effects
    // pFont
    // -------------------------------------------------------------------------

    TextObject makeShadowBanner(
        const PseudoFont::FontDefinition& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    TextObject makeShadowBanner(
        const PseudoFont::FontDefinition& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const PseudoFont::RenderOptions& options,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    TextObject makeShadowBanner(
        const PseudoFont::FontDefinition& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    TextObject makeShadowBanner(
        const PseudoFont::FontDefinition& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const PseudoFont::RenderOptions& options,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    Rendering::LayeredTextObject makeLayeredShadowBannerObject(
        const PseudoFont::FontDefinition& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    Rendering::LayeredTextObject makeLayeredShadowBannerObject(
        const PseudoFont::FontDefinition& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const PseudoFont::RenderOptions& shadowOptions,
        const PseudoFont::LayeredRenderOptions& mainOptions,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    Rendering::LayeredTextObject makeLayeredShadowBannerObject(
        const PseudoFont::FontDefinition& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    Rendering::LayeredTextObject makeLayeredShadowBannerObject(
        const PseudoFont::FontDefinition& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const PseudoFont::RenderOptions& shadowOptions,
        const PseudoFont::LayeredRenderOptions& mainOptions,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);
}
