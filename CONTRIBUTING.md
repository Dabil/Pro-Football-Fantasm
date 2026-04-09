
# Contributing
```
Thanks for your interest in contributing!

This project is just starting and before I accept pull requests I want to get it stable with a milestone or two under my belt.

Because this project is being built for Pro Football Fantasm there is ceratin functionality I need to get down first without distraction. After that this repository will be open. 

This project is currently focused on building a clean, modular **text UI engine**.
```
# Development Philosophy

The project emphasizes:
```markdown
* clean architecture
* readable code
* modular systems
* minimal dependencies

```

# Getting Started
```
1. Fork the repository
2. Create a branch
3. Make your changes
4. Submit a pull request

Example:
git checkout -b feature/add-scrollbar-widget

```

# Coding Guidelines

### Style
```
* C++17
* clear naming
* small functions
* readable logic over clever tricks
```
### Naming
```
Classes: PascalCase
Functions: camelCase
Files: PascalCase
Namespaces: lowercase

Example:
PageComposer
writeTextInRegion()
resolveRegion()
```
# Commit Message Guidance:

Keep commits short and clear: 
```
	 Single functionality added 
	 Single bug fixes
	 Project should build cleanly
	
```
# Commit Messages:

## Title:
```
	<= 72 Characters
	describes what changed
```	
## Adds:
```
	Functions added
	Files added
	Dependencies added
```
## Removes
```
	Functions removed
	Files removed
	Dependencies removed
```
## Body: (Optional but nice)
```
	 explains why
	 lists functionality
	
```

## Tags:
Begin all commit messages with a tag identifying what file is affected.
Example:
```
PageComposer: Add writeTextBlockInRegion helpers to PageComposer

Adds:
- writeTextBlockInRegion()
- writeTextBlockInRegionPreserveStyle()

These helpers allow multi-line text blocks to be aligned and rendered
inside named layout regions using the PageComposer API.
```

# Feature Ideas

Good areas for contribution:

* widget system
* scroll panels
* text input widgets
* layout improvements
* additional rendering backends

# Questions

Open a GitHub issue if you want to discuss ideas or make feature requests.