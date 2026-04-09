#include "Rendering/Styles/StylePolicyFactory.h"

StylePolicy StylePolicyFactory::buildFromCapabilities(
    const RendererCapabilities& capabilities,
    RendererStylePolicyProfile profile)
{
    StylePolicy policy = StylePolicy::PreserveIntent();

    policy = policy.withBasicColorMode(
        capabilities.supportsBasicColors()
        ? ColorRenderMode::Direct
        : ColorRenderMode::Omit);

    policy = policy.withIndexed256ColorMode(
        capabilities.supportsIndexed256Colors()
        ? ColorRenderMode::Direct
        : (capabilities.supportsBasicColors()
            ? ColorRenderMode::DowngradeToBasic
            : ColorRenderMode::Omit));

    policy = policy.withRgbColorMode(
        capabilities.supportsTrueColor()
        ? ColorRenderMode::Direct
        : (capabilities.supportsIndexed256Colors()
            ? ColorRenderMode::DowngradeToIndexed256
            : (capabilities.supportsBasicColors()
                ? ColorRenderMode::DowngradeToBasic
                : ColorRenderMode::Omit)));

    policy = policy.withBoldMode(
        textAttributeModeFromSupport(capabilities.bold, profile));
    policy = policy.withDimMode(
        textAttributeModeFromSupport(capabilities.dim, profile));
    policy = policy.withUnderlineMode(
        textAttributeModeFromSupport(capabilities.underline, profile));
    policy = policy.withReverseMode(
        textAttributeModeFromSupport(capabilities.reverse, profile));
    policy = policy.withInvisibleMode(
        textAttributeModeFromSupport(capabilities.invisible, profile));
    policy = policy.withStrikeMode(
        textAttributeModeFromSupport(capabilities.strike, profile));

    const bool allowSafeFallback = capabilities.usesPreserveStyleSafeFallback();

    policy = policy.withSlowBlinkMode(
        blinkModeFromSupport(capabilities.slowBlink, allowSafeFallback));
    policy = policy.withFastBlinkMode(
        blinkModeFromSupport(capabilities.fastBlink, allowSafeFallback));

    return policy;
}

TextAttributeRenderMode StylePolicyFactory::textAttributeModeFromSupport(
    RendererFeatureSupport support,
    RendererStylePolicyProfile profile)
{
    switch (profile)
    {
    case RendererStylePolicyProfile::ConsoleAttributeBackend:
        switch (support)
        {
        case RendererFeatureSupport::Supported:
            return TextAttributeRenderMode::Direct;

        case RendererFeatureSupport::Unknown:
            return TextAttributeRenderMode::Approximate;

        case RendererFeatureSupport::Emulated:
        case RendererFeatureSupport::Unsupported:
        default:
            return TextAttributeRenderMode::Omit;
        }

    case RendererStylePolicyProfile::TerminalSequenceBackend:
        switch (support)
        {
        case RendererFeatureSupport::Supported:
            return TextAttributeRenderMode::Direct;

        case RendererFeatureSupport::Emulated:
        case RendererFeatureSupport::Unknown:
        case RendererFeatureSupport::Unsupported:
        default:
            return TextAttributeRenderMode::Omit;
        }

    case RendererStylePolicyProfile::DirectPresentationBackend:
        switch (support)
        {
        case RendererFeatureSupport::Supported:
            return TextAttributeRenderMode::Direct;

        case RendererFeatureSupport::Emulated:
        case RendererFeatureSupport::Unknown:
            return TextAttributeRenderMode::Approximate;

        case RendererFeatureSupport::Unsupported:
        default:
            return TextAttributeRenderMode::Omit;
        }

    default:
        return TextAttributeRenderMode::Omit;
    }
}

BlinkRenderMode StylePolicyFactory::blinkModeFromSupport(
    RendererFeatureSupport support,
    bool allowSafeFallback)
{
    switch (support)
    {
    case RendererFeatureSupport::Supported:
        return BlinkRenderMode::Direct;

    case RendererFeatureSupport::Emulated:
        return allowSafeFallback
            ? BlinkRenderMode::Emulate
            : BlinkRenderMode::Omit;

    case RendererFeatureSupport::Unknown:
    case RendererFeatureSupport::Unsupported:
    default:
        return BlinkRenderMode::Omit;
    }
}