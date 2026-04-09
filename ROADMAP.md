TUI MASTER ROADMAP
REVISED: 03-31-26 COMPOSITING POLICY REFACTOR
REVISED: 03-31-26 COLOR + COMPOSITING INTEGRATION
========================================

PROJECT GOAL
Build a clean, extensible Text UI engine that:

1. feels natural to author in C++
2. supports page-building without Visual Studio later
3. preserves the best ideas from the old TUI engine
4. avoids baking legacy API mistakes into the new architecture
5. supports future windows, widgets, menus, panels, popups, and alternate renderers

The engine should be built in layers so that later features are built on stable foundations instead of temporary solutions.



============================================================
0. CORE DESIGN PRINCIPLES
============================================================

0.1 Renderer is a PURE EMISSION LAYER

* Renderer consumes fully-resolved cells
* Renderer MUST NOT:

  * resolve ThemeColor
  * downgrade color
  * apply style logic
* Renderer ONLY:

  * encodes final values to output


---

0.2 Single Source of Truth Systems
Each domain owns its logic:

| Domain            | Owner        |
| ----------------- | ------------ |
| Color resolution  | Color system |
| Style semantics   | Style system |
| Composition rules | WritePolicy  |
| Output encoding   | Renderer     |

NO cross-ownership allowed.

---

0.3 Explicit Data-Driven Behavior
No hidden state anywhere:

* WritePolicy = explicit compositing
* ColorResolver = explicit color decisions
* Style = explicit intent

---

0.4 Unicode-Correct Core

* Internal text = UTF-32
* ScreenBuffer = cell grid
* Layout handles graphemes + width

---

============================================================
UPDATED GLOBAL RULE (CRITICAL)
==============================

Pipeline MUST be:

UTF-8 → UTF-32 → Layout → ScreenCell →
Style (logical) → ColorResolver →
Resolved Color → Renderer

NO EXCEPTIONS.

FORMAT SUPPORT POLICY

1. Unified object rule
- All reusable loaded/generated visual text content must normalize into TextObject

2. Loader/exporter split
- Parsers/loaders interpret source formats
- Exporters/save systems write supported target formats
- Neither changes the rendering core model

3. Encoding boundary rule
- Text file input must decode at the boundary
- Internal model remains UTF-32
- Legacy code-page handling belongs only in conversion/loader utilities

4. Terminal-art rule
- ANSI-style sources are imported as structured glyph/style data
- Do not treat raw escape replay as the engine’s primary representation

5. Lossy export rule
- Saving/exporting to older formats is allowed only with explicit approximation/clamping behavior
- Never silently discard unsupported style or layout semantics

6. Future declarative format rule
- JSON / YAML / INI and similar structured formats belong to later phases as page/layout/widget declarations
- They must compile down to the same engine APIs, not create a second UI system

7. Future animation format rule
- Multi-frame text/ANSI formats belong in animation-oriented phases once timing/invalidation systems exist


============================================================
1. DEPENDENCY GRAPH
============================================================

This is the actual dependency order for implementation.

1. Core types
   ↓
2. Rendering core
   ↓
3. Style system (logical only)
   ↓
4. Object/asset system
   ↓
5. Color system (NEW CORE LAYER)
   ↓
6. Object/asset system
   ↓
7. Compositing / WritePolicy system
   ↓
8. PageComposer API 1.0
   ↓
9. Screen templates + regions + alignment
   ↓
10. Page interpreter
   ↓
11. Application input/event system
   ↓
12. Focus system
   ↓
13. Layering / surfaces / compositor
   ↓
14. Panels / popups / windows
   ↓
15. Menu system
   ↓
16. Widget system
   ↓
17. Scrolling / viewports
   ↓
18. Animation / timers
   ↓
19. Alternate render backends
   ↓
20. Tooling / page editor / advanced scripting

The roadmap below follows this order intentionally.



============================================================
2. PHASE 1 — CORE TYPES AND RENDERING FOUNDATION
============================================================

PURPOSE
Build the stable, Unicode-correct rendering core that everything else depends on.

WHY THIS PHASE COMES FIRST
Without this phase, every later system becomes harder to design and may require refactoring. This phase establishes:
- logical screen representation
- diff-based redraw
- strict separation of rendering vs composition
- a Unicode-native internal text model

This ensures all future systems operate on correct display semantics rather than ASCII assumptions.

IMPLEMENT IN THIS PHASE

2.1 Core types
- Point
- Size
- Rect
- Alignment enums
- orientation / anchor enums if needed
- basic utility enums for composition and layout

2.2 ScreenCell
Each cell should be able to represent:
- glyph (char32_t)
- style/theme reference
- cell metadata for display correctness (priority, flags, etc.)

Required metadata:
- CellKind:
  - Empty
  - Glyph
  - WideTrailing
  - CombiningContinuation

  Goal:
- support wide characters and combining marks without breaking layout
- remain lightweight and value-type

2.3 ScreenBuffer
Responsibilities:
- logical width/height
- resize
- clear
- get/set cell (cell-safe)
- inBounds
- Unicode-native writing
- text placement (not text parsing)
- fillRect
- drawFrame
- debug export (Unicode-safe)

PRIMARY API (Unicode-native):
- writeCodePoint(int x, int y, char32_t glyph, const Style&)
- writeText(int x, int y, const std::u32string&, const Style&)
- fillRect(const Rect&, char32_t glyph, const Style&)
- renderToU32String()
- renderToUtf8String()

LEGACY WRAPPERS (forwarding only):
- writeChar(..., char, ...)
- writeString(..., std::string, ...)
→ must decode UTF-8 and forward

IMPORTANT RULES:
- ScreenBuffer operates on display cells, not bytes
- must handle:
  - width 0 (combining)
  - width 1
  - width 2 (CJK/emoji)
- must manage trailing cells for wide glyphs
- must clean up overwritten continuation cells

NOT RESPONSIBLE FOR:
- UTF-8 decoding logic
- grapheme segmentation
- font capability decisions

2.4 Unicode utilities (NEW - REQUIRED)
Utilities/Unicode/

- UnicodeConversion
  - utf8ToU32
  - u32ToUtf8
  - u32ToUtf16
  - codePointToUtf16

- UnicodeWidth
  - measureCodePointWidth(char32_t)

- UnicodeGrapheme (Phase 1 basic / Phase 5 expanded)
  - segmentGraphemeClusters(...)


2.5 Surface
Responsibilities:
- wrap a ScreenBuffer
- serve as an off-screen composition target
- later support layering/window composition

2.6 IRenderer
Responsibilities:
- initialize
- shutdown
- present(ScreenBuffer)
- resize
- textCapabilities()

2.7 ConsoleRenderer
Responsibilities:
- all Win32 console-specific work
- UTF-16 output (not UTF-8 logic internally)
- live console size detection
- diff-based presentation
- resize polling

STRICT RULES:
- must NOT:
  - compute character width
  - perform grapheme segmentation
  - interpret text meaning
- must:
  - render already-laid-out cells
  - skip continuation cells
  - map Style → console attributes
  - use UnicodeConversion utilities

2.8 FrameDiff
Responsibilities:
- compare previous vs current frame
- dirty row/span detection
- support efficient redraw

Must compare:
- glyph
- style
- cell metadata (CellKind)

2.9 Application loop
Responsibilities:
- initialize renderer
- create main surface
- per-frame update
- per-frame render
- live resize polling

2.10 Resize handling
- startup size detection
- live resize detection
- resize logical surface and previous frame on size changes

OUTPUT OF PHASE 1
A Unicode-correct logical rendering engine with:
- proper cell semantics
- correct wide/combining behavior
- backend-independent text model
- efficient redraw

FILES / FOLDERS NEEDED

App/
  Application.h
  Application.cpp

Core/
  Point.h
  Size.h
  Rect.h
  Enums.h

Rendering/
  ScreenCell.h
  ScreenBuffer.h
  ScreenBuffer.cpp
  Surface.h
  Surface.cpp
  FrameDiff.h
  FrameDiff.cpp
  IRenderer.h
  ConsoleRenderer.h
  ConsoleRenderer.cpp

  Utilities/
  Unicode/
    UnicodeConversion.h
    UnicodeConversion.cpp
    UnicodeWidth.h
    UnicodeWidth.cpp
    UnicodeGrapheme.h
    UnicodeGrapheme.cpp

main.cpp



============================================================
3. PHASE 2 — STYLE + COLOR SYSTEM (INTEGRATED ANSI-LIKE AUTHORING MODEL)
============================================================

PURPOSE
Create a backend-agnostic style system with a **fully decoupled multi-fidelity color system** that preserves the authoring feel of ansi graphics with Themes.h, without tying the engine to raw ANSI transport strings.

---

CRITICAL RULE

Style defines **intent only**.
Color system resolves **actual output values**.
Renderer emits **final values only**.

---

WHY THIS PHASE COMES EARLY
Page composition depends on style semantics. The user should be able to think in “console styling” terms while the backend stays abstract. This directly preserves the semantic theme-building pattern from Themes and the composable styling vocabulary of ansi graphics.

IMPLEMENT IN THIS PHASE

UPDATE NOTES:
- No Unicode conflicts
- Must remain backend-agnostic
- Style must not assume ANSI transport
- Capability detection must inform rendering decisions without changing authored style data
- Diagnostic reporting must explain backend adaptation without coupling page code to backend internals

ADDITION:
- Style system must survive round-trip across:
  - ScreenBuffer
  - Renderer
  - alternate backends
- Output capability detection must determine what the active backend can actually render
- Capability limits must not destroy or downgrade authored style intent inside the logical model
- The renderer must be able to record what output adjustments were made so authors can understand display differences

3.1 Color abstraction
Support:
- standard colors
- bright colors
- 256 color representation
- future-capable RGB 24-bit true color representation

Requirements:
- color model must be backend-independent
- color model must distinguish logical color intent from backend realization
- authored colors must remain representable even if active backend cannot fully display them

3.2 Style struct
Support:
- fg color
- bg color
- bold
- dim
- underline
- slow blink
- fast blink
- reverse
- invisible
- strike

Requirements:
- Style is a logical description of appearance
- Style must not encode transport strings
- Style must be storable in ScreenCell / ScreenBuffer without loss of intent
- unsupported fields must still remain preserved in the logical model

3.3 Policy and Emulation
Define backend behavior for unsupported style features.

Support:
- direct support when backend can render a style field
- graceful downgrade when backend cannot render a style field
- optional emulation policy where appropriate
- explicit separation between:
  - logical style intent
  - backend capability
  - output mapping decision

Examples:
- true color may downgrade to nearest 256 or 16-color approximation
- underline may be dropped if backend does not support it
- blink may be ignored, mapped according to backend policy, or emulated by the renderer.
- unsupported effects must not corrupt other style fields

3.4 Style composition
Support authoring like:
- style::Bold + style::Fg(Color::Red)
- style::Bold + style::Fg(Color::Red) + style::Bg(Color::Green)

Requirements:
- composition must remain clean and author-friendly
- composition must operate on logical Style objects
- composed styles must remain independent of backend capability limits

3.5 Style merge behavior
Support:
- replace style
- preserve destination style
- merge style fields

Requirements:
- merge semantics must be explicit and reusable
- merge behavior must preserve authored style intent
- preserve-style behavior must remain available even when backend output is downgraded

3.6 Semantic theme namespaces
Examples:
- AppThemes
- UIThemes
- BannerThemes
- GameThemes later
- etc

Requirements:
- themes produce logical styles, not backend escape sequences
- themes must not depend on ConsoleRenderer internals
- themes should remain portable across future render backends

3.7 Backend mapping
ConsoleRenderer maps Style to backend capabilities.

Requirements:
- initial backend may support only a subset of Style richness
- model must still preserve future richness
- backend mapping must use capability information rather than hardcoded assumptions
- style-to-output conversion must be isolated from style authoring APIs

3.8 Preserve-style semantics
Needed because earlier docs explicitly relied on preserving destination formatting when no style override was supplied.

Requirements:
- preserve-style behavior must work at the logical buffer level
- renderer downgrade must not alter stored logical styles
- absence of a new style override must keep the existing stored style intact

3.9 Console capability model
Add a capability model that records what the active output backend can actually support.

Purpose:
- detect available console output features at startup
- store backend capability information in a reusable structured form
- allow renderer mapping code to choose direct output, downgrade, or omission
- keep authored style richer than currently available output when necessary

Support:
- VT / ANSI processing available or not
- standard 16-color support
- bright color support
- 256-color support
- RGB / true color support
- bold support
- dim support
- underline support
- reverse support
- invisible support
- strike support
- blink support
- preserve-style-safe fallback behavior
- optional future flags for alternate backends

Requirements:
- capability model must describe backend output ability, not authoring limits
- capability detection must be separate from Style and Color definitions
- capability information must be queryable by renderer mapping code
- capability information must be stable enough to cache after initialization
- capability detection must fail safely when a feature cannot be confirmed
- unknown support should prefer conservative fallback behavior

3.10 Console capability detection
Provide detection logic for the active console backend.

Responsibilities:
- inspect console mode / backend state during renderer initialization
- determine whether VT processing is enabled or can be enabled
- determine broad color/output tiers supported by the backend
- populate ConsoleCapabilities for later rendering decisions
- expose detection results to renderer code without spreading platform checks throughout the codebase

Requirements:
- detection logic must live in backend-specific code, not in Style or theme code
- detection should identify capability tiers rather than over-promise exact rendering behavior
- unsupported or uncertain features should be marked conservatively
- detection must not require the style authoring layer to know Windows console details
- future backends must be able to supply their own capability implementations

3.11 Renderer downgrade and adaptation behavior
Use Style + ConsoleCapabilities together to determine actual output behavior.

Support:
- render exact style when supported
- approximate color when partial support exists
- omit unsupported effects safely
- preserve logical style in ScreenBuffer regardless of rendered downgrade

Requirements:
- adaptation must occur during backend mapping/output
- downgrade rules must be centralized and predictable
- output adaptation must not mutate authored theme/style definitions
- output adaptation must not mutate stored ScreenCell styles unless explicitly requested by a separate transformation step
- an output file must be created that users can inspect to see what was set 

3.12 Capability diagnostics model
Add a diagnostics/reporting model that records capability state and renderer adaptation decisions.

Purpose:
- explain why authored styles may not appear exactly as intended
- expose backend limitations to page authors and developers
- record what the renderer did when exact output was not possible
- provide a stable debugging aid without requiring source-level backend knowledge

Support:
- record detected backend capabilities
- record renderer output policy decisions
- record style fields that were:
  - rendered directly
  - downgraded
  - approximated
  - omitted
  - emulated
  - preserved logically but not rendered physically
- record summary statistics and representative examples where helpful

Requirements:
- diagnostics must report backend realization decisions, not alter them
- diagnostics must remain optional and non-invasive
- diagnostics must not become required for normal rendering
- diagnostics data should be structured enough for future tooling
- diagnostics output should remain understandable to page authors

3.13 Renderer diagnostics logging
Provide a file-based diagnostics output that authors can inspect after startup or render activity.

Purpose:
- save capability and adaptation information to a human-readable file
- help page authors understand why display output differs from authored intent
- make backend limitations visible during development and testing

Possible contents:
- renderer/backend identity
- console mode / VT status
- detected capability flags
- supported color tier
- supported decoration features
- downgrade rules currently in effect
- emulation rules currently in effect
- examples of unsupported style requests encountered during rendering
- summary of omitted or approximated style effects

Examples of reported events:
- RGB foreground requested, downgraded to nearest 16-color value
- underline requested, omitted because backend does not support underline
- blink requested, not emitted because blink is disabled or unsupported
- reverse requested, emitted directly
- logical style preserved in ScreenBuffer, physical output adapted in renderer

Requirements:
- diagnostics logging must be separate from normal render output
- diagnostics file generation must not clutter console output
- diagnostics logging should be optional or configurable
- diagnostics logging must not require page authors to change how they author styles
- diagnostics file format should be simple, readable, and stable
- renderer should be able to append events or write a fresh report per run according to configuration

3.14 Author-facing rendering hints
Use diagnostics output to help page authors improve pages without exposing backend internals everywhere.

Purpose:
- help authors choose styles that are more portable
- help identify when a theme depends on features unavailable in the current console
- support future validation and tooling

Support:
- capability summary section
- warnings about unsupported theme/style usage
- suggestions for portable alternatives where reasonable
- distinction between:
  - authored logical style
  - backend-supported style
  - actual rendered result

Requirements:
- hints must be advisory only
- hints must not force authors to target one backend
- hints must not weaken the backend-agnostic style model

OUTPUT OF PHASE 2
A backend-agnostic style system with console-native authoring feel, plus a capability-aware backend mapping layer that can detect available console output features, preserve logical style richness, safely adapt rendered output to the active backend, and emit diagnostics explaining why authored styles may be displayed differently.

FILES / FOLDERS NEEDED

Rendering/Styles/
  Color.h
  Style.h
  Style.cpp
  StyleBuilder.h
  StyleBuilder.cpp
  StyleMerge.h
  StyleMerge.cpp
  BannerThemes.h
  UIThemes.h

Rendering/Capabilities/
  ConsoleCapabilities.h
  ConsoleCapabilities.cpp
  CapabilityReport.h
  CapabilityReport.cpp

Rendering/Diagnostics/
  RenderDiagnostics.h
  RenderDiagnostics.cpp
  RenderDiagnosticsWriter.h
  RenderDiagnosticsWriter.cpp

Rendering/Backends/
  ConsoleCapabilityDetector.h
  ConsoleCapabilityDetector.cpp

Optional if kept outside Rendering:
UI/Styles/
  FrameStyle.h
  UIStyle.h

DEFINITION OF DONE

- Color abstraction supports logical standard, bright, 256, and RGB color representations
- Style represents logical appearance without transport coupling
- Style composition API supports ansi-like authoring syntax
- Style merge behavior supports replace, preserve, and merge semantics
- Semantic themes produce reusable backend-agnostic logical styles
- ConsoleCapabilities records active backend output capabilities in structured form
- Console capability detection runs during renderer initialization
- ConsoleRenderer maps Style through capability-aware backend conversion
- Unsupported output features are downgraded, omitted, approximated, or emulated safely
- Diagnostics model records backend capability state and renderer adaptation behavior
- Diagnostics writer can save a readable report file for authors and developers
- ScreenBuffer and ScreenCell preserve logical style intent even when active output is limited
- Architecture remains portable to alternate render backends



============================================================
4. PHASE 3 — ASCII OBJECT / ASSET MODEL
============================================================

PURPOSE
Create reusable visual objects for composition. 

WHY THIS PHASE COMES EARLY
Page composition depends on style semantics. The user should be able to think in “console styling” terms while the backend stays abstract. This directly preserves the semantic theme-building pattern and the composable styling vocabulary from ansi.h. 

The page API must have stable primitives to place. The user must be able to rely on authored screens that depend on reusable text assets, text blocks, boxes, ANSI art, and generated banner content.

This phase must define a single reusable text-asset object model, while plain text, ANSI art, binary text art, and banner/font formats are handled by dedicated loaders or generators that normalize content into that shared object.

UPDATE NOTES:
- No Unicode conflicts
- Must remain backend-agnostic
- Style must not assume ANSI transport
- All text loaded from files must be decoded at the boundary and normalized immediately
- UTF-8 text sources must be converted to std::u32string immediately
- TextObject is the unified internal object model for reusable placed text assets
- Do NOT split the object model into:
  - AsciiObject
  - AnsiObject
  - UnicodeObject
- Those distinctions belong to loaders/parsers, not to the core composition type

ADDITION:
- Style system must survive round-trip across:
  - ScreenBuffer
  - Renderer
  - alternate backends

ARCHITECTURE RULE

Use this pipeline:

File or generator format
    ↓
Loader / parser
    ↓
Unified internal object model
    ↓
ScreenBuffer

Examples:
- .txt / .asc / .nfo / .diz → PlainTextLoader → TextObject
- .ans / .bin / .xbin / .adf → AnsiLoader / BinaryArtLoader → TextObject
- FIGlet / TOIlet → AsciiBanner → TextObject

WHY THIS IS THE CORRECT MODEL

ASCII is mostly an input/content constraint
ANSI is a formatting/control-sequence format
Unicode is the engine’s internal text representation model

If the project creates separate core object types like:
- AsciiObject
- AnsiObject
- UnicodeObject

then it mixes different concerns into the type system and creates:
- duplicated width/height/storage logic
- duplicated draw logic
- awkward conversions
- confusion about what PageComposer should accept
- difficulty supporting mixed-format content later

Instead:
- one object type
- many loaders/parsers
- one rendering target

That is the cleanest and most extensible design.

TEXT OBJECT STORAGE MODEL

TextObject should internally store normalized display content suitable for placement into ScreenBuffer.

Minimum responsibilities:
- own normalized internal content
- width
- height
- loaded state
- optional source metadata
- optional per-cell style information
- optional future metadata for compositing / layering

RECOMMENDED STORAGE SHAPE

Because this roadmap now includes:
- .ans
- .bin
- .xbin
- .adf
- styled loaded assets

TextObject should prefer a cell-grid-backed internal representation rather than a simple vector of plain strings.

That means TextObject should be able to represent:
- glyph
- style
- empty cells
- alignment-preserving spaces
- optional metadata needed for imported art

This keeps imported terminal-art assets first-class and avoids losing style/layout fidelity.

IMPLEMENT IN THIS PHASE
4.1 TextObject core
Support:
- FromFile
- FromTextBlock
- width / height
- loaded state
- draw into ScreenBuffer
- normalized internal cell/text storage
- style-aware imported content support

IMPORTANT:
- TextObject is a reusable visual object
- TextObject is NOT a renderer
- TextObject should contain no backend-specific output logic

4.2 Factory helpers
Support:
- Rectangle
- Bordered box
- optional line helpers
- optional fill/block helpers
- optional FIGlet text helper if it fits cleanly

Rules:
- factory helpers return TextObject
- factory helpers remain renderer-independent
- generated objects must be valid Unicode-aware object content

4.3 Plain text loader family
Support loading:
- .asc
- .txt
- .nfo
- .diz

Responsibilities:
- decode file text at the boundary
- support UTF-8 text input
- support legacy text-art encodings when explicitly needed
- normalize content into TextObject
- preserve intended line breaks and spacing

Notes:
- .nfo and .diz may require optional legacy code page support for classic art compatibility
- code-page handling must remain in loader/conversion utilities, never in ScreenBuffer or PageComposer

4.4 Text asset saving support
Support saving where it makes architectural sense:
- save TextObject to:
  - .txt
  - .asc
  - .diz
- optional save to .nfo when encoding policy is defined
- export plain text representations from TextObject

Rules:
- plain-text save paths must be explicit about encoding
- UTF-8 should be the default outbound text format unless a legacy format explicitly requires another encoding
- save/export logic must not bypass the unified object model

4.5 ANSI / terminal art loader family
Support loading:
- .ans
- .bin
- .xbin
- .adf

Responsibilities:
- parse source format into:
  - glyphs
  - styles
  - positioning/layout semantics as supported
- normalize imported content into TextObject
- never replay raw ANSI directly to the backend as the primary model

Rules:
- ANSI/control-sequence semantics must be translated into internal Style semantics
- imported content must become engine-owned structured data
- unknown or unsupported commands must fail gracefully without crashing

4.5A-G XP ART LOADER (REXPAINT FORMAT)
- XP ART LOADER STABILIZATION
- XP LAYER COMPOSITOR (REXPAINT FLATTENING + TRANSPARENCY RULES)
- RETAINED XP LAYER DOCUMENT MODEL
- LAYER VISIBILITY TOGGLES + COMPOSITING POLICY MODES
- LAYER METADATA PRESERVATION + DIAGNOSTICS INSPECTION SUPPORT
- XP MULTI-FRAME  ANIMATION IMPORT FOUNDATION

4.6 ANSI / terminal art saving support
Support saving where it makes architectural sense:
- save/export TextObject to:
  - .ans
  - .bin
- optional later:
  - .xbin
  - .adf

Rules:
- saving to terminal-art formats is an exporter responsibility
- exporter behavior must be explicit about format limitations
- if a TextObject contains styling/features the target format cannot express, exporter must:
  - clamp
  - approximate
  - or fail clearly
- never hide lossy conversion

4.6A XP ART SAVING SUPPORT (REXPAINT FORMAT)

4.6B XP ANIMATION EXPORT SUPPORT (REXPAINT FORMAT)

Support saving/exporting multi-frame XP content where it makes architectural sense.

Support:
- export retained XP sequence to:
  - multi-layer .xp files (static)
  - multi-frame XP sequences (engine-defined or future-compatible format)

Responsibilities:
- convert XpSequence / XpFrame → XP-compatible output
- preserve:
  - layer order
  - glyphs
  - RGB foreground/background
  - transparency (magenta handling)
- support optional frame labeling or metadata if present

Rules:
- exporter must be explicit about limitations:
  - XP format does not natively support animation → define:
    - frame-per-file strategy OR
    - engine-side sequence format (.xpseq or similar)
- do not silently discard:
  - hidden layers
  - compositing policy differences
- flattening must be optional and explicit:
  - allow export of:
    - flattened result
    - retained layered document

Notes:
- this phase establishes export symmetry with 4.5G import foundation
- animation playback is NOT implemented here



4.6BA XPSEQ SEQUENCE FORMAT (ENGINE-NATIVE XP ANIMATION CONTAINER)

Define and implement the .xpseq format as the engine-native container for XP-based animation sequences.

PURPOSE:
- provide a clean, declarative container for multi-frame XP animation
- avoid extending the .xp binary format beyond its intended scope
- integrate seamlessly with:
  - retained XP document model
  - asset system
  - PageComposer frame-aware placement
  - interpreter commands
  - Phase 13 animation playback system

CORE DESIGN:

.xpseq is a UTF-8 manifest format that describes a sequence of XP frames.

It must NOT:
- duplicate XP binary data
- introduce a second rendering model
- bypass retained XP document structures

It must:
- reference existing .xp files as frame sources
- optionally describe playback metadata
- remain human-readable and diagnosable

---

FORMAT OVERVIEW:

File type:
- UTF-8 text
- versioned header (e.g. "XPSEQ 1")

Structure:
- sequence-level properties
- ordered frame blocks

Example:

XPSEQ 1

name = IntroLogo
loop = true
default_frame_duration_ms = 125
default_composite = Default
default_visible_layers = All

frame {
    index = 0
    source = Assets/XP/logo_000.xp
}

frame {
    index = 1
    source = Assets/XP/logo_001.xp
    duration_ms = 250
}

---

SUPPORTED FIELDS:

Sequence-level (optional unless noted):
- name
- loop (true/false)
- ping_pong (future)
- default_frame_duration_ms
- default_fps (optional convenience)
- default_composite (maps to XpCompositeMode)
- default_visible_layers (All / Default / explicit list)

Frame-level (required: index, source):
- index (required)
- source (required path to .xp)
- label (optional)
- duration_ms (optional override)
- composite (optional override)
- visible_layers (optional override)

---

4.6BA.1 XPSEQ DATA MODEL

Introduce:

struct XpSequence
- name
- loop / playback flags
- default conversion settings
- vector<XpFrame>

struct XpFrame
- index
- source path
- shared_ptr<XpDocument>
- optional overrides:
  - duration
  - composite mode
  - visible layers

Rules:
- XpSequence owns frame ordering
- XpFrame references retained XpDocument (not flattened data)
- must support per-frame conversion overrides

---

4.6BA.2 XPSEQ LOADER

Add:

XpSequenceLoader

Responsibilities:
- parse .xpseq manifest
- validate header/version
- resolve frame source paths
- load referenced .xp files via existing XP loader
- construct XpSequence

Rules:
- must reuse:
  - XpArtLoader
  - retained XP document model
- must not duplicate XP parsing logic
- must support relative path resolution (relative to .xpseq file)
- must produce structured diagnostics

Diagnostics must report:
- invalid header/version
- missing frame blocks
- duplicate frame indices
- missing or invalid source paths
- XP load failures
- invalid composite/visibility values

---

4.6BA.3 XPSEQ ASSET INTEGRATION

Extend Asset system:

Support:
- loading .xpseq files
- caching XpSequence objects
- lookup by:
  - asset name
  - frame index

Rules:
- must integrate with AssetLibrary
- must not break existing TextObject or XP asset handling
- must allow:
  - retrieval of full sequence
  - retrieval of individual frame

---

4.6BA.4 XPSEQ → TEXTOBJECT CONVERSION PATH

Ensure compatibility with existing conversion APIs:

Support:
- buildTextObjectFromXpFrame(...)
- per-frame conversion using:
  - default sequence settings
  - frame-level overrides

Rules:
- conversion must remain:
  - explicit
  - deterministic
  - policy-driven
- must not embed conversion logic in:
  - PageComposer
  - interpreter

---

4.6BA.5 XPSEQ EXPORT SUPPORT

Extend XP export system:

Support:
- exporting XpSequence → .xpseq
- exporting frames:
  - as referenced .xp files
  - or linking to existing XP assets

Rules:
- must not inline XP binary data into .xpseq
- must preserve:
  - frame ordering
  - timing metadata
  - conversion defaults

Notes:
- integrates with 4.6B animation export support

---

4.6BA.6 XPSEQ VALIDATION + ROUND-TRIP SUPPORT

Extend validation system:

Support:
- load → export → reload parity checks
- validation of:
  - frame ordering
  - source resolution
  - metadata preservation

Rules:
- must report:
  - lossy metadata
  - missing frame references
  - inconsistent overrides

---

4.6BA.7 ARCHITECTURAL RULES

.xpseq must follow the engine pipeline:

XpSequence
    ↓
XpFrame (selected)
    ↓
buildTextObjectFromXpFrame(...)
    ↓
PageComposer
    ↓
ScreenBuffer
    ↓
Renderer

Rules:
- .xpseq must not introduce a new rendering path
- .xpseq must not bypass XP retained document model
- .xpseq must not embed renderer or layout behavior
- .xpseq must remain compatible with:
  - interpreter (Phase 6)
  - animation system (Phase 13)

---

OUTPUT OF 4.6BA

- A formal, engine-native XP animation container format
- A loader and asset integration path
- Full compatibility with:
  - XP retained documents
  - PageComposer
  - interpreter commands
  - animation playback system

This establishes .xpseq as the canonical multi-frame XP format for the engine.

4.6C XP ROUND-TRIP FIDELITY SUPPORT

Ensure XP import → export cycles preserve data as accurately as possible.

Purpose:
- validate retained XP architecture correctness
- support editing workflows
- prevent silent data loss

Support:
- load → retain → export → reload parity checks
- preservation of:
  - glyphs
  - RGB colors
  - layer count/order
  - transparency usage
  - document dimensions

Responsibilities:
- add validation helpers:
  - compare XpDocument structures
  - compare flattened results
- detect and report:
  - lossy conversions
  - unsupported conditions

Rules:
- exporter must emit warnings when:
  - style approximations occur
  - transparency rules differ
- diagnostics must integrate with existing warning system

Notes:
- this phase strengthens the XP pipeline as a true asset format, not just an import format



4.6D XP SEQUENCE / ANIMATION ASSET MODEL INTEGRATION

Extend asset system to treat XP sequences as first-class assets.

Support:
- AssetLoader support for:
  - single-frame XP
  - multi-frame XP sequences (future or engine-defined)
- caching of:
  - XpDocument
  - XpSequence

Responsibilities:
- integrate with AssetLibrary / caching layer
- allow lookup by:
  - asset name
  - frame index
- support:
  - retrieving specific frames
  - retrieving full sequence

Rules:
- must not break existing TextObject asset pipeline
- XP assets remain separate from TextObject but convertible to it
- do not force PageComposer to become frame-aware yet

Notes:
- prepares for animation playback in Phase 13



4.6E XP → TEXTOBJECT CONVERSION MODES

Formalize conversion from retained XP data into TextObject.

Support multiple conversion modes:
- Flattened (default)
- Visible-layers-only
- Policy-driven (uses XpCompositeMode)
- Per-frame conversion (for sequences)

Responsibilities:
- provide explicit API:
  - buildTextObjectFromXpDocument(...)
  - buildTextObjectFromXpFrame(...)
- allow:
  - conversion using custom compositing policy
  - conversion using layer visibility state

Rules:
- conversion must be:
  - deterministic
  - diagnosable
- must not embed compositing logic outside XP pipeline

Notes:
- ensures PageComposer integration remains clean and predictable



4.6F XP DIAGNOSTICS + VALIDATION SCREEN SUPPORT

Extend diagnostics system to support XP inspection workflows.

Support:
- visual or textual inspection of:
  - layers
  - visibility state
  - compositing results
  - metadata

Responsibilities:
- integrate XP metadata into:
  - ValidationScreen
  - diagnostics output
- provide helpers:
  - formatXpDocumentSummary(...)
  - formatXpLayerDetails(...)

Rules:
- diagnostics must remain optional
- must not introduce renderer-specific logic

Notes:
- prepares for future editor/debug tooling



4.6G XP EDITING / AUTHORING HOOKS (FOUNDATION ONLY)

Prepare XP system for future editing workflows without implementing a full editor.

Support:
- mutable retained XP document:
  - modify cells
  - toggle layer visibility
  - reorder layers
- safe update pathways:
  - mark dirty state
  - trigger recomposition

Responsibilities:
- define minimal APIs:
  - setCell(...)
  - setLayerVisibility(...)
  - reorderLayer(...)
- ensure compositing can be re-run safely after edits

Rules:
- do NOT build UI/editor in this phase
- do NOT bypass retained document model

Notes:
- this phase enables future:
  - XP editor
  - in-engine art tools
  - procedural generation



4.6H XP FORMAT EXTENSIBILITY + FUTURE COMPATIBILITY

Prepare XP system for format evolution and alternate sources.

Support:
- version-tolerant parsing and export
- extension points for:
  - additional metadata
  - alternate transparency rules
  - custom layer attributes

Responsibilities:
- isolate XP-specific constants and rules
- allow future:
  - alternate import sources (non-RexPaint XP-like formats)
  - extended XP variants

Rules:
- must not break current XP compatibility
- must not overgeneralize prematurely

Notes:
- protects against future format lock-in



4.6I XP ANIMATION PLAYBACK INTEGRATION (HOOK INTO PHASE 13)

Bridge XP sequences into the animation system.

Support:
- mapping:
  - XpSequence → Animation frames
- frame timing placeholders:
  - default duration
  - optional metadata-driven timing later

Responsibilities:
- provide adapter:
  - XpSequence → Animation
- allow PageComposer or UI layer to:
  - render frame N as TextObject

Rules:
- playback logic belongs to Phase 13 (Animation system)
- this phase only provides integration hooks

Notes:
- completes the XP pipeline from:
  import → retain → compose → animate → render

4.7 Asset loading / caching layer
Goal:
- load text assets from Assets directory
- centralize lookup
- avoid repeated disk work
- centralize format detection and dispatch

Responsibilities:
- asset path resolution
- extension/type dispatch
- cache previously loaded normalized TextObject content
- support future asset catalogs

4.8 AsciiBanner integration
Support:
- FIGlet loading
- TOIlet loading
- renderLines
- create TextObject output
- draw into ScreenBuffer through TextObject flow
- keep this as an object/text-generation helper, not a renderer

Rules:
- banner generation returns reusable object content
- banner generation must not directly write to the backend
- banner output should normalize into TextObject

4.9 Font loader support for custom bitmap/pseudo-graphical fonts
Support loading:
- .fon / custom bitmap font mapping formats

Purpose:
- custom UI font themes
- pseudo-graphical rendering in console
- character-to-glyph mapping support for generated text objects

Rules:
- this is a font/asset loading concern, not a renderer concern
- font loaders may feed:
  - AsciiBanner-like generators
  - future glyph/theme systems
- do not turn ScreenBuffer into a font engine

OUTPUT OF PHASE 3
Reusable visual assets that the page layer can place and compose, built around one unified TextObject model with format-specific loaders/parsers and optional exporters.

By the end of this phase the engine should support:
- one primary reusable text object type
- plain text asset loading
- ANSI / terminal art loading
- banner/font-generated text objects
- centralized asset lookup and caching
- saving/export where format fidelity makes sense

FILES / FOLDERS NEEDED

Rendering/Objects/
  TextObject.h
  TextObject.cpp
  AsciiBanner.h
  AsciiBanner.cpp
  ObjectFactory.h
  ObjectFactory.cpp
  PlainTextLoader.h
  PlainTextLoader.cpp
  AnsiLoader.h
  AnsiLoader.cpp
  BinaryArtLoader.h
  BinaryArtLoader.cpp
  TextObjectExporter.h
  TextObjectExporter.cpp
  FontLoader.h
  FontLoader.cpp

Assets/
  ASCII/
  ANSI/
  Fonts/
    FIGlet/
    TOIlet/
    Bitmap/

Utilities/
  AssetPaths.h
  AssetPaths.cpp

Optional later:
Data/Assets/
  AssetLibrary.h
  AssetLibrary.cpp



============================================================
5. PHASE 4 — COMPOSITING / WRITE POLICY SYSTEM (REFACTORED)
===========================================================

PURPOSE
Replace ambiguous write modes with a unified, explicit, and extensible compositing policy system that:

* preserves ANSI-style authoring ergonomics
* eliminates naming ambiguity (transparent vs fully transparent)
* supports future compositing features (depth, masking, layering)
* keeps renderer and style systems fully decoupled

---

WHY THIS PHASE IS FOUNDATIONAL

The page API must sit on top of a correct compositing model.

The previous API:

* mixed intent and behavior
* used unclear terminology
* duplicated functionality across multiple functions

This phase introduces:

* a **single compositing engine**
* driven by an explicit **WritePolicy**
* with **named presets layered on top**

This ensures:

* no behavioral duplication
* no API explosion
* maximum flexibility for future features

---

CRITICAL DESIGN RULE

There are TWO layers:

1. AUTHOR-FACING API (semantic, simple)

   * writeSolidObject(...)
   * writeVisibleObject(...)
   * etc.

2. ENGINE POLICY LAYER (explicit, extensible)

   * writeObject(object, WritePolicy)

ALL behavior must resolve through WritePolicy.

---

5.1 Source cell parts

Each source cell can contribute:

* Glyph (non-space character)
* Space (U+0020 or equivalent blank cell)
* Style

NOTE:
Glyph vs Space is a rendering distinction, not a Unicode distinction.

---

5.2 Glyph policy (REPLACES glyph/space booleans)

Define:

enum class GlyphPolicy
{
None,           // do not write glyphs
NonSpaceOnly,   // skip spaces
All             // write all glyphs including spaces
};

This replaces:

* writeGlyph
* writeSpaces

This avoids invalid combinations and simplifies reasoning.

---

5.3 Style policy

enum class StylePolicy
{
None,       // do not write style
Apply       // apply style based on overwrite rules
};

---

5.4 Source mask (optional filtering layer)

enum class SourceMask
{
AllCells,
GlyphCellsOnly,
SpaceCellsOnly
};

Purpose:

* filter source before compositing
* useful for advanced masking behavior

---

5.5 Overwrite rules (expanded)

Glyph overwrite:

enum class GlyphOverwrite
{
Always,
IfDestinationEmpty,
Never
};

Style overwrite:

enum class StyleOverwrite
{
Always,
IfDestinationUnstyled,
Never
};

---

5.6 Depth / layering policy (NEW)

Prepare for Phase 8 integration.

enum class DepthPolicy
{
Ignore,         // overwrite regardless of depth
Respect,        // only overwrite if passes depth test
BehindOnly      // only write if behind existing content
};

NOTE:
Depth is logical (composition order), not renderer-specific.

---

5.7 WritePolicy struct (CORE ENGINE TYPE)

struct WritePolicy
{
GlyphPolicy glyphPolicy = GlyphPolicy::All;
StylePolicy stylePolicy = StylePolicy::Apply;

```
SourceMask sourceMask = SourceMask::AllCells;

GlyphOverwrite glyphOverwrite = GlyphOverwrite::Always;
StyleOverwrite styleOverwrite = StyleOverwrite::Always;

DepthPolicy depthPolicy = DepthPolicy::Ignore;
```

};

RULES:

* MUST be immutable during a write call
* MUST fully define behavior (no hidden state)
* MUST be backend-agnostic
* MUST NOT depend on renderer

---

5.8 Core compositing function

Single entry point:

writeObject(const TextObject&, const WritePolicy&)

Responsibilities:

* iterate source cells
* apply sourceMask
* respect wide glyph atomicity
* evaluate overwrite rules
* apply glyph/style according to policy

CRITICAL RULE:
Wide glyphs must be treated as atomic units.

---

5.9 Named convenience presets (PRIMARY AUTHOR API)

These map directly to WritePolicy.

inline constexpr WritePolicy SolidObject =
{
GlyphPolicy::All,
StylePolicy::Apply
};

inline constexpr WritePolicy VisibleObject =
{
GlyphPolicy::NonSpaceOnly,
StylePolicy::Apply
};

inline constexpr WritePolicy GlyphsOnly =
{
GlyphPolicy::NonSpaceOnly,
StylePolicy::None
};

inline constexpr WritePolicy StyleMask =
{
GlyphPolicy::None,
StylePolicy::Apply
};

inline constexpr WritePolicy StyleBlock =
{
GlyphPolicy::None,
StylePolicy::Apply,
SourceMask::AllCells
};

---

5.10 Depth-aware presets (NO NEW ENGINE PATHS)

These are wrappers ONLY.

Examples:

writeVisibleObjectBehind(...)
→ WritePolicy with:
depthPolicy = DepthPolicy::BehindOnly

NO separate compositing logic allowed.

---

5.11 Advanced author usage (OPTIONAL)

Authors may directly use WritePolicy:

writeObject(object, customPolicy);

This is intended for:

* special masking behavior
* conditional overwrites
* advanced layering logic

---

5.12 Explicit non-goal: NO mutable state machine

The system MUST NOT support:

writer.setMode(...)
writer.write(...)

Reason:

* hidden state causes bugs
* order-dependent behavior
* difficult debugging

ALL writes must be explicit and self-contained.

---

5.13 Future extension hooks

This design allows future additions WITHOUT API break:

* blend modes (additive, xor, etc.)
* alpha-style logical blending
* stencil/mask buffers
* per-layer compositing rules

---

OUTPUT OF PHASE 4

A unified compositing system that:

* replaces ambiguous legacy write modes
* eliminates duplicate logic
* supports future layering systems
* provides both simple API and advanced control

---

FILES / FOLDERS NEEDED

Rendering/Composition/
WritePolicy.h
WritePolicy.cpp
GlyphPolicy.h
StylePolicy.h
SourceMask.h
OverwriteRule.h
DepthPolicy.h
ObjectWriter.h
ObjectWriter.cpp
WritePresets.h
WritePresets.cpp



============================================================
6. PHASE 5 — PAGE COMPOSER API 1.0
============================================================

PURPOSE
Build the real authoring API for creating TUI pages in C++.

WHY THIS PHASE COMES AFTER COMPOSITING
The page API should expose good author-facing names and placement features, but it should sit on top of stable lower-level rendering, style, object, and compositing primitives.

IMPORTANT DESIGN DECISION
Do not put all authoring behavior directly onto low-level ScreenBuffer.
Build a higher-level PageComposer or PageBuffer layer that writes into ScreenBuffer.

UPDATE NOTES:

TEXT API MUST CHANGE TO:

- writeText(x, y, std::u32string, style)
- writeTextUtf8(...) → wrapper only

Text layout responsibilities:
- must use UnicodeWidth + grapheme segmentation
- must NOT assume 1 char = 1 column

render() MUST BECOME:
- renderToUtf8String()

API 1.0 SHOULD INCLUDE

6.1 Page clearing
- clear(style)

6.2 Screen template loading
- createScreen(filename)
Behavior:
- load at (0,0)
- do not auto-clear first
This preserves the old layering workflow described in the docs. 

6.3 Named regions
- createRegion(x, y, w, h, name)
- hasRegion(name)
- getRegion(name)
- clearRegions()

6.4 Alignment vocabulary
- HorizontalAlign: Left, Center, Right
- VerticalAlign: Top, Center, Bottom

6.5 Text writing
- writeText(x, y, text, style)
- writeTextBlock(x, y, block, style)

6.6 Object writing by coordinate
- writeSolidObject(...)
- writeVisibleObject(...)
- writeGlyphsOnly(...)
- writeStyleMask(...)
- writeStyleBlock(...)
- plus generic writeObject(..., WritePolicy)

6.6A Object source abstraction (NEW)

PageComposer must accept object sources beyond raw TextObject without breaking the unified composition model.

Support:
- TextObject (existing)
- XP-derived objects via explicit conversion
- future asset types (animation frames, generated content)

Responsibilities:
- all non-TextObject inputs must be converted before composition
- PageComposer must NOT embed format-specific parsing logic

Rules:
- PageComposer operates ONLY on:
  - TextObject
  - WritePolicy
- conversion must happen through:
  - loader utilities
  - conversion helpers (e.g., XP → TextObject)

Notes:
- ensures PageComposer remains format-agnostic while supporting XP pipeline

6.7 Object writing by region + alignment
- same operations using region name and alignments

6.6B XP object placement support (NEW)

Allow XP-derived content to be placed through explicit conversion paths.

Support:
- place XP document via:
  - buildTextObjectFromXpDocument(...)
- place XP frame via:
  - buildTextObjectFromXpFrame(...)

Responsibilities:
- ensure compositing policy used during conversion is explicit
- allow passing:
  - XpCompositeMode
  - layer visibility configuration

Rules:
- PageComposer must not:
  - directly access XpDocument internals
- all XP placement must go through conversion APIs

Notes:
- keeps XP system cleanly layered under PageComposer

6.8 Full-screen aligned placement
- same operations using implicit full screen as region

6.9 Render / composition output (UPGRADE)

Replace:
- render() -> std::string

With:
- renderToUtf8String()
- renderToU32String()
- optional:
  - captureBuffer() → ScreenBuffer snapshot

Responsibilities:
- provide deterministic output for:
  - testing
  - animation systems
  - diagnostics

Rules:
- render output must reflect fully composed buffer state
- must not depend on renderer behavior

Notes:
- required for animation playback and snapshot testing

6.10 Optional future-friendly API helpers
- centerX()
- centerInRegion()
- anchor helpers
- named placement helpers

6.11 Structured data-driven object creation hooks
Prepare PageComposer to support future structured formats without coupling this phase to those parsers.

Future object/layout formats should be able to resolve into:
- TextObject
- PageComposer placement calls
- region/alignment-based placement

This keeps later declarative formats aligned with the same composition model rather than creating a second UI system.

6.12 Asset-aware placement layer

Introduce a thin integration layer between Asset system and PageComposer.

Support:
- place asset by name:
  - placeAsset("logo", x, y)
- resolve:
  - TextObject assets
  - XP assets (single-frame or sequence)

Responsibilities:
- resolve asset → correct runtime representation
- convert to TextObject if needed

Rules:
- PageComposer must not:
  - perform file IO
  - perform format detection
- must rely on AssetLibrary / loader system

Notes:
- prepares interpreter and data-driven layouts

6.13 Frame-aware placement (non-breaking extension)

Allow PageComposer to optionally work with frame-based content without requiring animation awareness.

Support:
- placeFrame(frameIndex, ...)
- placeSequenceFrame(assetName, frameIndex, ...)

Responsibilities:
- resolve frame → TextObject
- allow caller to control frame selection

Rules:
- PageComposer must NOT:
  - manage timing
  - manage playback
- this is a static placement of a selected frame

Notes:
- enables animation systems in Phase 13 to drive PageComposer

6.14 Composition diagnostics integration

Expose composition-level diagnostics for debugging and validation.

Support:
- record:
  - objects placed
  - regions used
  - WritePolicy usage
- optional debug output:
  - composition summary

Responsibilities:
- integrate with existing diagnostics system
- allow ValidationScreen to inspect:
  - composition decisions
  - object placement order

Rules:
- diagnostics must be optional
- must not affect runtime behavior

Notes:
- complements XP diagnostics system

6.15 Deterministic composition contract

Guarantee that PageComposer produces identical output for identical inputs.

Purpose:
- enable:
  - snapshot testing
  - animation frame stability
  - reproducible rendering

Requirements:
- no hidden state
- no dependency on:
  - timing
  - renderer state
- composition order must be explicit

Rules:
- all writes must be:
  - explicit
  - ordered
  - deterministic

Notes:
- critical for animation and testing systems

6.16 Animation system integration hooks

Prepare PageComposer to integrate cleanly with Phase 13 animation systems.

Support:
- external systems can:
  - drive frame index
  - recompose page per frame

Responsibilities:
- ensure PageComposer can be:
  - reused per frame without reset issues
- support:
  - fast recomposition
  - partial redraw compatibility

Rules:
- PageComposer must not:
  - manage animation timing
- must remain a pure composition layer

Notes:
- enables:
  - XP animation playback
  - animated banners
  - UI transitions

OUTPUT OF PHASE 5
The first complete authoring API for building pages cleanly in C++.

FILES / FOLDERS NEEDED

Composition/
  PageComposer.h
  PageComposer.cpp
  RegionRegistry.h
  RegionRegistry.cpp
  Alignment.h
  Placement.h
  Placement.cpp

Optional alias layer if you prefer:
Page/
  PageBuffer.h
  PageBuffer.cpp



============================================================
7. PHASE 6 — PAGE FILE FORMAT AND INTERPRETER
============================================================

PURPOSE
Allow people to build pages without Visual Studio.

WHY THIS PHASE COMES HERE
The interpreter should map onto an already-stable PageComposer API. It should not define the engine primitives; it should consume them.

UPDATE NOTES:

- ALL file input is UTF-8
- Interpreter must:
  - decode → U32 immediately
  - never operate on std::string internally

IMPLEMENT IN THIS PHASE

MODIFY 7.1 TO:

7.1 Define page language syntax
Commands should cover:
- CLEAR
- SCREEN
- REGION
- TEXT
- TEXTBLOCK
- OBJECT
- STYLE
- MODE
- ALIGN
- AT
- ASSET
- XP
- FRAME
- SEQUENCEFRAME
- optional comments

Rules:
- command set must remain a thin declarative layer over PageComposer
- commands must resolve into the same composition APIs used by native C++ code
- XP and animation-aware commands must not introduce a second rendering or composition model

7.2 Page parser
- tokenize commands
- parse arguments
- report line/file errors

7.3 Page interpreter
- resolve assets
- resolve themes/styles
- call PageComposer methods

7.3A Asset-aware interpreter resolution

Extend interpreter resolution so script commands can target asset-backed content, not just inline text/object definitions.

Support:
- resolve named TextObject assets
- resolve named XP assets
- resolve named XP sequences
- resolve frame-specific asset references

Responsibilities:
- use centralized asset lookup
- route resolved assets through:
  - TextObject placement
  - XP → TextObject conversion helpers
  - frame selection helpers where applicable

Rules:
- interpreter must not perform file-format parsing directly
- interpreter must not bypass AssetLibrary / loader systems
- all resolved content must still compose through PageComposer

Notes:
- this is the interpreter-side counterpart to Phase 5 asset-aware placement

7.3B XP document placement commands

Add script commands for placing retained XP content through explicit conversion paths.

Support commands such as:
- XP "assetName" AT x y
- XP "assetName" IN regionName ALIGN hAlign vAlign

Optional command arguments:
- COMPOSITE Default
- COMPOSITE GlyphPriority
- VISIBLELAYERS Default
- VISIBLELAYERS All
- VISIBLELAYERS "0,2,3"

Responsibilities:
- resolve the XP asset
- convert retained XP document to TextObject using explicit conversion settings
- place the resulting TextObject through PageComposer

Rules:
- interpreter must not directly manipulate XpDocument internals
- compositing and visibility choices must be explicit and diagnosable
- default behavior must match the default XP flattening path already defined by the loader/conversion system

Notes:
- preserves the separation between XP retained data and PageComposer composition

7.3C Frame-aware static placement commands

Allow scripts to place a chosen frame from a sequence without introducing playback logic into the interpreter.

Support commands such as:
- FRAME "assetName" 0 AT x y
- FRAME "assetName" 3 IN regionName ALIGN Center Top
- SEQUENCEFRAME "assetName" INDEX 5 AT x y

Responsibilities:
- resolve sequence asset
- select requested frame
- convert selected frame to TextObject
- compose the frame through normal PageComposer placement

Rules:
- frame selection is static for the current page execution unless later bound to an animation controller
- interpreter must not manage timing, playback, looping, or animation state here
- out-of-range frame requests must fail clearly with line/file diagnostics

Notes:
- this maps directly to Phase 5 frame-aware placement without making the interpreter itself an animation engine
- this also establishes the same frame-addressing vocabulary that Phase 13 animation playback will drive later

7.3D Named conversion / compositing options in script

Expose a small declarative vocabulary so scripts can request XP conversion behavior safely.

Support:
- COMPOSITE <mode>
- FRAME <index>
- VISIBLELAYERS <preset or explicit list>
- FLATTEN <true|false when supported>
- POLICY <named placement/composition preset when applicable>
- BIND <animationBindingName> for future animation-controlled frame sources

Responsibilities:
- parse simple, explicit option values
- validate against supported runtime options
- forward validated options into:
  - XP conversion helpers
  - PageComposer placement calls
  - future animation binding descriptors where applicable

Rules:
- option names should map to real engine enums/configuration rather than ad hoc string behavior
- unsupported options must produce explicit parser/interpreter diagnostics
- script options must not mutate global hidden state
- BIND declarations in Phase 6 are descriptive only; playback behavior belongs to Phase 13

Notes:
- keeps data-driven pages aligned with explicit engine policy types
- prepares script-authored placements to be animation-driven later without redesigning the language

7.4 Page asset references
- resolve ASCII assets
- resolve screen templates
- resolve fonts if needed later

7.4A XP and sequence asset references

Extend page asset references to include retained XP and frame-sequence assets.

Support:
- single XP document references
- XP sequence references
- optional future engine-defined XP sequence container references

Examples of referenced asset classes:
- screen template assets
- TextObject assets
- XP document assets
- XP sequence assets

Rules:
- asset references must resolve through the same centralized asset/path system
- interpreter syntax may distinguish:
  - OBJECT for TextObject-like assets
  - XP for retained XP assets
  - FRAME / SEQUENCEFRAME for frame-driven placement
- references must remain declarative and stable across native/API/script usage

7.5 Style/theme name lookup
- e.g. "Field", "Helmets", "Title"

7.6 Page execution result
- build final page into ScreenBuffer / Surface

7.6A Page execution must remain deterministic

Interpreter-driven page execution must produce the same composed result for the same script input and asset state.

Requirements:
- no hidden interpreter mode state
- no implicit dependence on playback time
- frame selection must be explicit in the script or provided by a higher-level animation system later

Rules:
- all XP and frame-aware commands must resolve into deterministic PageComposer calls
- output must remain suitable for:
  - snapshot testing
  - diagnostics
  - future animation recomposition loops

Notes:
- this matches the deterministic composition contract defined for PageComposer

7.7 Testing/examples
- create sample page files
- verify parity with equivalent C++ page definitions

7.7A XP / animation-aware script examples and tests

Add focused examples and tests for XP-aware and frame-aware script behavior.

Coverage should include:
- XP asset placed at explicit coordinates
- XP asset placed in a named region with alignment
- XP asset placed with alternate compositing mode
- selected frame from sequence placed statically
- invalid frame index diagnostics
- invalid asset reference diagnostics
- parity checks between:
  - equivalent C++ PageComposer usage
  - equivalent interpreted script usage

Rules:
- tests should validate final composed output and diagnostics
- tests should not depend on renderer-specific behavior
- frame-aware script tests must remain static/deterministic in Phase 6

Notes:
- this protects the script layer from diverging from the C++ authoring layer

7.8 Structured page/config input formats (initial declarative layer)
Support later parsing/import for:
- .json
- .yaml
- .ini

Purpose:
- declarative page descriptions
- configuration-driven authored screens
- future style/theme/layout definitions

Rules:
- these formats must compile down to the same PageComposer / TextObject / style APIs
- they must not introduce a second rendering or composition model
- use them for page/layout description, not as a replacement for ScreenBuffer or TextObject

7.8A Structured declarative support for XP and frame placement

When JSON / YAML / INI page formats are added, they must be able to describe the same XP-aware and frame-aware placement semantics as the script language.

Support:
- XP asset placement
- frame index selection
- region/alignment placement
- explicit conversion/compositing options

Rules:
- structured formats must compile down to the same interpreter/PageComposer operations
- structured formats must not create a parallel XP composition path
- XP/frame placement in structured formats must remain explicit and deterministic

7.9 Structured asset references
Structured page files may reference:
- TextObject assets
- fonts
- themes
- regions
- screen templates

All references must resolve through centralized asset/path systems.

7.10 Interpreter hooks for future animation systems

Prepare the interpreter so Phase 13 animation systems can drive frame changes without redesigning the page language.

Support:
- script-authored frame placeholders
- externally supplied frame index overrides
- named animation bindings
- page recomposition using selected frame values

Responsibilities:
- allow future animation system to bind:
  - sequence asset
  - current frame index
  - optional playback state identifier
- keep interpreter-side representation compatible with re-evaluation/recomposition
- preserve the original declarative source intent so the same page can be recomposed at different animation states

Rules:
- Phase 6 does NOT implement playback
- Phase 6 only defines script/addressing structure that later animation systems can drive
- timing, looping, interpolation, and invalidation belong to Phase 13

Notes:
- this cleanly bridges:
  - Phase 4.5G XP sequence foundation
  - Phase 4.6I animation playback integration
  - Phase 5 frame-aware placement
  - Phase 13 animation/timer systems
  
7.11 Declarative animation target references

Allow page scripts to declare animation targets that later systems can resolve without changing page composition syntax.

Support:
- naming a sequence-backed placement target
- associating a placement with an animation binding name
- optional default frame selection for non-animated execution

Examples of intent:
- a title logo sequence placed at a region
- a splash banner sequence bound to "IntroLogo"
- a decorative sequence with a default fallback frame

Responsibilities:
- preserve these references in the parsed/interpreted page model
- allow Phase 13 systems to discover which placed assets are animation-capable
- allow non-animated execution to fall back to explicit/default frame selection cleanly

Rules:
- target references must remain declarative
- target references must not execute playback behavior in Phase 6
- static execution path must still be deterministic when no animation system is active

Notes:
- this prevents script redesign later when animated page assets are introduced  
  
7.12 Recomposition-safe interpreter output

Interpreter output must be safe to re-run repeatedly as animation state changes.

Purpose:
- enable frame-driven recomposition from Phase 13 without rebuilding the page language
- ensure animation playback can reuse the same page definitions safely

Requirements:
- parsed page representation must remain reusable
- resolved asset references should support repeat evaluation
- frame-bound placements must be recomposed from declarative state, not patched through hidden mutable output state

Rules:
- interpreter must support:
  - parse once
  - re-evaluate many times
- animation systems must drive recomposition by supplying state, not by mutating parser behavior
- recomposition must remain deterministic for the same asset state and frame bindings

Notes:
- this is critical for efficient and correct animation playback on top of interpreted pages  
  
OUTPUT OF PHASE 6
A first interpreted page system that uses the same engine as the C++ API.

FILES / FOLDERS NEEDED

Interpreter/
  Token.h
  Lexer.h
  Lexer.cpp
  Parser.h
  Parser.cpp
  PageScriptAst.h
  PageScriptInterpreter.h
  PageScriptInterpreter.cpp
  PageCommand.h
  PageCommand.cpp
  ThemeResolver.h
  ThemeResolver.cpp

Assets/
  Pages/



============================================================
8. PHASE 7 — APPLICATION INPUT / EVENTS / COMMANDS
============================================================

PURPOSE
Move from passive page rendering to interactive TUI behavior.

WHY THIS PHASE COMES AFTER PAGE AUTHORING
You wanted page composition to feel simple first. Input and interactivity should be layered on once the display model is stable.

IMPLEMENT IN THIS PHASE

8.1 InputManager
- raw key polling
- normalization

8.2 KeyCode abstraction
- platform-independent keys

8.3 Command abstraction
- MoveUp
- MoveDown
- Confirm
- Cancel
- NextFocus
- PreviousFocus
- OpenHelp
etc.

8.4 Event model
- KeyEvent
- CommandEvent
- ResizeEvent
- TickEvent
- later MouseEvent

8.5 CommandMap
- key to command mapping

8.6 Event dispatch path
- application to active page/screen
- later to focused widget/window

OUTPUT OF PHASE 7
A clean event/input layer ready for menus and widgets.

FILES / FOLDERS NEEDED

Input/
  KeyCodes.h
  Command.h
  Event.h
  InputManager.h
  InputManager.cpp
  CommandMap.h
  CommandMap.cpp

============================================================
8–15 PHASES Rules
============================================================

GLOBAL UPDATE (applies to ALL REMAINING PHASES):

1. All text input:
   - UTF-8 at boundaries only
   - internal = std::u32string

2. All layout:
   - must use UnicodeWidth
   - must support width 0 / 1 / 2

3. No system may:
   - assume fixed-width characters
   - assume ASCII

4. Rendering:
   - always operates on ScreenCell grid

5. Widgets / UI:
   - must use grapheme-aware layout
   - must not break wide glyphs across boundaries

6. Debug / Testing:
   - must use UTF-8 snapshot output
   - must not truncate to char


============================================================
9. PHASE 8 — LAYERING / COMPOSITOR / PANELS / WINDOWS
============================================================

PURPOSE
Support layered UI, panels, popups, and future overlapping windows.

WHY THIS PHASE COMES BEFORE WIDGETS
Widgets and dialogs need a layered composition environment. This follows the original architecture guidance around surfaces/layers, panels/windows, and reusable popup structure. 

IMPLEMENT IN THIS PHASE

9.1 Layer/Surface instances
- position
- visibility
- z-order

9.2 Compositor
- compose multiple surfaces into final frame
- clipping
- ordering

9.3 UIElement base
- bounds
- visible
- enabled
- draw/update hooks

9.4 Panel
- titled rectangular region
- background/frame rendering

9.5 Window
- top-level panel semantics
- modal flag

9.6 PopupWindow
- open/close behavior
- modal display

9.7 WindowManager
- window list
- z-order
- show/hide
- modal routing

OUTPUT OF PHASE 8
Reusable layered UI foundation for windows and overlays.

FILES / FOLDERS NEEDED

Rendering/
  Compositor.h
  Compositor.cpp
  LayerInstance.h
  LayerInstance.cpp

UI/Base/
  UIElement.h
  UIElement.cpp
  WindowManager.h
  WindowManager.cpp

UI/Panels/
  Panel.h
  Panel.cpp
  Window.h
  Window.cpp
  PopupWindow.h
  PopupWindow.cpp



============================================================
10. PHASE 9 — FOCUS, MENU SYSTEM, STATUS BARS
============================================================

PURPOSE
Make interactive authored pages practical and consistent.

IMPLEMENT IN THIS PHASE

10.1 FocusManager
- current focused element
- set/next/previous focus
- modal awareness later

10.2 StatusBar
- command hints
- page-level instructions

10.3 Menu primitives
- MenuItem
- MenuModel
- MenuView
- MenuController

10.4 Navigation patterns
- vertical navigation
- horizontal navigation
- confirm/cancel
- wrap/clamp
- disabled item skipping

10.5 First interactive authored page
- title menu
- settings page
or
- help/about popup

10.6 Structured configuration support for menus and status content
Optional use of:
- .json
- .yaml
- .ini

for menu definitions, command hints, and authored interactive metadata, provided they resolve into the same runtime UI model.

This should remain data-driven configuration on top of the existing engine, not a competing architecture.

OUTPUT OF PHASE 9
The first genuinely usable interactive TUI pages.

FILES / FOLDERS NEEDED

UI/Base/
  FocusManager.h
  FocusManager.cpp

UI/Menus/
  MenuItem.h
  MenuItem.cpp
  MenuModel.h
  MenuModel.cpp
  MenuView.h
  MenuView.cpp
  MenuController.h
  MenuController.cpp

UI/Panels/
  StatusBar.h
  StatusBar.cpp



============================================================
11. PHASE 10 — VIEWPORTS / SCROLLING / HELP PANELS
============================================================

PURPOSE
Support content larger than the visible area.

IMPLEMENT IN THIS PHASE

11.1 Viewport
- content size
- view size
- scroll offsets

11.2 ScrollablePanel
- clipping-aware rendering
- keyboard scrolling

11.3 TextView / read-only panels
- manuals
- reports
- help text
- logs

11.4 Tile / grid-based content formats
Support grid-oriented asset/layout loading for larger structured content areas.

Candidate formats:
- .csv grid layouts
- custom tile/grid definitions
- Cogmind-style tileset-inspired mapping formats later if needed

Purpose:
- maps
- dashboards
- structured panels
- tile-oriented authored layouts

Rules:
- tile/grid data must normalize into engine-owned structures
- visible rendering must still end in ScreenCell / ScreenBuffer composition
- no special bypass around the rendering core

OUTPUT OF PHASE 10
A reusable scrolling system for complex text-heavy screens.

FILES / FOLDERS NEEDED

UI/Base/
  Viewport.h
  Viewport.cpp

UI/Panels/
  ScrollablePanel.h
  ScrollablePanel.cpp

UI/Widgets/
  TextView.h
  TextView.cpp



============================================================
12. PHASE 11 — WIDGET SYSTEM 1.0
============================================================

PURPOSE
Move from page composition + menus to reusable full UI controls.

IMPLEMENT IN THIS PHASE

12.1 Widget base
- bounds
- focusable
- draw
- update
- handleEvent

12.2 ContainerWidget
- owns child widgets
- child drawing
- child dispatch
- child focus traversal

12.3 Initial widgets
- Label
- Button
- ListBox
- TextView if not already placed earlier

12.4 Widget style integration
- use Style / UITheme roles
- support focused/disabled/selected states

OUTPUT OF PHASE 11
The first real reusable widget framework.

FILES / FOLDERS NEEDED

UI/Widgets/
  Widget.h
  Widget.cpp
  ContainerWidget.h
  ContainerWidget.cpp
  Label.h
  Label.cpp
  Button.h
  Button.cpp
  ListBox.h
  ListBox.cpp
  TextView.h
  TextView.cpp



============================================================
13. PHASE 12 — PAGE LANGUAGE EXTENSIONS FOR INTERACTIVE LAYOUTS
============================================================

PURPOSE
Extend interpreted pages beyond static composition.

IMPLEMENT IN THIS PHASE

13.1 Widget declarations in page files
- LABEL
- BUTTON
- MENU
- LIST
- PANEL

13.2 Interactive page binding
- commands
- actions
- selection IDs
- focus order

13.3 Interpreter support for widget trees
- parse widgets
- instantiate widgets
- attach to page/screen

13.4 Structured widget/layout import
Allow future import of widget/layout definitions from:
- .json
- .yaml
- .ini

These must instantiate the same widget trees and PageComposer placement semantics used by native code and interpreted page scripts.

OUTPUT OF PHASE 12
Interpreted interactive page layouts, not just static screens.

FILES / FOLDERS NEEDED

Interpreter/
  WidgetScriptAst.h
  WidgetFactory.h
  WidgetFactory.cpp
  ActionResolver.h
  ActionResolver.cpp



============================================================
14. PHASE 13 — ANIMATION / TIMERS / INVALIDATION
============================================================

PURPOSE
Add polish without breaking the architecture.

IMPLEMENT IN THIS PHASE

14.1 TickClock
- delta time
- timing helpers

14.2 Tick events
- periodic UI updates

14.3 Animation model
- duration
- elapsed
- progress
- target properties
- frame-based playback sources
- sequence-backed playback state
- optional named animation bindings for interpreted pages

Requirements:
- animation model must support:
  - time-driven frame selection
  - explicit frame index override
  - deterministic binding from animation state to composed page output
- animation model must remain separate from renderer logic

14.4 Invalidation system
- invalidate rect
- invalidate whole page
- invalidate element/window
This was part of the earlier architecture guidance and belongs here once richer UI is present. 

14.5 Animated / multi-frame text asset formats
Support later loading/playback of time-based text assets such as:
- multi-frame .ans animation
- .txt frame sequences
- XP sequence / retained XP animation assets
- custom .anim format

Purpose:
- animated banners
- splash/title effects
- lightweight TUI motion systems
- XP-backed animated art and overlays

Rules:
- animations must be modeled as frame sequences or time-based asset sets
- playback must compose through existing surfaces/buffers
- animation formats must not bypass compositing or PageComposer placement semantics
- interpreted page animation must drive the same frame-aware placement model defined in Phase 6 rather than introducing a parallel playback path

Purpose:
- animated banners
- splash/title effects
- lightweight TUI motion systems

Rules:
- animations must be modeled as frame sequences or time-based asset sets
- playback must compose through existing surfaces/buffers
- animation formats must not bypass compositing or PageComposer placement semantics

14.6 Sequence playback controller

Add a reusable controller for frame-sequence playback.

Support:
- play
- pause
- stop
- restart
- loop
- one-shot playback
- explicit frame selection
- optional playback rate control

Responsibilities:
- manage time-to-frame mapping
- expose current frame index for:
  - PageComposer-driven recomposition
  - interpreted page animation bindings
- remain asset-format agnostic where practical

Rules:
- controller must not render directly
- controller must not bypass PageComposer or ScreenBuffer
- controller must work equally for:
  - XP sequences
  - text frame sequences
  - future animation asset sources

Notes:
- this is the runtime counterpart to the frame-selection hooks introduced in Phase 6

14.7 Animation binding resolution for interpreted pages

Allow Phase 13 animation systems to bind runtime playback controllers to declarative animation targets defined in interpreted pages.

Support:
- bind named animation controller to:
  - sequence asset placement
  - frame placeholder
  - XP-backed animated placement
- update frame selection during page recomposition

Responsibilities:
- resolve script-declared animation target references
- map runtime animation state to frame-aware placement calls
- keep the interpreted page source declarative while runtime state remains external

Rules:
- runtime animation binding must not alter the meaning of the original page script
- interpreter AST / page model remains the source of truth for placement intent
- playback state remains external runtime data

Notes:
- this is the missing bridge that makes Phase 6 and Phase 13 fit together cleanly

14.8 Animation-driven recomposition loop

Support recomposing pages when animation state changes.

Responsibilities:
- detect when current frame selection changes
- trigger page recomposition
- produce updated composed buffer state for presentation
- cooperate with invalidation so only needed updates are redrawn when possible

Rules:
- recomposition must reuse:
  - PageComposer
  - interpreter placement model
  - deterministic composition contracts already defined earlier
- animation must not patch renderer output directly
- animation frame updates must flow through the same composition path as static page content

Notes:
- this keeps animation architectural behavior consistent with the rest of the engine

14.9 Animation invalidation integration

Integrate animation playback with invalidation and redraw behavior.

Support:
- invalidate whole page when simplest/necessary
- invalidate affected regions/objects when practical
- distinguish:
  - frame change requiring recomposition
  - animation state change with no visible output difference

Responsibilities:
- work with InvalidationRegion and frame diff systems
- avoid unnecessary redraw where animation only affects a known area
- remain correct before being optimal

Rules:
- correctness first:
  - full recomposition/full invalidation is acceptable initially
- optimization can be added later without changing the animation binding model
- invalidation logic must stay separate from renderer-specific output logic

Notes:
- this is especially important for XP overlays and animated title/splash assets

14.10 XP animation playback integration

Implement the runtime side of XP sequence playback promised by earlier XP phases.

Support:
- XP sequence frame playback
- XP frame selection through animation controllers
- XP sequence placement through:
  - native PageComposer code
  - interpreted page bindings

Responsibilities:
- use:
  - XpSequence
  - buildTextObjectFromXpFrame(...)
  - PageComposer frame-aware placement
- preserve explicit XP compositing/conversion policies during playback

Rules:
- XP animation playback must not bypass retained XP or conversion systems
- playback must remain diagnosable and deterministic for a given asset state and time state
- XP animation must use the same generic animation controller/binding system where possible

Notes:
- this completes the intended path from XP import foundation to actual XP animation playback

14.11 Animation diagnostics and tooling hooks

Expose runtime diagnostics for animation playback and binding behavior.

Support:
- report:
  - active animation bindings
  - current frame index
  - loop/one-shot state
  - invalid binding references
  - sequence asset resolution failures
- optional logging or inspector support for:
  - interpreted animation targets
  - XP sequence playback
  - recomposition activity

Responsibilities:
- integrate with existing diagnostics patterns
- help authors understand why an animation is or is not advancing as expected

Rules:
- diagnostics must remain optional
- diagnostics must not alter timing or playback behavior

Notes:
- complements interpreter diagnostics, PageComposer diagnostics, and XP diagnostics

OUTPUT OF PHASE 13
Stable time-based polish features layered on top of a finished UI model.

FILES / FOLDERS NEEDED

Animation/
  TickClock.h
  TickClock.cpp
  Animation.h
  Animation.cpp
  Animator.h
  Animator.cpp

Rendering/
  InvalidationRegion.h
  InvalidationRegion.cpp



PHASE 13 ANIMATION ARCHITECTURE RULE

Animation must follow this pipeline:

Animation state / controller
    ↓
Frame selection / binding resolution
    ↓
XP or sequence frame conversion to TextObject
    ↓
PageComposer recomposition
    ↓
ScreenBuffer / Surface
    ↓
Renderer

Rules:
- no direct renderer-side animation
- no bypass around PageComposer
- no hidden mutation of composed output outside the normal composition path

============================================================
15. PHASE 14 — ALTERNATE BACKENDS / BACKEND-NEUTRAL RENDERING
============================================================

PURPOSE
Future-proof the engine for non-console output.

IMPLEMENT IN THIS PHASE

15.1 Strengthen IRenderer seam
- backend-neutral contract
- style mapping abstraction if needed

15.2 Optional ANSI terminal backend

15.3 Optional SDL/SFML fake-terminal backend

15.4 Optional image/debug export backend

OUTPUT OF PHASE 14
Renderer flexibility without disturbing page API or interpreter.

FILES / FOLDERS NEEDED

Rendering/Backends/
  AnsiRenderer.h
  AnsiRenderer.cpp
  SDLRenderer.h
  SDLRenderer.cpp
  DebugImageRenderer.h
  DebugImageRenderer.cpp



============================================================
16. PHASE 15 — TOOLING / TESTING / PAGE AUTHORING QUALITY-OF-LIFE
============================================================

PURPOSE
Make the engine practical to maintain and use.

IMPLEMENT IN THIS PHASE

16.1 Render snapshot tests
- compare renderToString output
- verify layout regressions

16.2 Theme/style preview tools

16.3 Asset preview tools

16.4 Page linting / script validation

16.5 Optional page editor later

OUTPUT OF PHASE 15
A maintainable and author-friendly ecosystem around the engine.

FILES / FOLDERS NEEDED

Tools/
  ThemePreviewTool/
  PageLintTool/
  AssetPreviewTool/

Tests/
  RenderingTests/
  PageComposerTests/
  InterpreterTests/
  WidgetTests/



============================================================
17. UPDATED VISUAL STUDIO FOLDER / FILE LAYOUT
============================================================

This updates the earlier architecture/file roadmap to support API 1.0 and interpreter work.

TUI/
│
├── App/
│   ├── Application.h
│   └── Application.cpp
│
├── Core/
│   ├── Point.h
│   ├── Size.h
│   ├── Rect.h
│   └── Enums.h
│
├── Rendering/
│   ├── ScreenCell.h
│   ├── ScreenBuffer.h
│   ├── ScreenBuffer.cpp
│   ├── Surface.h
│   ├── Surface.cpp
│   ├── FrameDiff.h
│   ├── FrameDiff.cpp
│   ├── InvalidationRegion.h
│   ├── InvalidationRegion.cpp
│   ├── IRenderer.h
│   ├── ConsoleRenderer.h
│   ├── ConsoleRenderer.cpp
│   ├── Compositor.h
│   ├── Compositor.cpp
│   ├── LayerInstance.h
│   ├── LayerInstance.cpp
│   │
│   ├── Styles/
│   │   ├── Color.h
│   │   ├── Style.h
│   │   ├── Style.cpp
│   │   ├── StyleBuilder.h
│   │   ├── StyleBuilder.cpp
│   │   ├── StyleMerge.h
│   │   ├── StyleMerge.cpp
│   │   ├── BannerThemes.h
│   │   └── UIThemes.h
│   │
│   ├── Objects/
│   │   ├── TextObject.h
│   │   ├── TextObject.cpp
│   │   ├── AsciiBanner.h
│   │   ├── AsciiBanner.cpp
│   │   ├── ObjectFactory.h
│   │   ├── ObjectFactory.cpp
│   │   ├── PlainTextLoader.h
│   │   ├── PlainTextLoader.cpp
│   │   ├── AnsiLoader.h
│   │   ├── AnsiLoader.cpp
│   │   ├── BinaryArtLoader.h
│   │   ├── BinaryArtLoader.cpp
│   │   ├── TextObjectExporter.h
│   │   ├── TextObjectExporter.cpp
│   │   ├── FontLoader.h
│   │   └── FontLoader.cpp
│   │
│   └── Composition/
│       ├── SourceMask.h
│       ├── OverwriteRule.h
│       ├── WritePolicy.h
│       ├── WritePolicy.cpp
│       ├── ObjectWriter.h
│       ├── ObjectWriter.cpp
│       ├── WritePresets.h
│       └── WritePresets.cpp
│
├── Composition/
│   ├── PageComposer.h
│   ├── PageComposer.cpp
│   ├── RegionRegistry.h
│   ├── RegionRegistry.cpp
│   ├── Alignment.h
│   ├── Placement.h
│   └── Placement.cpp
│
├── Input/
│   ├── KeyCodes.h
│   ├── Command.h
│   ├── Event.h
│   ├── InputManager.h
│   ├── InputManager.cpp
│   ├── CommandMap.h
│   └── CommandMap.cpp
│
├── Interpreter/
│   ├── Token.h
│   ├── Lexer.h
│   ├── Lexer.cpp
│   ├── Parser.h
│   ├── Parser.cpp
│   ├── PageScriptAst.h
│   ├── PageScriptInterpreter.h
│   ├── PageScriptInterpreter.cpp
│   ├── PageCommand.h
│   ├── PageCommand.cpp
│   ├── ThemeResolver.h
│   ├── ThemeResolver.cpp
│   ├── WidgetScriptAst.h
│   ├── WidgetFactory.h
│   ├── WidgetFactory.cpp
│   ├── ActionResolver.h
│   └── ActionResolver.cpp
│
├── UI/
│   ├── Base/
│   │   ├── UIElement.h
│   │   ├── UIElement.cpp
│   │   ├── FocusManager.h
│   │   ├── FocusManager.cpp
│   │   ├── WindowManager.h
│   │   ├── WindowManager.cpp
│   │   ├── Viewport.h
│   │   └── Viewport.cpp
│   │
│   ├── Panels/
│   │   ├── Panel.h
│   │   ├── Panel.cpp
│   │   ├── Window.h
│   │   ├── Window.cpp
│   │   ├── PopupWindow.h
│   │   ├── PopupWindow.cpp
│   │   ├── StatusBar.h
│   │   ├── StatusBar.cpp
│   │   ├── ScrollablePanel.h
│   │   └── ScrollablePanel.cpp
│   │
│   ├── Menus/
│   │   ├── MenuItem.h
│   │   ├── MenuItem.cpp
│   │   ├── MenuModel.h
│   │   ├── MenuModel.cpp
│   │   ├── MenuView.h
│   │   ├── MenuView.cpp
│   │   ├── MenuController.h
│   │   └── MenuController.cpp
│   │
│   └── Widgets/
│       ├── Widget.h
│       ├── Widget.cpp
│       ├── ContainerWidget.h
│       ├── ContainerWidget.cpp
│       ├── Label.h
│       ├── Label.cpp
│       ├── Button.h
│       ├── Button.cpp
│       ├── ListBox.h
│       ├── ListBox.cpp
│       ├── TextView.h
│       └── TextView.cpp
│
├── Screens/
│   ├── Screen.h
│   ├── Screen.cpp
│   ├── TitleScreen.h
│   ├── TitleScreen.cpp
│   ├── MainMenuScreen.h
│   └── MainMenuScreen.cpp
│
├── Animation/
│   ├── TickClock.h
│   ├── TickClock.cpp
│   ├── Animation.h
│   ├── Animation.cpp
│   ├── Animator.h
│   └── Animator.cpp
│
├── Game/
├── Data/
├── Utilities/
│   ├── Unicode/
│   │   ├── UnicodeConversions.h
│   │   ├── UnicodeConversions.cpp
│   │   ├── UnicodeWidth.h
│   │   ├── UnicodeWidth.cpp
│   │   ├── UnicodeGrapheme.h
│   │   ├── UnicodeGrampheme.cpp
│   ├── AssetPaths.h
│   ├── AssetPaths.cpp
│   ├── StringUtils.h
│   ├── StringUtils.cpp
│   ├── Logger.h
│   └── Logger.cpp
│
├── Assets/
│   ├── ASCII/
│   ├── ANSI/
│   ├── Fonts/
│   │   ├── FIGlet/
│   │   ├── TOIlet/
│   │   └── Bitmap/
│   ├── Pages/
│   ├── Screens/
│   └── Themes/
│
├── Tests/
│   ├── RenderingTests/
│   ├── PageComposerTests/
│   ├── InterpreterTests/
│   └── WidgetTests/
│
└── Tools/
    ├── ThemePreviewTool/
    ├── PageLintTool/
    └── AssetPreviewTool/



============================================================
18. WHAT SHOULD BE BUILT NEXT, SPECIFICALLY
============================================================

Do not jump to widgets or old page ports.

Build next in this exact order:

1. Finish Phase 1 cleanly if anything remains unstable
2. Finish Phase 2 style system cleanly
3. Build Phase 3 TextObject / asset system around one unified object model
4. Add plain text loaders first (.txt / .asc / .nfo / .diz)
5. Add ANSI / terminal art loaders next (.ans / .bin first, .xbin / .adf after core import path is stable)
6. Add TextObject export/save support where fidelity is clear
7. Add banner/font loading and generation support
8. Build Phase 4 WritePolicy/compositing system
9. Build Phase 5 PageComposer API 1.0
10. Test authored pages in C++
11. Then build Phase 6 interpreter

That is the correct path to avoid temporary solutions.



============================================================
19. API 1.0 MINIMUM LOCK-IN CHECKLIST
============================================================

Before starting interpreter work, API 1.0 should support all of this:

- clear(style)
- createScreen(filename)
- createRegion(...)
- hasRegion / getRegion / clearRegions
- writeText
- writeTextBlock
- writeSolidObject
- writeVisibleObject
- writeGlyphsOnly
- writeStyleMask
- writeStyleBlock
- writeObject with explicit WritePolicy
- coordinate placement
- region + alignment placement
- render() to string
- semantic named styles like BannerThemes
- style preservation / merge semantics

If any of these are missing, interpreter work should wait.



============================================================
20. FINAL PROJECT DIRECTION
============================================================

The engine is now intentionally aimed at supporting:

1. clean C++ page composition
2. data-driven page composition
3. later widgets and interactive layouts
4. later alternate backends
5. long-term maintainability without large refactors

The key idea is:
build the page-composition vocabulary first,
then let both C++ and interpreted pages use it.

============================================================
OUTPUT OF UPDATED ROADMAP 03/21/26
============================================================

A fully Unicode-correct TUI engine that:

- preserves clean architecture
- avoids future refactors
- supports real-world text (emoji, CJK, combining marks)
- remains backend-neutral
- maintains authoring simplicity