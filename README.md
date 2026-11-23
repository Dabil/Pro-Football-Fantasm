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


## TUI-AUDIO-001 - PFF menu music and GameTime cues

The owner approved SDL3_mixer and requested a menu playlist with football
effects in GameTime. Audio/SdlAudio is a reusable TUI playback service
with a separate music track and sound-effect track. It is independent of
renderer selection. PFFApplication plays the MP3s in filename order,
wraps at the end, pauses music during GameTime, and resumes the same
track on return. GameTimeView selects one cue per newly displayed play
or penalty record. PFF ships three original synthesized PCM WAV files
for the referee whistle, kickoff and scoring crowd. The original MP3s
are used from the existing asset folder. Music and effects use starter
gain values of 0.24 and 0.60.

The current backend is optional on Windows x64. Missing runtime or audio
device leaves the UI usable and emits an audio diagnostic. Win32 retains
silent startup. The VS2022 x64 build copies the audio assets, mixer DLL and
notices into the output. The official SDL3_mixer 3.2.4 Visual C archive SHA256
is f4263ed5082fb7018059d64952017534e26821e9e878ce6b8c924b77cb17c4fb.
The included notices cover SDL_mixer and its built-in dr_libs, stb_vorbis
and TiMidity components. The reusable playback service is an upstream
candidate. The menu playlist and football cues are PFF-specific.
[SDL3_mixer release and source](https://github.com/libsdl-org/SDL_mixer/releases/tag/release-3.2.4)

Reviewed the local PFF Classic Free Tier v0.7 UI section, PFF UI Screen
Inventory and Navigation v0.1.1 and TUI SDL Renderer v0.3 draft. The owner
direction specifies the audio behavior. Project Sources currency remains
unverified. No specification files changed.

Windows 10 Debug and Release x64 builds passed. A disposable PFF replay
fixture verified mix output using SDL's dummy audio driver on the SDL,
Console and Terminal renderers. All eight MP3s and three WAVs loaded.
It verified playlist wrap, pausing the same music track for GameTime
and resuming it on return, and a cue without repeat for the same play.
Release package startup passed both normally and with a missing mixer
DLL or unavailable audio device. Physical speaker playback, F5 and user
acceptance remain open.
Win32 Debug also built and launched with the Console renderer.
