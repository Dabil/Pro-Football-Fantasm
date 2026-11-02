# Pro Football Fantasm (PFF)

Pro Football Fantasm (PFF) is a modern C++17 recreation and evolution of a classic tabletop-style football simulation game, built on a custom Text User Interface (TUI) engine.

The project combines strategic play-calling, probabilistic outcomes, and real-time game state management with a console-driven UI designed for performance, clarity, and extensibility.

PFF serves both as a playable football simulation and as a showcase for the modular TUI framework supporting layered rendering, custom XP (RexPaint) assets, and future expansion into advanced gameplay systems such as playbooks, team tendencies, and full league management.

---

##  Features

*  Strategic play-calling (offense vs defense)
*  Probabilistic play resolution system
*  Full game loop (downs, distance, clock, scoring)
*  Real-time scoreboard and game state tracking
*  Play-by-play drive log
*  Modular architecture (domain logic separated from UI)
*  Custom Text User Interface (TUI) engine
*  Support for RexPaint `.xp` assets
*  Built with C++17 and object-oriented design

---

##  Tech Stack

* **Language:** C++17
* **Architecture:** Object-Oriented Design (OOP)
* **UI:** Custom Text User Interface (TUI) Engine
* **Rendering:** Terminal-based (console host or modern terminal)
* **Assets:** RexPaint `.xp` format
* **Build System:** Visual Studio (`.sln` / `.vcxproj`)

---

##  Project Structure

```text
Pro Football Fantasm/
│
├── PFF/
│   ├── App/            # Application entry + bootstrap
│   ├── Core/           # Engine + shared systems
│   ├── Rendering/      # TUI rendering system
│   ├── Screens/        # UI screens (game, menus, etc.)
│   ├── ThirdParty/     # External dependencies - zlib for .xp RexPaint files
│   ├── Utilities/      # Helper utilities
│   ├── main.cpp        # Entry point
│   └── PFF.vcxproj     # Main project file
│
├── Hike.sln            # Visual Studio solution
├── README.md
└── ROADMAP.md
```

> Note: The project currently integrates the TUI engine directly. Future versions may separate engine and game into distinct modules.

---

##  Getting Started

### Prerequisites

* Windows 10/11
* Visual Studio 2022 (or newer)
* C++ Desktop Development workload installed

### Setup

1. Clone the repository:

```bash
git clone https://github.com/yourusername/pro-football-fantasm.git
```

2. Open the solution:

```text
PFF.sln
```

3. Build the solution:

* `Build → Build Solution` (Ctrl + Shift + B)

4. Run the project:

* `Debug → Start Without Debugging` (Ctrl + F5)

---

##  Current Status

### Commissioner's Office manifest build update (pending user acceptance)

The Create Core DB UI & Manifest Integration proposed v0.1 from the
"Design manifest files" chat governs this change. The user's November 1, 2026
decision in the implementation chat supersedes older workspace specifications
for this update only. Database/C++ v0.8, PFF Free Tier v0.7 and 1989 Data &
Implementation v0.2 workspace exports remain reference material; their Project
Sources currency was not independently verified.

Build Core DB now opens a named-output form with installed ruleset/season
dropdowns, package count, reload and a single Build Core Database action.

The user's follow-up naming request permits spaces within the Core database
basename (still up to 64 characters). Trailing spaces, empty/all-space names,
reserved filenames and path characters remain invalid. The rendered-screen
regression builds a filename containing a space and checks that typing Space
does not trigger the application-level Confirm action.

Paths resolve relative to each v0.1 manifest. Invalid manifests produce diagnostics.
Only explicitly listed supported inputs enter the existing dependency-ordered
importers. The user's latest UI request replaces the two-stage validate/build
workflow: one click validates and publishes once through the shared builder on
a worker, while the UI shows a building message. The content heading remains
Review Core Database Build. Source bytes are checked before publication.
Existing output files are protected: Continue returns to the office, where a
new basename can be chosen. Replacement is deliberately not exposed
by this initial UI. Temporary construction reserves a sibling `.building`
directory atomically and never deletes a pre-existing temporary artifact.

The installed First Edition/1989 manifests list 25 files, all now imported.
The user's follow-up request in this implementation chat authorizes the 15
chart importers and fixes to text editing and control contrast. PFFChartImport.inl
adds family-specific structure, selectors and coverage validation for special
teams, breakaway, endurance, fumble, injury, interception point/return, penalties,
scramble, solitaire, weather/turf effects, and three Wild Play categories.
ChartDefinitions/ChartRecords/ChartValues provide queryable family/parent/field
data with source line spans; ChartScripts preserves multiline scripts separately.
The additive ChartImportSchemaVersion=1 extension applies to newly built Cores;
existing Core/World files are not migrated. Script execution remains explicitly
UNCOMPILED or DRAFT, and original ruling/status metadata is preserved. Successful
data import does not certify WPSL execution or full simulation readiness.
Unknown source files/fields, bad nesting, duplicate selectors and inconsistent
self-declared counts fail validation rather than disappearing. No source package was trimmed.
The user's subsequent modding request removes the edition/year allowlist,
FirstEdition-only player-profile restriction, fixed 1989 package totals and
hardcoded chart row totals/coverage gates. Any package using the supported
source grammar may attempt a build. Source/manifest season or ruleset-label
differences produce warnings; copied payloads are not rewritten to match folder
or display names. Self-declared counts, parsing, references, uniqueness and
SQLite integrity remain validated. This is build permission, not certification
of the football data or its simulation behavior.
Add-on discovery reports installed `.pffaddon` files as unsupported;
v0.1 supplies no concrete compatibility grammar or add-on importers, so none
are offered as compatible checkboxes. The section remains visible with its
empty state. The Commissioner's Office New League button now uses the selected
Core through the focused setup described below.

Manifest builds record exact package keys/versions, source paths relative to
the source root, and explicitly labeled FNV-1a-64 source-byte checksums in
CoreMetadata. These noncryptographic checksums do not claim canonical semantic
content identity or full simulation readiness. No SQL schema migration,
dependency, vendor API, or original TUI/GCE repository change is involved.
The inspected build uses C++17/v143, vendored SQLite header version 3.51.2,
and zlib header version 1.3.2 (despite its directory being named zlib-1.3.1).
These existing dependencies were preserved; vendor upstream commit baselines
are unknown from the inspected repository evidence.

Regression checks: from a VS2022 x64 Native Tools Command Prompt, run
`PFF\Database\RunCoreManifestTests.cmd`. It builds Debug x64 and tests parsing,
discovery, basename safety, all 25 imports, chart counts/source/script status,
validate-only/build, provenance, existing-file protection, malformed chart
rollback, and the rendered screen's normalized Backspace/Delete, dropdown,
review and successful full build flow. Writable fixtures stay in ignored
`bin/ManifestTests/`; installed sources and user databases are untouched.
After a build attempt, the result screen has one Continue action, with keyboard
focus on it. Continue restores the existing Commissioner screen and clears the
build form, review results and package selections. It preserves the application's
loaded league context. There is no intermediate Build/Back confirmation screen,
and no second import pass for UI validation. Build actions disappear immediately
when work starts; successful and failed results both show only Continue.
Regression checks exercise one-click publication, responsive building/results
rendering with no intermediate actions, failure diagnostics and Continue/reset.
Regression fixtures cover copied 1990/1991 manifests with 1989 payloads, an
arbitrary ruleset label and reduced play/chart counts, plus Continue/reset.
These regression fixtures passed. For this follow-up the affected sources
compiled as Debug x64, but the normal executable was locked by the running app
(LNK1168). A separately named PFF-ModdingFixes.exe linked from the current app
objects and existing vendor libraries, and the standalone regression runner
passed. Close the running app before the next normal VS rebuild/F5 session.
Interactive Visual Studio F5 verification remains a separate manual check.
Validation on November 1, 2026: Debug x64 solution build and the complete
regression runner passed, including the rendered screen checks. Full rebuilds
also reported warnings in unchanged TUI startup/encoding and developer-screen
code. Release/x86 configurations and interactive F5 were not tested. No
authoritative specification was edited or published.

Vendor change record TUI-VT-EDIT-001 (pending user acceptance): local
vendor/TUI/Input/InputManager.cpp normalizes DEL (U+007F) and native Backspace/
Delete keys; WindowsConsoleKeyboardSource.cpp consumes numeric CSI editing
sequences including their '~' terminator, using private VtEditingKey.h. A bounded
25 ms wait permits adjacent console input batches; key-up records no longer
split the sequence. This fixes the user's reported missing Backspace and literal
'~' on Delete without changing widget or framework public APIs or dependencies.
The imported upstream commit is unknown. Classification: potential general-purpose
upstream candidate; standalone input-source/terminal verification is recommended
before any separately authorized transfer. PFF uses high-contrast existing theme
styles for input/list text; other screens retain their styles. The regression
runner exercises normalization through a fake raw input source and the actual
TextInput/screen, plus the shared VT Delete decoder. Physical terminal/VS F5
testing remains manual. No original framework repository was accessed.

PFF is currently in **early non-playable development**.

### New League setup (November 2, 2026; pending user acceptance)

The user's six-point New League request in this implementation chat governs
this update. The Commissioner screen now opens a league-name field (spaces
allowed), a dropdown of readable Core databases in Data, recorded Core details,
a save-path preview, existing save folders, Create League and Cancel. Package
identities/versions and ruleset/season are read from the selected Core, never
inferred from its filename. Missing metadata and legacy warning history remain
explicitly unknown. Newly built Cores record actual build warnings in
CoreMetadata; existing Cores are never modified to invent a warning history.

The league name is used verbatim as its Data/World/<name>/World.db folder/name.
Invalid names and any existing destination folder block creation. An atomic
folder reservation and a second conflict check protect existing saves. Creating
uses the existing GCE World builder with the selected Core's recorded season;
its verified SHA-256 snapshot is preserved with the save. Creation runs on a
worker; completion loads the saved league through the existing application
load callback. Failure preserves existing saves; an unsuccessful attempt may
leave its own non-loadable folder, shown as an existing folder on the next setup.
No recursive cleanup of save material is performed.

PFF/Database/LeagueSetup.h owns PFF discovery/name policy; GCE's schema, snapshot
ownership and public API remain unchanged. No original TUI/GCE repository was
accessed. No dependencies or toolchain versions changed. The existing README
baseline/reference status above still applies; no authoritative specification
was edited or published. Tests use copies in bin/ManifestTests and cover spaces,
Core selection, truthful incomplete metadata/warnings, duplicate-name blocking,
creation/loading, Cancel, and survival after the global Core is replaced.
The normal executable was locked by the running app, so validation uses a
separately linked Debug x64 PFF-LeagueSetup.exe. Physical terminal/F5 verification
remains manual; close the running app before a normal rebuild/restart.
The final regression run passed, including recorded warning display, legacy
unknowns, duplicate-folder protection, actual creation/load and byte-for-byte
snapshot isolation after replacing the global Core. The user's existing Core
databases and saves were not modified by development or tests.

### Working

* Core TUI rendering engine
* XP asset loading (RexPaint)
* Screen system and layout
* Basic application loop

### In Progress

* Game state model (score, down, distance, clock)
* Play selection system
* Play resolution engine
* Game screen integration

### Planned

* Pro Football Fantasm 1990 Core Game
* Support for all 3 Modes of play (Generic, Basic, and Advanced)
* Full rosters, with offensive and defensive alignment boards.
* CPU vs CPU Simulation, Solitaire Play, or PvP (up to 28 players!)
* Weather Effects, Injuries, Penalties, Wild Plays, etc. 
* League and stat tracking
* Save/load system
* Support for original Team Tendency Charts Add-on

---

##  Architecture Overview

PFF follows a layered architecture:

### Domain Layer (Game Logic)

* `GameState`
* `PlayCall`
* `PlayResult`
* Rules engine and simulation logic

### Presentation Layer

* View models derived from game state
* UI-friendly formatting of data

### Screen / Controller Layer

* Handles input
* Updates game state
* Triggers rendering

### TUI Engine

* Rendering
* Layout
* Text objects
* XP asset support

---

##  Design Goals

*  Fast, responsive console UI
*  Clean separation of concerns
*  Extensible gameplay systems
*  Faithful tabletop-style mechanics
*  Developer-friendly architecture

---

##  Screenshots

*(Coming soon)*

---

##  Roadmap

See [ROADMAP.md](ROADMAP.md) for full development plan.

---

##  Contributing

Contributions are welcome!

Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

---

##  License

This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

---

##  Inspiration

Pro Football Fantasm is inspired by the classic 1990 tabletop sports simulation of the same name, combining strategy, probability, and imagination into an interactive digital format.

---

##  Future Vision

The long-term goal is to evolve PFF into:

* A full-featured football simulation engine
* A powerful terminal-based UI framework
* A customizable platform for sports simulation enthusiasts

---

*Built with passion for football, simulation, and great software design.*
