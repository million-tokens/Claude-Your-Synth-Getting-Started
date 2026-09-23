#!/usr/bin/env python3
"""The status line of the workshop configuration.

Claude Code runs this script every time it redraws the status line and pipes it
a JSON object about the session on stdin. The script prints one line:

    Claude-Your-Synth-Getting-Started (my-synth) in 12.4k out 1.8k ctx 14.2k $0.42

in and out are the input and output tokens of the most recent API response, so
their sum, ctx, is the size of the context right now. Thinking is billed as
output and is part of out. ctx turns red with a warning sign when Claude Code
reports more than 200k tokens. The cost is the only running total: it adds up
every call of the session.

"./workshop.sh profile setup" copies this file into the workshop configuration,
next to the settings.json that names it. A broken status line must never break
the session, so missing values show as 0 and a bad input prints a short line.
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

RED = "\033[31m"
RESET = "\033[0m"


def short(tokens: int) -> str:
    if tokens < 1000:
        return str(tokens)
    return f"{tokens / 1000:.1f}k"


def git_branch(directory: str) -> str:
    try:
        result = subprocess.run(
            ["git", "-C", directory, "branch", "--show-current"],
            capture_output=True,
            text=True,
            timeout=1,
            check=False,
        )
    except (OSError, subprocess.SubprocessError):
        return ""
    return result.stdout.strip()


def render(data: dict, branch: str) -> str:
    directory = (data.get("workspace") or {}).get("current_dir") or data.get("cwd") or ""
    context = data.get("context_window") or {}
    tokens_in = int(context.get("total_input_tokens") or 0)
    tokens_out = int(context.get("total_output_tokens") or 0)
    cost = float((data.get("cost") or {}).get("total_cost_usd") or 0)

    where = Path(directory).name
    if branch:
        where = f"{where} ({branch})"

    total = f"ctx {short(tokens_in + tokens_out)}"
    if data.get("exceeds_200k_tokens"):
        total = f"{RED}⚠ {total}{RESET}"

    return f"{where} in {short(tokens_in)} out {short(tokens_out)} {total} ${cost:.2f}"


def main() -> None:
    try:
        data = json.load(sys.stdin)
    except ValueError:
        print("status line: no session data")
        return
    directory = (data.get("workspace") or {}).get("current_dir") or data.get("cwd") or "."
    print(render(data, git_branch(directory)))


if __name__ == "__main__":
    main()
