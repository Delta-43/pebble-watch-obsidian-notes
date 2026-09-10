# Troubleshooting

Real errors hit while building and testing this, with exact causes and fixes. Check here first if
something breaks — several of these produce no obvious error message pointing at the real cause.

## `docker compose up` recreated/replaced an unrelated project's container

**Symptom**: running `docker compose up -d` from `docker/` prints `Found orphan containers (...) for
this project`, and/or a service you didn't expect gets `Recreate`d.

**Cause**: Compose defaults a project's name to its working directory's name if you don't set one
explicitly. If you (or another project you also run, like `pebble-index-research-agent`) have *another*
compose file that also lives in a directory literally called `docker/`, **and** both files define a
service with the same name (both define `mcp-obsidian`), Compose treats them as the same project and a
config-changed service gets recreated — potentially replacing a real, already-running container from the
other project with this one's, silently, with no confirmation prompt. This actually happened once during
this project's own development (see `TODO.md` Phase 6c) — no data was lost (the affected service is
stateless) but it was a real, disruptive mistake.

**Fix, already applied**: `docker/docker-compose.yml` pins an explicit top-level `name: delta-notes`, so
this can't happen regardless of what the parent directory is called. If you fork/rename this project,
keep that line, or set your own unique `name:`.

**General lesson if you hit something like this elsewhere**: before running `docker compose up` on a
host you know runs other projects' containers, run `docker ps -a` first and check for name collisions,
especially if your compose file's directory shares a name with another project's.

## n8n: `PATH_REJECTED: Path must be a portable vault-relative path`

**Symptom**: the webhook returns 500, and the `Create Note` node's error is `PATH_REJECTED`, even though
the computed path looks fine when you read the workflow.

**Cause**: an n8n string field set to `="<js expression>"` (a bare `=` prefix, no `{{ }}`) is **not**
valid n8n expression syntax — n8n only evaluates `{{ ... }}` regions inside a field that starts with
`=`; everything else is left as literal text. A bare `="Watch Inbox/" + $now...` gets sent through
*unevaluated*, literally including the `=`, quotes, `+`, and `$now` text — which obviously isn't a valid
vault-relative path.

**Fix**: always wrap the whole expression in `{{ }}`: `={{ "Watch Inbox/" + $now... }}`. This project's
own `Build Note` node hit exactly this bug during development and is now fixed — if you edit its
expressions, keep the `{{ }}` wrapping.

## n8n: `Could not connect to your MCP server` / `getaddrinfo EAI_AGAIN mcp-obsidian`

**Symptom**: the `Create Note` node can't reach `mcp-obsidian` at all.

**Cause**: the workflow's `endpointUrl` hardcodes the hostname `mcp-obsidian`. Docker's embedded DNS only
resolves that hostname for containers that are actually named (or aliased to) `mcp-obsidian` **on the
same Docker network** n8n is on. If you're running the standalone `docker-compose.yml`, the *service key*
is `mcp-obsidian` (Compose auto-aliases by service name regardless of `container_name`), so this should
just work — but if you started `mcp-obsidian` some other way (a plain `docker run` with a different
`--name`, for instance), you need to add a matching network alias yourself:
```bash
docker network connect --alias mcp-obsidian <n8n's-network> <your-actual-container-name>
```

## n8n: webhook returns 404 right after activating

**Symptom**: `curl` against `/webhook/delta-notes` returns 404 immediately after you activated the
workflow in the UI (or via `n8n publish:workflow`).

**Cause**: n8n only builds its list of active webhook routes once, at process startup. Activating a
workflow updates the database immediately, but the running process doesn't pick up the new route until
it restarts — same behavior as the sibling project's Local File Trigger.

**Fix**: `docker restart <your-n8n-container-name>`, then retest. (A restart also isn't instant — give it
a few seconds after the container reports "Up" before testing; n8n logs "Activated workflow ..." near
the end of its own startup sequence, not the beginning.)

## Watch shows "Failed to save note" even though the note was actually saved

**Symptom**: rare, and specifically follows a slow/flaky Bluetooth moment — the watch shows a failure,
but the note you dictated already exists in the vault.

**Cause**: the Pebble AppMessage protocol automatically retries a message if it doesn't get acknowledged
in time (built-in behavior over a lossy link, not a bug). If the *first* delivery actually succeeded but
its ack got lost, the watch retries, and the second delivery hits `obsidian_create_note`'s atomicity
guard (it refuses to overwrite an existing file) with the very same computed filename — since the
filename is timestamp + text based, a retry within the same minute produces an identical path. The n8n
call correctly fails (no data corruption, the original note is untouched), but the watch surfaces that
failure to you even though the note you dictated is actually sitting in your vault.

**Not fixed, by design for now**: cosmetic-only (no data loss or duplication), and only reachable via a
genuine delivery retry, which is uncommon. If you hit "Failed to save note," check `Watch Inbox/` before
assuming nothing happened — it may already be there.

## Emulator won't launch: `libSDL2-2.0.so.0: cannot open shared object file`

**Symptom**: `pebble install --emulator <platform>` fails immediately with this error (only relevant if
you're developing against the emulator rather than real hardware).

**Fix**:
```bash
sudo apt install -y libsdl2-2.0-0 libglib2.0-0 libpixman-1-0 zlib1g libsndio7.0
```
Add `--vnc` to every emulator-interacting `pebble` command (installs, screenshots, `emu-button`, `logs`)
in a headless/no-X11 environment.

## "Developer Connection" isn't where the generic Pebble docs say it is

**Symptom**: you enabled "Developer Mode" somewhere in Settings but can't find a "Developer Connection"
menu item.

**Cause**: the generic `developer.repebble.com` docs page describing this is stale relative to the
current `coredevices/mobileapp`. The real toggle is **not** behind a general "Developer Mode" switch.

**Fix** (checked against the actual app source, not just the docs page): Settings → **Phone** tab →
**Connectivity** section → **"Use LAN developer connection"** toggle. Then, separately, go to your
Watches list, tap your specific watch's **⋮** menu, and tap **Developer Connection** to actually start it
and reveal the IP address to use with `pebble install --phone <ip>`.
