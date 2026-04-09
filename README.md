![GitHub release](https://img.shields.io/github/v/release/YOURNAME/YOURREPO)
# TUI
A modern **text-mode UI engine** designed for building terminal-style applications and games.

Originally created to power a cogmind style graphical text game. It was a phase I went through, and then the project died and this TUI was shelved for a few years. 

But I gained renewed interst in continuing its development while creating a digital version of the tabletop football simulation **Pro Football Fantasm**. I just thought this would be the perfect retro 80's and early 90's feel for that style of game.

The engine is evolving into a flexible **terminal UI framework** for building console-style interfaces and games using C++.

## Screenshots

*Reminder from Dabil: (Add screenshots or animated GIFs here once you have something exciting to share)*
```
    ┌─────────────────────────────────────────────┐
    │ TUI **text-mode UI Engine                   │
    │                                             │
    │ Press Any Key To Continue...                │
    └─────────────────────────────────────────────┘
```
# Features

## Current capabilities:
	- Layout regions and composable screen design
	- ASCII object rendering
	- Flexible style system
	- Theme-based styling
	- Word-wrapped text rendering
	- Text alignment helpers
	- Page composition system

## API examples:
```C++
	page.writeTextInRegion(
		"Footer",
		HorizontalAlign::Center,
		VerticalAlign::Bottom,
		"Press Any Key...",
		Themes::Danger);
```
# Architecture
## The engine is structured into several core systems:
```
	App/
		Application loop
		Screen manager

	Composition/
		PageComposer
		RegionRegistry
		Layout helpers

	Rendering/
		ScreenBuffer
		ConsoleRenderer
		Object rendering
		Style system

	Screens/
		Game screens and UI pages

	Assets/
		ASCII art
		fonts
```
## Example Page Code:    
```
	page.createDesignScreen(200, 62, "DesignScreen");

	page.writeVisibleObject(
		m_helmet1,
		HorizontalAlign::Left,
		VerticalAlign::Center,
		"DesignScreen",
		Themes::Helmets);
```
# Building

	Requirements:
		Windows
		Visual Studio 2022
		C++17

	Build steps:
		git clone https://github.com/YOURNAME/YOURPROJECT.git
		open TUI.sln
		build
		run

# Project Goals
## The engine is evolving toward:
```
	1) a reusable text UI toolkit
	2) support for multiple render backends
	3) layout and widget systems
	4) scriptable page definitions
	5) retro terminal visual effect
```	
# Roadmap

	See: Roadmap.md

# Contributing

	Not accepting pull requests at the moment for general participation while getting this 
	to a stable version but will in the near future. There is a high likelyhood of a 
	pull request being accepted if it fixes a critical bug.
	
	Feedback and feature requests are welcome. 
	
	The intitial implementation for this proejct will be for PFF, so there is certain 
	functionality I want to focus on without distraction.

	See: CONTRIBUTING.md


# License

	GPL 3.0
	See: LICENSE

# Author
	Created by Dabil

