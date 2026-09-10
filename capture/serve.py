#!/usr/bin/env python3
"""Show a recording in the browser while the session is still running.

proxy.py drops one JSON file per API call into the run directory. This viewer
serves the two pages of render.py and profile.py from those files, rebuilt
whenever a new file lands, and the pages reload themselves when that happens.
So the page in the browser follows the session as it goes.

The viewer is a separate process from the proxy on purpose. The proxy sits in
the critical path of the session and must stay dumb; the rendering, which is
allowed to be slow or to fail, happens here. A broken viewer costs a page in the
browser and nothing else.

When the viewer stops, it writes the final pages into the run directory, so that
"./capture.sh render" has nothing left to do and the recording reads the same
way afterwards.

Routes:
    /                        what changed between calls (index.html)
    /context_profiler.html   what one call is made of
    /status                  {"count": N}, the number of recorded calls
"""

from __future__ import annotations

import argparse
import json
import signal
import sys
import threading
import time
import traceback
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import profile  # noqa: E402 - the renderers live next to this file
import render  # noqa: E402

DEFAULT_PORT = 8790
PAGES = {"index.html": "render", "context_profiler.html": "profile"}

# Injected into every served page. Polls /status; reloads when a call landed;
# when the viewer is gone, says where the static copy of the page is.
LIVE_JS = r"""
(function () {
    var count = __COUNT__, saved = __SAVED__;
    var pill = document.createElement('div');
    pill.id = 'live';
    pill.style.cssText = 'position:fixed;top:.6rem;right:.8rem;z-index:9;padding:.3rem .6rem;border-radius:999px;' +
        'font:600 12px/1.4 ui-monospace,monospace;background:#0a7b34;color:#fff;box-shadow:0 1px 4px rgba(0,0,0,.3)';
    pill.textContent = 'live · ' + count + ' call' + (count === 1 ? '' : 's');
    document.body.appendChild(pill);
    function ended() {
        pill.style.background = '#666';
        pill.textContent = 'session ended · saved to ' + saved;
        pill.title = saved;
    }
    function tick() {
        fetch('/status', { cache: 'no-store' })
            .then(function (r) { return r.json(); })
            .then(function (s) {
                if (s.count !== count) { location.reload(); return; }
                setTimeout(tick, 1000);
            })
            .catch(ended);
    }
    setTimeout(tick, 1000);
})();
"""

WAITING_HTML = """<!doctype html>
<html lang="en"><head><meta charset="utf-8"><title>capture __RUN__</title>
<style>
:root { color-scheme: light dark; }
body { margin:0; padding:4rem 1.5rem; font:15px/1.6 -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; }
main { max-width:640px; margin:0 auto; }
h1 { font-size:1.4rem; }
p { color:#888; }
</style></head>
<body><main>
<h1>Waiting for the first API call</h1>
<p>The session is recording to <code>__RUN__</code>. As soon as Claude Code sends its first request,
this page becomes the recording, and it follows the session from then on.</p>
</main></body></html>
"""


def count_records(run_dir: Path) -> int:
    """Complete files only. The proxy writes a .part and renames it when done."""
    return sum(1 for _ in run_dir.glob("[0-9]*.json"))


class Pages:
    """Renders the two pages for the current state of the run, once per state."""

    def __init__(self, run_dir: Path) -> None:
        self.run_dir = run_dir
        self.lock = threading.Lock()
        self.cache: dict[tuple[str, int], str] = {}

    def meta(self) -> dict:
        path = self.run_dir / "meta.json"
        return json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}

    def build(self, name: str) -> str:
        records = render.load_run(self.run_dir)
        calls = [r for r in records if render.is_messages_call(r)]
        other = [r for r in records if not render.is_messages_call(r)]
        exchanges = render.build_exchanges(calls)
        if PAGES[name] == "profile":
            return profile.render_page(self.run_dir.resolve(), exchanges, profile.build_profile(exchanges), self.meta())
        return render.render_page(self.run_dir.resolve(), exchanges, other, self.meta())

    def page(self, name: str, count: int) -> str:
        key = (name, count)
        with self.lock:
            if key not in self.cache:
                # One state, one render. The cache is keyed by count, so the
                # previous state's pages go as soon as a new call has landed.
                self.cache = {k: v for k, v in self.cache.items() if k[1] == count}
                self.cache[key] = self.build(name)
            return self.cache[key]

    def live(self, name: str, count: int) -> str:
        if count == 0:
            body = WAITING_HTML.replace("__RUN__", render.esc(str(self.run_dir)))
        else:
            body = self.page(name, count)
        saved = json.dumps(str(self.run_dir.resolve() / name))
        script = LIVE_JS.replace("__COUNT__", str(count)).replace("__SAVED__", saved)
        return body.replace("</body>", f"<script>{script}</script></body>")

    def write_final(self) -> int:
        """The static pages, as capture.sh render would write them."""
        count = count_records(self.run_dir)
        if not count:
            return 0
        for name in PAGES:
            (self.run_dir / name).write_text(self.page(name, count), encoding="utf-8")
        return count


class Handler(BaseHTTPRequestHandler):
    server_version = "capture-viewer"
    sys_version = ""

    def log_message(self, fmt: str, *args) -> None:  # noqa: A003 - base class name
        pass

    def reply(self, status: int, body: str, content_type: str = "text/html; charset=utf-8") -> None:
        raw = body.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self) -> None:  # noqa: N802 - base class name
        pages: Pages = self.server.pages
        path = self.path.split("?", 1)[0]
        count = count_records(pages.run_dir)
        if path == "/status":
            self.reply(200, json.dumps({"count": count}), "application/json")
            return
        name = "index.html" if path in ("/", "/index.html") else path.lstrip("/")
        if name not in PAGES:
            self.reply(404, "not found", "text/plain")
            return
        try:
            self.reply(200, pages.live(name, count))
        except Exception:  # noqa: BLE001 - a renderer bug must show in the browser, not kill the viewer
            self.reply(500, f"<pre>{render.esc(traceback.format_exc())}</pre>")


def open_on_first_call(run_dir: Path, url: str, stop: threading.Event) -> None:
    """Open the browser when there is something to show, not before: opening it
    at startup would take the focus from the terminal just as the session starts."""
    while not stop.is_set():
        if count_records(run_dir):
            webbrowser.open(url)
            return
        stop.wait(0.5)


def install_shutdown(server: ThreadingHTTPServer) -> None:
    # See proxy.py: the handler must not call shutdown() on the serving thread,
    # and a background job inherits SIGINT as ignored unless it is set here.
    def handle(signum, frame):  # noqa: ARG001 - signal handler signature
        threading.Thread(target=server.shutdown, daemon=True).start()

    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, handle)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("run", help="capture run directory")
    parser.add_argument("-p", "--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--open", action="store_true", help="open the browser at the first recorded call")
    args = parser.parse_args()

    run_dir = Path(args.run)
    if not run_dir.is_dir():
        print(f"viewer: no such run directory: {run_dir}", file=sys.stderr)
        return 1

    try:
        server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    except OSError as error:
        print(f"viewer: cannot start: {error}", file=sys.stderr)
        return 1
    server.daemon_threads = True
    server.pages = Pages(run_dir)

    url = f"http://127.0.0.1:{args.port}/"
    print(f"viewer: {url} shows {run_dir} as it goes", flush=True)

    stop = threading.Event()
    if args.open:
        threading.Thread(target=open_on_first_call, args=(run_dir, url, stop), daemon=True).start()

    install_shutdown(server)
    server.serve_forever()
    server.server_close()
    stop.set()

    # The proxy may still be finishing its last file. Give it a moment.
    time.sleep(0.2)
    count = server.pages.write_final()
    if count:
        print(f"viewer: {count} calls -> {run_dir / 'index.html'}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
