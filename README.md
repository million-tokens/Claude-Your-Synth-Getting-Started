## `CLAUDE.md` — Instructions for the AI Assistant

`CLAUDE.md` is the top-level brief that Claude Code reads at the start of every session. It follows the **progressive disclosure principle**: the file itself is intentionally short, listing only pointers to specialized instruction files in `docs/`. Claude loads those files on demand, when the task at hand actually requires them.

```
CLAUDE.md
└── docs/
    ├── common-guidelines-code.md   # applies to all code
    ├── cpp-guidelines.md           # loaded when working on C++ files
    └── bash-guidelines.md          # loaded when working on shell scripts
```

**What each file covers:**

- **`common-guidelines-code.md`** — universal rules for any language: ask before assuming, surface tradeoffs, plan before coding, keep things simple, don't touch unrelated code, write a naive correct solution first then optimize.

  > **Fun fact:** this file is a deliberate blend of two ideas. The terse, grammar-sacrificing style comes from Matt Pocock's principle — *"Be extremely concise. Sacrifice grammar for the sake of concision"* ([source](https://x.com/mattpocockuk/status/1974528553569137095)) — because LLMs process dense bullet points more efficiently than prose. The engineering philosophy (no assumptions, surface tradeoffs, naive-first then optimize) draws from Andrej Karpathy's observations on how LLMs reason and where they go wrong ([source](https://x.com/karpathy/status/2015883857489522876)). The result is instructions that are both easy for a human to scan and optimally shaped for an AI to follow.
- **`cpp-guidelines.md`** — C++20-specific: smart pointers only (no raw `new`/`delete`), use Concepts instead of SFINAE, prefer `std::span` and `std::ranges`, LLVM formatting, zero clang-tidy warnings.
- **`bash-guidelines.md`** — shell script rules: `#!/usr/bin/env bash` shebang, `set -euo pipefail` safety, double-quote all variables, ShellCheck must pass with zero warnings.

This structure keeps the main file readable while ensuring Claude only loads context that is relevant to the current task — avoiding information overload on both sides.

---


## Style

Four files work together to keep code looking and behaving the same way for every developer and every tool.

---

### `.editorconfig` — The Baseline for Every File

Sets the lowest-level text rules so that your editor saves files the same way regardless of which editor you use:

- **Spaces, not tabs** — indentation is always 4 spaces
- **Unix line endings** (LF) — no Windows-style CRLF sneaking in
- **No trailing whitespace** — lines are always clean at the end
- **Final newline** — every file ends with a newline

Any editor that supports EditorConfig (VS Code, CLion, Vim, Emacs, etc.) picks this up automatically.

---

### `.clang-format` — Automatic C++ Formatting

Defines exactly how C++ code should be laid out — braces, spacing, line length, and more:

- Based on the **LLVM style** as the starting point
- **4-space indentation** (matches `.editorconfig`)
- **120-character line limit** — readable on modern screens without excessive wrapping
- **No short functions on a single line** — each function body gets its own lines
- **Namespace closing comments** auto-inserted for readability

Run `clang-format -i <file>` to reformat a file, or let the hook below do it automatically.

---

### `.clang-tidy` — C++ Code Quality Checks

Goes beyond formatting to catch real problems and bad patterns in C++ code:

- **`bugprone-*`** — catches common coding mistakes likely to cause bugs
- **`cppcoreguidelines-*`** — enforces the C++ Core Guidelines (safe, modern C++)
- **`modernize-*`** — nudges code toward modern C++ idioms (C++11 and later)
- **`performance-*`** — flags patterns that are unnecessarily slow
- **`readability-*`** — keeps code clear and easy to follow

A few noisy checks are turned off (magic numbers, trailing return types) to keep warnings actionable. Checks apply only to project headers under `include/`.

---

### `.claude/settings.json` — Claude Code Enforces the Rules Automatically

Configures Claude Code (the AI assistant) so formatting is applied without you having to think about it:

- **Auto-format hook** — every time Claude edits or writes a `.cpp`, `.hpp`, `.h`, or `.c` file, `clang-format` runs on it immediately. The file is always formatted before you even see it.
- **Allowed commands** — Claude can run `cmake`, `make`, `find`, `ls`, and a few others without asking for permission each time, keeping the workflow fast.

This means the AI assistant follows the same style rules as the rest of the toolchain.

---

## `build.sh` — One-Command Build and Run

```bash
./build.sh
```

# Starting

Make sure ccflare proxy is running 
```bash
export ANTHROPIC_BASE_URL=http://localhost:8080/v1/anthropic
```

Ready to go! Run:

```bash
claude
```
