# Claude Your Synth — Getting Started

A minimal C++20 starter project for the workshop by million-tokens.com. It builds a JUCE console
application and prints the JUCE version. Use it as the base for the synth you write during the
workshop.

---

## Architecture

The project is deliberately small. One CMake target builds one executable.

```
CMakeLists.txt   # CMake project, fetches JUCE 8.0.4, defines the JuceMinimal target
build.sh         # configures, builds and runs the executable
src/Main.cpp     # entry point
tests/           # test sources (empty for now)
docs/            # code guidelines for Claude Code
CLAUDE.md        # top-level brief for Claude Code
```

- **Build system:** CMake 3.22 or later.
- **Dependency:** CMake `FetchContent` downloads JUCE 8.0.4 at configure time. You do not install
  JUCE yourself. The first configure step takes some minutes.
- **Target:** `JuceMinimal`, a console application. It links `juce::juce_core` plus the JUCE
  recommended configuration and warning flags. The build disables the JUCE web browser and cURL
  support.
- **Standard:** C++20. CMake also writes `compile_commands.json` for clang-tidy and the editor.

---

## Build and run

```bash
./build.sh
```

The script configures a Release build in `build/`, compiles it with all available cores, then runs
the executable. If the build gives no binary, the script prints a message and exits with an error.

---

## Start a Claude Code session

Make sure that the ccflare proxy runs, then point Claude Code to it:

```bash
export ANTHROPIC_BASE_URL=http://localhost:8080/v1/anthropic
```

Ready to go! Run:

```bash
claude
```

---

## `CLAUDE.md` — instructions for the AI assistant

`CLAUDE.md` is the top-level brief that Claude Code reads at the start of every session. It follows
the **progressive disclosure principle**: the file itself stays short and lists only pointers to
this README and to the specialized instruction files in `docs/`. Claude loads those files on
demand, when the task needs them.

```
CLAUDE.md
├── README.md                      # project, architecture and build steps
└── docs/
    ├── common-guidelines-code.md  # applies to all code
    ├── cpp-guidelines.md          # loaded when working on C++ files
    └── bash-guidelines.md         # loaded when working on shell scripts
```

Each file in `docs/` holds the rules themselves. Read the file to get the rules. This README does
not repeat them.

> **Fun fact:** `common-guidelines-code.md` is a deliberate blend of two ideas. The terse,
> grammar-sacrificing style comes from Matt Pocock's principle — *"Be extremely concise. Sacrifice
> grammar for the sake of concision"*
> ([source](https://x.com/mattpocockuk/status/1974528553569137095)) — because LLMs process dense
> bullet points more efficiently than prose. The engineering philosophy (no assumptions, surface
> tradeoffs, naive-first then optimize) draws from Andrej Karpathy's observations on how LLMs reason
> and where they go wrong ([source](https://x.com/karpathy/status/2015883857489522876)). The result
> is instructions that a human can scan easily and an AI can follow well.

---

## Style toolchain

Four configuration files keep code and behaviour the same for every developer and every tool.

### `.editorconfig` — the baseline for every file

Sets the lowest-level text rules, so that your editor saves files the same way:

- **Spaces, not tabs** — indentation is always 4 spaces
- **Unix line endings** (LF) — no Windows-style CRLF gets in
- **No trailing whitespace** — lines stay clean at the end
- **Final newline** — every file ends with a newline

Any editor with EditorConfig support (VS Code, CLion, Vim, Emacs and others) reads this file
automatically.

### `.clang-format` — automatic C++ formatting

Defines the layout of C++ code — braces, spacing, line length and more:

- Starts from the **LLVM style**
- **4-space indentation**, the same as `.editorconfig`
- **120-character line limit** — readable on modern screens without too much wrapping
- **No short functions on a single line** — each function body gets its own lines
- **Namespace closing comments** for readability

To reformat a file, run `clang-format -i <file>`. The Claude Code hook below does this for you.

### `.clang-tidy` — C++ code quality checks

Catches real problems and bad patterns:

- **`bugprone-*`** — common mistakes that cause bugs
- **`cppcoreguidelines-*`** — the C++ Core Guidelines
- **`modernize-*`** — modern C++ idioms
- **`performance-*`** — patterns that are unnecessarily slow
- **`readability-*`** — code that stays clear

The configuration turns off two noisy checks (magic numbers, trailing return types) to keep the
warnings actionable. Header checks apply only to project headers under `src/`.

### `.claude/settings.json` — Claude Code applies the rules for you

- **Auto-format hook** — each time Claude edits or writes a `.cpp`, `.hpp`, `.h` or `.c` file,
  `clang-format` runs on it immediately. The file is formatted before you see it.
- **Allowed commands** — Claude runs `cmake`, `make`, `find`, `ls` and a few others without a
  permission prompt, which keeps the workflow fast.

The AI assistant therefore follows the same style rules as the rest of the toolchain.
