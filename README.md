# Claude Your Synth — Getting Started

A minimal C++20 starter project for the workshop by million-tokens.com. It builds a JUCE console
application and prints the JUCE version. Use it as the base for the synth you write during the
workshop.

---

## Architecture

The project is deliberately small. One CMake target builds the application, a second one builds the
tests.

```
CMakeLists.txt         # CMake project, fetches JUCE 8.0.4, defines the MiniSynth target
build.sh               # configures, builds and runs the executable
test.sh                # builds and runs the tests
workshop.sh            # moves the checkout between the lesson tags, sets up the Claude profile
src/Main.cpp           # entry point
tests/CMakeLists.txt   # fetches Catch2 v3, defines the MiniSynthTests target
tests/ExampleTest.cpp  # example tests
docs/                  # code guidelines for Claude Code
CLAUDE.md              # top-level brief for Claude Code
```

- **Build system:** CMake 3.22 or later.
- **Dependencies:** CMake `FetchContent` downloads JUCE 8.0.4 and Catch2 v3.7.1 at configure time.
  You do not install them yourself. The first configure step takes some minutes.
- **Targets:** `MiniSynth`, a console application, and `MiniSynthTests`, the test binary. Both
  link `juce::juce_core` plus the JUCE recommended configuration and warning flags. The build
  disables the JUCE web browser and cURL support.
- **Standard:** C++20. CMake also writes `compile_commands.json` for clang-tidy and the editor.

---

## Build and run

```bash
./build.sh
```

The script configures a Release build in `build/`, compiles the `MiniSynth` target with all
available cores, then runs the executable. If the build gives no binary, the script prints a message
and exits with an error. The script does not compile the tests.

---

## Test

```bash
./test.sh
```

The script uses the same `build/` directory, so it reuses the JUCE download. It builds only the
`MiniSynthTests` target, then runs the tests with `ctest`. CMake registers each Catch2 test case
as a separate `ctest` test. If a test fails, `ctest` prints its output.

To run the test binary directly, and to get the Catch2 command-line options, run:

```bash
./build/tests/MiniSynthTests --help
```

Add new test files to `tests/`, then list them in the `add_executable` command in
`tests/CMakeLists.txt`.

---

## Workshop navigation

`workshop.sh` moves your checkout between the lessons of the workshop. Each lesson is an annotated
git tag in the upstream repository, for example `lesson-03-filter`. The tag message holds the lesson
title.

```bash
./workshop.sh list                     # show the lesson tags
./workshop.sh reset lesson-03-filter   # move your branch to the state of lesson 03
./workshop.sh reset main               # go back to the starting point
./workshop.sh reset                    # choose the lesson from a menu
./workshop.sh pull                     # get the latest fixes from upstream main
```

Work on your own branch. The script refuses to change `main`, because `main` keeps your reference
copy of the starter. To make a work branch, run:

```bash
git switch -c my-work
```

`reset` discards local commits and uncommitted changes on the current branch. The script prints a
warning and asks for confirmation first.

The script needs SSH access to GitHub. It adds a temporary `upstream` remote, fetches, then removes
the remote again. The lesson tags stay in your repository, so `git switch --detach lesson-03-filter`
also works after the first run.

Two environment variables change the source:

- `WORKSHOP_UPSTREAM` - the upstream repository URL or a local path. Default: the workshop
  repository over SSH.
- `WORKSHOP_TAG_PREFIX` - the prefix that marks a lesson tag. Default: `lesson-`.

### For the workshop author: how to make a lesson

The tags are the only thing that the workshop needs. A lesson branch is not necessary, because a tag
keeps its commit alive on its own. Work with a detached HEAD, then push the tag alone:

```bash
git switch --detach main
# ... write the code for lesson 01 ...
git commit -am "Add a sine oscillator"
git tag -a lesson-01-oscillator -m "Add a sine oscillator"
git push origin lesson-01-oscillator

# ... write the code for lesson 02 on top ...
git commit -am "Add an ADSR envelope"
git tag -a lesson-02-envelope -m "Add an ADSR envelope"
git push origin lesson-02-envelope

git switch main
```

The repository keeps the tags and nothing else. No lesson branch appears in `git branch -a`, and
`git log main` shows only the starter history.

Two rules keep the menu correct:

- Start every tag name with the prefix, then a zero-padded number. The menu sorts by tag name, so
  `lesson-01` must come before `lesson-10`.
- Use annotated tags (`git tag -a`). The script shows the tag message as the lesson title.

To move a tag after a fix, force the update on both sides:

```bash
git tag -f -a lesson-01-oscillator -m "Add a sine oscillator"
git push --force origin lesson-01-oscillator
```

The student gets the new position at the next `./workshop.sh reset`, because the script fetches the
tags with a force refspec.

To remove a lesson, delete the tag on both sides:

```bash
git tag -d lesson-01-oscillator
git push origin --delete lesson-01-oscillator
```

---

## Isolated Claude configuration

The workshop runs on a separate Claude Code configuration. Your own settings, skills, hooks and
session history stay in `~/.claude`. The workshop does not read them and does not change them.

[Agent Deck](https://github.com/asheshgoplani/agent-deck) supplies the mechanism. A named profile
points Claude Code at a different configuration directory. `workshop.sh` makes the profile for you:

```bash
./workshop.sh profile setup    # make the workshop profile and its configuration
./workshop.sh profile status   # show which configuration the profile uses
./workshop.sh profile remove   # delete the profile and its configuration
```

`setup` does four things. It makes the directory `~/claude-synth-workshop/claude-config`. It adds
the profile to Agent Deck. It adds this block to `~/.config/agent-deck/config.toml`, after it copies
that file to `config.toml.workshop-bak`:

```toml
[profiles.synth-workshop.claude]
config_dir = "/Users/you/claude-synth-workshop/claude-config"
```

Then it installs the Agent Deck hooks into the new configuration and prints the result. Run `setup`
again at any time. It changes nothing that is already correct.

To start the workshop, give Agent Deck the profile:

```bash
agent-deck -p synth-workshop
```

The first Claude session in the new profile starts logged out, because the sign-in record lives in
the configuration directory. Run `claude auth login` once inside the session.

To go back to your own setup, start Agent Deck without the option. Nothing else is necessary,
because the script deletes nothing from `~/.claude`.

Two environment variables change the target:

- `WORKSHOP_PROFILE` - the name of the Agent Deck profile. Default: `synth-workshop`.
- `WORKSHOP_HOME` - the directory of the workshop configuration. Default: `~/claude-synth-workshop`.

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
