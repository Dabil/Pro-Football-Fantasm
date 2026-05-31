# PSEUDO FONT (pfont / pfont2) DEVELOPER MANUAL

---

# 1. OVERVIEW

A **pFont** is a **generator-driven text asset format** that maps characters to authored multi-cell glyphs.

Unlike traditional fonts:

* ❌ No runtime rendering logic
* ❌ No glyph rasterization
* ❌ No renderer involvement

Instead:

```
pFont file → pFontLoader → FontDefinition → Generator → (Layered)TextObject → ScreenBuffer → Renderer
```

pFonts are **content**, not rendering instructions.

---

# 2. COMPLETE FORMAT DIAGRAM

Below is the **full pfont2 structure and all supported options**.

```
pfont2

# --------------------------
# FONT METADATA
# --------------------------

name=<string>

glyph_width=<int>
glyph_height=<int>

letter_spacing=<int>        # spacing between glyphs
word_spacing=<int>          # spacing for ' '
line_spacing=<int>          # multiline spacing

fallback=<char>             # fallback glyph if missing
transparent=<char>          # character treated as transparent

# --------------------------
# FONT DEFAULT STYLE
# --------------------------

default_style.foreground=<color>
default_style.background=<color>
default_style.bold=<true|false>
default_style.dim=<true|false>
default_style.underline=<true|false>
default_style.slow_blink=<true|false>
default_style.fast_blink=<true|false>
default_style.reverse=<true|false>
default_style.invisible=<true|false>
default_style.strike=<true|false>

# --------------------------
# GLYPH DEFINITION
# --------------------------

[glyph <char>]

# Optional glyph-level style
glyph_style.foreground=<color>
glyph_style.background=<color>
glyph_style.bold=<true|false>
...

    # ----------------------
    # LAYER BLOCK
    # ----------------------
    [layer <name>]

    z=<int>                  # draw order
    visible=<true|false>     # initial visibility

    # Optional layer style
    style.foreground=<color>
    style.background=<color>
    style.bold=<true|false>
    ...

    row=<string>
    row=<string>
    row=<string>
    ...

    [endlayer]

[endglyph]
```

---

# 3. CORE CONCEPTS

## 3.1 Glyph

Each character maps to a **fixed-size grid**:

```
glyph_width x glyph_height
```

Example:

```
row=.┌─┐.
row=┌...┐
row=├───┤
row=│...│
row=.└─┘.
```

---

## 3.2 Transparent Cells

Defined by:

```
transparent=.
```

* `.` = not written (skipped)
* Allows background from underlying content to show through

---

## 3.3 Layers

Each glyph can have **multiple layers**:

* Each layer is a full grid
* Layers are composited using:

  * `z` order
  * `visible` flag

Layers enable:

* animation
* assembly effects
* glow overlays
* outlines
* flicker / neon effects

---

## 3.4 Style Hierarchy

Styles are applied in this order:

```
font default
    ↓
glyph style
    ↓
layer style
    ↓
cell style (future/optional)
```

Later levels override earlier ones.

---

# 4. SUPPORTED STYLE PROPERTIES

All styles support:

```
foreground=<color>
background=<color>

bold=true|false
dim=true|false
underline=true|false

slow_blink=true|false
fast_blink=true|false

reverse=true|false
invisible=true|false
strike=true|false
```

---

## 4.1 Color Values

Typical values:

```
black
red
green
yellow
blue
magenta
cyan
white

bright_black
bright_red
bright_green
bright_yellow
bright_blue
bright_magenta
bright_cyan
bright_white
```

---

# 5. GENERATION MODEL

pFonts do NOT render directly.

They are used like:

```
FontDefinition → generate → LayeredTextObject → flatten → TextObject → draw
```

---

## 5.1 Flat Generation

```
TextObject obj = generateTextObject(font, "WATER");
```

---

## 5.2 Layered Generation

```
LayeredTextObject obj = generateLayeredTextObject(font, "WATER");
```

Then:

```
obj.setLayerVisibility(...)
TextObject frame = obj.flattenVisibleOnly();
```

---

# 6. BEST PRACTICE PATTERNS

---

## 6.1 CLEAN BASE FONT

Use font defaults for shared styling:

```text
default_style.background=blue
default_style.bold=true
```

Avoid repeating style in every layer.

---

## 6.2 LAYERED ANIMATION (ASSEMBLY)

Example pattern:

```
[layer stage_1] → sparse
[layer stage_2] → partial
[layer stage_3] → almost complete
[layer final]   → finished
```

Then animate by toggling visibility.

---

## 6.3 NEON EFFECT

```
[layer glow]
style.foreground=bright_magenta
style.bold=true

[layer core]
style.foreground=white
```

Glow layer sits behind core layer.

---

## 6.4 OUTLINE FONT

```
[layer outline]
style.foreground=black

[layer fill]
style.foreground=white
```

---

## 6.5 BACKGROUND BLOCK FONT

To ensure full background coverage:

* Use:

  ```
  transparent=.
  ```
* And generate with:

  ```
  transparentSpaces = false
  ```

This fills empty cells with styled background.

---

## 6.6 PERFORMANCE TIP

* Keep glyph size small (e.g. 5x5 or 7x7)
* Avoid excessive layers unless needed
* Prefer reuse of shared structure

---

# 7. COMMON MISTAKES

---

## ❌ Using foreground instead of background

Wrong:

```
default_style.foreground=red
```

Correct:

```
default_style.background=red
```

---

## ❌ Forgetting transparent character

If missing:

```
transparent=.
```

Your font will not behave correctly.

---

## ❌ Overriding styles unintentionally

Layer styles override font defaults.

If you set:

```
style.foreground=white
```

But not background, background must come from font default.

---

## ❌ Expecting renderer behavior

pFonts do NOT:

* scale
* kerning
* wrap automatically

Everything is explicit.

---

# 8. MINIMAL WORKING EXAMPLE

```text
pfont2

name=Simple Box
glyph_width=3
glyph_height=3
transparent=.

default_style.foreground=white
default_style.background=blue

[glyph A]

[layer base]
z=0
visible=true
row=┌─┐
row=├─┤
row=│.│
[endlayer]

[endglyph]
```

---

# 9. ADVANCED USE CASES

pFonts enable things FIGlet cannot:

* layered animation (assembly, dissolve, wave)
* per-layer styling
* mixed glyph composition
* partial glyph reveal
* authored visual effects (not algorithmic)

---

# 10. DESIGN PHILOSOPHY

pFonts are:

* **authored visual assets**
* **generator-driven**
* **renderer-agnostic**
* **fully deterministic**

They are NOT:

* runtime fonts
* layout engines
* rendering systems

---

# 11. FINAL CHECKLIST

Before using a pFont:

* ✔ glyph size consistent
* ✔ transparent char defined
* ✔ fallback glyph defined
* ✔ layers ordered correctly
* ✔ styles applied at correct level
* ✔ test with:

  * flat generation
  * layered generation

---

# 12. SUMMARY

pFonts give you:

* total visual control
* layered composition
* animation-ready text assets
* reusable styled text generation

They are a **core building block** of advanced TUI visual systems.

---

END OF MANUAL
