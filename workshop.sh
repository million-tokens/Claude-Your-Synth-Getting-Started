#!/usr/bin/env bash
set -euo pipefail

# Workshop helper. It moves your checkout between the checkpoints of the upstream
# workshop repository, and it sets up an isolated Claude Code configuration for the
# workshop. See the "Workshop navigation" section of README.md.

UPSTREAM_URL="${WORKSHOP_UPSTREAM:-git@github.com:million-tokens/Claude-Your-Synth-Getting-Started.git}"
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
  profile status       Show the workshop configuration.
  profile remove       Delete the workshop configuration.
  claude [args...]     Start Claude Code on that configuration.

Environment:
  WORKSHOP_UPSTREAM    Upstream repository URL. Default: the workshop repository over SSH.
  WORKSHOP_HOME        Directory of the workshop configuration. Default: ~/claude-synth-workshop

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

print_launch_help() {
    cat <<HELP

To start the workshop, run:

    ./workshop.sh claude

That is "claude" with CLAUDE_CONFIG_DIR set to the workshop configuration.

The first Claude session on this configuration starts logged out. Run
"claude auth login" once inside it. To go back to your own setup, start "claude"
as usual.
HELP
}

cmd_profile_status() {
    require_claude

    profile_exists ||
        die "No workshop configuration at ${PROFILE_CONFIG_DIR} yet. Run: ./workshop.sh profile setup"

    printf '\nConfig: %s\n\n' "${PROFILE_CONFIG_DIR}"

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

# A workshop session: claude on the isolated configuration. Any further argument
# goes on to claude.
cmd_claude() {
    require_claude
    profile_exists ||
        die "No workshop configuration yet. Run: ./workshop.sh profile setup"
    printf 'Config: %s\n' "${PROFILE_CONFIG_DIR}"
    CLAUDE_CONFIG_DIR="${PROFILE_CONFIG_DIR}" exec claude "$@"
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
