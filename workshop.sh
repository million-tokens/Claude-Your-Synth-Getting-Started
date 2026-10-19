#!/usr/bin/env bash
set -euo pipefail

# Workshop helper. It moves your checkout between the checkpoints of the upstream
# workshop repository, and it sets up an isolated Claude Code configuration for the
# workshop. See the "Workshop navigation" section of README.md.

UPSTREAM_URL="${WORKSHOP_UPSTREAM:-git@github.com:MindWrapper/Claude-Your-Synth-Getting-Started.git}"
# Checkpoints are commits under this ref namespace. A clone fetches only branches
# and tags, so they stay out of sight until list or reset fetches them.
CHECKPOINT_REFS="refs/checkpoints"
BASE_BRANCH="main"
REMOTE="upstream"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

WORKSHOP_HOME="${WORKSHOP_HOME:-${HOME}/claude-synth-workshop}"
PROFILE_CONFIG_DIR="${WORKSHOP_HOME}/claude-config"
PROFILE_SETTINGS="${PROFILE_CONFIG_DIR}/settings.json"
STATUSLINE_SOURCE="${SCRIPT_DIR}/claude-config/statusline.py"
PROFILE_STATUSLINE="${PROFILE_CONFIG_DIR}/statusline.py"

WORKSHOP_CAPTURE="${WORKSHOP_CAPTURE:-1}"
CAPTURE_PROXY="${SCRIPT_DIR}/capture/proxy.py"
CAPTURE_DIR="${WORKSHOP_CAPTURE_DIR:-${SCRIPT_DIR}/captures}"
CAPTURE_PORT="${WORKSHOP_CAPTURE_PORT:-8788}"
CAPTURE_PID=""
CAPTURE_LOG=""
CAPTURE_RUN=""

WORKSHOP_VIEWER="${WORKSHOP_VIEWER:-1}"
CAPTURE_VIEWER="${SCRIPT_DIR}/capture/serve.py"
VIEWER_PID=""
VIEWER_LOG=""
VIEWER_URL=""

SESSION_CONFIG_DIR=""

REMOTE_EXISTED="unknown"

die() {
    printf 'Error: %s\n' "$*" >&2
    exit 1
}

warn() {
    printf 'Warning: %s\n' "$*" >&2
}

usage() {
    cat <<'USAGE'
Usage: ./workshop.sh <command> [argument]

Checkpoint commands:
  list                 Show the checkpoints of the workshop.
  reset [checkpoint]   Move the current branch to a checkpoint: its full name (30-waveforms),
                       its number (30) or its name (waveforms). Use "main" for the start.
  pull                 Merge the latest upstream main into the current branch.

Claude configuration commands:
  profile setup        Make the isolated Claude configuration for the workshop.
  profile status       Show the workshop configuration and the recorder state.
  profile remove       Delete the workshop configuration.
  claude [args...]     Start a workshop session on that configuration, and record
                       what it sends to the model. The recording opens in the
                       browser at the first API call and follows the session.
  claude-own [args...] The same recording, on your own configuration (~/.claude).
                       Compare its recording with a workshop session to see what
                       your own settings, skills and MCP servers add to the prompt.

Environment:
  WORKSHOP_UPSTREAM    Upstream repository URL. Default: the workshop repository over SSH.
  WORKSHOP_HOME        Directory of the workshop configuration. Default: ~/claude-synth-workshop
  WORKSHOP_CAPTURE     Set to 0 to start the session without recording. Default: 1
  WORKSHOP_CAPTURE_DIR Directory of the recordings. Default: ./captures
  WORKSHOP_CAPTURE_PORT
                       First port the recorder tries. Default: 8788
  WORKSHOP_VIEWER      Set to 0 to record without the live page in the browser. Default: 1

If you give no checkpoint, the script shows a menu.
USAGE
}

require_git_repo() {
    git rev-parse --git-dir >/dev/null 2>&1 || die "This directory is not a git repository."
}

current_branch() {
    git branch --show-current
}

refuse_protected_branch() {
    local action="$1"
    if [[ "$(current_branch)" == "${BASE_BRANCH}" ]]; then
        die "You cannot ${action} the ${BASE_BRANCH} branch. Run: git switch -c my-work"
    fi
}

require_clean_tree() {
    if [[ -n "$(git status --porcelain)" ]]; then
        git status --short
        die "Commit or stash your changes first."
    fi
}

confirm() {
    local answer
    read -r -p "$1 [y/N] " answer
    [[ "${answer}" == "y" || "${answer}" == "Y" ]]
}

# Add the upstream remote if it is absent, and remember that we added it.
connect_upstream() {
    if git remote get-url "${REMOTE}" >/dev/null 2>&1; then
        REMOTE_EXISTED="yes"
        git remote set-url "${REMOTE}" "${UPSTREAM_URL}"
    else
        REMOTE_EXISTED="no"
        git remote add "${REMOTE}" "${UPSTREAM_URL}"
    fi
}

# Remove the upstream remote again if the script added it. The trap calls this.
disconnect_upstream() {
    if [[ "${REMOTE_EXISTED}" == "no" ]]; then
        git remote remove "${REMOTE}" >/dev/null 2>&1 || true
    fi
}

# Fetch upstream main and the checkpoints. The plus sign makes a moved upstream
# checkpoint overwrite the stale local copy, and --prune drops the ones upstream
# removed.
fetch_upstream() {
    printf 'Fetching %s ...\n' "${UPSTREAM_URL}"
    git fetch --quiet "${REMOTE}" "${BASE_BRANCH}" ||
        die "Cannot fetch ${BASE_BRANCH} from ${UPSTREAM_URL}. Check your SSH access."
    git fetch --quiet --prune "${REMOTE}" \
        "+${CHECKPOINT_REFS}/*:${CHECKPOINT_REFS}/*" ||
        die "Cannot fetch the checkpoints from ${UPSTREAM_URL}."
}

# Print one line per checkpoint: <name><TAB><subject>, in name order.
checkpoints() {
    git for-each-ref --sort=refname \
        --format='%(refname:lstrip=2)%09%(contents:subject)' \
        "${CHECKPOINT_REFS}/"
}

# Show a menu and print the chosen ref. The menu goes to stderr, so that the
# caller can capture the ref from stdout.
select_checkpoint() {
    local refs=() labels=() name subject choice count i

    refs+=("${REMOTE}/${BASE_BRANCH}")
    labels+=("main - the starting point")
    while IFS=$'\t' read -r name subject; do
        refs+=("${CHECKPOINT_REFS}/${name}")
        labels+=("${name} - ${subject}")
    done < <(checkpoints)

    count="${#refs[@]}"

    printf '\nWhich checkpoint do you want to reset to?\n\n' >&2
    for ((i = 0; i < count; i++)); do
        printf '  %2d) %s\n' "$((i + 1))" "${labels[i]}" >&2
    done
    printf '\n' >&2
    read -r -p "Number [1-${count}]: " choice
    if [[ ! "${choice}" =~ ^[0-9]+$ ]] || ((choice < 1 || choice > count)); then
        die "Invalid selection: ${choice}"
    fi

    printf '%s\n' "${refs[choice - 1]}"
}

# Print the ref of the checkpoint the user named: the full name (30-waveforms),
# the number (30) or the name (waveforms).
resolve_checkpoint() {
    local wanted="$1" name subject
    while IFS=$'\t' read -r name subject; do
        if [[ "${name}" == "${wanted}" || "${name}" == "${wanted}-"* || "${name}" == *"-${wanted}" ]]; then
            printf '%s\n' "${CHECKPOINT_REFS}/${name}"
            return 0
        fi
    done < <(checkpoints)

    printf 'Error: no checkpoint "%s". Available checkpoints:\n' "${wanted}" >&2
    checkpoints >&2
    exit 1
}

cmd_list() {
    connect_upstream
    fetch_upstream
    printf '\nCheckpoints:\n\n'
    local name subject
    while IFS=$'\t' read -r name subject; do
        printf '  %-16s %s\n' "${name}" "${subject}"
    done < <(checkpoints)
    printf '\n'
}

cmd_reset() {
    local wanted="${1:-}" target
    refuse_protected_branch "reset"
    connect_upstream
    fetch_upstream

    if [[ -z "${wanted}" ]]; then
        target="$(select_checkpoint)"
    elif [[ "${wanted}" == "${BASE_BRANCH}" ]]; then
        target="${REMOTE}/${BASE_BRANCH}"
    else
        target="$(resolve_checkpoint "${wanted}")"
    fi

    if [[ -n "$(git status --porcelain)" ]]; then
        printf '\nWarning: a hard reset discards these uncommitted changes:\n\n'
        git status --short
        printf '\n'
        confirm "Continue?" || die "Cancelled."
    fi

    printf 'Resetting %s to %s ...\n' "$(current_branch)" "${target}"
    git reset --hard "${target}"
}

cmd_pull() {
    refuse_protected_branch "pull into"
    require_clean_tree
    connect_upstream
    fetch_upstream
    printf 'Merging %s/%s into %s ...\n' "${REMOTE}" "${BASE_BRANCH}" "$(current_branch)"
    if ! git merge "${REMOTE}/${BASE_BRANCH}"; then
        die "The merge stopped. Fix the conflicts, then run: git commit"
    fi
}

# --- The isolated Claude configuration ---------------------------------------
#
# CLAUDE_CONFIG_DIR is a Claude Code variable: it names the directory that holds
# the settings, the skills, the hooks, the sign-in and the session history.
# "workshop.sh claude" starts Claude Code with the variable set to the workshop
# directory, for that session only. Your own ~/.claude stays untouched, and it
# comes back when you start "claude" without the variable.

require_claude() {
    command -v claude >/dev/null 2>&1 ||
        die "Claude Code is not installed. See https://code.claude.com/docs/en/setup"
}

profile_exists() {
    [[ -d "${PROFILE_CONFIG_DIR}" ]]
}

# --- Recording what the model receives ---------------------------------------
#
# ANTHROPIC_BASE_URL is a Claude Code variable: it names the endpoint that Claude
# Code sends its requests to. "workshop.sh claude" starts the capture proxy on a
# local port, points that variable at it, and stops the proxy again when the
# session ends. The proxy forwards every request to the API untouched and keeps a
# copy, so that you can see what the model actually received. See "See what the
# model actually received" in README.md.
#
# A second process, the viewer, serves the recording as a page that follows the
# session, and opens it in the browser at the first call. When the session ends,
# the viewer writes the final pages into the recording.
#
# Recording is best effort. If it cannot start, the session runs without it: a
# broken recorder must never cost you a session. The same goes for the viewer.

port_is_free() {
    ! (exec 3<>"/dev/tcp/127.0.0.1/$1") 2>/dev/null
}

find_free_port() {
    local port="$1" last=$(($1 + 20))
    while ((port <= last)); do
        if port_is_free "${port}"; then
            printf '%s\n' "${port}"
            return 0
        fi
        port=$((port + 1))
    done
    return 1
}

capture_available() {
    [[ "${WORKSHOP_CAPTURE}" != "0" ]] &&
        [[ -f "${CAPTURE_PROXY}" ]] &&
        command -v python3 >/dev/null 2>&1
}

start_capture() {
    local port waited=0

    capture_available || return 1

    if ! port="$(find_free_port "${CAPTURE_PORT}")"; then
        warn "no free port near ${CAPTURE_PORT}. The session runs without a recording."
        return 1
    fi

    CAPTURE_LOG="$(mktemp -t workshop-capture)"
    # -u: the log is a file, and Python would hold its output back otherwise.
    python3 -u "${CAPTURE_PROXY}" --port "${port}" --dir "${CAPTURE_DIR}" >"${CAPTURE_LOG}" 2>&1 &
    CAPTURE_PID=$!

    # Claude Code must not send its first request before the proxy listens.
    while port_is_free "${port}"; do
        if ((waited > 50)) || ! kill -0 "${CAPTURE_PID}" 2>/dev/null; then
            warn "the recorder did not start. The session runs without a recording."
            cat "${CAPTURE_LOG}" >&2
            rm -f "${CAPTURE_LOG}"
            CAPTURE_PID=""
            return 1
        fi
        sleep 0.1
        waited=$((waited + 1))
    done

    CAPTURE_PORT="${port}"
    # The proxy names its run directory on its first line of output.
    CAPTURE_RUN="$(sed -n 's/^capture: recording to //p' "${CAPTURE_LOG}" | head -n 1)"
}

viewer_available() {
    [[ "${WORKSHOP_VIEWER}" != "0" ]] && [[ -f "${CAPTURE_VIEWER}" ]]
}

# Serve the recording as a live page, and open it in the browser at the first
# call. Needs a running recorder. Best effort, like the recorder itself.
start_viewer() {
    local port waited=0

    viewer_available || return 1
    if [[ -z "${CAPTURE_RUN}" ]]; then
        warn "the recorder did not say where it writes. The recording is rendered afterwards."
        return 1
    fi

    if ! port="$(find_free_port "$((CAPTURE_PORT + 1))")"; then
        warn "no free port for the viewer near ${CAPTURE_PORT}. The recording is rendered afterwards."
        return 1
    fi

    VIEWER_LOG="$(mktemp -t workshop-viewer)"
    python3 -u "${CAPTURE_VIEWER}" "${CAPTURE_RUN}" --port "${port}" --open >"${VIEWER_LOG}" 2>&1 &
    VIEWER_PID=$!

    while port_is_free "${port}"; do
        if ((waited > 50)) || ! kill -0 "${VIEWER_PID}" 2>/dev/null; then
            warn "the viewer did not start. The recording is rendered afterwards."
            cat "${VIEWER_LOG}" >&2
            rm -f "${VIEWER_LOG}"
            VIEWER_PID=""
            return 1
        fi
        sleep 0.1
        waited=$((waited + 1))
    done

    VIEWER_URL="http://127.0.0.1:${port}/"
}

# Ask first, insist later, and never block the shell that is trying to exit.
stop_process() {
    local pid="$1" waited=0

    # TERM rather than KILL, so the process finishes the file it is writing.
    kill -TERM "${pid}" 2>/dev/null || true
    while kill -0 "${pid}" 2>/dev/null; do
        if ((waited > 50)); then
            kill -KILL "${pid}" 2>/dev/null || true
            break
        fi
        sleep 0.1
        waited=$((waited + 1))
    done

    wait "${pid}" 2>/dev/null || true
}

# The proxy goes first, so that no call lands after the viewer wrote the final
# pages. The trap calls this.
stop_capture() {
    if [[ -n "${CAPTURE_PID}" ]]; then
        stop_process "${CAPTURE_PID}"
        CAPTURE_PID=""
        rm -f "${CAPTURE_LOG}"
    fi
    if [[ -n "${VIEWER_PID}" ]]; then
        stop_process "${VIEWER_PID}"
        VIEWER_PID=""
        rm -f "${VIEWER_LOG}"
    fi
}

newest_capture_run() {
    find "${CAPTURE_DIR}" -mindepth 1 -maxdepth 1 -type d -name '20*' 2>/dev/null | sort | tail -n 1
}

print_capture_result() {
    local run count

    run="${CAPTURE_RUN:-$(newest_capture_run)}"
    [[ -n "${run}" ]] || return 0

    count="$(find "${run}" -maxdepth 1 -name '[0-9]*.json' | wc -l | tr -d ' ')"
    printf '\nRecorded %s API calls in %s\n' "${count}" "${run}"
    if [[ -f "${run}/index.html" ]]; then
        printf 'The page is saved. To open it again, run:\n\n    open %s\n\n' "${run}/index.html"
    else
        printf 'To see what the model received, run:\n\n    ./capture.sh render\n\n'
    fi
}

capture_status_line() {
    if [[ "${WORKSHOP_CAPTURE}" == "0" ]]; then
        printf 'off (WORKSHOP_CAPTURE=0)\n'
    elif [[ ! -f "${CAPTURE_PROXY}" ]]; then
        printf 'unavailable: %s is missing\n' "${CAPTURE_PROXY}"
    elif ! command -v python3 >/dev/null 2>&1; then
        printf 'unavailable: python3 is not installed\n'
    else
        printf 'on, port %s, into %s\n' "${CAPTURE_PORT}" "${CAPTURE_DIR}"
    fi
}

print_launch_help() {
    cat <<HELP

To start the workshop, run:

    ./workshop.sh claude

That is "claude" with CLAUDE_CONFIG_DIR set to the workshop configuration, and
the session recorded. The recording opens in your browser at the first API call
and follows the session. To start a session without a recording, set
WORKSHOP_CAPTURE=0; to record without the browser page, set WORKSHOP_VIEWER=0.

The first Claude session on this configuration starts logged out. Run
"claude auth login" once inside it. To go back to your own setup, start "claude"
as usual.
HELP
}

cmd_profile_status() {
    require_claude

    profile_exists ||
        die "No workshop configuration at ${PROFILE_CONFIG_DIR} yet. Run: ./workshop.sh profile setup"

    printf '\nConfig:    %s\n' "${PROFILE_CONFIG_DIR}"
    printf 'Recording: %s\n\n' "$(capture_status_line)"

    printf 'The workshop configuration is isolated. Your own ~/.claude is untouched.\n'
}

# The settings that keep the workshop prompt to what the workshop needs. Claude
# Code adds the claude.ai connectors of the signed-in account (Gmail, Google
# Calendar and the like) to every session as MCP servers, and offers the
# Artifact tool, whose description alone is a fifth of the tool text. Neither
# belongs in the workshop, and both are switched off here.
#
# The settings also turn on the status line of the workshop: the context size
# and the session cost under the input box. The script is copied into the
# configuration rather than named in the checkout, because "workshop.sh reset"
# can move the checkout to a checkpoint that does not have it.
#
# A settings.json or a status line script that is already there is left alone,
# like everything else in the configuration. If the settings lack any of this,
# the script says so.
write_profile_settings() {
    if [[ ! -e "${PROFILE_STATUSLINE}" ]]; then
        cp "${STATUSLINE_SOURCE}" "${PROFILE_STATUSLINE}"
    fi
    command -v python3 >/dev/null 2>&1 ||
        warn "python3 is not installed, so the status line of the workshop stays empty."

    if [[ -e "${PROFILE_SETTINGS}" ]]; then
        if ! grep -q disableClaudeAiConnectors "${PROFILE_SETTINGS}" || ! grep -q '"Artifact"' "${PROFILE_SETTINGS}"; then
            warn "${PROFILE_SETTINGS} is already there and does not switch off the claude.ai connectors" \
                "and the Artifact tool. Add \"disableClaudeAiConnectors\": true and \"permissions\": {\"deny\": [\"Artifact\"]}" \
                "to it, or remove the configuration and set it up again."
        fi
        if ! grep -q statusLine "${PROFILE_SETTINGS}"; then
            warn "${PROFILE_SETTINGS} is already there and has no status line. Add" \
                "\"statusLine\": {\"type\": \"command\", \"command\": \"python3 '${PROFILE_STATUSLINE}'\"}" \
                "to it, or remove the configuration and set it up again."
        fi
        return 0
    fi
    cat >"${PROFILE_SETTINGS}" <<SETTINGS
{
  "disableClaudeAiConnectors": true,
  "permissions": {
    "deny": ["Artifact"]
  },
  "statusLine": {
    "type": "command",
    "command": "python3 '${PROFILE_STATUSLINE}'"
  }
}
SETTINGS
}

cmd_profile_setup() {
    require_claude

    if profile_exists; then
        printf 'The workshop configuration %s is already there.\n' "${PROFILE_CONFIG_DIR}"
    else
        printf 'Making the workshop configuration %s ...\n' "${PROFILE_CONFIG_DIR}"
        mkdir -p "${PROFILE_CONFIG_DIR}"
    fi
    write_profile_settings

    cmd_profile_status
    print_launch_help
}

cmd_profile_remove() {
    profile_exists || die "There is no workshop configuration at ${PROFILE_CONFIG_DIR}."

    printf '\nThis removes the directory %s, with the sign-in and every session record in it.\n\n' "${PROFILE_CONFIG_DIR}"
    confirm "Continue?" || die "Cancelled."

    rm -rf "${PROFILE_CONFIG_DIR}"
    printf 'Removed. Your own ~/.claude is untouched.\n'
}

# Run claude on the configuration in SESSION_CONFIG_DIR. An empty value keeps
# the caller's own configuration: the variable is then left alone, so that a
# CLAUDE_CONFIG_DIR from the environment still counts.
run_claude() {
    if [[ -n "${SESSION_CONFIG_DIR}" ]]; then
        CLAUDE_CONFIG_DIR="${SESSION_CONFIG_DIR}" claude "$@"
    else
        claude "$@"
    fi
}

# Start a session on the given configuration, with the traffic recorded. Any
# further argument goes on to claude.
start_session() {
    local status=0

    SESSION_CONFIG_DIR="$1"
    shift

    require_claude

    printf 'Config:    %s\n' "${SESSION_CONFIG_DIR:-${CLAUDE_CONFIG_DIR:-${HOME}/.claude}}"

    # Without a recording there is nothing to clean up afterwards.
    if ! start_capture; then
        if capture_available; then
            printf 'Recording: none, see the warning above\n\n'
        else
            printf 'Recording: %s\n\n' "$(capture_status_line)"
        fi
        run_claude "$@"
        return 0
    fi

    trap stop_capture EXIT INT TERM
    printf 'Recording: capture proxy started on http://127.0.0.1:%s, writing to %s\n' "${CAPTURE_PORT}" "${CAPTURE_RUN:-${CAPTURE_DIR}}"
    if start_viewer; then
        printf 'Viewer:    %s opens in your browser at the first API call\n' "${VIEWER_URL}"
    elif viewer_available; then
        printf 'Viewer:    none, see the warning above\n'
    else
        printf 'Viewer:    off\n'
    fi
    printf 'Nothing else about the session changes.\n\n'

    ANTHROPIC_BASE_URL="http://127.0.0.1:${CAPTURE_PORT}" run_claude "$@" || status=$?

    stop_capture
    print_capture_result
    return "${status}"
}

# A workshop session on the isolated configuration.
cmd_claude() {
    profile_exists ||
        die "No workshop configuration yet. Run: ./workshop.sh profile setup"
    start_session "${PROFILE_CONFIG_DIR}" "$@"
}

# A session on your own configuration, recorded the same way. Put its recording
# next to a workshop one to see what your own setup adds to every request.
cmd_claude_own() {
    start_session "" "$@"
}

cmd_profile() {
    local action="${1:-status}"
    case "${action}" in
        setup) cmd_profile_setup ;;
        status) cmd_profile_status ;;
        remove) cmd_profile_remove ;;
        *)
            usage
            die "Unknown profile action: ${action}"
            ;;
    esac
}

main() {
    local command="${1:-}"
    [[ -n "${command}" ]] || {
        usage
        exit 1
    }
    shift

    # The profile and claude commands work on the Claude configuration and not
    # the checkout, so they need no git repository and no upstream remote.
    case "${command}" in
        profile)
            cmd_profile "${1:-status}"
            return 0
            ;;
        claude)
            cmd_claude "$@"
            return 0
            ;;
        claude-own)
            cmd_claude_own "$@"
            return 0
            ;;
    esac

    require_git_repo
    trap disconnect_upstream EXIT

    case "${command}" in
        list) cmd_list ;;
        reset) cmd_reset "${1:-}" ;;
        pull) cmd_pull ;;
        -h | --help | help) usage ;;
        *)
            usage
            die "Unknown command: ${command}"
            ;;
    esac
}

main "$@"
