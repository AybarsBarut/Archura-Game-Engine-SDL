# Contributing to Archura Engine

First off, thank you for considering contributing to the Archura Game Engine! It's people like you that make Archura a great tool for building games.

The following is a set of guidelines for contributing to Archura Game Engine and its packages, which are hosted in the [AybarsBarut/Archura-Game-Engine-SDL](https://github.com/AybarsBarut/Archura-Game-Engine-SDL) repository on GitHub. These are mostly guidelines, not rules. Use your best judgment, and feel free to propose changes to this document in a pull request.

## Table of Contents

1.  [Code of Conduct](#code-of-conduct)
2.  [How Can I Contribute?](#how-can-i-contribute)
    *   [Reporting Bugs](#reporting-bugs)
    *   [Suggesting Enhancements](#suggesting-enhancements)
    *   [Your First Code Contribution](#your-first-code-contribution)
    *   [Pull Requests](#pull-requests)
3.  [Styleguides](#styleguides)
    *   [Git Commit Messages](#git-commit-messages)
    *   [C++ Styleguide](#c-styleguide)
    *   [C# Styleguide](#c-styleguide)
4.  [Community](#community)

---

## Code of Conduct

This project and everyone participating in it is governed by the [Archura Engine Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code. Please report unacceptable behavior to the project maintainers.

## How Can I Contribute?

### Reporting Bugs

This section guides you through submitting a bug report for Archura Engine. Following these guidelines helps maintainers and the community understand your report, reproduce the behavior, and find related reports.

*   **Check the Discussions & Issues:** Before creating bug reports, please check the existing GitHub Issues and GitHub Discussions as you might find out that you don't need to create one. When you are creating a bug report, please include as many details as possible.
*   **Use a clear and descriptive title** for the issue to identify the problem.
*   **Describe the exact steps which reproduce the problem** in as many details as possible. Provide a test minimal game code if possible.
*   **Provide specific examples to demonstrate the steps**, such as snippets of your code (`Main.cpp`, your C# scripts) or screenshots.
*   **Include Crash Logs:** If the engine crashes, please include the generated `crash_log.txt` or standard error outputs.

### Suggesting Enhancements

This section guides you through submitting an enhancement suggestion for Archura Engine, including completely new features and minor improvements to existing functionality.

*   **Use GitHub Discussions:** First, pitch your idea in the [GitHub Discussions](https://github.com/AybarsBarut/Archura-Game-Engine-SDL/discussions) under the "Ideas" or "Feature Requests" category to see if it aligns with the engine's roadmap.
*   **Explain why this enhancement would be useful** to most Archura Engine users.
*   **Provide a mock-up or API design** if proposing a new Engine Core component or ECS Script API.

### Your First Code Contribution

Unsure where to begin contributing to Archura? You can start by looking through these issue labels:

*   **`good first issue`**: issues which should only require a few lines of code, and a test or two.
*   **`help wanted`**: issues which should be a bit more involved than `good first issue` issues.

### Pull Requests

The process described here has several goals:
*   Maintain Archura's quality
*   Fix problems that are important to users
*   Engage the community in working toward the best possible engine.

Please follow these steps to have your contribution considered by the maintainers:

1.  Follow all instructions in [the template](.github/PULL_REQUEST_TEMPLATE.md) (if provided).
2.  Follow the [styleguides](#styleguides).
3.  Ensure your code builds successfully with CMake on the targeted platform (Windows).
4.  After you submit your pull request, verify that all status checks are passing.

## Styleguides

### Git Commit Messages

*   **Use the present tense** ("Add feature" not "Added feature").
*   **Use the imperative mood** ("Move cursor to..." not "Moves cursor to...").
*   **Limit the first line to 72 characters or less.**
*   Reference issues and pull requests liberally after the first line.
*   Group related changes logically. Do not mix refactoring, feature additions, and unrelated bug fixes in a single commit.

### C++ Styleguide

*   **Naming Conventions:**
    *   Classes/Structs: `PascalCase` (e.g., `RenderSystem`, `Texture`)
    *   Functions/Methods: `PascalCase` (e.g., `Update()`, `Initialize()`)
    *   Member Variables: Prefix `m_` followed by `PascalCase` (e.g., `m_WindowWidth`, `m_Renderer`)
    *   Local Variables: `camelCase` (e.g., `dt`, `tempBuffer`)
    *   Namespaces: The core engine is wrapped in the `Archura` namespace.
*   **Headers:** Use `#pragma once` as the include guard.
*   **Memory Management:** Prefer smart pointers (`std::unique_ptr`, `std::shared_ptr`) over raw pointers where ownership is implied. Use raw pointers only for observing (non-owning) references.

### C# Styleguide

*   Follow the standard Microsoft C# coding conventions.
*   Classes, Methods, Properties: `PascalCase` (e.g., `PlayerController`, `OnUpdate`).
*   Private fields: prefix `_` followed by `camelCase` (e.g., `_moveSpeed`).
*   Scripts should inherit from the appropriate base classes provided in `ScriptCore` (e.g., `Entity`).

## Community

Join the conversation!
*   **GitHub Discussions:** For general questions, Q&A, and showcasing your projects.
*   **Issues:** Strictly for verifiable bugs and planned features.

Thank you for contributing to Archura Engine!
