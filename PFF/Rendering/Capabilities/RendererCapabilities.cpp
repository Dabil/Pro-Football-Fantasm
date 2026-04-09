#include "Rendering/Capabilities/RendererCapabilities.h"

/*

how renderers and diagnostics should consume this data

Renderer should use this type in two steps:

RendererCapabilities capabilities = RendererCapabilities::Conservative();

Then during initialization:

detect whether VT processing was actually enabled
populate a RendererCapabilities instance
build StylePolicy from that capability data
keep the capability object available for diagnostics and future reporting

The practical flow should be:

RendererCapabilities
    -> StylePolicy
    -> resolved Style
    -> backend mapping/output

Recommended usage patterns:

renderers should store one RendererCapabilities m_capabilities;
style mapping code should query helpers like:
supportsRgb24()
supportsIndexed256Colors()
supportsBrightBasicColorsDirect()
supportsUnderlineDirect()
usesPreserveStyleSafeFallback()
mayEmulateSlowBlink()

optionalBackendFlags is intentionally a simple reserved extension point.
The current code should not invent behavior from unknown flag bits.
It should only surface them for diagnostics until future backends define them.

diagnostics screens should report both the raw flags and the resulting
renderer behavior:
VT enabled or not
color tier
bright basic color support
preserve-style-safe fallback behavior
optional backend flags
direct support vs unsupported vs emulated vs unknown

For the current backend, BasicWin32 must stay conservative:

- standard basic colors are real and directly usable
- bright basic colors are also real and directly usable through the Win32
  attribute intensity palette
- that bright color support is a color-palette capability, not a promise
  about bold/dim text semantics
- decorative/style semantics such as bold, dim, underline, reverse, and
  invisible are not safe to promise as direct support across hosts
- blink is not directly provided by the Win32 attribute path, but the current
  renderer may intentionally emulate it
- if the renderer later chooses to approximate a visual effect, that should
  be reflected in runtime adaptation data rather than overstated here

Only use VirtualTerminal() once you truly add a VT output path rather than
merely enabling the mode flag.

That keeps the model honest, conservative, and reusable.

*/

RendererCapabilities RendererCapabilities::Conservative()
{
    RendererCapabilities capabilities;

    capabilities.virtualTerminalProcessing = false;
    capabilities.unicodeOutput = true;
    capabilities.preserveStyleSafeFallback = true;
    capabilities.optionalBackendFlags = 0;
    capabilities.colorTier = ColorSupport::Basic16;

    /*
        The existing model already treated Basic16 as available.
        Keep that practical default, but now expose bright basic colors
        explicitly rather than leaving them implicit in the tier alone.
    */
    capabilities.brightBasicColors = RendererFeatureSupport::Supported;

    capabilities.bold = RendererFeatureSupport::Unknown;
    capabilities.dim = RendererFeatureSupport::Unknown;
    capabilities.underline = RendererFeatureSupport::Unknown;
    capabilities.reverse = RendererFeatureSupport::Unknown;
    capabilities.invisible = RendererFeatureSupport::Unknown;
    capabilities.strike = RendererFeatureSupport::Unsupported;
    capabilities.slowBlink = RendererFeatureSupport::Emulated;
    capabilities.fastBlink = RendererFeatureSupport::Emulated;

    return capabilities;
}

RendererCapabilities RendererCapabilities::BasicWin32()
{
    RendererCapabilities capabilities = Conservative();

    capabilities.virtualTerminalProcessing = false;
    capabilities.preserveStyleSafeFallback = true;
    capabilities.optionalBackendFlags = 0;
    capabilities.colorTier = ColorSupport::Basic16;

    /*
        Several richer style semantics are not reliable enough to advertise as
        direct support:

        - bold/dim are often approximated through intensity rather than true
          weight/brightness semantics
        - underline depends on host/font behavior and is not broadly reliable
        - reverse may be visually approximated by swapping colors, but that is
          not the same as guaranteed direct backend semantic support
        - invisible can be approximated visually, but true invisibility is not
          something this path should promise

        These remain Unknown to force conservative renderer policy.

        Blink is modeled as Emulated rather than Unsupported because the active
        renderer/backend path can intentionally simulate blink timing without
        mutating authored logical style data.
    */

    capabilities.brightBasicColors = RendererFeatureSupport::Supported;
    capabilities.bold = RendererFeatureSupport::Unknown;
    capabilities.dim = RendererFeatureSupport::Unknown;
    capabilities.underline = RendererFeatureSupport::Unknown;
    capabilities.reverse = RendererFeatureSupport::Unknown;
    capabilities.invisible = RendererFeatureSupport::Unknown;
    capabilities.strike = RendererFeatureSupport::Unsupported;
    capabilities.slowBlink = RendererFeatureSupport::Emulated;
    capabilities.fastBlink = RendererFeatureSupport::Emulated;

    return capabilities;
}

RendererCapabilities RendererCapabilities::VirtualTerminal()
{
    RendererCapabilities capabilities = Conservative();

    capabilities.virtualTerminalProcessing = true;
    capabilities.preserveStyleSafeFallback = true;
    capabilities.optionalBackendFlags = 0;
    capabilities.colorTier = ColorSupport::Rgb24;

    /*
        Even when VT processing is enabled, exact behavior can still vary by
        host and font environment. These defaults describe intended capability,
        not a guarantee that every terminal host behaves identically.

        This profile is meant for a future true VT presentation path, not the
        current Win32-attribute renderer.
    */

    capabilities.brightBasicColors = RendererFeatureSupport::Supported;
    capabilities.bold = RendererFeatureSupport::Supported;
    capabilities.dim = RendererFeatureSupport::Supported;
    capabilities.underline = RendererFeatureSupport::Supported;
    capabilities.reverse = RendererFeatureSupport::Supported;
    capabilities.invisible = RendererFeatureSupport::Supported;
    capabilities.strike = RendererFeatureSupport::Supported;
    capabilities.slowBlink = RendererFeatureSupport::Emulated;
    capabilities.fastBlink = RendererFeatureSupport::Emulated;

    return capabilities;
}

bool RendererCapabilities::supportsBasicColors() const
{
    return colorTier >= ColorSupport::Basic16;
}

bool RendererCapabilities::supportsBrightBasicColors() const
{
    return brightBasicColors != RendererFeatureSupport::Unsupported;
}

bool RendererCapabilities::supportsIndexed256Colors() const
{
    return colorTier >= ColorSupport::Indexed256;
}

bool RendererCapabilities::supportsRgb24() const
{
    return colorTier >= ColorSupport::Rgb24;
}

bool RendererCapabilities::supportsTrueColor() const
{
    return supportsRgb24();
}

bool RendererCapabilities::supportsBrightBasicColorsDirect() const
{
    return brightBasicColors == RendererFeatureSupport::Supported;
}

bool RendererCapabilities::supportsBoldDirect() const
{
    return bold == RendererFeatureSupport::Supported;
}

bool RendererCapabilities::supportsDimDirect() const
{
    return dim == RendererFeatureSupport::Supported;
}

bool RendererCapabilities::supportsUnderlineDirect() const
{
    return underline == RendererFeatureSupport::Supported;
}

bool RendererCapabilities::supportsReverseDirect() const
{
    return reverse == RendererFeatureSupport::Supported;
}

bool RendererCapabilities::supportsInvisibleDirect() const
{
    return invisible == RendererFeatureSupport::Supported;
}

bool RendererCapabilities::supportsStrikeDirect() const
{
    return strike == RendererFeatureSupport::Supported;
}

bool RendererCapabilities::supportsSlowBlinkDirect() const
{
    return slowBlink == RendererFeatureSupport::Supported;
}

bool RendererCapabilities::supportsFastBlinkDirect() const
{
    return fastBlink == RendererFeatureSupport::Supported;
}

bool RendererCapabilities::mayEmulateSlowBlink() const
{
    return slowBlink == RendererFeatureSupport::Emulated;
}

bool RendererCapabilities::mayEmulateFastBlink() const
{
    return fastBlink == RendererFeatureSupport::Emulated;
}

bool RendererCapabilities::usesPreserveStyleSafeFallback() const
{
    return preserveStyleSafeFallback;
}

bool RendererCapabilities::hasOptionalBackendFlags() const
{
    return optionalBackendFlags != 0;
}

bool RendererCapabilities::hasOptionalBackendFlag(RendererBackendExtensionFlag flag) const
{
    const std::uint32_t mask = static_cast<std::uint32_t>(flag);
    return (optionalBackendFlags & mask) != 0;
}