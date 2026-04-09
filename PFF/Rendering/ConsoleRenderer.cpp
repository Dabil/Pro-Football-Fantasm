#include "Rendering/ConsoleRenderer.h"

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Rendering/Diagnostics/RenderDiagnosticsWriter.h"
#include "Rendering/Capabilities/ColorSupport.h"
#include "Rendering/Styles/ColorResolver.h"
#include "Utilities/Unicode/UnicodeConversion.h"
#include "Utilities/Unicode/UnicodeWidth.h"
#include "Rendering/Styles/StylePolicyFactory.h"

/*
    ConsoleRenderer is now a backend/output layer.

    Checklist:
        - Remove local codePointToUtf16 ownership
        - Use UnicodeConversion::u32ToUtf16
        - Present only visible leading glyphs
        - Skip continuation glyph rendering
        - Group output into same-style spans

    Phase 2 diagnostics integration:
        - detect backend capabilities
        - build StylePolicy from capabilities
        - record runtime adaptation decisions when authored style is resolved
        - serialize structured diagnostics through RenderDiagnosticsWriter
*/

/*

Phase 2: Diagnostics Update
How the new runtime flow works:

Renderer initialization
- ConsoleRenderer detects console/backend capabilities through ConsoleCapabilityDetector.
- It builds StylePolicy from those detected capabilities.
- It initializes RenderDiagnostics with:
  - detected ConsoleCapabilities
  - resolved StylePolicy
  - cleared runtime counters/examples
- It writes the first structured report through RenderDiagnosticsWriter.

Rendering path
- ScreenBuffer still stores authored logical Style exactly as authored.
- During writeSpan(), ConsoleRenderer calls setStyle(runStyle).
- setStyle():
  - resolves the authored Style through StylePolicy
  - records structured runtime diagnostics into CapabilityReport
  - then maps only the resolved presentation style to Win32 attributes
- Diagnostics therefore describe actual renderer adaptation behavior, not just static capability detection.

Diagnostics semantics
- Direct: authored feature survived unchanged and is physically rendered by the current Win32 path.
- Downgraded: a richer authored color was reduced to a lower tier.
- Approximated: same-tier color changed to a nearest available value.
- Omitted: authored feature was removed by policy.
- Emulated: policy deferred the feature to renderer emulation.
- LogicalOnly: the feature still exists in resolved style state, but this Win32 attribute output path does not physically emit it.

Shutdown
- ConsoleRenderer flushes the final structured diagnostics report through RenderDiagnosticsWriter.
- The old console_style_adaptation_report.txt path is no longer the active implementation.
- The main diagnostics file becomes render_diagnostics_report.txt.

*/

/*
Default behavior
- Diagnostics are now off by default because RenderDiagnostics starts disabled.
- initialize() no longer writes any hardcoded report file.

Enable diagnostics
- Before initialize(), call:
  - renderer->setDiagnosticsEnabled(true);

Choose overwrite behavior
- Before initialize(), call:
  - renderer->setDiagnosticsAppendMode(false);
- The report writer will truncate the file when shutdown() flushes the final report.

Choose append behavior
- Before initialize(), call:
  - renderer->setDiagnosticsAppendMode(true);
- The report writer will append instead of replace.

Choose a custom path
- Before initialize(), call:
  - renderer->setDiagnosticsOutputPath("my_render_report.txt");

Access the full config object directly
- ConsoleRenderer now exposes:
  - diagnostics()
  - diagnostics() const
- That lets you reuse the existing RenderDiagnostics model without inventing a second config system.

Runtime behavior
- Capability detection still happens during initialize().
- The renderer stores capabilities and resolved policy into the structured diagnostics model.
- Runtime adaptation events are recorded only when diagnostics are enabled.
- The report is written once at shutdown() through RenderDiagnosticsWriter.

Failure behavior
- If report writing fails, rendering still proceeds normally.
- Diagnostics remain fully separate from visible console output and do not alter ScreenBuffer or authored Style data.

A typical call site now looks like this before initialize():

auto renderer = std::make_unique<ConsoleRenderer>();
renderer->setDiagnosticsEnabled(true);
renderer->setDiagnosticsAppendMode(false);
renderer->setDiagnosticsOutputPath("render_diagnostics_report.txt");
*/

namespace
{
    constexpr WORD kForegroundMask =
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;

    constexpr WORD kBackgroundMask =
        BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY;

    constexpr std::size_t kMaxRecordedExamples = 24;
    constexpr std::size_t kMaxExamplesPerFeatureKind = 3;

    WORD basicForegroundBits(Color::Basic color)
    {
        switch (color)
        {
        case Color::Basic::Black:         return 0;
        case Color::Basic::Red:           return FOREGROUND_RED;
        case Color::Basic::Green:         return FOREGROUND_GREEN;
        case Color::Basic::Yellow:        return FOREGROUND_RED | FOREGROUND_GREEN;
        case Color::Basic::Blue:          return FOREGROUND_BLUE;
        case Color::Basic::Magenta:       return FOREGROUND_RED | FOREGROUND_BLUE;
        case Color::Basic::Cyan:          return FOREGROUND_GREEN | FOREGROUND_BLUE;
        case Color::Basic::White:         return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

        case Color::Basic::BrightBlack:   return FOREGROUND_INTENSITY;
        case Color::Basic::BrightRed:     return FOREGROUND_RED | FOREGROUND_INTENSITY;
        case Color::Basic::BrightGreen:   return FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case Color::Basic::BrightYellow:  return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case Color::Basic::BrightBlue:    return FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        case Color::Basic::BrightMagenta: return FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        case Color::Basic::BrightCyan:    return FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        case Color::Basic::BrightWhite:   return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        default:                          return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        }
    }

    WORD basicBackgroundBits(Color::Basic color)
    {
        switch (color)
        {
        case Color::Basic::Black:         return 0;
        case Color::Basic::Red:           return BACKGROUND_RED;
        case Color::Basic::Green:         return BACKGROUND_GREEN;
        case Color::Basic::Yellow:        return BACKGROUND_RED | BACKGROUND_GREEN;
        case Color::Basic::Blue:          return BACKGROUND_BLUE;
        case Color::Basic::Magenta:       return BACKGROUND_RED | BACKGROUND_BLUE;
        case Color::Basic::Cyan:          return BACKGROUND_GREEN | BACKGROUND_BLUE;
        case Color::Basic::White:         return BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;

        case Color::Basic::BrightBlack:   return BACKGROUND_INTENSITY;
        case Color::Basic::BrightRed:     return BACKGROUND_RED | BACKGROUND_INTENSITY;
        case Color::Basic::BrightGreen:   return BACKGROUND_GREEN | BACKGROUND_INTENSITY;
        case Color::Basic::BrightYellow:  return BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_INTENSITY;
        case Color::Basic::BrightBlue:    return BACKGROUND_BLUE | BACKGROUND_INTENSITY;
        case Color::Basic::BrightMagenta: return BACKGROUND_RED | BACKGROUND_BLUE | BACKGROUND_INTENSITY;
        case Color::Basic::BrightCyan:    return BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY;
        case Color::Basic::BrightWhite:   return BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | BACKGROUND_INTENSITY;
        default:                          return 0;
        }
    }

    WORD swapForegroundToBackground(WORD fg)
    {
        WORD bg = 0;

        if (fg & FOREGROUND_RED)       bg |= BACKGROUND_RED;
        if (fg & FOREGROUND_GREEN)     bg |= BACKGROUND_GREEN;
        if (fg & FOREGROUND_BLUE)      bg |= BACKGROUND_BLUE;
        if (fg & FOREGROUND_INTENSITY) bg |= BACKGROUND_INTENSITY;

        return bg;
    }

    WORD swapBackgroundToForeground(WORD bg)
    {
        WORD fg = 0;

        if (bg & BACKGROUND_RED)       fg |= FOREGROUND_RED;
        if (bg & BACKGROUND_GREEN)     fg |= FOREGROUND_GREEN;
        if (bg & BACKGROUND_BLUE)      fg |= FOREGROUND_BLUE;
        if (bg & BACKGROUND_INTENSITY) fg |= FOREGROUND_INTENSITY;

        return fg;
    }

    TextAttributeRenderMode textAttributeModeFromSupport(RendererFeatureSupport support)
    {
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
    }

    StylePolicy buildStylePolicyFromCapabilities(const RendererCapabilities& capabilities)
    {
        return StylePolicyFactory::buildFromCapabilities(
            capabilities,
            RendererStylePolicyProfile::ConsoleAttributeBackend);
    }

    WORD resolvedStyleToAttributes(const Style& style, WORD defaultAttributes)
    {
        WORD attributes = defaultAttributes;

        if (style.hasForeground() && style.foreground()->isBasic())
        {
            attributes &= ~kForegroundMask;
            attributes |= basicForegroundBits(style.foreground()->basic());
        }

        if (style.hasBackground() && style.background()->isBasic())
        {
            attributes &= ~kBackgroundMask;
            attributes |= basicBackgroundBits(style.background()->basic());
        }

        if (style.bold())
        {
            attributes |= FOREGROUND_INTENSITY;
        }

        if (style.dim())
        {
            attributes &= ~FOREGROUND_INTENSITY;
        }

        if (style.reverse())
        {
            const WORD fg = attributes & kForegroundMask;
            const WORD bg = attributes & kBackgroundMask;

            attributes &= ~kForegroundMask;
            attributes &= ~kBackgroundMask;

            attributes |= swapBackgroundToForeground(bg);
            attributes |= swapForegroundToBackground(fg);
            attributes |= COMMON_LVB_REVERSE_VIDEO;
        }
        else
        {
            attributes &= ~COMMON_LVB_REVERSE_VIDEO;
        }

        if (style.underline())
        {
            attributes |= COMMON_LVB_UNDERSCORE;
        }
        else
        {
            attributes &= ~COMMON_LVB_UNDERSCORE;
        }

        if (style.invisible())
        {
            const WORD bg = attributes & kBackgroundMask;
            attributes &= ~kForegroundMask;
            attributes |= swapBackgroundToForeground(bg);
        }

        return attributes;
    }

    bool isContinuationCell(const ScreenCell& cell)
    {
        return cell.kind == CellKind::WideTrailing ||
            cell.kind == CellKind::CombiningContinuation;
    }

    char32_t cellToPresentedGlyph(const ScreenCell& cell)
    {
        if (isContinuationCell(cell))
        {
            return U'\0';
        }

        if (cell.kind == CellKind::Glyph)
        {
            return cell.glyph;
        }

        return U' ';
    }

    const char* basicColorToString(Color::Basic color)
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

    std::string colorToString(const std::optional<Color>& color)
    {
        if (!color.has_value())
        {
            return "None";
        }

        if (color->isDefault())
        {
            return "Default";
        }

        std::ostringstream stream;

        if (color->isBasic())
        {
            stream << "Basic(" << basicColorToString(color->basic()) << ")";
            return stream.str();
        }

        if (color->isIndexed256())
        {
            stream << "Indexed256(" << static_cast<int>(color->index256()) << ")";
            return stream.str();
        }

        if (color->isTrueColor())
        {
            stream
                << "TrueColor("
                << static_cast<int>(color->red()) << ","
                << static_cast<int>(color->green()) << ","
                << static_cast<int>(color->blue()) << ")";
            return stream.str();
        }

        return "UnknownColor";
    }

    bool rendererPhysicallyRendersColor(const std::optional<Color>& color)
    {
        return color.has_value() && (color->isDefault() || color->isBasic());
    }

    LogicalStyleValueState logicalStateFromAttributeState(const Style::AttributeState& state)
    {
        if (!state.has_value())
        {
            return LogicalStyleValueState::Unspecified;
        }

        return *state
            ? LogicalStyleValueState::ExplicitlyEnabled
            : LogicalStyleValueState::ExplicitlyDisabled;
    }

    bool shouldCaptureExample(const CapabilityReport& report, StyleFeature feature, StyleAdaptationKind kind)
    {
        return report.examples().size() < kMaxRecordedExamples &&
            report.getCount(feature, kind) <= kMaxExamplesPerFeatureKind;
    }

    bool shouldCaptureLogicalStateExample(
        const CapabilityReport& report,
        StyleFeature feature,
        LogicalStyleValueState logicalState)
    {
        std::size_t matchingExamples = 0;

        for (const StyleLogicalStateExample& example : report.logicalStateExamples())
        {
            if (example.feature == feature && example.logicalState == logicalState)
            {
                ++matchingExamples;
            }
        }

        return report.logicalStateExamples().size() < kMaxRecordedExamples &&
            matchingExamples < 1;
    }

    std::string buildColorExampleDetail(
        const char* label,
        const std::optional<Color>& authoredColor,
        const std::optional<Color>& presentedColor)
    {
        std::ostringstream detail;
        detail
            << label
            << " authored=" << colorToString(authoredColor)
            << ", presented=" << colorToString(presentedColor);
        return detail.str();
    }

    std::string buildTextExampleDetail(
        const char* label,
        LogicalStyleValueState logicalState,
        bool presentedEnabled)
    {
        std::ostringstream detail;
        detail
            << label
            << " logicalState=" << CapabilityReport::toString(logicalState)
            << ", presented=" << (presentedEnabled ? "true" : "false");
        return detail.str();
    }
}

ConsoleRenderer::ConsoleRenderer()
    : m_blinkEpoch(std::chrono::steady_clock::now())
{
}

ConsoleRenderer::~ConsoleRenderer()
{
    shutdown();
}

bool ConsoleRenderer::maximizeConsole()
{
    if (m_hOut == INVALID_HANDLE_VALUE || m_hOut == nullptr)
    {
        return false;
    }

    COORD largestSize = GetLargestConsoleWindowSize(m_hOut);
    if (largestSize.X == 0 || largestSize.Y == 0)
    {
        return false;
    }

    COORD bufferSize{};
    bufferSize.X = largestSize.X;
    bufferSize.Y = largestSize.Y;

    if (!SetConsoleScreenBufferSize(m_hOut, bufferSize))
    {
        return false;
    }

    SMALL_RECT windowRect{};
    windowRect.Left = 0;
    windowRect.Top = 0;
    windowRect.Right = largestSize.X - 1;
    windowRect.Bottom = largestSize.Y - 1;

    if (!SetConsoleWindowInfo(m_hOut, TRUE, &windowRect))
    {
        return false;
    }

    HWND hwnd = GetConsoleWindow();
    if (hwnd != nullptr)
    {
        ShowWindow(hwnd, SW_MAXIMIZE);
    }

    return true;
}

bool ConsoleRenderer::initialize()
{
    if (m_initialized)
    {
        return true;
    }

    m_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    m_hIn = GetStdHandle(STD_INPUT_HANDLE);

    if (m_hOut == INVALID_HANDLE_VALUE || m_hOut == nullptr)
    {
        return false;
    }

    if (m_hIn == INVALID_HANDLE_VALUE || m_hIn == nullptr)
    {
        return false;
    }

    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(m_hOut, &info))
    {
        return false;
    }

    m_defaultAttributes = info.wAttributes;
    m_originalOutputCodePage = GetConsoleOutputCP();
    m_originalInputCodePage = GetConsoleCP();

    DWORD outMode = 0;
    if (GetConsoleMode(m_hOut, &outMode))
    {
        m_originalOutputMode = outMode;
        m_haveOriginalOutputMode = true;
    }

    DWORD inMode = 0;
    if (GetConsoleMode(m_hIn, &inMode))
    {
        m_originalInputMode = inMode;
        m_haveOriginalInputMode = true;
    }

    CONSOLE_CURSOR_INFO cursorInfo{};
    if (GetConsoleCursorInfo(m_hOut, &cursorInfo))
    {
        m_cursorWasVisible = (cursorInfo.bVisible != FALSE);
    }

    if (!configureConsole())
    {
        restoreConsoleState();
        return false;
    }

    maximizeConsole();

    if (!queryVisibleConsoleSize(m_consoleWidth, m_consoleHeight))
    {
        m_consoleWidth = 120;
        m_consoleHeight = 40;
    }

    m_previousFrame.resize(m_consoleWidth, m_consoleHeight);
    m_previousFrame.clear();

    m_stylePolicy = buildStylePolicyFromCapabilities(m_capabilities);
    initializeDiagnosticsState();

    m_currentStyle = Style{};
    m_firstPresent = true;
    m_blinkEpoch = std::chrono::steady_clock::now();
    m_lastSlowBlinkVisibilityState = true;
    m_lastFastBlinkVisibilityState = true;
    m_lastFrameUsedBlinkEmulation = false;
    m_initialized = true;

    flushDiagnosticsReport();

    return true;
}

void ConsoleRenderer::shutdown()
{
    if (!m_initialized)
    {
        return;
    }

    flushDiagnosticsReport();
    resetStyle();
    restoreConsoleState();

    m_initialized = false;
    m_firstPresent = true;
    m_currentStyle = Style{};
    m_lastSlowBlinkVisibilityState = true;
    m_lastFastBlinkVisibilityState = true;
    m_lastFrameUsedBlinkEmulation = false;
}

void ConsoleRenderer::present(const ScreenBuffer& frame)
{
    if (!m_initialized)
    {
        throw std::runtime_error("ConsoleRenderer::present called before initialize().");
    }

    const bool forceFullPresentForBlink = shouldForceFullPresentForBlink(frame);

    if (m_firstPresent ||
        forceFullPresentForBlink ||
        m_previousFrame.getWidth() != frame.getWidth() ||
        m_previousFrame.getHeight() != frame.getHeight())
    {
        m_previousFrame.resize(frame.getWidth(), frame.getHeight());
        m_previousFrame.clear();

        writeFullFrame(frame);

        m_previousFrame = frame;
        m_firstPresent = false;
        return;
    }

    writeDirtySpans(frame);
    m_previousFrame = frame;
}

void ConsoleRenderer::resize(int width, int height)
{
    m_consoleWidth = width;
    m_consoleHeight = height;

    m_previousFrame.resize(width, height);
    m_previousFrame.clear();

    m_firstPresent = true;
    m_currentStyle = Style{};
}

bool ConsoleRenderer::pollResize()
{
    int newWidth = 0;
    int newHeight = 0;

    if (!queryVisibleConsoleSize(newWidth, newHeight))
    {
        return false;
    }

    if (newWidth == m_consoleWidth && newHeight == m_consoleHeight)
    {
        return false;
    }

    resize(newWidth, newHeight);
    return true;
}

int ConsoleRenderer::getConsoleWidth() const
{
    return m_consoleWidth;
}

int ConsoleRenderer::getConsoleHeight() const
{
    return m_consoleHeight;
}

TextBackendCapabilities ConsoleRenderer::textCapabilities() const
{
    TextBackendCapabilities capabilities;
    capabilities.supportsUtf16Output = true;
    capabilities.supportsCombiningMarks = false;
    capabilities.supportsEastAsianWide = true;
    capabilities.supportsEmoji = false;
    capabilities.supportsFontFallback = false;
    return capabilities;
}

void ConsoleRenderer::setDiagnosticsEnabled(bool enabled)
{
    m_renderDiagnostics.setEnabled(enabled);
}

bool ConsoleRenderer::diagnosticsEnabled() const
{
    return m_renderDiagnostics.isEnabled();
}

void ConsoleRenderer::setDiagnosticsOutputPath(const std::string& outputPath)
{
    m_renderDiagnostics.setOutputPath(outputPath);
}

const std::string& ConsoleRenderer::diagnosticsOutputPath() const
{
    return m_renderDiagnostics.outputPath();
}

void ConsoleRenderer::setDiagnosticsAppendMode(bool appendMode)
{
    m_renderDiagnostics.setAppendMode(appendMode);
}

void ConsoleRenderer::setStartupDiagnosticsContext(const StartupDiagnosticsContext& startupDiagnostics)
{
    m_startupDiagnostics = startupDiagnostics;
}

bool ConsoleRenderer::diagnosticsAppendMode() const
{
    return m_renderDiagnostics.appendMode();
}

RenderDiagnostics& ConsoleRenderer::diagnostics()
{
    return m_renderDiagnostics;
}

const RenderDiagnostics& ConsoleRenderer::diagnostics() const
{
    return m_renderDiagnostics;
}

void ConsoleRenderer::writeFullFrame(const ScreenBuffer& frame)
{
    for (int y = 0; y < frame.getHeight(); ++y)
    {
        writeSpan(frame, y, 0, frame.getWidth() - 1);
    }
}

void ConsoleRenderer::writeDirtySpans(const ScreenBuffer& frame)
{
    const std::vector<DirtySpan> spans = FrameDiff::diffRows(m_previousFrame, frame);

    for (const DirtySpan& span : spans)
    {
        writeSpan(frame, span.y, span.xStart, span.xEnd);
    }
}

void ConsoleRenderer::writeSpan(const ScreenBuffer& frame, int y, int xStart, int xEnd)
{
    if (y < 0 || y >= frame.getHeight())
    {
        return;
    }

    if (frame.getWidth() <= 0)
    {
        return;
    }

    if (xStart < 0)
    {
        xStart = 0;
    }

    if (xEnd >= frame.getWidth())
    {
        xEnd = frame.getWidth() - 1;
    }

    if (xStart > xEnd)
    {
        return;
    }

    int x = xStart;

    while (x <= xEnd)
    {
        const ScreenCell& firstCell = frame.getCell(x, y);
        if (isContinuationCell(firstCell))
        {
            ++x;
            continue;
        }

        const Style runStyle = firstCell.style;
        const int runStart = x;

        std::u32string runText;
        std::vector<CellWidth> runGlyphWidths;

        runText.reserve(static_cast<std::size_t>(xEnd - runStart + 1));
        runGlyphWidths.reserve(static_cast<std::size_t>(xEnd - runStart + 1));

        while (x <= xEnd)
        {
            const ScreenCell& cell = frame.getCell(x, y);
            if (cell.style != runStyle)
            {
                break;
            }

            if (!isContinuationCell(cell))
            {
                const char32_t glyph = cellToPresentedGlyph(cell);
                if (glyph != U'\0')
                {
                    runText.push_back(glyph);
                    runGlyphWidths.push_back(UnicodeWidth::measureCodePointWidth(glyph));
                }
            }

            ++x;
        }

        const ResolvedStyle resolvedRunStyle = m_stylePolicy.resolve(runStyle);
        if (!isBlinkVisibleForResolvedStyle(resolvedRunStyle))
        {
            std::u32string maskedText;
            maskedText.reserve(runText.size() * 2);

            for (std::size_t i = 0; i < runText.size(); ++i)
            {
                const char32_t glyph = runText[i];
                const CellWidth width = runGlyphWidths[i];

                if (glyph == U' ')
                {
                    maskedText.push_back(U' ');
                    continue;
                }

                if (width == CellWidth::Two)
                {
                    maskedText.push_back(U' ');
                    maskedText.push_back(U' ');
                    continue;
                }

                maskedText.push_back(U' ');
            }

            runText = std::move(maskedText);
        }

        moveCursor(runStart, y);
        setResolvedStyle(runStyle, resolvedRunStyle);

        if (!runText.empty())
        {
            const std::wstring utf16 = UnicodeConversion::u32ToUtf16(runText);

            if (!utf16.empty())
            {
                DWORD written = 0;
                WriteConsoleW(
                    m_hOut,
                    utf16.data(),
                    static_cast<DWORD>(utf16.size()),
                    &written,
                    nullptr);
            }
        }
    }
}

bool ConsoleRenderer::shouldForceFullPresentForBlink(const ScreenBuffer& frame)
{
    bool usesSlowBlinkEmulation = false;
    bool usesFastBlinkEmulation = false;
    collectBlinkEmulationUsage(frame, usesSlowBlinkEmulation, usesFastBlinkEmulation);

    const bool usesBlinkEmulation = usesSlowBlinkEmulation || usesFastBlinkEmulation;
    const bool currentSlowBlinkVisibilityState = usesSlowBlinkEmulation
        ? isBlinkCurrentlyVisible(false)
        : true;
    const bool currentFastBlinkVisibilityState = usesFastBlinkEmulation
        ? isBlinkCurrentlyVisible(true)
        : true;

    if (!usesBlinkEmulation)
    {
        m_lastFrameUsedBlinkEmulation = false;
        m_lastSlowBlinkVisibilityState = true;
        m_lastFastBlinkVisibilityState = true;
        return false;
    }

    const bool slowBlinkChanged =
        usesSlowBlinkEmulation &&
        currentSlowBlinkVisibilityState != m_lastSlowBlinkVisibilityState;

    const bool fastBlinkChanged =
        usesFastBlinkEmulation &&
        currentFastBlinkVisibilityState != m_lastFastBlinkVisibilityState;

    const bool shouldForce =
        !m_firstPresent &&
        m_lastFrameUsedBlinkEmulation &&
        (slowBlinkChanged || fastBlinkChanged);

    m_lastFrameUsedBlinkEmulation = true;
    m_lastSlowBlinkVisibilityState = currentSlowBlinkVisibilityState;
    m_lastFastBlinkVisibilityState = currentFastBlinkVisibilityState;

    return shouldForce;
}

void ConsoleRenderer::collectBlinkEmulationUsage(
    const ScreenBuffer& frame,
    bool& usesSlowBlinkEmulation,
    bool& usesFastBlinkEmulation) const
{
    usesSlowBlinkEmulation = false;
    usesFastBlinkEmulation = false;

    for (int y = 0; y < frame.getHeight(); ++y)
    {
        for (int x = 0; x < frame.getWidth(); ++x)
        {
            const ScreenCell& cell = frame.getCell(x, y);
            const ResolvedStyle resolved = m_stylePolicy.resolve(cell.style);

            usesSlowBlinkEmulation = usesSlowBlinkEmulation || resolved.emulateSlowBlink;
            usesFastBlinkEmulation = usesFastBlinkEmulation || resolved.emulateFastBlink;

            if (usesSlowBlinkEmulation && usesFastBlinkEmulation)
            {
                return;
            }
        }
    }
}

bool ConsoleRenderer::isBlinkVisibleForResolvedStyle(const ResolvedStyle& resolvedStyle) const
{
    if (resolvedStyle.emulateFastBlink && !isBlinkCurrentlyVisible(true))
    {
        return false;
    }

    if (resolvedStyle.emulateSlowBlink && !isBlinkCurrentlyVisible(false))
    {
        return false;
    }

    return true;
}

bool ConsoleRenderer::isBlinkCurrentlyVisible(bool fastBlink) const
{
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = now - m_blinkEpoch;

    const double cycleSeconds = fastBlink ? 0.5 : 1.0;
    const double phase = std::fmod(elapsed.count(), cycleSeconds);

    return phase < (cycleSeconds * 0.5);
}

void ConsoleRenderer::moveCursor(int x, int y)
{
    COORD pos{};
    pos.X = static_cast<SHORT>(x);
    pos.Y = static_cast<SHORT>(y);

    SetConsoleCursorPosition(m_hOut, pos);
}

void ConsoleRenderer::setStyle(const Style& style)
{
    setResolvedStyle(style, m_stylePolicy.resolve(style));
}

void ConsoleRenderer::setResolvedStyle(const Style& authoredStyle, const ResolvedStyle& resolvedStyle)
{
    const Style& presentedStyle = resolvedStyle.presentedStyle;

    recordStyleUsage(authoredStyle, resolvedStyle);

    if (presentedStyle == m_currentStyle)
    {
        return;
    }

    const WORD attributes = resolvedStyleToAttributes(presentedStyle, m_defaultAttributes);
    SetConsoleTextAttribute(m_hOut, attributes);

    m_currentStyle = presentedStyle;
}

void ConsoleRenderer::resetStyle()
{
    SetConsoleTextAttribute(m_hOut, m_defaultAttributes);
    m_currentStyle = Style{};
}

bool ConsoleRenderer::queryVisibleConsoleSize(int& width, int& height) const
{
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(m_hOut, &info))
    {
        return false;
    }

    width = static_cast<int>(info.srWindow.Right - info.srWindow.Left + 1);
    height = static_cast<int>(info.srWindow.Bottom - info.srWindow.Top + 1);

    return width > 0 && height > 0;
}

bool ConsoleRenderer::configureConsole()
{
    if (!SetConsoleOutputCP(CP_UTF8))
    {
        return false;
    }

    if (!SetConsoleCP(CP_UTF8))
    {
        return false;
    }

    const ConsoleCapabilityDetectionResult detection =
        ConsoleCapabilityDetector::detectAndConfigure(
            m_hOut,
            m_hIn,
            m_originalOutputMode,
            m_originalInputMode,
            m_haveOriginalOutputMode,
            m_haveOriginalInputMode);

    if (m_haveOriginalOutputMode && !detection.hasConfiguredOutputMode)
    {
        return false;
    }

    if (m_haveOriginalInputMode && !detection.hasConfiguredInputMode)
    {
        return false;
    }

    m_capabilities = detection.capabilities;

    m_virtualTerminalEnabled = detection.virtualTerminalWasEnabled;
    m_virtualTerminalEnableAttempted = detection.virtualTerminalEnableAttempted;
    m_virtualTerminalEnableSucceeded = detection.virtualTerminalEnableSucceeded;

    m_configuredOutputMode = detection.configuredOutputMode;
    m_configuredInputMode = detection.configuredInputMode;
    m_haveConfiguredOutputMode = detection.hasConfiguredOutputMode;
    m_haveConfiguredInputMode = detection.hasConfiguredInputMode;

    CONSOLE_CURSOR_INFO cursorInfo{};
    if (GetConsoleCursorInfo(m_hOut, &cursorInfo))
    {
        cursorInfo.bVisible = FALSE;
        SetConsoleCursorInfo(m_hOut, &cursorInfo);
    }

    return true;
}

void ConsoleRenderer::restoreConsoleState()
{
    if (m_hOut != INVALID_HANDLE_VALUE && m_hOut != nullptr)
    {
        SetConsoleTextAttribute(m_hOut, m_defaultAttributes);

        if (m_haveOriginalOutputMode)
        {
            SetConsoleMode(m_hOut, m_originalOutputMode);
        }

        CONSOLE_CURSOR_INFO cursorInfo{};
        if (GetConsoleCursorInfo(m_hOut, &cursorInfo))
        {
            cursorInfo.bVisible = m_cursorWasVisible ? TRUE : FALSE;
            SetConsoleCursorInfo(m_hOut, &cursorInfo);
        }
    }

    if (m_hIn != INVALID_HANDLE_VALUE && m_hIn != nullptr)
    {
        if (m_haveOriginalInputMode)
        {
            SetConsoleMode(m_hIn, m_originalInputMode);
        }
    }

    if (m_originalOutputCodePage != 0)
    {
        SetConsoleOutputCP(m_originalOutputCodePage);
    }

    if (m_originalInputCodePage != 0)
    {
        SetConsoleCP(m_originalInputCodePage);
    }
}

void ConsoleRenderer::initializeDiagnosticsState()
{
    m_renderDiagnostics.resetRuntimeData();

    BackendStateSnapshot backendState;
    backendState.startupConfigSource = m_startupDiagnostics.startupConfigSource;
    backendState.startupConfigFileFound = m_startupDiagnostics.startupConfigFileFound;
    backendState.configuredHostPreference = toString(m_startupDiagnostics.configuredHostPreference);
    backendState.configuredRendererPreference = toString(m_startupDiagnostics.configuredRendererPreference);

    backendState.requestedHostKind = toString(m_startupDiagnostics.requestedHost);
    backendState.actualHostKind = toString(m_startupDiagnostics.actualHost);
    backendState.requestedRendererIdentity = toString(m_startupDiagnostics.requestedRenderer);

    if (m_startupDiagnostics.actualRenderer != RendererKind::Unknown)
    {
        backendState.rendererIdentity = toString(m_startupDiagnostics.actualRenderer);
    }
    else
    {
        backendState.rendererIdentity = m_rendererIdentity;
    }

    backendState.activeRenderPath = "BasicWin32AttributePath";

    backendState.relaunchAttempted = m_startupDiagnostics.relaunchAttempted;
    backendState.relaunchPerformed = m_startupDiagnostics.relaunchPerformed;
    backendState.launchedByWindowsTerminalFlag = m_startupDiagnostics.launchedByWindowsTerminalFlag;
    backendState.windowsTerminalSessionHint = m_startupDiagnostics.windowsTerminalSessionHint;

    backendState.virtualTerminalEnableAttempted = m_virtualTerminalEnableAttempted;
    backendState.virtualTerminalEnableSucceeded = m_virtualTerminalEnableSucceeded;
    backendState.virtualTerminalProcessingActive = m_virtualTerminalEnableSucceeded;
    backendState.activeRenderPathUsesVirtualTerminalOutput = false;

    backendState.configuredOutputMode = 0;
    backendState.configuredInputMode = 0;
    backendState.hasConfiguredOutputMode = false;
    backendState.hasConfiguredInputMode = false;

    CapabilityReport& report = m_renderDiagnostics.report();
    report.setCapabilities(m_capabilities);
    report.setPolicy(m_stylePolicy);
    report.setBackendState(backendState);
}

void ConsoleRenderer::flushDiagnosticsReport() const
{
    if (!m_renderDiagnostics.isEnabled())
    {
        return;
    }

    const bool wroteSuccessfully = RenderDiagnosticsWriter::write(m_renderDiagnostics);
    (void)wroteSuccessfully;
}


void ConsoleRenderer::recordStyleUsage(const Style& authoredStyle, const ResolvedStyle& resolvedStyle)
{
    if (!m_renderDiagnostics.isEnabled())
    {
        return;
    }

    const Style& presentedStyle = resolvedStyle.presentedStyle;

    recordColorFeature(
        StyleFeature::ForegroundColor,
        resolvedStyle.foregroundColorDiagnostics);

    recordColorFeature(
        StyleFeature::BackgroundColor,
        resolvedStyle.backgroundColorDiagnostics);

    recordTextFeature(
        StyleFeature::Bold,
        authoredStyle.boldState(),
        presentedStyle.bold(),
        presentedStyle.bold(),
        m_stylePolicy.boldMode());

    recordTextFeature(
        StyleFeature::Dim,
        authoredStyle.dimState(),
        presentedStyle.dim(),
        presentedStyle.dim(),
        m_stylePolicy.dimMode());

    recordTextFeature(
        StyleFeature::Underline,
        authoredStyle.underlineState(),
        presentedStyle.underline(),
        presentedStyle.underline(),
        m_stylePolicy.underlineMode());

    recordBlinkFeature(
        StyleFeature::SlowBlink,
        authoredStyle.slowBlinkState(),
        presentedStyle.slowBlink(),
        resolvedStyle.emulateSlowBlink,
        false);

    recordBlinkFeature(
        StyleFeature::FastBlink,
        authoredStyle.fastBlinkState(),
        presentedStyle.fastBlink(),
        resolvedStyle.emulateFastBlink,
        false);

    recordTextFeature(
        StyleFeature::Reverse,
        authoredStyle.reverseState(),
        presentedStyle.reverse(),
        presentedStyle.reverse(),
        m_stylePolicy.reverseMode());

    recordTextFeature(
        StyleFeature::Invisible,
        authoredStyle.invisibleState(),
        presentedStyle.invisible(),
        presentedStyle.invisible(),
        m_stylePolicy.invisibleMode());

    recordTextFeature(
        StyleFeature::Strike,
        authoredStyle.strikeState(),
        presentedStyle.strike(),
        false,
        m_stylePolicy.strikeMode());
}

void ConsoleRenderer::recordColorFeature(
    StyleFeature feature,
    const std::optional<ColorResolutionDiagnostics>& diagnostics)
{
    if (!diagnostics.has_value())
    {
        return;
    }

    CapabilityReport& report = m_renderDiagnostics.report();
    const std::optional<Color>& presentedColor = diagnostics->resolvedColor;

    if (diagnostics->wasOmitted())
    {
        report.recordOmitted(feature);

        if (shouldCaptureExample(report, feature, StyleAdaptationKind::Omitted))
        {
            report.addColorAdaptationExample(
                feature,
                StyleAdaptationKind::Omitted,
                *diagnostics);

            report.addExample(
                feature,
                StyleAdaptationKind::Omitted,
                "Color omitted after resolver/policy adaptation.");
        }

        return;
    }

    if (diagnostics->wasDowngraded())
    {
        report.recordDowngraded(feature);

        if (shouldCaptureExample(report, feature, StyleAdaptationKind::Downgraded))
        {
            report.addColorAdaptationExample(
                feature,
                StyleAdaptationKind::Downgraded,
                *diagnostics);

            report.addExample(
                feature,
                StyleAdaptationKind::Downgraded,
                "Color downgraded by resolver according to supported tier.");
        }
    }
    else
    {
        if (!rendererPhysicallyRendersColor(presentedColor))
        {
            report.recordLogicalOnly(feature);

            if (shouldCaptureExample(report, feature, StyleAdaptationKind::LogicalOnly))
            {
                report.addColorAdaptationExample(
                    feature,
                    StyleAdaptationKind::LogicalOnly,
                    *diagnostics);

                report.addExample(
                    feature,
                    StyleAdaptationKind::LogicalOnly,
                    "Resolved color remained logical-only on the active Win32 attribute path.");
            }
        }
        else
        {
            report.recordDirect(feature);

            if (shouldCaptureExample(report, feature, StyleAdaptationKind::Direct))
            {
                report.addColorAdaptationExample(
                    feature,
                    StyleAdaptationKind::Direct,
                    *diagnostics);
            }
        }
    }

    if (presentedColor.has_value() && !rendererPhysicallyRendersColor(presentedColor))
    {
        report.recordLogicalOnly(feature);

        if (shouldCaptureExample(report, feature, StyleAdaptationKind::LogicalOnly))
        {
            report.addExample(
                feature,
                StyleAdaptationKind::LogicalOnly,
                "Presented color is resolved but not directly emitted by the Win32 attribute path.");
        }
    }
}

void ConsoleRenderer::recordTextFeature(
    StyleFeature feature,
    const Style::AttributeState& authoredState,
    bool presentedEnabled,
    bool physicallyRendered,
    TextAttributeRenderMode renderMode)
{
    CapabilityReport& report = m_renderDiagnostics.report();
    const LogicalStyleValueState logicalState = logicalStateFromAttributeState(authoredState);

    if (shouldCaptureLogicalStateExample(report, feature, logicalState))
    {
        std::ostringstream detail;
        detail
            << "Logical field observed with state="
            << CapabilityReport::toString(logicalState)
            << ", presented=" << (presentedEnabled ? "true" : "false");
        report.addLogicalStateExample(feature, logicalState, detail.str());
    }

    if (!authoredState.has_value())
    {
        return;
    }

    if (!*authoredState)
    {
        if (shouldCaptureExample(report, feature, StyleAdaptationKind::Direct))
        {
            report.addExample(
                feature,
                StyleAdaptationKind::Direct,
                buildTextExampleDetail("Explicit disable preserved", logicalState, presentedEnabled),
                logicalState);
        }

        return;
    }

    if (!presentedEnabled)
    {
        report.recordOmitted(feature);

        if (shouldCaptureExample(report, feature, StyleAdaptationKind::Omitted))
        {
            report.addExample(
                feature,
                StyleAdaptationKind::Omitted,
                buildTextExampleDetail("Style omitted", logicalState, presentedEnabled),
                logicalState);
        }

        return;
    }

    if (!physicallyRendered)
    {
        report.recordLogicalOnly(feature);

        if (shouldCaptureExample(report, feature, StyleAdaptationKind::LogicalOnly))
        {
            report.addExample(
                feature,
                StyleAdaptationKind::LogicalOnly,
                buildTextExampleDetail("Style preserved logically but not emitted by the Win32 attribute path", logicalState, presentedEnabled),
                logicalState);
        }

        return;
    }

    if (renderMode == TextAttributeRenderMode::Approximate)
    {
        report.recordApproximated(feature);

        if (shouldCaptureExample(report, feature, StyleAdaptationKind::Approximated))
        {
            report.addExample(
                feature,
                StyleAdaptationKind::Approximated,
                buildTextExampleDetail("Style approximated through the current Win32 attribute path", logicalState, presentedEnabled),
                logicalState);
        }

        return;
    }

    report.recordDirect(feature);

    if (shouldCaptureExample(report, feature, StyleAdaptationKind::Direct))
    {
        report.addExample(
            feature,
            StyleAdaptationKind::Direct,
            buildTextExampleDetail("Style rendered directly", logicalState, presentedEnabled),
            logicalState);
    }
}

void ConsoleRenderer::recordBlinkFeature(
    StyleFeature feature,
    const Style::AttributeState& authoredState,
    bool presentedEnabled,
    bool emulated,
    bool physicallyRendered)
{
    CapabilityReport& report = m_renderDiagnostics.report();
    const LogicalStyleValueState logicalState = logicalStateFromAttributeState(authoredState);

    if (shouldCaptureLogicalStateExample(report, feature, logicalState))
    {
        std::ostringstream detail;
        detail
            << "Logical field observed with state="
            << CapabilityReport::toString(logicalState)
            << ", presented=" << (presentedEnabled ? "true" : "false");
        report.addLogicalStateExample(feature, logicalState, detail.str());
    }

    if (!authoredState.has_value())
    {
        return;
    }

    if (!*authoredState)
    {
        if (shouldCaptureExample(report, feature, StyleAdaptationKind::Direct))
        {
            report.addExample(
                feature,
                StyleAdaptationKind::Direct,
                buildTextExampleDetail("Explicit blink disable preserved", logicalState, presentedEnabled),
                logicalState);
        }

        return;
    }

    if (emulated)
    {
        report.recordEmulated(feature);

        if (shouldCaptureExample(report, feature, StyleAdaptationKind::Emulated))
        {
            report.addExample(
                feature,
                StyleAdaptationKind::Emulated,
                buildTextExampleDetail("Emulated blink", logicalState, presentedEnabled),
                logicalState);
        }

        return;
    }

    if (!presentedEnabled)
    {
        report.recordOmitted(feature);

        if (shouldCaptureExample(report, feature, StyleAdaptationKind::Omitted))
        {
            report.addExample(
                feature,
                StyleAdaptationKind::Omitted,
                buildTextExampleDetail("Blink omitted", logicalState, presentedEnabled),
                logicalState);
        }

        return;
    }

    if (!physicallyRendered)
    {
        report.recordLogicalOnly(feature);

        if (shouldCaptureExample(report, feature, StyleAdaptationKind::LogicalOnly))
        {
            report.addExample(
                feature,
                StyleAdaptationKind::LogicalOnly,
                buildTextExampleDetail("Blink preserved logically but not emitted by the Win32 attribute path", logicalState, presentedEnabled),
                logicalState);
        }

        return;
    }

    report.recordDirect(feature);

    if (shouldCaptureExample(report, feature, StyleAdaptationKind::Direct))
    {
        report.addExample(
            feature,
            StyleAdaptationKind::Direct,
            buildTextExampleDetail("Blink rendered directly", logicalState, presentedEnabled),
            logicalState);
    }
}

