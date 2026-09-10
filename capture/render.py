#!/usr/bin/env python3
"""Turn a directory of captured exchanges into one readable HTML page.

Runs offline, over the files that proxy.py wrote. Nothing here touches the
network or a live session, so it is free to be slow and thorough.

Every request carries the whole conversation again, so a page that printed each
request in full would be the session repeated N times. This renderer prints, for
each request, only what changed since the previous request of the same
conversation: the system prompt, the tool schemas and the cache breakpoints when
they move, and the messages appended since the last step.
"""

from __future__ import annotations

import argparse
import hashlib
import html
import json
import sys
from datetime import datetime
from pathlib import Path

MESSAGES_PATH = "/v1/messages"


# --- reading the capture --------------------------------------------------


def load_run(run_dir: Path) -> list[dict]:
    records = []
    for path in sorted(run_dir.glob("[0-9]*.json")):
        try:
            records.append(json.loads(path.read_text(encoding="utf-8")))
        except json.JSONDecodeError:
            print(f"render: skipping unreadable {path.name}", file=sys.stderr)
    records.sort(key=lambda r: r.get("index", 0))
    return records


def is_messages_call(record: dict) -> bool:
    return record.get("method") == "POST" and record.get("path", "").startswith(MESSAGES_PATH) and "request_json" in record


def parse_sse(text: str) -> list[dict]:
    events = []
    for block in text.split("\n\n"):
        payload = "".join(line[6:] for line in block.splitlines() if line.startswith("data: "))
        if not payload:
            continue
        try:
            events.append(json.loads(payload))
        except json.JSONDecodeError:
            pass
    return events


def reassemble(record: dict) -> dict:
    """Rebuild the answer from the stream, or read it from a plain response."""
    if "response_json" in record:
        body = record["response_json"]
        return {
            "blocks": body.get("content") or [],
            "usage": body.get("usage") or {},
            "stop_reason": body.get("stop_reason"),
            "error": body.get("error"),
        }

    events = parse_sse(record.get("response_sse") or "")
    blocks: dict[int, dict] = {}
    usage: dict = {}
    stop_reason = None
    error = None
    for event in events:
        kind = event.get("type")
        if kind == "message_start":
            usage.update((event.get("message") or {}).get("usage") or {})
        elif kind == "content_block_start":
            blocks[event.get("index", len(blocks))] = dict(event.get("content_block") or {})
        elif kind == "content_block_delta":
            block = blocks.setdefault(event.get("index", 0), {"type": "text"})
            delta = event.get("delta") or {}
            for field, target in (("text", "text"), ("thinking", "thinking"), ("partial_json", "partial_json")):
                if field in delta:
                    block[target] = (block.get(target) or "") + delta[field]
        elif kind == "message_delta":
            usage.update(event.get("usage") or {})
            stop_reason = (event.get("delta") or {}).get("stop_reason", stop_reason)
        elif kind == "error":
            error = event.get("error")
    return {
        "blocks": [blocks[i] for i in sorted(blocks)],
        "usage": usage,
        "stop_reason": stop_reason,
        "error": error,
    }


# --- shaping the request --------------------------------------------------


def strip_cache_control(value):
    """A copy without cache_control, for comparing content across requests."""
    if isinstance(value, dict):
        return {k: strip_cache_control(v) for k, v in value.items() if k != "cache_control"}
    if isinstance(value, list):
        return [strip_cache_control(v) for v in value]
    return value


def canon(value) -> str:
    return json.dumps(strip_cache_control(value), sort_keys=True, ensure_ascii=False)


def system_blocks(body: dict) -> list[dict]:
    system = body.get("system")
    if system is None:
        return []
    if isinstance(system, str):
        return [{"type": "text", "text": system}]
    return system


def block_text(block) -> str:
    if isinstance(block, str):
        return block
    for field in ("text", "thinking", "partial_json"):
        if block.get(field):
            return block[field]
    if block.get("type") == "tool_use":
        return json.dumps(block.get("input") or {}, indent=2, ensure_ascii=False)
    if block.get("type") == "tool_result":
        content = block.get("content")
        if isinstance(content, list):
            return "\n".join(block_text(part) for part in content)
        return content if isinstance(content, str) else json.dumps(content, indent=2, ensure_ascii=False)
    return json.dumps(block, indent=2, ensure_ascii=False)


def message_text(message: dict) -> str:
    content = message.get("content")
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        return "\n".join(block_text(block) for block in content)
    return ""


def chain_key(body: dict) -> str:
    """Requests of one conversation share their first message.

    A subagent, a compaction and the quota probe each open their own first
    message, so they land in their own chain instead of polluting the diff of
    the main conversation.
    """
    messages = body.get("messages") or []
    seed = canon(messages[0]) if messages else canon(system_blocks(body)[:1])
    return hashlib.sha1(seed.encode("utf-8")).hexdigest()[:8]


def breakpoints(body: dict) -> list[str]:
    """Where the request asks the API to cut a prompt-cache segment."""
    marks = []
    for i, block in enumerate(system_blocks(body)):
        if isinstance(block, dict) and block.get("cache_control"):
            marks.append(f"system block {i}")
    tools = body.get("tools") or []
    for i, tool in enumerate(tools):
        if tool.get("cache_control"):
            marks.append(f"tool {i} ({tool.get('name', '?')}), last of {len(tools)}")
    for i, message in enumerate(body.get("messages") or []):
        content = message.get("content")
        if isinstance(content, list):
            for j, block in enumerate(content):
                if isinstance(block, dict) and block.get("cache_control"):
                    marks.append(f"message {i} block {j} ({message.get('role')})")
    return marks


def common_prefix(previous: list, current: list) -> int:
    n = 0
    for a, b in zip(previous, current):
        if canon(a) != canon(b):
            break
        n += 1
    return n


def usage_row(usage: dict) -> dict:
    fresh = usage.get("input_tokens", 0) or 0
    cache_read = usage.get("cache_read_input_tokens", 0) or 0
    cache_write = usage.get("cache_creation_input_tokens", 0) or 0
    return {
        "prompt_in": fresh + cache_read + cache_write,
        "fresh": fresh,
        "cache_read": cache_read,
        "cache_write": cache_write,
        "output": usage.get("output_tokens", 0) or 0,
    }


def build_exchanges(records: list[dict]) -> list[dict]:
    """One entry per API call, with its diff against the previous call."""
    exchanges = []
    previous_by_chain: dict[str, dict] = {}
    chain_numbers: dict[str, int] = {}

    for record in records:
        body = record["request_json"]
        key = chain_key(body)
        chain_numbers.setdefault(key, len(chain_numbers) + 1)
        previous = previous_by_chain.get(key)

        messages = body.get("messages") or []
        tools = body.get("tools") or []
        systems = system_blocks(body)
        answer = reassemble(record)
        row = usage_row(answer["usage"])

        if previous:
            kept = common_prefix(previous["messages"], messages)
            previous_tools = {t.get("name") for t in previous["tools"]}
            current_tools = {t.get("name") for t in tools}
            system_changed = canon(previous["systems"]) != canon(systems)
            tools_added = sorted(current_tools - previous_tools)
            tools_removed = sorted(previous_tools - current_tools)
            new_turn = row["prompt_in"] - previous["row"]["prompt_in"] - previous["row"]["output"]
        else:
            kept = 0
            system_changed = True
            tools_added = sorted(t.get("name", "?") for t in tools)
            tools_removed = []
            new_turn = None

        exchange = {
            "record": record,
            "body": body,
            "chain": chain_numbers[key],
            "chain_key": key,
            "first_in_chain": previous is None,
            "systems": systems,
            "tools": tools,
            "messages": messages,
            "kept": kept,
            "system_changed": system_changed,
            "tools_added": tools_added,
            "tools_removed": tools_removed,
            "breakpoints": breakpoints(body),
            "breakpoints_moved": previous is not None and breakpoints(body) != previous["breakpoints"],
            "answer": answer,
            "row": row,
            "new_turn": new_turn,
        }
        exchanges.append(exchange)
        previous_by_chain[key] = exchange
    return exchanges


# --- writing the page -----------------------------------------------------

CSS = """
:root { color-scheme: light dark; --bg:#fff; --fg:#1a1a1a; --muted:#666; --line:#e0e0e0;
        --card:#fafafa; --code:#f4f4f5; --accent:#7a3fd4; --add:#0a7b34; --del:#b3261e; }
@media (prefers-color-scheme: dark) {
  :root { --bg:#16161a; --fg:#e6e6e6; --muted:#9a9aa5; --line:#2c2c33;
          --card:#1d1d22; --code:#232329; --accent:#b18cf0; --add:#4ac97e; --del:#ff7b72; }
}
* { box-sizing: border-box; }
body { margin:0; padding:2rem 1.5rem 6rem; background:var(--bg); color:var(--fg);
       font:14px/1.55 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; }
main { max-width: 1100px; margin: 0 auto; }
h1 { font-size:1.5rem; margin:0 0 .25rem; }
h2 { font-size:1.05rem; margin:2.5rem 0 .75rem; padding-bottom:.35rem; border-bottom:1px solid var(--line); }
h3 { font-size:.95rem; margin:0; }
.sub { color:var(--muted); margin:0 0 1.5rem; }
table { border-collapse:collapse; width:100%; font-variant-numeric:tabular-nums; font-size:13px; }
th, td { padding:.3rem .5rem; text-align:right; border-bottom:1px solid var(--line); white-space:nowrap; }
th:first-child, td:first-child, th:nth-child(3), td:nth-child(3) { text-align:left; }
th { color:var(--muted); font-weight:600; }
.scroll { overflow-x:auto; }
.card { border:1px solid var(--line); border-radius:8px; background:var(--card); margin:1rem 0; }
.card > summary { padding:.6rem .8rem; cursor:pointer; list-style:none; display:flex;
                  gap:.6rem; align-items:baseline; flex-wrap:wrap; }
.card > summary::-webkit-details-marker { display:none; }
.card > summary:hover { background:var(--code); }
.body { padding:0 .8rem .8rem; }
.tag { font:600 11px/1 ui-monospace, monospace; padding:.25rem .4rem; border-radius:4px;
       background:var(--code); color:var(--muted); }
.tag.hot { color:var(--accent); }
.tag.add { color:var(--add); }
.tag.del { color:var(--del); }
.mono { font-family: ui-monospace, SFMono-Regular, Menlo, monospace; }
pre { margin:.4rem 0; padding:.6rem .7rem; background:var(--code); border-radius:6px;
      max-height:26rem; overflow:auto; white-space:pre-wrap; word-break:break-word;
      font:12px/1.5 ui-monospace, SFMono-Regular, Menlo, monospace; }
details.inner { border-top:1px solid var(--line); padding-top:.5rem; margin-top:.5rem; }
details.inner > summary { cursor:pointer; color:var(--muted); font-size:13px; }
.msg { margin:.6rem 0; }
.role { font:600 11px/1 ui-monospace, monospace; text-transform:uppercase;
        letter-spacing:.06em; color:var(--accent); }
.unchanged { color:var(--muted); font-size:13px; font-style:italic; }
.controls { display:flex; gap:.5rem; margin:1rem 0; flex-wrap:wrap; }
button { font:inherit; padding:.3rem .7rem; border:1px solid var(--line); border-radius:6px;
         background:var(--card); color:var(--fg); cursor:pointer; }
button:hover { background:var(--code); }
"""

JS = """
document.querySelectorAll('[data-toggle]').forEach(function (btn) {
    btn.addEventListener('click', function () {
        var open = btn.dataset.toggle === 'open';
        document.querySelectorAll('details.card').forEach(function (d) { d.open = open; });
        persist();
    });
});
// The live viewer reloads the page when a call lands. Remember which cards the
// reader opened or closed, so that the reload does not fold them back.
var storeKey = 'cards:' + document.title;
function persist() {
    var open = {};
    document.querySelectorAll('details.card').forEach(function (d) { open[d.id] = d.open; });
    try { sessionStorage.setItem(storeKey, JSON.stringify(open)); } catch (e) { /* no storage */ }
}
(function () {
    var open;
    try { open = JSON.parse(sessionStorage.getItem(storeKey)); } catch (e) { open = null; }
    if (open) document.querySelectorAll('details.card').forEach(function (d) { if (d.id in open) d.open = open[d.id]; });
    document.querySelectorAll('details.card').forEach(function (d) { d.addEventListener('toggle', persist); });
})();
"""


def esc(value) -> str:
    return html.escape(str(value), quote=False)


def pre(text: str) -> str:
    return f"<pre>{esc(text)}</pre>"


def render_block(block) -> str:
    if isinstance(block, str):
        return pre(block)
    kind = block.get("type", "?")
    label = kind
    if kind == "tool_use":
        label = f"tool_use &rarr; {esc(block.get('name', '?'))}"
    elif kind == "tool_result":
        label = "tool_result" + (" (error)" if block.get("is_error") else "")
    elif kind in ("image", "document"):
        return f'<div class="unchanged">[{esc(kind)}]</div>'
    return f'<div><span class="tag">{label}</span>{pre(block_text(block))}</div>'


def render_message(message: dict) -> str:
    content = message.get("content")
    blocks = content if isinstance(content, list) else [content]
    inner = "".join(render_block(block) for block in blocks if block is not None)
    return f'<div class="msg"><div class="role">{esc(message.get("role", "?"))}</div>{inner}</div>'


def render_system(exchange: dict) -> str:
    systems = exchange["systems"]
    chars = sum(len(block_text(block)) for block in systems)
    if not exchange["system_changed"]:
        return f'<details class="inner"><summary>System prompt unchanged &mdash; {len(systems)} blocks, {chars:,} chars</summary>{"".join(render_block(b) for b in systems)}</details>'
    mark = "changed" if not exchange["first_in_chain"] else "full"
    return (
        f'<details class="inner" open><summary>System prompt ({mark}) &mdash; '
        f"{len(systems)} blocks, {chars:,} chars</summary>"
        f'{"".join(render_block(b) for b in systems)}</details>'
    )


def render_tools(exchange: dict) -> str:
    tools = exchange["tools"]
    if not tools:
        return ""
    added, removed = exchange["tools_added"], exchange["tools_removed"]
    if exchange["first_in_chain"]:
        head = f"{len(tools)} tool schemas"
    elif added or removed:
        parts = []
        if added:
            parts.append(f'<span class="tag add">+{len(added)}</span> {esc(", ".join(added))}')
        if removed:
            parts.append(f'<span class="tag del">-{len(removed)}</span> {esc(", ".join(removed))}')
        head = f"Tools changed &mdash; {' '.join(parts)} (now {len(tools)})"
    else:
        head = f"Tools unchanged &mdash; {len(tools)}"
    names = ", ".join(esc(t.get("name", "?")) for t in tools)
    schemas = pre(json.dumps(tools, indent=2, ensure_ascii=False))
    return (
        f'<details class="inner"><summary>{head}</summary>'
        f'<div class="unchanged mono">{names}</div>'
        f'<details class="inner"><summary>full schemas</summary>{schemas}</details></details>'
    )


def render_breakpoints(exchange: dict) -> str:
    marks = exchange["breakpoints"]
    if not marks:
        return ""
    moved = ' <span class="tag hot">moved</span>' if exchange["breakpoints_moved"] else ""
    items = "".join(f"<li>{esc(m)}</li>" for m in marks)
    return (
        f'<details class="inner"><summary>Cache breakpoints &mdash; {len(marks)}{moved}</summary>'
        f'<ul class="mono">{items}</ul></details>'
    )


def render_messages(exchange: dict) -> str:
    messages = exchange["messages"]
    kept = exchange["kept"]
    new = messages[kept:]
    head = ""
    if kept:
        earlier = "".join(render_message(m) for m in messages[:kept])
        head = (
            f'<details class="inner"><summary>{kept} earlier messages, replayed unchanged</summary>'
            f"{earlier}</details>"
        )
    if not new:
        return head + '<div class="unchanged">No new messages: the request repeats the conversation.</div>'
    label = "Conversation" if exchange["first_in_chain"] else f"New since the previous call ({len(new)})"
    body = "".join(render_message(m) for m in new)
    return head + f'<details class="inner" open><summary>{label}</summary>{body}</details>'


def render_answer(exchange: dict) -> str:
    answer = exchange["answer"]
    if answer.get("error"):
        return f'<div><span class="tag del">error</span>{pre(json.dumps(answer["error"], indent=2))}</div>'
    if not answer["blocks"]:
        return '<div class="unchanged">No answer captured.</div>'
    body = "".join(render_block(block) for block in answer["blocks"])
    stop = esc(answer.get("stop_reason") or "?")
    return f'<details class="inner" open><summary>Response &mdash; stop_reason {stop}</summary>{body}</details>'


def render_summary(exchanges: list[dict]) -> str:
    rows = []
    for exchange in exchanges:
        row = exchange["row"]
        record = exchange["record"]
        new_turn = "-" if exchange["new_turn"] is None else f"{exchange['new_turn']:,}"
        rows.append(
            "<tr>"
            f'<td><a href="#x{record["index"]}">{record["index"]}</a></td>'
            f'<td>{exchange["chain"]}</td>'
            f'<td class="mono">{esc(exchange["body"].get("model", "?"))}</td>'
            f'<td>{row["prompt_in"]:,}</td><td>{row["fresh"]:,}</td>'
            f'<td>{row["cache_read"]:,}</td><td>{row["cache_write"]:,}</td>'
            f'<td>{row["output"]:,}</td><td>{new_turn}</td>'
            f'<td>{record.get("duration_ms", 0):,}</td>'
            "</tr>"
        )
    header = (
        "<tr><th>req</th><th>chain</th><th>model</th><th>prompt_in</th><th>fresh</th>"
        "<th>cache_read</th><th>cache_write</th><th>output</th><th>new_turn</th><th>ms</th></tr>"
    )
    return f'<div class="scroll"><table>{header}{"".join(rows)}</table></div>'


def render_totals(exchanges: list[dict]) -> str:
    fresh = sum(e["row"]["fresh"] for e in exchanges)
    read = sum(e["row"]["cache_read"] for e in exchanges)
    write = sum(e["row"]["cache_write"] for e in exchanges)
    output = sum(e["row"]["output"] for e in exchanges)
    served = read + write + fresh
    rate = (read / served * 100) if served else 0
    largest = max((e["row"]["prompt_in"] for e in exchanges), default=0)
    chains = len({e["chain_key"] for e in exchanges})
    return (
        f"<p class='sub'>{len(exchanges)} API calls in {chains} conversation chains &middot; "
        f"cache hit rate {rate:.1f}% &middot; largest prompt {largest:,} tokens &middot; "
        f"fresh input {fresh:,} &middot; cache reads {read:,} &middot; cache writes {write:,} &middot; "
        f"output {output:,}</p>"
    )


def render_exchange(exchange: dict) -> str:
    record = exchange["record"]
    row = exchange["row"]
    first = " &middot; first of chain" if exchange["first_in_chain"] else ""
    tags = [
        f'<span class="tag">#{record["index"]}</span>',
        f'<span class="tag">chain {exchange["chain"]}{first}</span>',
        f'<span class="mono">{esc(exchange["body"].get("model", "?"))}</span>',
        f'<span class="unchanged">{row["prompt_in"]:,} in &middot; {row["cache_read"]:,} cached &middot; '
        f'{row["output"]:,} out &middot; {record.get("duration_ms", 0):,}ms</span>',
    ]
    if exchange["system_changed"] and not exchange["first_in_chain"]:
        tags.append('<span class="tag hot">system changed</span>')
    if exchange["tools_added"] and not exchange["first_in_chain"]:
        tags.append('<span class="tag add">tools added</span>')
    if record.get("aborted"):
        tags.append('<span class="tag del">aborted</span>')
    if record.get("status") and record["status"] >= 400:
        tags.append(f'<span class="tag del">HTTP {record["status"]}</span>')

    body = (
        render_system(exchange)
        + render_tools(exchange)
        + render_breakpoints(exchange)
        + render_messages(exchange)
        + render_answer(exchange)
    )
    open_attr = " open" if exchange["first_in_chain"] else ""
    return (
        f'<details class="card" id="x{record["index"]}"{open_attr}>'
        f'<summary>{"".join(tags)}</summary>'
        f'<div class="body">{body}</div></details>'
    )


def render_other(records: list[dict]) -> str:
    if not records:
        return ""
    rows = "".join(
        f'<tr><td>{r.get("index")}</td><td class="mono">{esc(r.get("method"))} {esc(r.get("path"))}</td>'
        f'<td>{esc(r.get("status", r.get("error", "-")))}</td><td>{r.get("duration_ms", 0):,}</td></tr>'
        for r in records
    )
    return (
        "<h2>Other traffic</h2>"
        '<div class="scroll"><table>'
        "<tr><th>req</th><th>call</th><th>status</th><th>ms</th></tr>"
        f"{rows}</table></div>"
    )


def render_page(run_dir: Path, exchanges: list[dict], other: list[dict], meta: dict) -> str:
    started = meta.get("started_at", "?")
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>capture {esc(run_dir.name)}</title>
<style>{CSS}</style></head>
<body><main>
<h1>What the model received</h1>
<p class="sub">{esc(run_dir)} &middot; started {esc(started)} &middot;
upstream {esc(meta.get("upstream", "?"))} &middot; rendered {datetime.now().strftime("%Y-%m-%d %H:%M")} &middot;
<a href="context_profiler.html">what the prompt is made of</a></p>
{render_totals(exchanges)}
<h2>Requests</h2>
<p class="sub"><code>prompt_in</code> is the whole prompt of that call. <code>fresh</code> is what the
cache did not hold. <code>new_turn</code> is <code>prompt_in[N] - prompt_in[N-1] - output[N-1]</code>:
the tokens your message and the tool results added, which every later call carries again.</p>
{render_summary(exchanges)}
<h2>Exchanges</h2>
<div class="controls">
  <button data-toggle="open">Expand all</button>
  <button data-toggle="closed">Collapse all</button>
</div>
{"".join(render_exchange(e) for e in exchanges)}
{render_other(other)}
</main><script>{JS}</script></body></html>
"""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("run", nargs="?", default="captures/latest", help="capture run directory")
    parser.add_argument("-o", "--out", help="output file. Default <run>/index.html")
    args = parser.parse_args()

    run_dir = Path(args.run)
    if not run_dir.is_dir():
        print(f"render: no such run directory: {run_dir}", file=sys.stderr)
        return 1

    records = load_run(run_dir)
    if not records:
        print(f"render: no captured exchanges in {run_dir}", file=sys.stderr)
        return 1

    calls = [r for r in records if is_messages_call(r)]
    other = [r for r in records if not is_messages_call(r)]
    if not calls:
        print(f"render: {len(records)} records, none of them a {MESSAGES_PATH} call", file=sys.stderr)
        return 1

    meta_path = run_dir / "meta.json"
    meta = json.loads(meta_path.read_text(encoding="utf-8")) if meta_path.exists() else {}

    exchanges = build_exchanges(calls)
    out = Path(args.out) if args.out else run_dir / "index.html"
    out.write_text(render_page(run_dir.resolve(), exchanges, other, meta), encoding="utf-8")
    print(f"render: {len(exchanges)} exchanges -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
