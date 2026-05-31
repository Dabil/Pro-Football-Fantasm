#include "Rendering/Styles/ColorResolutionDiagnostics.h"

bool ColorResolutionDiagnostics::wasDowngraded() const
{
    switch (reason)
    {
    case ColorAdaptationReason::ConcreteIndexedDowngradedToBasic16:
    case ColorAdaptationReason::ConcreteRgbDowngradedToIndexed256:
    case ColorAdaptationReason::ConcreteRgbDowngradedToBasic16:
    case ColorAdaptationReason::ThemeIndexedDowngradedToBasic16:
    case ColorAdaptationReason::ThemeRgbDowngradedToIndexed256:
    case ColorAdaptationReason::ThemeRgbDowngradedToBasic16:
        return true;

    default:
        return false;
    }
}

bool ColorResolutionDiagnostics::wasOmitted() const
{
    switch (reason)
    {
    case ColorAdaptationReason::OmittedByPolicy:
    case ColorAdaptationReason::OmittedNoColorSupport:
        return true;

    default:
        return false;
    }
}

bool ColorResolutionDiagnostics::wasDirect() const
{
    return !wasDowngraded() && !wasOmitted();
}

const char* ColorResolutionDiagnostics::toString(ColorAdaptationReason reason)
{
    switch (reason)
    {
    case ColorAdaptationReason::ConcreteBasicUsedDirect:
        return "Concrete basic color used directly";

    case ColorAdaptationReason::ConcreteIndexedUsedDirect:
        return "Concrete indexed color used directly";

    case ColorAdaptationReason::ConcreteRgbUsedDirect:
        return "Concrete RGB color used directly";

    case ColorAdaptationReason::ConcreteIndexedDowngradedToBasic16:
        return "Concrete indexed color downgraded to Basic16";

    case ColorAdaptationReason::ConcreteRgbDowngradedToIndexed256:
        return "Concrete RGB color downgraded to Indexed256";

    case ColorAdaptationReason::ConcreteRgbDowngradedToBasic16:
        return "Concrete RGB color downgraded to Basic16";

    case ColorAdaptationReason::ThemeBasicUsedDirect:
        return "ThemeColor basic fallback used directly";

    case ColorAdaptationReason::ThemeIndexedUsedDirect:
        return "ThemeColor indexed fallback used directly";

    case ColorAdaptationReason::ThemeRgbUsedDirect:
        return "ThemeColor RGB authored value used directly";

    case ColorAdaptationReason::ThemeIndexedDowngradedToBasic16:
        return "ThemeColor indexed fallback downgraded to Basic16";

    case ColorAdaptationReason::ThemeRgbDowngradedToIndexed256:
        return "ThemeColor RGB authored value downgraded to Indexed256";

    case ColorAdaptationReason::ThemeRgbDowngradedToBasic16:
        return "ThemeColor RGB authored value downgraded to Basic16";

    case ColorAdaptationReason::OmittedByPolicy:
        return "Color omitted by policy";

    case ColorAdaptationReason::OmittedNoColorSupport:
        return "Color omitted because no color support is available";

    default:
        return "Unknown";
    }
}