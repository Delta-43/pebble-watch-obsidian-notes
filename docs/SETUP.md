# Setup guide

A step-by-step deployment guide, validated end-to-end against a real Pebble Time 2 and a real n8n/
Obsidian setup. For the *why* behind each decision, see [`ARCHITECTURE.md`](ARCHITECTURE.md) (and
[`../PLAN.md`](../PLAN.md) for the original design intent); for exact error messages and fixes if
something goes wrong, see [`TROUBLESHOOTING.md`](TROUBLESHOOTING.md).

Commands below use placeholder values in `<angle brackets>` — replace them with your own.

## Prerequisites

- A mic-equipped Pebble watch (Pebble Time, Time Steel, Time Round, Pebble 2, Pebble 2 Duo, or Core
  Devices' Pebble Time 2), paired with the [Pebble mobile app](https://github.com/coredevices/mobileapp)
- **LAN Developer Connection** enabled on your phone, to install the watchapp directly — Settings →
  **Phone** tab → **Connectivity** section → enable **"Use LAN developer connection"**, then from your
  Watches list, tap your watch's **⋮** menu → **Developer Connection** → start it to get an IP address.
  (This is a real, checked navigation path — an earlier version of the public docs described a
  different, incorrect one; see `TROUBLESHOOTING.md` if this doesn't match what you see.)
- An Obsidian vault, mounted as a plain directory somewhere Docker can reach — however it gets there
  (Self-hosted LiveSync, Syncthing, iCloud Drive, or Obsidian just running on that machine) is outside
  this project's scope
- Docker Engine + the Compose plugin
- An existing, self-hosted n8n instance, reachable from wherever `mcp-obsidian` will run

## Phase 0 — Build and install the watchapp

```bash
git clone https://github.com/Delta-43/pebble-watch-obsidian-notes.git
cd pebble-watch-obsidian-notes/watchapp

python3 -m venv .venv
./.venv/bin/pip install pebble-tool
source .venv/bin/activate
pebble sdk install latest

pebble build
pebble install --phone <ip-address-from-developer-connection>
```

On the watch: press SELECT (or, on a touchscreen model like the Pebble Time 2, tap the mic icon at the
bottom of the screen — both do the same thing). You should see the native dictation UI, then (once
you've completed Phase 2
below) "Sending..." followed by "Saved!" or a failure reason. Before the backend is wired up, seeing
**"Not configured"** here is expected and correct — it means dictation and the watch→phone leg both
work; you just haven't told the phone where to send notes yet.

## Phase 1 — Pick a backend path

Both paths end up importing the exact same `n8n/workflows/delta-notes.json` (Phase 2) — the only
difference is where the workflow's `obsidian_create_note` call gets its `mcp-obsidian` from.

- **Standalone** — you don't already run [`pebble-index-research-agent`](https://github.com/Delta-43/pebble-index-research-agent).
  Go to [Phase 1a](#phase-1a--standalone-run-mcp-obsidian-yourself).
- **Integrated** — you already have that project deployed, with its own `mcp-obsidian` running against
  your vault. Go to [Phase 1b](#phase-1b--integrated-reuse-an-existing-mcp-obsidian) — it's much shorter.

### Phase 1a — Standalone: run `mcp-obsidian` yourself

```bash
cd docker
cp .env.example .env
```

Edit `docker/.env`:

- **`VAULT_PATH`** — absolute host directory of your vault.
- **`VAULT_NAME`** — leave as `notes` unless you have a specific reason to change it (must match what
  the n8n workflow sends — see [Reconfiguring things later](#reconfiguring-things-later) if you do
  change it).
- **`N8N_NETWORK_NAME`** — the Docker network your *existing* n8n container is already on:
  ```bash
  docker inspect <your-n8n-container-name> --format '{{json .NetworkSettings.Networks}}'
  ```

Then:

```bash
mkdir -p "$VAULT_PATH/.obsidian"   # obsidian-mcp refuses a vault without one
docker compose up -d
```

Verify it's reachable from n8n:

```bash
docker exec <your-n8n-container-name> wget -qO- --timeout=3 http://mcp-obsidian:8801/sse
```

Should return `event: endpoint` / `data: /messages/?session_id=...` then hang (correct — it's a
long-lived SSE stream; `Ctrl+C` or let it time out).

Now go to [Phase 2](#phase-2--import-the-n8n-workflow).

### Phase 1b — Integrated: reuse an existing `mcp-obsidian`

Nothing to deploy. Just confirm your existing `mcp-obsidian` container is reachable from n8n by the
hostname `mcp-obsidian` (it will be, if it's a Compose service literally named `mcp-obsidian` — Compose
auto-aliases services by their service name regardless of `container_name`):

```bash
docker exec <your-n8n-container-name> wget -qO- --timeout=3 http://mcp-obsidian:8801/sse
```

If that hangs after printing `event: endpoint` (correct — Ctrl+C out of it) you're already set; go to
[Phase 2](#phase-2--import-the-n8n-workflow). If the hostname is actually different in your setup, you'll
need to edit the workflow's **Create Note** node's `endpointUrl` after importing it (Phase 2).

## Phase 2 — Import the n8n workflow

```bash
docker cp n8n/workflows/delta-notes.json <your-n8n-container-name>:/tmp/delta-notes.json
docker exec <your-n8n-container-name> n8n import:workflow --input=/tmp/delta-notes.json
```

**Add a Header Auth credential** (the imported workflow's `Delta Notes Webhook` node ships with no
credential attached, deliberately — never commit real secrets). In the n8n editor:

1. Credentials → New → **Header Auth**.
2. **Name**: `X-Auth-Token` (must match exactly — this is the actual HTTP header name, not a label).
3. **Value**: a strong random token, e.g. generate one with `openssl rand -hex 24`.
4. Open the imported workflow, click the **Delta Notes Webhook** node, set **Authentication** to
   **Header Auth**, and select the credential you just created.

**Activate the workflow**, then restart n8n:

```bash
docker restart <your-n8n-container-name>
```

n8n only registers a webhook's route at process boot — activating while it's already running updates the
database immediately, but the route won't actually respond until it restarts. (Same quirk as the sibling
project's Local File Trigger — see `TROUBLESHOOTING.md`.)

## Phase 3 — Configure the watchapp

On your phone, in the Pebble app: go to your app library (the **Locker**), find **Delta Notes** (either
under "Active" if it's the app currently running on your watch, or in your regular installed-apps list),
and tap its **Settings** button (it has one because it bundles a config page).

Enter:

- **Webhook URL**: `https://<your-n8n-host>/webhook/delta-notes`
- **Auth Token**: the same value you put in the Header Auth credential above

Save.

## Phase 4 — Test it

Press SELECT on the watch, dictate something, wait for it to confirm the transcription. Within a few
seconds you should see **"Saved!"** on the watch. Check:

- **n8n's Executions list** — a new run of "Delta Notes → Watch Inbox".
- **Your vault's `Watch Inbox/` folder** — a new note, filename = timestamp + the first few words of
  what you said, frontmatter tags `[pebble_watch, quick_note]`.
- **Your phone/desktop Obsidian** — the note should sync down automatically, the same way notes from any
  other device do.

## Reconfiguring things later

**Change the vault name** (`VAULT_NAME` in `docker/.env`, standalone path only) — the workflow's
**Create Note** node hardcodes `"vault": "notes"` in its JSON input. If you use a different name, open
the workflow in the n8n editor, click **Create Note**, and update the `"vault"` value in its JSON input
to match, then re-activate.

**Rotate the auth token** — generate a new one, update the Header Auth credential's **Value** in n8n,
and update the watchapp's config page (Phase 3) to match. No restart needed for either side.

**Change the note destination folder or tags** — edit the **Build Note** node's `notePath`/`noteContent`
expressions in the n8n editor (or `n8n/workflows/delta-notes.json` directly, then re-import — this
overwrites any customization you've made in the live workflow, same caveat as the sibling project).

**Pulling an updated version of this repo** — `git pull`. If `n8n/workflows/delta-notes.json` changed and
you've customized your live workflow, don't blindly re-import — re-importing overwrites your credential
attachment and any edits. Re-apply the specific change manually instead, or re-import and re-attach the
credential afterward.
