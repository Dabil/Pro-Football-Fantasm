#include "Rendering/Capabilities/CapabilityReport.h"

#include <sstream>

namespace
{
    const char* basicColorName(Color::Basic color)
    {
        switch (color)
        {
        case Color::Basic::Black:         return "Black";
        case Color::Basic::Red:           return "Red";
        case Color::Basic::Green:         return "Green";
        case Color::Basic::Yellow:        return "Yellow";
        case Color::Basic::Blue:          return "Blue";
        case Color::Basic::Magenta:       return "Magenta";
        case Color::Basic::Cyan:          return "Cyan";
        case Color::Basic::White:         return "White";
        case Color::Basic::BrightBlack:   return "BrightBlack";
        case Color::Basic::BrightRed:     return "BrightRed";
        case Color::Basic::BrightGreen:   return "BrightGreen";
        case Color::Basic::BrightYellow:  return "BrightYellow";
        case Color::Basic::BrightBlue:    return "BrightBlue";
        case Color::Basic::BrightMagenta: return "BrightMagenta";
        case Color::Basic::BrightCyan:    return "BrightCyan";
        case Color::Basic::BrightWhite:   return "BrightWhite";
        default:                          return "UnknownBasic";
        }
    }
}

CapabilityReport::CapabilityReport()
{
    const StyleFeature features[] =
    {
        StyleFeature::ForegroundColor,
        StyleFeature::BackgroundColor,
        StyleFeature::Bold,
        StyleFeature::Dim,
        StyleFeature::Underline,
        StyleFeature::SlowBlink,
        StyleFeature::FastBlink,
        StyleFeature::Reverse,
        StyleFeature::Invisible,
        StyleFeature::Strike
    };

    const StyleAdaptationKind kinds[] =
    {
        StyleAdaptationKind::Direct,
        StyleAdaptationKind::Downgraded,
        StyleAdaptationKind::Approximated,
        StyleAdaptationKind::Omitted,
        StyleAdaptationKind::Emulated,
        StyleAdaptationKind::LogicalOnly
    };

    for (StyleFeature feature : features)
    {
        for (StyleAdaptationKind kind : kinds)
        {
            StyleAdaptationCounter counter;
            counter.feature = feature;
            counter.kind = kind;
            counter.count = 0;
            m_counters.push_back(counter);
        }
    }
}

void CapabilityReport::setCapabilities(const RendererCapabilities& capabilities)
{
    m_capabilities = capabilities;
}

void CapabilityReport::setPolicy(const StylePolicy& policy)
{
    m_policy = policy;
}

void CapabilityReport::setBackendState(const BackendStateSnapshot& backendState)
{
    m_backendState = backendState;
}

void CapabilityReport::setRendererSelectionTrace(const RendererSelectionTrace& selectionTrace)
{
    m_rendererSelectionTrace = selectionTrace;
}

const RendererCapabilities& CapabilityReport::capabilities() const
{
    return m_capabilities;
}

const StylePolicy& CapabilityReport::policy() const
{
    return m_policy;
}

const BackendStateSnapshot& CapabilityReport::backendState() const
{
    return m_backendState;
}

const RendererSelectionTrace& CapabilityReport::rendererSelectionTrace() const
{
    return m_rendererSelectionTrace;
}

void CapabilityReport::setTerminalPresentPerformance(const TerminalPresentPerformanceSnapshot& performance)
{
    m_terminalPresentPerformance = performance;
}

const TerminalPresentPerformanceSnapshot& CapabilityReport::terminalPresentPerformance() const
{
    return m_terminalPresentPerformance;
}

void CapabilityReport::recordDirect(StyleFeature feature)
{
    increment(feature, StyleAdaptationKind::Direct);
}

void CapabilityReport::recordDowngraded(StyleFeature feature)
{
    increment(feature, StyleAdaptationKind::Downgraded);
}

void CapabilityReport::recordApproximated(StyleFeature feature)
{
    increment(feature, StyleAdaptationKind::Approximated);
}

void CapabilityReport::recordOmitted(StyleFeature feature)
{
    increment(feature, StyleAdaptationKind::Omitted);
}

void CapabilityReport::recordEmulated(StyleFeature feature)
{
    increment(feature, StyleAdaptationKind::Emulated);
}

void CapabilityReport::recordLogicalOnly(StyleFeature feature)
{
    increment(feature, StyleAdaptationKind::LogicalOnly);
}

void CapabilityReport::addExample(
    StyleFeature feature,
    StyleAdaptationKind kind,
    const std::string& detail,
    LogicalStyleValueState logicalState)
{
    StyleAdaptationExample example;
    example.feature = feature;
    example.kind = kind;
    example.logicalState = logicalState;
    example.detail = detail;
    m_examples.push_back(example);
}

void CapabilityReport::addLogicalStateExample(
    StyleFeature feature,
    LogicalStyleValueState logicalState,
    const std::string& detail)
{
    StyleLogicalStateExample example;
    example.feature = feature;
    example.logicalState = logicalState;
    example.detail = detail;
    m_logicalStateExamples.push_back(example);
}

void CapabilityReport::addColorAdaptationExample(
    StyleFeature feature,
    StyleAdaptationKind kind,
    const ColorResolutionDiagnostics& diagnostics)
{
    ColorAdaptationExample example;
    example.feature = feature;
    example.kind = kind;
    example.supportedTier = diagnostics.supportedTier;
    example.reason = diagnostics.reason;
    example.authoredColor = diagnostics.authoredColor;
    example.resolvedColor = diagnostics.resolvedColor;
    m_colorAdaptationExamples.push_back(example);
}

std::size_t CapabilityReport::getCount(StyleFeature feature, StyleAdaptationKind kind) const
{
    for (const StyleAdaptationCounter& counter : m_counters)
    {
        if (counter.feature == feature && counter.kind == kind)
        {
            return counter.count;
        }
    }

    return 0;
}

std::size_t CapabilityReport::getTotalCount(StyleAdaptationKind kind) const
{
    std::size_t total = 0;

    for (const StyleAdaptationCounter& counter : m_counters)
    {
        if (counter.kind == kind)
        {
            total += counter.count;
        }
    }

    return total;
}

const std::vector<StyleAdaptationCounter>& CapabilityReport::counters() const
{
    return m_counters;
}

const std::vector<StyleAdaptationExample>& CapabilityReport::examples() const
{
    return m_examples;
}

const std::vector<StyleLogicalStateExample>& CapabilityReport::logicalStateExamples() const
{
    return m_logicalStateExamples;
}

const std::vector<ColorAdaptationExample>& CapabilityReport::colorAdaptationExamples() const
{
    return m_colorAdaptationExamples;
}

void CapabilityReport::clearRuntimeData()
{
    for (StyleAdaptationCounter& counter : m_counters)
    {
        counter.count = 0;
    }

    m_examples.clear();
    m_logicalStateExamples.clear();
    m_colorAdaptationExamples.clear();
    m_terminalPresentPerformance = TerminalPresentPerformanceSnapshot{};
}

bool CapabilityReport::hasRuntimeData() const
{
    for (const StyleAdaptationCounter& counter : m_counters)
    {
        if (counter.count > 0)
        {
            return true;
        }
    }

    return !m_examples.empty()
        || !m_logicalStateExamples.empty()
        || !m_colorAdaptationExamples.empty();
}

const char* CapabilityReport::toString(ColorSupport support)
{
    switch (support)
    {
    case ColorSupport::None:
        return "None";

    case ColorSupport::Basic16:
        return "Basic16";

    case ColorSupport::Indexed256:
        return "Indexed256";

    case ColorSupport::Rgb24:
        return "Rgb24";

    default:
        return "Unknown";
    }
}

const char* CapabilityReport::toString(RendererFeatureSupport support)
{
    switch (support)
    {
    case RendererFeatureSupport::Unsupported:
        return "Unsupported";

    case RendererFeatureSupport::Supported:
        return "Supported";

    case RendererFeatureSupport::Emulated:
        return "Emulated";

    case RendererFeatureSupport::Unknown:
        return "Unknown";

    default:
        return "Unknown";
    }
}

const char* CapabilityReport::toString(ColorRenderMode mode)
{
    switch (mode)
    {
    case ColorRenderMode::Direct:
        return "Direct";

    case ColorRenderMode::DowngradeToBasic:
        return "DowngradeToBasic";

    case ColorRenderMode::DowngradeToIndexed256:
        return "DowngradeToIndexed256";

    case ColorRenderMode::Omit:
        return "Omit";

    default:
        return "Unknown";
    }
}

const char* CapabilityReport::toString(TextAttributeRenderMode mode)
{
    switch (mode)
    {
    case TextAttributeRenderMode::Direct:
        return "Direct";

    case TextAttributeRenderMode::Approximate:
        return "Approximate";

    case TextAttributeRenderMode::Omit:
        return "Omit";

    default:
        return "Unknown";
    }
}

const char* CapabilityReport::toString(BlinkRenderMode mode)
{
    switch (mode)
    {
    case BlinkRenderMode::Direct:
        return "Direct";

    case BlinkRenderMode::Emulate:
        return "Emulate";

    case BlinkRenderMode::Omit:
        return "Omit";

    default:
        return "Unknown";
    }
}

const char* CapabilityReport::toString(StyleFeature feature)
{
    switch (feature)
    {
    case StyleFeature::ForegroundColor:
        return "ForegroundColor";

    case StyleFeature::BackgroundColor:
        return "BackgroundColor";

    case StyleFeature::Bold:
        return "Bold";

    case StyleFeature::Dim:
        return "Dim";

    case StyleFeature::Underline:
        return "Underline";

    case StyleFeature::SlowBlink:
        return "SlowBlink";

    case StyleFeature::FastBlink:
        return "FastBlink";

    case StyleFeature::Reverse:
        return "Reverse";

    case StyleFeature::Invisible:
        return "Invisible";

    case StyleFeature::Strike:
        return "Strike";

    default:
        return "Unknown";
    }
}

const char* CapabilityReport::toString(StyleAdaptationKind kind)
{
    switch (kind)
    {
    case StyleAdaptationKind::Direct:
        return "Direct";

    case StyleAdaptationKind::Downgraded:
        return "Downgraded";

    case StyleAdaptationKind::Approximated:
        return "Approximated";

    case StyleAdaptationKind::Omitted:
        return "Omitted";

    case StyleAdaptationKind::Emulated:
        return "Emulated";

    case StyleAdaptationKind::LogicalOnly:
        return "LogicalOnly";

    default:
        return "Unknown";
    }
}

const char* CapabilityReport::toString(LogicalStyleValueState state)
{
    switch (state)
    {
    case LogicalStyleValueState::Unspecified:
        return "Unspecified";

    case LogicalStyleValueState::ExplicitlyEnabled:
        return "ExplicitlyEnabled";

    case LogicalStyleValueState::ExplicitlyDisabled:
        return "ExplicitlyDisabled";

    default:
        return "Unknown";
    }
}

const char* CapabilityReport::toString(ColorAdaptationReason reason)
{
    return ColorResolutionDiagnostics::toString(reason);
}

std::string CapabilityReport::formatColor(const Color& color)
{
    std::ostringstream stream;

    if (color.isDefault())
    {
        return "Default";
    }

    if (color.isBasic16())
    {
        stream << "Basic16(" << basicColorName(color.basic()) << ")";
        return stream.str();
    }

    if (color.isIndexed256())
    {
        stream << "Indexed256(" << static_cast<int>(color.index256()) << ")";
        return stream.str();
    }

    if (color.isRgb())
    {
        stream
            << "Rgb24("
            << static_cast<int>(color.red()) << ","
            << static_cast<int>(color.green()) << ","
            << static_cast<int>(color.blue()) << ")";
        return stream.str();
    }

    return "UnknownColor";
}

std::string CapabilityReport::formatAuthoredColor(const Style::StyleColorValue& colorValue)
{
    if (colorValue.hasConcreteColor())
    {
        return std::string("Concrete ") + formatColor(*colorValue.concreteColor());
    }

    if (colorValue.hasThemeColor())
    {
        const ThemeColor& themeColor = *colorValue.themeColor();

        std::ostringstream stream;
        stream << "ThemeColor(";

        bool wroteAny = false;

        if (themeColor.hasBasic())
        {
            stream << "basic=" << formatColor(*themeColor.basic());
            wroteAny = true;
        }

        if (themeColor.hasIndexed())
        {
            if (wroteAny)
            {
                stream << ", ";
            }

            stream << "indexed=" << formatColor(*themeColor.indexed());
            wroteAny = true;
        }

        if (themeColor.hasRgb())
        {
            if (wroteAny)
            {
                stream << ", ";
            }

            stream << "rgb=" << formatColor(*themeColor.rgb());
        }

        stream << ")";
        return stream.str();
    }

    return "Unspecified";
}

void CapabilityReport::increment(StyleFeature feature, StyleAdaptationKind kind)
{
    for (StyleAdaptationCounter& counter : m_counters)
    {
        if (counter.feature == feature && counter.kind == kind)
        {
            ++counter.count;
            return;
        }
    }
}