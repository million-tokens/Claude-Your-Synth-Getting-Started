#!/usr/bin/env bash
#
# See what the session actually sent to the model.
#
# "start" runs a proxy that records the wire traffic between Claude Code and the
# Anthropic API. "render" turns a recording into two HTML pages: what changed
# between calls (index.html) and what one call is made of (profile.html).
#
# The transcript that Claude Code writes to ~/.claude/projects holds the
# conversation, but not the system prompt, the tool schemas or the cache
# breakpoints. Only the requests hold those, so only a proxy can show them.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CAPTURE_DIR="${CAPTURE_DIR:-${SCRIPT_DIR}/captures}"
CAPTURE_PORT="${CAPTURE_PORT:-8788}"
OPEN_BROWSER=1

usage() {
    cat <<'EOF'
Usage: ./capture.sh <command> [options]

Commands:
  start              record a session. Runs until Ctrl-C.
  render [RUN]       render a recording to HTML, two pages. Default: the newest one.
  list               list the recordings.

Options:
  -p PORT            port of the proxy. Default 8788, or CAPTURE_PORT.
  -d DIR             directory of the recordings. Default ./captures, or CAPTURE_DIR.
  -O                 render: do not open the page in a browser.
  -h                 this help.

Typical run, in two terminals:

  ./capture.sh start                                # terminal 1
  export ANTHROPIC_BASE_URL=http://127.0.0.1:8788   # terminal 2
  claude                                            # terminal 2
  ./capture.sh render                               # after the session

The recording holds your prompts and the answers of the model, so treat it like
the rest of the session. Credentials are not recorded. "captures/" stays out of
git.
EOF
}

require_python() {
    if ! command -v python3 >/dev/null 2>&1; then
        echo "capture: python3 is necessary. Install the Xcode command line tools: xcode-select --install" >&2
        exit 1
    fi
}

newest_run() {
    local run
    run="$(find "${CAPTURE_DIR}" -mindepth 1 -maxdepth 1 -type d -name '20*' 2>/dev/null | sort | tail -n 1)"
    if [[ -z "${run}" ]]; then
        echo "capture: no recordings in ${CAPTURE_DIR}. Run ./capture.sh start first." >&2
        exit 1
    fi
    echo "${run}"
}

open_page() {
    local page="$1"
    if [[ "${OPEN_BROWSER}" -eq 0 ]]; then
        return
    fi
    if [[ -n "${BROWSER:-}" ]]; then
        "${BROWSER}" "${page}" >/dev/null 2>&1 &
    elif command -v open >/dev/null 2>&1; then
        open "${page}"
    fi
}

cmd_start() {
    require_python
    exec python3 "${SCRIPT_DIR}/capture/proxy.py" --port "${CAPTURE_PORT}" --dir "${CAPTURE_DIR}"
}

cmd_render() {
    require_python
    local run="${1:-}"
    if [[ -z "${run}" ]]; then
        run="$(newest_run)"
    fi
    python3 "${SCRIPT_DIR}/capture/render.py" "${run}"
    python3 "${SCRIPT_DIR}/capture/profile.py" "${run}"
    open_page "${run}/index.html"
}

cmd_list() {
    if [[ ! -d "${CAPTURE_DIR}" ]]; then
        echo "capture: no recordings yet"
        return
    fi
    local run count
    while IFS= read -r run; do
        count="$(find "${run}" -maxdepth 1 -name '[0-9]*.json' | wc -l | tr -d ' ')"
        printf '%s  %5s exchanges\n' "$(basename "${run}")" "${count}"
    done < <(find "${CAPTURE_DIR}" -mindepth 1 -maxdepth 1 -type d -name '20*' | sort)
}

COMMAND="${1:-}"
if [[ -n "${COMMAND}" ]]; then
    shift
fi

while [[ $# -gt 0 ]]; do
    case "$1" in
        -p)
            CAPTURE_PORT="$2"
            shift 2
            ;;
        -d)
            CAPTURE_DIR="$2"
            shift 2
            ;;
        -O)
            OPEN_BROWSER=0
            shift
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        -*)
            echo "capture: unknown option $1" >&2
            exit 1
            ;;
        *)
            POSITIONAL="$1"
            shift
            ;;
    esac
done

case "${COMMAND}" in
    start) cmd_start ;;
    render) cmd_render "${POSITIONAL:-}" ;;
    list) cmd_list ;;
    -h | --help | "") usage ;;
    *)
        echo "capture: unknown command ${COMMAND}" >&2
        usage >&2
        exit 1
        ;;
esac
