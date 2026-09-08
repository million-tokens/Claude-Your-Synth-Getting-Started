# Claude Your Synth — Getting Started

A minimal C++20 starter project for the workshop by million-tokens.com. It builds a JUCE console
application and prints the JUCE version. Use it as the base for the synth you write during the
workshop.

---

## Architecture

The project is deliberately small. One CMake target builds the application, a second one builds the
tests.

```
CMakeLists.txt             # CMake project, fetches JUCE 8.0.4, defines the MiniSynth target
build.sh                   # configures, builds and runs the executable
test.sh                    # builds and runs the tests
workshop.sh                # moves the checkout between the lesson tags, sets up the Claude profile
capture.sh                 # records what a session sends to the model, renders it as HTML
capture/                   # the proxy and the renderer behind capture.sh
src/Main.cpp               # entry point
tests/CMakeLists.txt       # fetches Catch2 v3, defines the MiniSynthTests target
tests/ProductNameTest.cpp  # smoke test: the product name macro is "MiniSynth"
docs/                      # code guidelines for Claude Code
CLAUDE.md                  # top-level brief for Claude Code
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

To start the workshop:

```bash
./workshop.sh claude
```

This is `agent-deck -p synth-workshop`, and any argument after `claude` passes straight through. It
also records the session: it starts the capture proxy, sets `ANTHROPIC_BASE_URL` to it for that
session only, and stops it again when you quit. See "See what the model actually received" below.
Agent Deck stays the launcher, because the profile carries the isolated configuration and the hooks
that `profile setup` installed.

The first Claude session in the new profile starts logged out, because the sign-in record lives in
the configuration directory. Run `claude auth login` once inside the session.

To go back to your own setup, start Agent Deck without the option. Nothing else is necessary,
because the script deletes nothing from `~/.claude`.

These environment variables change the target:

- `WORKSHOP_PROFILE` - the name of the Agent Deck profile. Default: `synth-workshop`.
- `WORKSHOP_HOME` - the directory of the workshop configuration. Default: `~/claude-synth-workshop`.
- `WORKSHOP_CAPTURE` - set to `0` to start the session without recording it. Default: `1`.
- `WORKSHOP_CAPTURE_DIR` - where the recordings go. Default: `./captures`.
- `WORKSHOP_CAPTURE_PORT` - the first port the recorder tries. Default: `8788`.

---

## Start a Claude Code session

```bash
./workshop.sh claude
```

That is `agent-deck -p synth-workshop`, with the session recorded. Any argument after `claude` goes
on to Agent Deck. To start a session without a recording, run `agent-deck -p synth-workshop`
yourself, or set `WORKSHOP_CAPTURE=0`. The next section says what the recording is for.

---

## See what the model actually received

The model is a stateless function. It keeps nothing between calls, so every request carries the
whole context again: a system prompt, the schema of every tool, and the conversation so far. That
payload is what your instructions, your `CLAUDE.md` and your context hygiene actually produce, and
it is the thing worth looking at when a session behaves in a way you did not expect.

Claude Code's own transcript in `~/.claude/projects/` does not hold it. The transcript records the
conversation — your messages, the answers, the tool calls — but not the system prompt, not the tool
schemas and not the cache breakpoints. Those exist only on the wire. To see them, you have to record
the requests.

`capture.sh` does that with a proxy. Claude Code sends its requests to whatever `ANTHROPIC_BASE_URL`
names, so a local proxy in front of the API sees every byte.

**`./workshop.sh claude` does this for you.** It starts the recorder, points the session at it, stops
it when you quit, and tells you where the recording is:

```bash
./workshop.sh claude      # work as usual. The session is recorded.
./capture.sh render       # afterwards: the page
```

Recording is best effort. If the recorder cannot start — no `python3`, no free port, nowhere to
write — the script says so and starts the session anyway. A broken recorder never costs you a
session. `WORKSHOP_CAPTURE=0` turns it off, and `./workshop.sh profile status` shows the current
state.

To record a session that you start yourself, run the two halves by hand:

```bash
./capture.sh start                                # terminal 1: the recorder
export ANTHROPIC_BASE_URL=http://127.0.0.1:8788   # terminal 2
agent-deck -p synth-workshop                      # terminal 2: work as usual
./capture.sh render                               # after the session: the page
```

`start` runs until Ctrl-C and writes one JSON file per API call into `captures/<timestamp>/`.
`render` turns a recording into a single `captures/<timestamp>/index.html` and opens it. Inside a
session, the `/capture` command renders the current recording.

```
capture.sh          # start | render | list
capture/proxy.py    # the recorder. Forwards to the API, keeps a copy
capture/render.py   # the reader. Recording -> one HTML page
captures/           # the recordings. Stays out of git
```

The two halves are separate on purpose. The proxy sits in the critical path of your session, so it
parses nothing and decides nothing: it moves bytes and drops a copy on disk. Everything that could
be wrong or slow lives in `render.py`, which runs afterwards over the files.

### What the page shows

Every request repeats the previous one, extended. A page that printed each request in full would be
the session written out N times, so the renderer prints only what **changed** since the previous
call of the same conversation:

- **System prompt** — in full on the first call, then only when it changes. A change mid-session
  means the harness injected something: a skill, a reminder, a compaction.
- **Tool schemas** — the names always, the full JSON on demand, and a marker when a tool appears or
  disappears. This is usually the largest fixed block in the prompt.
- **Cache breakpoints** — where the request asks the API to cut a cache segment. When a breakpoint
  moves, everything after it must be written to the cache again, and the call gets expensive.
- **New messages** — the ones appended since the last call. The replayed history stays folded away.
- **The answer** — reassembled from the stream, with its `stop_reason` and token counts.

Requests are grouped into **chains** by their first message. A subagent, a compaction and the main
conversation each open their own first message, so each gets its own chain instead of polluting the
others' diffs.

### The token table

The page opens with one row per API call. The numbers come from the API's own `usage`, so they are
what you are billed for:

```
  req  chain  model         prompt_in   fresh   cache_read   cache_write   output   new_turn    ms
    1      1  claude-opus-5     33933       2            0         33931       18          -  4210
    2      1  claude-opus-5     34057       2        33931           124       94        106  2980
    3      1  claude-opus-5     44114       2        34055         10057      425       9963  7104
```

- `prompt_in` is the whole prompt of that call. It only grows.
- `fresh` is the part the cache did not hold. It stays near zero, because almost every token is
  replayed history.
- `cache_read` of call N is close to `prompt_in` of call N-1: each call is the previous one,
  extended.
- `new_turn` is `prompt_in[N] - prompt_in[N-1] - output[N-1]` — the tokens your message and the tool
  results added. A large value marks a file read or a wide tool result that entered the conversation
  and now rides along in every later call.
- A drop in `prompt_in` means a compaction.

A low cache hit rate makes a session expensive. The summary line at the top gives the rate, the
largest prompt of the run and the totals.

### Options and limits

```bash
./capture.sh start -p 9000        # another port
./capture.sh list                 # the recordings
./capture.sh render <dir> -O      # a named recording, do not open a browser
```

- `CAPTURE_PORT`, `CAPTURE_DIR` and `CAPTURE_UPSTREAM` set the same things from the environment.
  Through `workshop.sh` the names are `WORKSHOP_CAPTURE`, `WORKSHOP_CAPTURE_PORT` and
  `WORKSHOP_CAPTURE_DIR`. A busy port is not an error: the recorder walks forward until it finds a
  free one.
- The proxy asks the API for uncompressed responses, so that the recording is readable text. That
  costs some bandwidth on the way in and nothing else.
- Credentials are never written: the recording stores `authorization` and `x-api-key` as
  `<redacted>`. Everything else in the recording — your prompts, your files, the answers — is as
  sensitive as the session itself. `captures/` is in `.gitignore`.
- The proxy only sees what crosses the network. Hooks, local tool execution and anything Claude Code
  resolves without an API call leave no trace in the recording.
- Only sessions that ran through the proxy are recorded. There is nothing to look at afterwards for
  a session that did not.

To collect numbers for a fleet of sessions rather than the content of one, use the OpenTelemetry
export instead: `CLAUDE_CODE_ENABLE_TELEMETRY=1` with the `OTEL_*` variables. See
<https://code.claude.com/docs/en/monitoring-usage>.

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
