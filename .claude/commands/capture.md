---
description: Render the recorded API traffic of this session into HTML
allowed-tools: Bash(./capture.sh:*)
---

Run `./capture.sh render $ARGUMENTS`, then report to the user the path that the
script names.

The script reads the requests that `./capture.sh start` recorded and writes two
pages per recording: `index.html`, what changed between calls, and
`context_profiler.html`, what one call is made of. Do not read the recording or the page into the
context: both hold the whole session, repeated once per API call.

If the script reports that there are no recordings, tell the user that the proxy
was not running for this session, and give them the two commands: `./capture.sh
start` in one terminal, then `export ANTHROPIC_BASE_URL=http://127.0.0.1:8788`
before `claude` in the other.
