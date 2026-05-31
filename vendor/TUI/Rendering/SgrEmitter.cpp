#include "Rendering/SgrEmitter.h"

#include <string>

#include "Rendering/Styles/Color.h"

/*
    SgrEmitter design

    SgrEmitter is a VT-only low-level emitter that:
        - tracks the current presented Style
        - computes a diff from current style to target style
        - emits a single minimal SGR sequence only when something changed
        - never looks at ThemeColor, authored semantics, or downgrade policy
        - assumes incoming Style is already fully resolved

    Additional Phase 3 responsibility:
        - respect the already-detected backend color tier when physically
          emitting VT color opcodes
        - never invent a downgrade path
        - simply omit unsupported richer color channels from physical emission

    Its responsibilities remain intentionally narrow:
        - setCapabilities(...)
        - reset()
        - currentStyle()
        - appendTransitionTo(out, targetStyle)
        - appendReset(out)

    The main improvement is that style removal no longer forces ESC[0m + full rebuild.
    Instead, it emits only the needed reset codes:
        - foreground off - 39
        - background off - 49
        - underline off - 24
        - blink off - 25
        - reverse off - 27
        - invisible off - 28
        - strike off - 29
        - bold/dim changes - 22 plus re-enable whichever of bold/dim should remain on

    Real delta batching instead of reset-per-run behavior
*/

namespace
{
    void appendCode(std::string& out, int code)
    {
        if (!out.empty() && out.back() != '[')
        {
            out += ';';
        }

        out += std::to_string(code);
    }

    void appendBasicForeground(std::string& out, Color::Basic color)
    {
        switch (color)
        {
        case Color::Basic::Black:         appendCode(out, 30); break;
        case Color::Basic::Red:           appendCode(out, 31); break;
        case Color::Basic::Green:         appendCode(out, 32); break;
        case Color::Basic::Yellow:        appendCode(out, 33); break;
        case Color::Basic::Blue:          appendCode(out, 34); break;
        case Color::Basic::Magenta:       appendCode(out, 35); break;
        case Color::Basic::Cyan:          appendCode(out, 36); break;
        case Color::Basic::White:         appendCode(out, 37); break;
        case Color::Basic::BrightBlack:   appendCode(out, 90); break;
        case Color::Basic::BrightRed:     appendCode(out, 91); break;
        case Color::Basic::BrightGreen:   appendCode(out, 92); break;
        case Color::Basic::BrightYellow:  appendCode(out, 93); break;
        case Color::Basic::BrightBlue:    appendCode(out, 94); break;
        case Color::Basic::BrightMagenta: appendCode(out, 95); break;
        case Color::Basic::BrightCyan:    appendCode(out, 96); break;
        case Color::Basic::BrightWhite:   appendCode(out, 97); break;
        }
    }

    void appendBasicBackground(std::string& out, Color::Basic color)
    {
        switch (color)
        {
        case Color::Basic::Black:         appendCode(out, 40); break;
        case Color::Basic::Red:           appendCode(out, 41); break;
        case Color::Basic::Green:         appendCode(out, 42); break;
        case Color::Basic::Yellow:        appendCode(out, 43); break;
        case Color::Basic::Blue:          appendCode(out, 44); break;
        case Color::Basic::Magenta:       appendCode(out, 45); break;
        case Color::Basic::Cyan:          appendCode(out, 46); break;
        case Color::Basic::White:         appendCode(out, 47); break;
        case Color::Basic::BrightBlack:   appendCode(out, 100); break;
        case Color::Basic::BrightRed:     appendCode(out, 101); break;
        case Color::Basic::BrightGreen:   appendCode(out, 102); break;
        case Color::Basic::BrightYellow:  appendCode(out, 103); break;
        case Color::Basic::BrightBlue:    appendCode(out, 104); break;
        case Color::Basic::BrightMagenta: appendCode(out, 105); break;
        case Color::Basic::BrightCyan:    appendCode(out, 106); break;
        case Color::Basic::BrightWhite:   appendCode(out, 107); break;
        }
    }

    bool supportsColor(const RendererCapabilities& capabilities, const Color& color)
    {
        if (color.isDefault())
        {
            return true;
        }

        if (color.isBasic16())
        {
            if (!capabilities.supportsBasicColors())
            {
                return false;
            }

            if (color.isBrightBasic() && !capabilities.supportsBrightBasicColors())
            {
                return false;
            }

            return true;
        }

        if (color.isIndexed256())
        {
            return capabilities.supportsIndexed256Colors();
        }

        if (color.isRgb())
        {
            return capabilities.supportsRgb24();
        }

        return false;
    }

    void appendForeground(std::string& out, const Color& color)
    {
        if (color.isDefault())
        {
            appendCode(out, 39);
            return;
        }

        if (color.isBasic16())
        {
            appendBasicForeground(out, color.basic());
            return;
        }

        if (color.isIndexed256())
        {
            appendCode(out, 38);
            appendCode(out, 5);
            appendCode(out, static_cast<int>(color.index256()));
            return;
        }

        if (color.isRgb())
        {
            appendCode(out, 38);
            appendCode(out, 2);
            appendCode(out, static_cast<int>(color.red()));
            appendCode(out, static_cast<int>(color.green()));
            appendCode(out, static_cast<int>(color.blue()));
            return;
        }
    }

    void appendBackground(std::string& out, const Color& color)
    {
        if (color.isDefault())
        {
            appendCode(out, 49);
            return;
        }

        if (color.isBasic16())
        {
            appendBasicBackground(out, color.basic());
            return;
        }

        if (color.isIndexed256())
        {
            appendCode(out, 48);
            appendCode(out, 5);
            appendCode(out, static_cast<int>(color.index256()));
            return;
        }

        if (color.isRgb())
        {
            appendCode(out, 48);
            appendCode(out, 2);
            appendCode(out, static_cast<int>(color.red()));
            appendCode(out, static_cast<int>(color.green()));
            appendCode(out, static_cast<int>(color.blue()));
            return;
        }
    }

    bool boolStateChanged(bool currentValue, bool targetValue)
    {
        return currentValue != targetValue;
    }
}

void SgrEmitter::setCapabilities(const RendererCapabilities& capabilities)
{
    m_capabilities = capabilities;
}

void SgrEmitter::reset()
{
    m_currentStyle = Style{};
}

const Style& SgrEmitter::currentStyle() const
{
    return m_currentStyle;
}

void SgrEmitter::appendTransitionTo(std::string& out, const Style& targetStyle)
{
    const Style sanitizedTargetStyle = sanitizeForEmission(targetStyle);

    if (sanitizedTargetStyle == m_currentStyle)
    {
        return;
    }

    appendDeltaStyle(out, sanitizedTargetStyle);
    m_currentStyle = sanitizedTargetStyle;
}

void SgrEmitter::appendReset(std::string& out)
{
    out += "\x1b[0m";
    m_currentStyle = Style{};
}

Style SgrEmitter::sanitizeForEmission(const Style& style) const
{
    Style sanitized = style;

    if (sanitized.hasForeground() && !supportsColor(m_capabilities, *sanitized.foreground()))
    {
        sanitized = sanitized.withoutForeground();
    }

    if (sanitized.hasBackground() && !supportsColor(m_capabilities, *sanitized.background()))
    {
        sanitized = sanitized.withoutBackground();
    }

    if (sanitized.bold() && !m_capabilities.supportsBoldDirect())
    {
        sanitized = sanitized.withBold(false);
    }

    if (sanitized.dim() && !m_capabilities.supportsDimDirect())
    {
        sanitized = sanitized.withDim(false);
    }

    if (sanitized.underline() && !m_capabilities.supportsUnderlineDirect())
    {
        sanitized = sanitized.withUnderline(false);
    }

    if (sanitized.reverse() && !m_capabilities.supportsReverseDirect())
    {
        sanitized = sanitized.withReverse(false);
    }

    if (sanitized.invisible() && !m_capabilities.supportsInvisibleDirect())
    {
        sanitized = sanitized.withInvisible(false);
    }

    if (sanitized.strike() && !m_capabilities.supportsStrikeDirect())
    {
        sanitized = sanitized.withStrike(false);
    }

    if (sanitized.slowBlink() && !m_capabilities.supportsSlowBlinkDirect())
    {
        sanitized = sanitized.withSlowBlink(false);
    }

    if (sanitized.fastBlink() && !m_capabilities.supportsFastBlinkDirect())
    {
        sanitized = sanitized.withFastBlink(false);
    }

    return sanitized;
}

void SgrEmitter::appendFullStyle(std::string& out, const Style& style) const
{
    std::string sgr = "\x1b[0";

    if (style.hasForeground())
    {
        appendForeground(sgr, *style.foreground());
    }

    if (style.hasBackground())
    {
        appendBackground(sgr, *style.background());
    }

    if (style.bold())      appendCode(sgr, 1);
    if (style.dim())       appendCode(sgr, 2);
    if (style.underline()) appendCode(sgr, 4);
    if (style.slowBlink()) appendCode(sgr, 5);
    if (style.fastBlink()) appendCode(sgr, 6);
    if (style.reverse())   appendCode(sgr, 7);
    if (style.invisible()) appendCode(sgr, 8);
    if (style.strike())    appendCode(sgr, 9);

    sgr += 'm';
    out += sgr;
}

void SgrEmitter::appendDeltaStyle(std::string& out, const Style& targetStyle) const
{
    std::string sgr = "\x1b[";
    bool changed = false;

    if (m_currentStyle.foreground() != targetStyle.foreground())
    {
        if (targetStyle.hasForeground())
        {
            appendForeground(sgr, *targetStyle.foreground());
        }
        else
        {
            appendCode(sgr, 39);
        }

        changed = true;
    }

    if (m_currentStyle.background() != targetStyle.background())
    {
        if (targetStyle.hasBackground())
        {
            appendBackground(sgr, *targetStyle.background());
        }
        else
        {
            appendCode(sgr, 49);
        }

        changed = true;
    }

    const bool currentBold = m_currentStyle.bold();
    const bool targetBold = targetStyle.bold();
    const bool currentDim = m_currentStyle.dim();
    const bool targetDim = targetStyle.dim();

    if (currentBold != targetBold || currentDim != targetDim)
    {
        if (currentBold || currentDim)
        {
            appendCode(sgr, 22);
            changed = true;
        }

        if (targetBold)
        {
            appendCode(sgr, 1);
            changed = true;
        }

        if (targetDim)
        {
            appendCode(sgr, 2);
            changed = true;
        }
    }

    if (boolStateChanged(m_currentStyle.underline(), targetStyle.underline()))
    {
        appendCode(sgr, targetStyle.underline() ? 4 : 24);
        changed = true;
    }

    const bool currentBlink = m_currentStyle.slowBlink() || m_currentStyle.fastBlink();
    const bool targetBlink = targetStyle.slowBlink() || targetStyle.fastBlink();

    if (currentBlink != targetBlink)
    {
        appendCode(sgr, targetBlink
            ? (targetStyle.fastBlink() ? 6 : 5)
            : 25);
        changed = true;
    }
    else if (targetBlink)
    {
        if (m_currentStyle.slowBlink() != targetStyle.slowBlink() ||
            m_currentStyle.fastBlink() != targetStyle.fastBlink())
        {
            appendCode(sgr, targetStyle.fastBlink() ? 6 : 5);
            changed = true;
        }
    }

    if (boolStateChanged(m_currentStyle.reverse(), targetStyle.reverse()))
    {
        appendCode(sgr, targetStyle.reverse() ? 7 : 27);
        changed = true;
    }

    if (boolStateChanged(m_currentStyle.invisible(), targetStyle.invisible()))
    {
        appendCode(sgr, targetStyle.invisible() ? 8 : 28);
        changed = true;
    }

    if (boolStateChanged(m_currentStyle.strike(), targetStyle.strike()))
    {
        appendCode(sgr, targetStyle.strike() ? 9 : 29);
        changed = true;
    }

    if (!changed)
    {
        return;
    }

    sgr += 'm';
    out += sgr;
}
