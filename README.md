# Claude Your Synth — Getting Started

A minimal C++20 starter project for the workshop by million-tokens.com. It builds a JUCE console
application and prints the JUCE version. Use it as the base for the synth you write during the
workshop.

---

## What's in the repo

```
CMakeLists.txt  # CMake project, fetches JUCE and Catch2, defines the MiniSynth target
build.sh        # configures, builds and runs the executable
test.sh         # builds and runs the tests
workshop.sh     # moves the checkout between the checkpoints, sets up the Claude profile
src/Main.cpp    # entry point
tests/          # Catch2 tests
```

CMake downloads JUCE and Catch2 at configure time. The first build takes some minutes.

---

## Build, run and test

```bash
./build.sh   # configure a Release build in build/, compile MiniSynth, run it
./test.sh    # build the tests in the same build/ directory and run them
```

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
warning and asks for confirmation first. The script needs SSH access to GitHub.

---

## Start Claude Code with the workshop profile

The workshop uses a separate Claude Code configuration. Your own settings, skills, hooks and
session history stay in `~/.claude`. The workshop does not read them and does not change them.

```bash
./workshop.sh profile setup    # make the workshop configuration (run once)
./workshop.sh claude           # start a session on that configuration
```

The first session starts logged out. Run `claude auth login` once inside the session. To go back to
your own setup, start `claude` as usual. `./workshop.sh -h` lists all the commands, including
`profile status` and `profile remove`.

The profile switches off the claude.ai connectors and the Artifact tool, because these add much
text to every prompt. It also installs a status line under the input box:

```
Claude-Your-Synth-Getting-Started (my-synth) in 12.4k out 1.8k ctx 14.2k $0.42
```

`ctx` is the size of the context now. It turns red above 200k. The cost is for the whole session.
The status line needs `python3`.
