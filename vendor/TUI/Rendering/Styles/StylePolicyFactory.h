#pragma once

#include "Rendering/Capabilities/RendererCapabilities.h"
#include "Rendering/Styles/StylePolicy.h"

enum class RendererStylePolicyProfile
{
    ConsoleAttributeBackend,
    TerminalSequenceBackend,
    DirectPresentationBackend
};

class StylePolicyFactory
{
public:
    static StylePolicy buildFromCapabilities(
        const RendererCapabilities& capabilities,
        RendererStylePolicyProfile profile);

private:
    static TextAttributeRenderMode textAttributeModeFromSupport(
        RendererFeatureSupport support,
        RendererStylePolicyProfile profile);

    static BlinkRenderMode blinkModeFromSupport(
        RendererFeatureSupport support,
        bool allowSafeFallback);
};