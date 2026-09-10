# Architecture

## Goal

Let a Pebble watch dictate a short note and have it land as a plain Markdown file in an Obsidian vault,
with as little custom code and as little new infrastructure as possible. Sibling to
[`pebble-index-research-agent`](https://github.com/Delta-43/pebble-index-research-agent) (voice notes
from a Pebble Index 01 ring → AI-researched notes), but deliberately much narrower in scope: no audio
handling, no web research, no dependency on the ring or its LiveSync/MinIO sync pipeline.

## Why a watch app, not a ring app

The ring's own pipeline works by having its companion app (`coredevices/mobileapp`) record audio,
transcribe it (its `experimental`/`index-ai` modules — a real, phone-native audio pipeline), and save
the transcript into the vault, where the *sibling* project's own n8n workflow picks it up. Replicating
any of that for a watch app would mean owning audio capture, upload, and transcription — a large system
this project has no reason to build.

Instead, the classic Pebble SDK ships a **Dictation API**
(`dictation_session_create()`/`dictation_session_start()`) that does the mic capture *and* the cloud
transcription itself, handing the watchapp back a plain C string. The watchapp's own code never touches
audio at all — it just asks for text and gets text. This was the single biggest open risk going in
(would Core Devices' current cloud recognizer backend actually work?) and is now **confirmed working on
real Pebble Time 2 hardware** — see [Real findings](#real-findings-verified-not-assumed) below.

## Why a direct webhook, not Obsidian LiveSync's sync protocol

The ring's notes reach the vault via Obsidian LiveSync's own sync protocol — chunked, end-to-end
encrypted, replicated through MinIO — which the sibling project's `livesync-cli` decrypts into plain
files on a server, for n8n to watch via a Local File Trigger. Speaking that same protocol from
watch-side JavaScript (PebbleKit JS) would mean reimplementing LiveSync's chunking/encryption/
replication logic in a Pebble watchapp's phone-side JS sandbox — a large lift for "dictate a quick
note."

Instead: PebbleKit JS (confirmed real and current — `coredevices/mobileapp`'s own README documents it
explicitly, with both network access and config-page support) makes a plain authenticated HTTPS POST to
an n8n **Webhook Trigger**. This is the "standard building blocks, no custom code" equivalent of the
sibling project's own design philosophy, applied to a much simpler job.

## Why no AI agent in the note-saving path

The sibling project's research pipeline genuinely needs an LLM — it has to interpret a fragment,
research it, and synthesize a note. Saving a dictated note is not that: it's a deterministic action
(build a filename, build frontmatter, write the file) with no reasoning involved. Routing it through an
AI Agent node anyway would add cost, latency, and a failure mode ("what if the model decides to do
something else") for zero benefit.

n8n has two different MCP-related node types, easy to conflate: `@n8n/n8n-nodes-langchain.mcpClientTool`
(designed to be wired into an AI Agent's tool belt) and the plain `@n8n/n8n-nodes-langchain.mcpClient`
(calls one named MCP tool directly, as an ordinary deterministic workflow step, no agent involved). This
project's workflow uses the latter — found by inspecting a real n8n instance's own installed node
definitions (`dist/nodes/mcp/**`) rather than guessing, since the two are easy to confuse and the wrong
one would have silently required an LLM credential for no reason.

## Why `mcp-obsidian` is reused, not rebuilt

The sibling project already publishes a generic, ring-agnostic `mcp-obsidian` image
(`ghcr.io/delta-43/pebble-index-research-agent/mcp-obsidian:latest` — configured entirely via
`VAULT_NAME`/`VAULT_MOUNT` env vars, no ring- or research-specific logic baked in). Building a second,
near-identical image for this project would be pure duplication. Confirmed the image is actually public
and pullable with zero authentication.

## Two deployment paths, one n8n workflow

Both paths import the exact same `n8n/workflows/delta-notes.json` — the only difference is where the
workflow's `obsidian_create_note` call gets its `mcp-obsidian` from:

- **Standalone** (`docker/docker-compose.yml`) — for anyone who doesn't already run the sibling
  project. Ships *only* `mcp-obsidian`, bind-mounted to a local vault directory, joined to an
  `external: true` n8n network. n8n itself is never bundled, matching the sibling project's own
  convention exactly.
- **Integrated** — for anyone who already runs `pebble-index-research-agent`. No new containers at all:
  the same workflow just points at the already-running `mcp-obsidian`, reachable by Docker DNS since
  Compose auto-aliases a service by its service name (`mcp-obsidian`) regardless of `container_name`.

Notes land in their own `Watch Inbox/` folder, not the ring's `Index Inbox/` — deliberately decoupled
from the research pipeline's trigger, so this project stays genuinely standalone. Wiring research into
watch notes later, if ever wanted, is an additive branch on the workflow, not a rewrite of this app.

## Watch app architecture

`watchapp/src/c/` is one module per responsibility rather than a single file, each with a `.h` exposing
only what other modules need:

| Module | Owns |
|---|---|
| `main.c` | Window lifecycle only — creates the window, wires every other module together |
| `status_display` | The on-screen text (idle prompt / "Sending..." / result) and its auto-revert-to-idle timer |
| `note_transport` | Whether the app is currently "busy" sending a note, and the AppMessage conversation with the phone |
| `dictation_handler` | The `DictationSession` — starting it, routing its result onward |
| `mic_icon` | The tap-to-record icon (touch platforms only) |

The mic icon and all touch-handling code are gated by `#ifdef PBL_TOUCH` — a compile-time *capability*
check, not a check for "Pebble Time 2" specifically. Confirmed via symbol inspection
(`arm-none-eabi-nm`) that touch code is compiled into the `emery` (Pebble Time 2) binary only, and
absent from basalt/chalk/diorite/flint — which also surfaced that `flint` (Pebble 2 Duo), despite being
the other new Core Devices platform, does *not* have a touchscreen. Any future Core Devices touch
hardware picks up the mic icon automatically, with zero code changes, by virtue of this being a
capability gate rather than a platform allowlist.

## Watch ↔ phone protocol

Two AppMessage keys carry the whole conversation:

- `noteText` (watch → phone): the dictated text, sent once dictation succeeds.
- `resultStatus` / `resultMessage` (phone → watch): whether the webhook call succeeded, and an optional
  short failure reason.

The phone side (`watchapp/src/pkjs/index.js`) uses **Clay** (`@rebble/clay`) for the on-phone
configuration page (webhook URL + auth token) rather than a hand-hosted HTML page — the current
recommended approach, and it needs no hosting at all. Configured with `autoHandleEvents: false` so Clay
stores the settings in phone-side `localStorage` only; the watch itself never needs them, only the JS
does, so there's no reason to also relay them over AppMessage.

## n8n workflow logic

`n8n/workflows/delta-notes.json`, three nodes:

1. **Delta Notes Webhook** (`n8n-nodes-base.webhook`) — `POST /webhook/delta-notes`, Header Auth
   (credential shipped unattached — never commit secrets, see `docs/SETUP.md`), `responseMode: lastNode`
   so the phone's HTTP response only comes back *after* the note is actually written, not immediately on
   receipt — the watch's "Saved!"/"Failed" screen genuinely reflects what happened.
2. **Build Note** (`n8n-nodes-base.set`) — computes the note's path (`Watch Inbox/<timestamp, HHmmss>
   Watch Note.md` — fixed suffix rather than derived from the dictated text, so titles stay
   predictable/sortable; seconds precision avoids `obsidian_create_note`'s overwrite guard colliding two
   distinct same-minute notes) and content (YAML frontmatter + the dictated text). Every expression here is wrapped
   in `={{ ... }}` — a bare `="<expr>"` prefix is *not* valid n8n expression syntax and silently sends
   the literal, unparsed text instead (a real bug hit and fixed during development — see
   `docs/TROUBLESHOOTING.md`).
3. **Create Note** (`@n8n/n8n-nodes-langchain.mcpClient`) — calls `obsidian_create_note` on
   `mcp-obsidian` directly, `vault: "notes"`, `create_parents: true` (so `Watch Inbox/` doesn't need to
   pre-exist).

## Real findings (verified, not assumed)

| Question | Finding |
|---|---|
| Does the Dictation API's cloud recognizer actually work on real Pebble Time 2 hardware? | **✅ Confirmed on real hardware.** The single biggest open risk in the whole project — real dictation, real transcribed text, via Core Devices' cloud backend. |
| Does the watch→phone AppMessage leg work on real hardware? | **✅ Confirmed** — with no webhook configured, the app correctly showed "Not configured" after a real dictation, proving the full watch→phone round trip. |
| Does tapping the mic icon actually start dictation on real hardware? | **✅ Confirmed.** The emulator's own touch controller isn't wired to VNC pointer input (tested, inconclusive there), so this specifically needed real hardware. |
| Is the n8n workflow's node schema actually correct? | **✅ Confirmed** by importing the real JSON into a real (local, throwaway) n8n instance via `n8n import:workflow`, and separately by a real webhook POST producing a real note with correct frontmatter, read back from disk. |
| Does the standalone `docker-compose.yml` actually work as a public user would run it? | **✅ Confirmed** via a real `docker compose up -d` against a scratch vault + throwaway n8n — but this test also caused a real incident (see below), now fixed. |
| Can PebbleKit JS actually reach an arbitrary HTTPS endpoint from inside `coredevices/mobileapp`? | **✅ Confirmed on real hardware** — the real webhook call, from the real phone app, succeeded. |

**Incident, self-caught and fixed**: testing the standalone `docker-compose.yml` via a literal
`docker compose up -d` recreated/replaced the *sibling* project's real, already-running `mcp-obsidian`
container. Root cause: both compose files live in directories named `docker/`, and neither set an
explicit project `name:`, so Compose treated them as the same project and collided on the shared service
name `mcp-obsidian`. No data loss (the service is stateless), fully recovered by re-running the sibling
project's own compose file. Fixed at the root: `docker/docker-compose.yml` now pins `name: delta-notes`
explicitly. Full detail in `docs/TROUBLESHOOTING.md`.

## Deployment topology

Standalone path — everything below runs as Docker containers, alongside (not merged into) your existing
n8n:

- `n8n` (already running, external, never bundled by this project)
- `mcp-obsidian` (this project's compose file — reused image, bind-mounted to your vault directory, on
  n8n's external network, no host port published — network isolation is the security boundary, matching
  the sibling project's own reasoning for the same design choice)

Integrated path — no new containers; the same n8n workflow reuses the sibling project's own
already-running `mcp-obsidian`.

See [`docker/docker-compose.yml`](../docker/docker-compose.yml) for the current service definition, and
[`docs/SETUP.md`](SETUP.md) for the exact deployment steps for both paths.
