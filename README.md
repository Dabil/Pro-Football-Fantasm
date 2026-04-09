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

PFF is currently in **early non-playable development**.

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
