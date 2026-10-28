# Claude Your Synth — Getting Started

A minimal C++20 starter project for the workshop by million-tokens.com. It builds a JUCE console
application and prints the JUCE version. Use it as the base for the synth you write during the
workshop.

---

## Architecture

```
CMakeLists.txt              # CMake project, fetches JUCE 8.0.4, defines the MiniSynth target
build.sh                    # configures, builds and runs the executable
test.sh                     # builds and runs the tests
workshop.sh                 # moves the checkout between the checkpoints, sets up the Claude profile
claude-config/statusline.py # the status line that profile setup installs
src/Main.cpp                # entry point
tests/                      # Catch2 v3 tests, the MiniSynthTests target
docs/                       # code guidelines for Claude Code
AGENTS.md                   # top-level brief for coding agents
```

- **Build system:** CMake 3.22 or later.
- **Dependencies:** CMake `FetchContent` downloads JUCE 8.0.4 and Catch2 v3.7.1 at configure time.
  You do not install them yourself. The first configure step takes some minutes.
- **Targets:** `MiniSynth`, a console application, and `MiniSynthTests`, the test binary. Both
  link `juce::juce_core`.
- **Standard:** C++20. CMake also writes `compile_commands.json` for clang-tidy and the editor.

---

## Build, run and test

```bash
./build.sh   # configure a Release build in build/, compile MiniSynth, run it
./test.sh    # build MiniSynthTests in the same build/ directory, run the tests with ctest
```

`build.sh` does not compile the tests. To get the Catch2 options, run
`./build/tests/MiniSynthTests --help`. Add new test files to `tests/`, then list them in the
`add_executable` command in `tests/CMakeLists.txt`.

---

## Workshop navigation

`workshop.sh` moves your checkout between the checkpoints of the workshop. There is one checkpoint
after each prompt, named by a number and a word: `10-plugin`, `20-sound`, `30-waveforms`, and so on.

```bash
./workshop.sh list            # show the checkpoints
./workshop.sh reset 30        # move your branch to the checkpoint 30-waveforms
./workshop.sh reset waveforms # the same: the full name, the number or the word all work
./workshop.sh reset main      # go back to the starting point
./workshop.sh reset           # choose the checkpoint from a menu
./workshop.sh pull            # get the latest fixes from upstream main
```

Work on your own branch, for example `git switch -c my-work`. The script refuses to change `main`,
because `main` keeps your reference copy of the starter.

`reset` discards local commits and uncommitted changes on the current branch. The script prints a
warning and asks for confirmation first.

The script needs SSH access to GitHub. A clone does not fetch the checkpoints; `list` and `reset`
fetch them into `refs/checkpoints/`. After that, `git switch --detach refs/checkpoints/30-waveforms`
also works.

### For the workshop author: how to make a checkpoint

A checkpoint is a commit under `refs/checkpoints/`, not a branch and not a tag, so it never shows
up in a fresh clone. Commit on a detached HEAD, then push the commit to its checkpoint name:

```bash
git switch --detach refs/checkpoints/20-sound
git commit -am "Waveforms: sine, saw, square and noise"
git push origin HEAD:refs/checkpoints/30-waveforms
```

- Name it with a number, a dash and one word. The menu sorts by name, so keep the number
  zero-padded to two digits.
- Numbers go up in tens, which leaves room to add one later: `25-noise` sits between `20-sound`
  and `30-waveforms`.
- The menu shows the commit subject as the checkpoint title.
- To move a checkpoint, push the new commit with `--force`. The next `reset` of a student gets the
  new position.
- To remove one, run `git push origin --delete refs/checkpoints/<name>`.
- To list what is upstream: `git ls-remote origin 'refs/checkpoints/*'`.

---

## Start Claude Code with the workshop profile

The workshop uses a separate Claude Code configuration. Your own settings, skills, hooks and
session history stay in `~/.claude`. The workshop does not read them and does not change them.

```bash
./workshop.sh profile setup    # make the workshop configuration (run once)
./workshop.sh claude           # start a session on that configuration
./workshop.sh profile status   # show the configuration
./workshop.sh profile remove   # delete the configuration
```

`setup` makes `~/claude-synth-workshop/claude-config` with a `settings.json`. The settings switch
off the claude.ai connectors and the Artifact tool, because these add much text to every prompt.
The settings also install a status line under the input box:

```
Claude-Your-Synth-Getting-Started (my-synth) in 12.4k out 1.8k ctx 14.2k $0.42
```

`ctx` is the size of the context now. It changes to red when it is more than 200k. The cost is for
the whole session. The status line needs `python3`. You can run `setup` again at any time. It does
not change a configuration that already exists.

`./workshop.sh claude` is `claude` with `CLAUDE_CONFIG_DIR` set to the workshop directory. Any
argument after `claude` goes to Claude Code. The first session starts logged out. Run
`claude auth login` once inside the session.

To go back to your own setup, start `claude` as usual. The script deletes nothing from `~/.claude`.
`./workshop.sh -h` lists the commands and the environment variables.

---

## `AGENTS.md` — instructions for the AI assistant

Coding agents read `AGENTS.md` at the start of every session. The file stays short. It points to
this README and to the guideline files in `docs/`. Claude reads those files only when the task
needs them:

```
AGENTS.md
├── README.md                      # project, architecture and build steps
└── docs/
    ├── common-guidelines-code.md  # applies to all code
    ├── cpp-guidelines.md          # for C++ files
    └── bash-guidelines.md         # for shell scripts
```

---

## Style toolchain

- `.editorconfig` — 4 spaces, LF line endings, no trailing whitespace, a final newline.
- `.clang-format` — LLVM style, 4-space indentation, 120-character lines. Run
  `clang-format -i <file>`.
- `.clang-tidy` — the `bugprone`, `cppcoreguidelines`, `modernize`, `performance` and
  `readability` checks. The header checks apply only to the headers under `src/`.
- `.claude/settings.json` — a hook runs `clang-format` on each C++ file that Claude edits. Claude
  runs `cmake`, `make` and some other commands without a permission prompt.
