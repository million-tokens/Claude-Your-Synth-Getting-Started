#!/usr/bin/env bash
set -euo pipefail

# Workshop helper. It moves your checkout between the lesson tags of the upstream
# workshop repository, and it sets up an isolated Claude Code configuration for the
# workshop. See the "Workshop navigation" section of README.md.

UPSTREAM_URL="${WORKSHOP_UPSTREAM:-git@github.com:MindWrapper/Claude-Your-Synth-Getting-Started.git}"
TAG_PREFIX="${WORKSHOP_TAG_PREFIX:-lesson-}"
BASE_BRANCH="main"
REMOTE="upstream"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

PROFILE_NAME="${WORKSHOP_PROFILE:-synth-workshop}"
WORKSHOP_HOME="${WORKSHOP_HOME:-${HOME}/claude-synth-workshop}"
PROFILE_CONFIG_DIR="${WORKSHOP_HOME}/claude-config"
DECK_CONFIG="${XDG_CONFIG_HOME:-${HOME}/.config}/agent-deck/config.toml"

WORKSHOP_CAPTURE="${WORKSHOP_CAPTURE:-1}"
CAPTURE_PROXY="${SCRIPT_DIR}/capture/proxy.py"
CAPTURE_DIR="${WORKSHOP_CAPTURE_DIR:-${SCRIPT_DIR}/captures}"
CAPTURE_PORT="${WORKSHOP_CAPTURE_PORT:-8788}"
CAPTURE_PID=""
CAPTURE_LOG=""

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

Lesson commands:
  list                 Show the lesson tags of the workshop.
  reset [lesson-tag]   Move the current branch to a lesson tag. Use "main" for the start.
  pull                 Merge the latest upstream main into the current branch.

Claude configuration commands:
  profile setup        Make the isolated Claude configuration for the workshop.
  profile status       Show which Claude configuration the workshop profile uses.
  profile remove       Delete the workshop profile and its Claude configuration.
  claude [args...]     Start a workshop session through Agent Deck, and record
                       what it sends to the model. See the recording afterwards
                       with "./capture.sh render".

Environment:
  WORKSHOP_UPSTREAM    Upstream repository URL. Default: the workshop repository over SSH.
  WORKSHOP_TAG_PREFIX  Prefix of the lesson tags. Default: lesson-
  WORKSHOP_PROFILE     Name of the Agent Deck profile. Default: synth-workshop
  WORKSHOP_HOME        Directory of the workshop configuration. Default: ~/claude-synth-workshop
  WORKSHOP_CAPTURE     Set to 0 to start the session without recording. Default: 1
  WORKSHOP_CAPTURE_DIR Directory of the recordings. Default: ./captures
  WORKSHOP_CAPTURE_PORT
                       First port the recorder tries. Default: 8788

If you give no lesson-tag, the script shows a menu.
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

# Fetch upstream main and the lesson tags. The plus sign in the tag refspec makes
# a moved upstream tag overwrite the stale local copy.
fetch_upstream() {
    printf 'Fetching %s ...\n' "${UPSTREAM_URL}"
    git fetch --quiet "${REMOTE}" "${BASE_BRANCH}" ||
        die "Cannot fetch ${BASE_BRANCH} from ${UPSTREAM_URL}. Check your SSH access."
    git fetch --quiet "${REMOTE}" \
        "+refs/tags/${TAG_PREFIX}*:refs/tags/${TAG_PREFIX}*" ||
        die "Cannot fetch the ${TAG_PREFIX}* tags from ${UPSTREAM_URL}."
}

# Print one line per lesson tag: <tag><TAB><subject>, in tag name order.
lesson_tags() {
    git for-each-ref --sort=refname \
        --format='%(refname:short)%09%(contents:subject)' \
        "refs/tags/${TAG_PREFIX}*"
}

# Show a menu and print the chosen tag. The menu goes to stderr, so that the
# caller can capture the tag from stdout.
select_lesson() {
    local names=() labels=() tag subject choice count i

    names+=("${REMOTE}/${BASE_BRANCH}")
    labels+=("main - the starting point")
    while IFS=$'\t' read -r tag subject; do
        names+=("${tag}")
        labels+=("${tag} - ${subject}")
    done < <(lesson_tags)

    count="${#names[@]}"

    printf '\nWhich lesson do you want to reset to?\n\n' >&2
    for ((i = 0; i < count; i++)); do
        printf '  %2d) %s\n' "$((i + 1))" "${labels[i]}" >&2
    done
    printf '\n' >&2
    read -r -p "Number [1-${count}]: " choice
    if [[ ! "${choice}" =~ ^[0-9]+$ ]] || ((choice < 1 || choice > count)); then
        die "Invalid selection: ${choice}"
    fi

    printf '%s\n' "${names[choice - 1]}"
}

# Check that the given lesson tag exists locally after the fetch.
require_lesson_tag() {
    local tag="$1"
    if ! git rev-parse --quiet --verify "refs/tags/${tag}" >/dev/null; then
        printf 'Error: no lesson tag "%s". Available tags:\n' "${tag}" >&2
        lesson_tags >&2
        exit 1
    fi
}

cmd_list() {
    connect_upstream
    fetch_upstream
    printf '\nLesson tags:\n\n'
    local tag subject
    while IFS=$'\t' read -r tag subject; do
        printf '  %-28s %s\n' "${tag}" "${subject}"
    done < <(lesson_tags)
    printf '\n'
}

cmd_reset() {
    local lesson="${1:-}" target
    refuse_protected_branch "reset"
    connect_upstream
    fetch_upstream

    if [[ -z "${lesson}" ]]; then
        target="$(select_lesson)"
    elif [[ "${lesson}" == "${BASE_BRANCH}" ]]; then
        target="${REMOTE}/${BASE_BRANCH}"
    else
        require_lesson_tag "${lesson}"
        target="${lesson}"
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
# Agent Deck starts each Claude session with CLAUDE_CONFIG_DIR set to the
# configuration directory that it resolves. The resolution order is: the
# CLAUDE_CONFIG_DIR variable, then the active profile, then [claude].config_dir,
# then ~/.claude. The workshop sets the profile level. Your own ~/.claude stays
# untouched, and it comes back when you start agent-deck without the profile.

require_agent_deck() {
    command -v agent-deck >/dev/null 2>&1 ||
        die "Agent Deck is not installed. See https://github.com/asheshgoplani/agent-deck"
    [[ -f "${DECK_CONFIG}" ]] ||
        die "No Agent Deck configuration at ${DECK_CONFIG}. Start agent-deck once, then try again."
}

deck_profile_exists() {
    agent-deck profile list 2>/dev/null |
        sed 's/^[* ]*//' |
        grep -qE "^${PROFILE_NAME}( |$)"
}

deck_config_has_profile() {
    grep -qF "[profiles.${PROFILE_NAME}.claude]" "${DECK_CONFIG}"
}

# Print the settings file that Agent Deck resolves for the workshop profile.
# It prints nothing when the hooks are not installed yet.
#
# This uses the AGENTDECK_PROFILE variable and not the -p option, because
# "hooks status -p <name>" ignores the profile in Agent Deck 1.12.
resolved_config_file() {
    AGENTDECK_PROFILE="${PROFILE_NAME}" agent-deck hooks status 2>/dev/null |
        awk '/^Config:/ { print $2 }' || true
}

# Delete the [profiles.<name>.claude] block from the Agent Deck configuration.
# The block ends at the next section header, or at the end of the file.
remove_profile_block() {
    local temp
    temp="$(mktemp "${DECK_CONFIG}.XXXXXX")"
    awk -v header="[profiles.${PROFILE_NAME}.claude]" '
        { line = $0; gsub(/^[ \t]+|[ \t]+$/, "", line) }
        line == header { skip = 1; next }
        skip == 1 && line ~ /^\[/ { skip = 0 }
        skip == 0 { print }
    ' "${DECK_CONFIG}" >"${temp}"
    mv "${temp}" "${DECK_CONFIG}"
}

# --- Recording what the model receives ---------------------------------------
#
# ANTHROPIC_BASE_URL is a Claude Code variable: it names the endpoint that Claude
# Code sends its requests to. "workshop.sh claude" starts the capture proxy on a
# local port, points that variable at it, and stops the proxy again when the
# session ends. The proxy forwards every request to the API untouched and keeps a
# copy, so that you can see afterwards what the model actually received. See
# "See what the model actually received" in README.md.
#
# Recording is best effort. If it cannot start, the session runs without it: a
# broken recorder must never cost you a session.

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
    python3 "${CAPTURE_PROXY}" --port "${port}" --dir "${CAPTURE_DIR}" >"${CAPTURE_LOG}" 2>&1 &
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
}

# Ask first, insist later, and never block the shell that is trying to exit.
stop_capture() {
    local pid="${CAPTURE_PID}" waited=0

    [[ -n "${pid}" ]] || return 0
    CAPTURE_PID=""

    # TERM rather than KILL, so the proxy finishes the file it is writing.
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
    rm -f "${CAPTURE_LOG}"
}

newest_capture_run() {
    find "${CAPTURE_DIR}" -mindepth 1 -maxdepth 1 -type d -name '20*' 2>/dev/null | sort | tail -n 1
}

print_capture_result() {
    local run count

    run="$(newest_capture_run)"
    [[ -n "${run}" ]] || return 0

    count="$(find "${run}" -maxdepth 1 -name '[0-9]*.json' | wc -l | tr -d ' ')"
    printf '\nRecorded %s API calls in %s\n' "${count}" "${run}"
    printf 'To see what the model received, run:\n\n    ./capture.sh render\n\n'
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

That is "agent-deck -p ${PROFILE_NAME}" with the session recorded. When it ends,
run "./capture.sh render" to see the requests as the model received them. To
start a session without a recording, run "agent-deck -p ${PROFILE_NAME}" yourself.

The first Claude session in this profile starts logged out. Run "claude auth login"
once inside it. To go back to your own setup, start agent-deck without the option.
HELP
}

cmd_profile_status() {
    require_agent_deck

    local expected="${PROFILE_CONFIG_DIR}/settings.json"
    local actual
    actual="$(resolved_config_file)"

    printf '\nProfile:  %s\n' "${PROFILE_NAME}"
    printf 'Expected: %s\n' "${expected}"

    if [[ -z "${actual}" ]]; then
        printf 'Resolved: none\n\n'
        printf 'Run "./workshop.sh profile setup" first.\n' >&2
        die "The profile resolves no configuration. Check its block in ${DECK_CONFIG}."
    fi

    printf 'Resolved: %s\n' "${actual}"
    printf 'Recording: %s\n\n' "$(capture_status_line)"

    if [[ "${actual}" != "${expected}" ]]; then
        die "The profile uses another configuration. Check the block in ${DECK_CONFIG}."
    fi

    printf 'The workshop configuration is isolated. Your own ~/.claude is untouched.\n'
}

cmd_profile_setup() {
    require_agent_deck

    mkdir -p "${PROFILE_CONFIG_DIR}"

    # Every agent-deck command below gets CLAUDE_CONFIG_DIR. That variable is the
    # first step of the resolution order, so these commands write into the
    # workshop configuration even when the profile block is absent or wrong. Your
    # own ~/.claude/settings.json is then safe from an accidental write.
    #
    # Do not export the variable for the whole function. cmd_profile_status must
    # run without it, because it has to test the profile itself.

    if deck_profile_exists; then
        printf 'The Agent Deck profile "%s" is already there.\n' "${PROFILE_NAME}"
    else
        printf 'Making the Agent Deck profile "%s" ...\n' "${PROFILE_NAME}"
        CLAUDE_CONFIG_DIR="${PROFILE_CONFIG_DIR}" \
            agent-deck profile create "${PROFILE_NAME}" >/dev/null ||
            die "Cannot make the profile ${PROFILE_NAME}."
    fi

    if deck_config_has_profile; then
        printf 'The profile block is already in %s.\n' "${DECK_CONFIG}"
    else
        cp "${DECK_CONFIG}" "${DECK_CONFIG}.workshop-bak"
        printf 'Copied %s to %s.workshop-bak\n' "${DECK_CONFIG}" "${DECK_CONFIG}"
        cat >>"${DECK_CONFIG}" <<TOML

[profiles.${PROFILE_NAME}.claude]
config_dir = "${PROFILE_CONFIG_DIR}"
TOML
        printf 'Added the profile block to %s.\n' "${DECK_CONFIG}"
    fi

    printf 'Installing the Agent Deck hooks into the workshop configuration ...\n'
    CLAUDE_CONFIG_DIR="${PROFILE_CONFIG_DIR}" agent-deck hooks install >/dev/null ||
        die "Cannot install the Agent Deck hooks."

    cmd_profile_status
    print_launch_help
}

cmd_profile_remove() {
    require_agent_deck

    printf '\nThis removes:\n'
    printf '  - the Agent Deck profile "%s"\n' "${PROFILE_NAME}"
    printf '  - the profile block in %s\n' "${DECK_CONFIG}"
    printf '  - the directory %s, with every session record in it\n\n' "${PROFILE_CONFIG_DIR}"
    confirm "Continue?" || die "Cancelled."

    # Agent Deck asks for its own confirmation, and it exits with 0 when you
    # cancel. Answer its question, then look at the profile list for the result.
    if deck_profile_exists; then
        printf 'y\n' | agent-deck profile delete "${PROFILE_NAME}" >/dev/null || true
        if deck_profile_exists; then
            die "Cannot delete the profile ${PROFILE_NAME}."
        fi
    fi

    if deck_config_has_profile; then
        cp "${DECK_CONFIG}" "${DECK_CONFIG}.workshop-bak"
        remove_profile_block
    fi

    rm -rf "${PROFILE_CONFIG_DIR}"
    printf 'Removed. Your own ~/.claude is untouched.\n'
}

# Start a workshop session through Agent Deck, with the traffic recorded.
#
# Agent Deck is the launcher and not "claude" itself, because the profile carries
# the isolated configuration and the hooks that "profile setup" installed. Any
# argument goes on to agent-deck.
cmd_claude() {
    local status=0

    require_agent_deck
    deck_profile_exists ||
        die "No workshop profile \"${PROFILE_NAME}\" yet. Run: ./workshop.sh profile setup"

    # Without a recording there is nothing to clean up afterwards, so hand the
    # process over to Agent Deck and keep this script out of the way.
    if ! start_capture; then
        exec agent-deck -p "${PROFILE_NAME}" "$@"
    fi

    trap stop_capture EXIT INT TERM
    printf 'Recording this session on port %s. Nothing else about the session changes.\n' "${CAPTURE_PORT}"

    ANTHROPIC_BASE_URL="http://127.0.0.1:${CAPTURE_PORT}" \
        agent-deck -p "${PROFILE_NAME}" "$@" || status=$?

    stop_capture
    print_capture_result
    return "${status}"
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
