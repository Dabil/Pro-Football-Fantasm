#pragma once

#include <cstddef>
#include <string_view>

#include "Assets/Models/AsciiBannerFont.h"
#include "Rendering/Objects/AsciiBanner.h"
#include "Rendering/Objects/TextObject.h"
#include "Rendering/Styles/Style.h"

namespace BannerFactory
{
    struct LayeredBanner
    {
        TextObject main;
        TextObject shadow;
        int shadowOffsetX = 0;
        int shadowOffsetY = 0;

        bool empty() const
        {
            return main.getWidth() <= 0 || main.getHeight() <= 0;
        }
    };

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
    // UTF-8 overloads
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
    // UTF-32 overloads
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
    // Generic layered helpers
    // -------------------------------------------------------------------------

    LayeredBanner makeLayeredBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle);

    LayeredBanner makeLayeredBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const AsciiBanner::RenderOptions& options);

    LayeredBanner makeLayeredBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle);

    LayeredBanner makeLayeredBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const AsciiBanner::RenderOptions& options);

    // -------------------------------------------------------------------------
    // Shadow effects
    // Flattened helpers return a single composited TextObject.
    // Layered helpers preserve independent layers and carry shadow offsets.
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

    LayeredBanner makeLayeredShadowBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    LayeredBanner makeLayeredShadowBanner(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const AsciiBanner::RenderOptions& options,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    LayeredBanner makeLayeredShadowBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);

    LayeredBanner makeLayeredShadowBanner(
        const AsciiBannerFont& font,
        std::u32string_view text,
        const Style& mainStyle,
        const Style& shadowStyle,
        const AsciiBanner::RenderOptions& options,
        int shadowOffsetX = 1,
        int shadowOffsetY = 1);
}