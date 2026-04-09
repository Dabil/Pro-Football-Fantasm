#include "Screens/TerminalCapabilitiesScreen.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <string>

#include "Core/Rect.h"
#include "Rendering/Capabilities/CapabilityReport.h"
#include "Rendering/Diagnostics/RenderDiagnostics.h"
#include "Rendering/IRenderer.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "Rendering/Styles/Themes.h"
#include "Rendering/Surface.h"

namespace
{
    constexpr int MinimumWidth = 100;
    constexpr int MinimumHeight = 32;

    std::string boolText(bool value)
    {
        return value ? "Yes" : "No";
    }

    std::string featureSupportSummary(RendererFeatureSupport support)
    {
        return CapabilityReport::toString(support);
    }

    std::string clipText(const std::string& text, int width)
    {
        if (width <= 0)
        {
            return {};
        }

        if (static_cast<int>(text.size()) <= width)
        {
            return text;
        }

        if (width <= 3)
        {
            return text.substr(0, static_cast<std::size_t>(width));
        }

        return text.substr(0, static_cast<std::size_t>(width - 3)) + "...";
    }

    void writeClippedLine(
        ScreenBuffer& buffer,
        int x,
        int y,
        int width,
        const std::string& text,
        const Style& style)
    {
        if (width <= 0)
        {
            return;
        }

        buffer.writeString(x, y, clipText(text, width), style);
    }

    void fillPanel(ScreenBuffer& buffer, int x, int y, int width, int height, const Style& style)
    {
        if (width <= 0 || height <= 0)
        {
            return;
        }

        buffer.fillRect(Rect{ Point{ x, y }, Size{ width, height } }, U' ', style);
    }

    void drawPanel(ScreenBuffer& buffer, int x, int y, int width, int height, const std::string& title)
    {
        if (width < 4 || height < 3)
        {
            return;
        }

        buffer.drawFrame(
            Rect{ Point{ x, y }, Size{ width, height } },
            Themes::Frame,
            U'┌', U'┐', U'└', U'┘', U'─', U'│');

        writeClippedLine(buffer, x + 2, y, std::max(0, width - 4), " " + title + " ", Themes::SectionHeader);
    }

    Style swatchStyle(Color::Basic basicColor)
    {
        const bool useDarkForeground =
            basicColor == Color::Basic::White ||
            basicColor == Color::Basic::Yellow ||
            basicColor == Color::Basic::BrightWhite ||
            basicColor == Color::Basic::BrightYellow ||
            basicColor == Color::Basic::BrightCyan ||
            basicColor == Color::Basic::BrightGreen;

        return style::Fg(useDarkForeground
            ? Color::FromBasic(Color::Basic::Black)
            : Color::FromBasic(Color::Basic::BrightWhite))
            + style::Bg(Color::FromBasic(basicColor));
    }

    const char* featureLabel(StyleFeature feature)
    {
        switch (feature)
        {
        case StyleFeature::ForegroundColor: return "Foreground";
        case StyleFeature::BackgroundColor: return "Background";
        case StyleFeature::Bold:            return "Bold";
        case StyleFeature::Dim:             return "Dim";
        case StyleFeature::Underline:       return "Underline";
        case StyleFeature::SlowBlink:       return "SlowBlink";
        case StyleFeature::FastBlink:       return "FastBlink";
        case StyleFeature::Reverse:         return "Reverse";
        case StyleFeature::Invisible:       return "Invisible";
        case StyleFeature::Strike:          return "Strike";
        default:                            return "Unknown";
        }
    }
}

TerminalCapabilitiesScreen::TerminalCapabilitiesScreen(const IRenderer* renderer)
    : m_renderer(renderer)
{
}

void TerminalCapabilitiesScreen::onEnter()
{
    m_elapsedSeconds = 0.0;
}

void TerminalCapabilitiesScreen::update(double deltaTime)
{
    m_elapsedSeconds += deltaTime;
}

void TerminalCapabilitiesScreen::draw(Surface& surface)
{
    ScreenBuffer& buffer = surface.buffer();

    const int screenWidth = buffer.getWidth();
    const int screenHeight = buffer.getHeight();

    if (screenWidth < MinimumWidth || screenHeight < MinimumHeight)
    {
        buffer.writeString(2, 2, "TerminalCapabilitiesScreen needs at least 100x32.", Themes::Warning);
        return;
    }

    const RenderDiagnostics* diagnostics = (m_renderer != nullptr)
        ? m_renderer->renderDiagnostics()
        : nullptr;

    if (diagnostics == nullptr)
    {
        buffer.writeString(2, 2, "No renderer diagnostics are available.", Themes::Warning);
        return;
    }

    const CapabilityReport& report = diagnostics->report();

    buffer.fillRect(
        Rect{ Point{ 0, 0 }, Size{ screenWidth, screenHeight } },
        U' ',
        style::Bg(Color::FromBasic(Color::Basic::Black)));

    buffer.drawFrame(
        Rect{ Point{ 0, 0 }, Size{ screenWidth, screenHeight } },
        Themes::AccentSurface,
        U'╔', U'╗', U'╚', U'╝', U'═', U'║');

    buffer.writeString(3, 0, "[ Terminal Capabilities Validation ]", Themes::PanelTitle);
    buffer.writeString(
        screenWidth - 34,
        0,
        "[ Visual backend verification ]",
        Themes::Info);

    const int leftWidth = (screenWidth / 2) - 2;
    const int rightX = leftWidth + 2;
    const int rightWidth = screenWidth - rightX - 1;

    drawSummaryPanel(buffer, report, 1, 2, screenWidth - 2);
    drawFeatureMatrix(buffer, report, 1, 8, leftWidth, screenHeight - 9);
    drawColorTierPanel(buffer, report, rightX, 8, rightWidth, (screenHeight - 9) / 2 + 2);
    drawTextCapabilityPanel(buffer, rightX, 8 + ((screenHeight - 9) / 2 + 2), rightWidth, screenHeight - (8 + ((screenHeight - 9) / 2 + 2)) - 1);
}

void TerminalCapabilitiesScreen::drawSummaryPanel(
    ScreenBuffer& buffer,
    const CapabilityReport& report,
    int x,
    int y,
    int width) const
{
    drawPanel(buffer, x, y, width, 5, "Active Backend");

    const BackendStateSnapshot& backend = report.backendState();
    const RendererCapabilities& capabilities = report.capabilities();

    const std::string line1 =
        "Renderer=" + backend.rendererIdentity +
        "  Host=" + backend.actualHostKind +
        "  Path=" + backend.activeRenderPath;

    const std::string line2 =
        "VT attempted=" + boolText(backend.virtualTerminalEnableAttempted) +
        "  VT succeeded=" + boolText(backend.virtualTerminalEnableSucceeded) +
        "  VT active=" + boolText(backend.activeRenderPathUsesVirtualTerminalOutput);

    const std::string line3 =
        "Color tier=" + std::string(CapabilityReport::toString(capabilities.colorTier)) +
        "  Bright basic=" + featureSupportSummary(capabilities.brightBasicColors) +
        "  Preserve-safe fallback=" + boolText(capabilities.preserveStyleSafeFallback);

    writeClippedLine(buffer, x + 2, y + 1, width - 4, line1, Themes::Text);
    writeClippedLine(buffer, x + 2, y + 2, width - 4, line2, Themes::Text);
    writeClippedLine(buffer, x + 2, y + 3, width - 4, line3, Themes::Text);
}

void TerminalCapabilitiesScreen::drawFeatureMatrix(
    ScreenBuffer& buffer,
    const CapabilityReport& report,
    int x,
    int y,
    int width,
    int height) const
{
    drawPanel(buffer, x, y, width, height, "Feature Support Matrix");

    const RendererCapabilities& capabilities = report.capabilities();
    const StylePolicy& policy = report.policy();

    struct Row
    {
        StyleFeature feature;
        std::string capability;
        std::string policyMode;
    };

    const std::array<Row, 10> rows =
    { {
        { StyleFeature::ForegroundColor, CapabilityReport::toString(capabilities.colorTier), CapabilityReport::toString(policy.rgbColorMode()) },
        { StyleFeature::BackgroundColor, CapabilityReport::toString(capabilities.colorTier), CapabilityReport::toString(policy.rgbColorMode()) },
        { StyleFeature::Bold,            CapabilityReport::toString(capabilities.bold),      CapabilityReport::toString(policy.boldMode()) },
        { StyleFeature::Dim,             CapabilityReport::toString(capabilities.dim),       CapabilityReport::toString(policy.dimMode()) },
        { StyleFeature::Underline,       CapabilityReport::toString(capabilities.underline), CapabilityReport::toString(policy.underlineMode()) },
        { StyleFeature::SlowBlink,       CapabilityReport::toString(capabilities.slowBlink), CapabilityReport::toString(policy.slowBlinkMode()) },
        { StyleFeature::FastBlink,       CapabilityReport::toString(capabilities.fastBlink), CapabilityReport::toString(policy.fastBlinkMode()) },
        { StyleFeature::Reverse,         CapabilityReport::toString(capabilities.reverse),   CapabilityReport::toString(policy.reverseMode()) },
        { StyleFeature::Invisible,       CapabilityReport::toString(capabilities.invisible), CapabilityReport::toString(policy.invisibleMode()) },
        { StyleFeature::Strike,          CapabilityReport::toString(capabilities.strike),    CapabilityReport::toString(policy.strikeMode()) }
    } };

    writeClippedLine(buffer, x + 2, y + 1, width - 4, "Feature          Capability      Policy", Themes::Focused);

    int rowY = y + 3;
    for (const Row& row : rows)
    {
        if (rowY >= y + height - 7)
        {
            break;
        }

        std::ostringstream line;
        line << featureLabel(row.feature);

        const std::string featureText = line.str();
        std::string paddedFeature = featureText;
        while (paddedFeature.size() < 16)
        {
            paddedFeature.push_back(' ');
        }

        std::string capText = row.capability;
        while (capText.size() < 15)
        {
            capText.push_back(' ');
        }

        writeClippedLine(
            buffer,
            x + 2,
            rowY,
            width - 4,
            paddedFeature + capText + row.policyMode,
            Themes::Text);

        ++rowY;
    }

    const int swatchTop = std::max(rowY + 1, y + height - 6);
    writeClippedLine(buffer, x + 2, swatchTop, width - 4, "Basic 16 palette", Themes::SectionHeader);

    const std::array<Color::Basic, 16> basicColors =
    { {
        Color::Basic::Black,
        Color::Basic::Red,
        Color::Basic::Green,
        Color::Basic::Yellow,
        Color::Basic::Blue,
        Color::Basic::Magenta,
        Color::Basic::Cyan,
        Color::Basic::White,
        Color::Basic::BrightBlack,
        Color::Basic::BrightRed,
        Color::Basic::BrightGreen,
        Color::Basic::BrightYellow,
        Color::Basic::BrightBlue,
        Color::Basic::BrightMagenta,
        Color::Basic::BrightCyan,
        Color::Basic::BrightWhite
    } };

    int swatchX = x + 2;
    int swatchY = swatchTop + 1;

    for (int i = 0; i < 16; ++i)
    {
        const int drawX = swatchX + ((i % 8) * 6);
        const int drawY = swatchY + (i / 8);

        if (drawX + 4 < x + width - 1 && drawY < y + height - 1)
        {
            if (i < 10)
            {
                buffer.writeString(drawX, drawY, "   " + std::to_string(i) + " ", swatchStyle(basicColors[static_cast<std::size_t>(i)]));
            }
            else
            {
                buffer.writeString(drawX, drawY, "  " + std::to_string(i) + " ", swatchStyle(basicColors[static_cast<std::size_t>(i)]));
            }
        }
    }
}

void TerminalCapabilitiesScreen::drawColorTierPanel(
    ScreenBuffer& buffer,
    const CapabilityReport& report,
    int x,
    int y,
    int width,
    int height) const
{
    drawPanel(buffer, x, y, width, height, "Color Tier Visual Checks");

    const RendererCapabilities& capabilities = report.capabilities();

    writeClippedLine(
        buffer,
        x + 2,
        y + 1,
        width - 4,
        "Advertised tier: " + std::string(CapabilityReport::toString(capabilities.colorTier)),
        Themes::Text);

    int row = y + 3;
    writeClippedLine(buffer, x + 2, row++, width - 4, "Indexed 256 sample strip", Themes::SectionHeader);

    const bool showIndexed = capabilities.supportsIndexed256Colors() || capabilities.supportsRgb24();
    if (showIndexed)
    {
        for (int i = 0; i < 24; ++i)
        {
            const int drawX = x + 2 + (i * 2);
            if (drawX + 1 >= x + width - 1)
            {
                break;
            }

            const int colorIndex = 16 + (i * 9);
            buffer.writeString(
                drawX,
                row,
                "  ",
                style::Bg(Color::FromIndexed(colorIndex)));
        }
    }
    else
    {
        writeClippedLine(buffer, x + 2, row, width - 4, "Current backend does not advertise Indexed256.", Themes::MutedText);
    }

    row += 2;
    writeClippedLine(buffer, x + 2, row++, width - 4, "RGB gradient strip", Themes::SectionHeader);

    if (capabilities.supportsRgb24())
    {
        const int usableWidth = std::max(1, std::min(width - 4, 54));
        for (int i = 0; i < usableWidth; ++i)
        {
            const double t = static_cast<double>(i) / std::max(1, usableWidth - 1);
            const int r = static_cast<int>(255.0 * t);
            const int g = static_cast<int>(255.0 * (1.0 - std::abs((t * 2.0) - 1.0)));
            const int b = static_cast<int>(255.0 * (1.0 - t));

            buffer.writeString(
                x + 2 + i,
                row,
                " ",
                style::Bg(Color::FromRgb(r, g, b)));
        }
    }
    else
    {
        writeClippedLine(buffer, x + 2, row, width - 4, "Current backend does not advertise RGB24.", Themes::MutedText);
    }

    row += 2;
    writeClippedLine(buffer, x + 2, row++, width - 4, "Attribute presentation samples", Themes::SectionHeader);

    const Style base = style::Fg(Color::FromBasic(Color::Basic::BrightWhite))
        + style::Bg(Color::FromBasic(Color::Basic::Black));

    const std::array<std::pair<std::string, Style>, 8> samples =
    { {
        { "BOLD",      base + style::Bold },
        { "DIM",       base + style::Dim },
        { "UNDER",     base + style::Underline },
        { "REVERSE",   base + style::Reverse },
        { "STRIKE",    base + style::Strike },
        { "SLOWBLINK", base + style::SlowBlink },
        { "FASTBLINK", base + style::FastBlink },
        { "INVISIBLE?",    base + style::Invisible }
    } };

    int sampleY = row;
    for (const auto& sample : samples)
    {
        if (sampleY >= y + height - 1)
        {
            break;
        }

        buffer.writeString(x + 2, sampleY, sample.first + " ", Themes::MutedText);
        buffer.writeString(x + 14, sampleY, "SampleText", sample.second);
        ++sampleY;
    }
}

void TerminalCapabilitiesScreen::drawTextCapabilityPanel(
    ScreenBuffer& buffer,
    int x,
    int y,
    int width,
    int height) const
{
    drawPanel(buffer, x, y, width, height, "Text Backend Checks");

    if (m_renderer == nullptr)
    {
        writeClippedLine(buffer, x + 2, y + 1, width - 4, "No renderer available.", Themes::Warning);
        return;
    }

    const TextBackendCapabilities textCaps = m_renderer->textCapabilities();

    writeClippedLine(buffer, x + 2, y + 1, width - 4, "UTF-16 output:      " + boolText(textCaps.supportsUtf16Output), Themes::Text);
    writeClippedLine(buffer, x + 2, y + 2, width - 4, "Combining marks:    " + boolText(textCaps.supportsCombiningMarks), Themes::Text);
    writeClippedLine(buffer, x + 2, y + 3, width - 4, "East Asian wide:    " + boolText(textCaps.supportsEastAsianWide), Themes::Text);
    writeClippedLine(buffer, x + 2, y + 4, width - 4, "Emoji:              " + boolText(textCaps.supportsEmoji), Themes::Text);
    writeClippedLine(buffer, x + 2, y + 5, width - 4, "Font fallback:      " + boolText(textCaps.supportsFontFallback), Themes::Text);

    writeClippedLine(buffer, x + 2, y + 7, width - 4, "Sample glyph checks", Themes::SectionHeader);

    buffer.writeString(x + 2, y + 8, "ASCII      : ABC xyz 123 +-*/", Themes::Text);
    buffer.writeString(x + 2, y + 9, "Box drawing: ╔═╦═╗  ╚═╩═╝", Themes::Text);

    if (textCaps.supportsEastAsianWide)
    {
        buffer.writeString(x + 2, y + 10, "Wide chars : 你好 世界", Themes::Text);
    }
    else
    {
        buffer.writeString(x + 2, y + 10, "Wide chars : unsupported / unverified", Themes::MutedText);
    }

    if (textCaps.supportsCombiningMarks)
    {
        buffer.writeString(x + 2, y + 11, "Combining  : e\xCC\x81  a\xCC\x82  o\xCC\x88", Themes::Text);
    }
    else
    {
        buffer.writeString(x + 2, y + 11, "Combining  : unsupported / unverified", Themes::MutedText);
    }

    if (textCaps.supportsEmoji)
    {
        buffer.writeString(x + 2, y + 12, "Emoji      : 🙂 🚀 ⚙️", Themes::Text);
    }
    else
    {
        buffer.writeString(x + 2, y + 12, "Emoji      : unsupported / unverified", Themes::MutedText);
    }
}