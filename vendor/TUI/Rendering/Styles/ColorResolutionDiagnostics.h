#pragma once

#include <optional>

#include "Rendering/Capabilities/ColorSupport.h"
#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/Style.h"

enum class ColorAdaptationReason
{
    ConcreteBasicUsedDirect,
    ConcreteIndexedUsedDirect,
    ConcreteRgbUsedDirect,

    ConcreteIndexedDowngradedToBasic16,
    ConcreteRgbDowngradedToIndexed256,
    ConcreteRgbDowngradedToBasic16,

    ThemeBasicUsedDirect,
    ThemeIndexedUsedDirect,
    ThemeRgbUsedDirect,

    ThemeIndexedDowngradedToBasic16,
    ThemeRgbDowngradedToIndexed256,
    ThemeRgbDowngradedToBasic16,

    OmittedByPolicy,
    OmittedNoColorSupport
};

struct ColorResolutionDiagnostics
{
    Style::StyleColorValue authoredColor;
    ColorSupport supportedTier = ColorSupport::None;
    std::optional<Color> resolvedColor;
    ColorAdaptationReason reason = ColorAdaptationReason::OmittedNoColorSupport;

    bool wasDowngraded() const;
    bool wasOmitted() const;
    bool wasDirect() const;

    static const char* toString(ColorAdaptationReason reason);
};