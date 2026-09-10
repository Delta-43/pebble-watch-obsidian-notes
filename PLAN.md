# PLAN.md — Delta Notes

Design intent for this build, agreed with the user before any code was written. This is the source of
truth for *why* things are shaped this way — `TODO.md` tracks *whether* each piece is done yet. Once
real building starts, the "already-built docs" (`README.md`, `docs/ARCHITECTURE.md`, `docs/SETUP.md`)
take over as the canonical description of the finished thing; this file stays as the historical design
record and shouldn't need to change unless a decision below is actually overturned.

## What we're building

A Pebble watchapp ("Delta Notes") for the Core Devices **Pebble Time 2** that lets you dictate a short
note on your wrist and have it saved directly into your Obsidian vault as a plain Markdown note — no
manual copy/paste, no phone typing. Sibling to `pebble-index-research-agent` (the Pebble Index 01 ring →
AI-researched-note pipeline) but intentionally much simpler: no audio handling in our own code, no web
research step, no dependency on the ring or its LiveSync/MinIO vault-mirror pipeline. Can run fully
standalone, or be pointed at an existing `pebble-index-research-agent` n8n deployment.

## End-to-end flow

```
[Pebble Time 2 watch]
  press SELECT → dictation_session_start()
  → native OS "confirm transcription" dialog (built into the Dictation API, no UI work needed)
  → on confirm: AppMessage → phone

[PebbleKit JS — runs inside the official coredevices/mobileapp on the phone]
  receives text via AppMessage
  → reads webhook URL + auth token from the on-watch/on-phone config page (set once, at install)
  → HTTPS POST { text, timestamp } → https://<your-n8n-host>/webhook/delta-notes
  → relays success/failure back to the watch via AppMessage

[n8n — external, never bundled by this project]
  Webhook Trigger node (Header Auth: shared secret token)
  → obsidian_create_note (MCP Client Tool → mcp-obsidian)
  → writes into "Watch Inbox/" in the vault, tags [pebble_watch, quick_note],
    title = timestamp + first few words of the dictated text

[Watch shows a ✓ Saved / ✗ Failed result screen based on the phone's reply]
```

## Why this shape (key decisions and the reasoning behind them)

- **No audio handling anywhere in our code.** The Pebble SDK's `Dictation` API does the mic capture and
  sends audio to a cloud recognizer itself, returning plain transcribed text to the watchapp. This is the
  single biggest simplification versus trying to replicate what the Index 01 ring's own audio-recording +
  "online transcription" pipeline does (that lives in `coredevices/mobileapp`'s `experimental`/`index-ai`
  modules — a much bigger, phone-native-audio-handling system we don't need to touch).
  **Risk**: this specific API's cloud backend on Core Devices' current infrastructure is unverified on
  real hardware as of this writing — first thing to spike (see `TODO.md` Phase 1) before building
  anything else.
- **PebbleKit JS over a native companion shim.** `coredevices/mobileapp`'s own README confirms PebbleKit
  JS is real, current, and gives bundled watchapp JS both network access (`XMLHttpRequest`) and
  config-page support — exactly what's needed, no extra app to build.
- **Direct HTTPS webhook, not LiveSync's sync protocol.** The ring's notes reach the vault via Obsidian
  LiveSync's own sync protocol (chunked, E2EE, replicated to MinIO), which n8n then watches via a Local
  File Trigger. Speaking that same protocol from PebbleKit JS would mean reimplementing LiveSync's
  chunking/encryption/replication in watch-app JS — far too complex for "dictate a quick note." A plain
  authenticated HTTPS POST to a dedicated n8n Webhook Trigger is the standard-building-blocks equivalent,
  and it's what the user explicitly chose.
- **n8n is always external, never bundled.** Matches `pebble-index-research-agent`'s own convention
  exactly (its `docker-compose.yml` treats `minio` and `n8n` as pre-existing, joined only via an
  `external: true` Docker network keyed by `N8N_NETWORK_NAME`). Confirmed directly with the user: this
  project's own container(s) should not try to run n8n.
- **Reuse the sibling project's `mcp-obsidian` image rather than building a new one.** It's already
  published (`ghcr.io/delta-43/pebble-index-research-agent/mcp-obsidian:latest`) and already generic
  (`VAULT_NAME`/`VAULT_MOUNT` env vars, no ring/research-specific logic inside it) — no reason to fork or
  rebuild it just for a different vault subfolder.
- **`Watch Inbox/` instead of `Index Inbox/`.** Keeps this app genuinely standalone and decoupled from
  the ring's research-agent trigger, per the user's explicit call ("this app only needs to save the note
  ... research pipeline trigger may or may not be added based on user decision ... can be used standalone
  if wished"). If research-on-watch-notes is ever wanted, it's an additive branch on the same n8n
  workflow, not a rewrite of this app.
- **Config page over hardcoded webhook URL/token.** Required regardless, since this is meant to be a
  *publicly published* watchapp — other installers need to point it at their own n8n, not the author's.
- **No new network exposure required.** The user's n8n is already public (`$N8N_HOST` — see `.env`/
  `.env.example` at repo root; same reverse-proxy setup MinIO uses for LiveSync). Adding a Webhook
  Trigger there is one more authenticated
  path on infrastructure that already exists — not a new service to expose.

## Two deployment paths, one n8n workflow

Both paths import the exact same `n8n/workflows/delta-notes.json`. The only difference is where
`mcp-obsidian` (the tool the workflow calls to actually write the note) comes from:

1. **Standalone** — you don't already run `pebble-index-research-agent`. This repo's own
   `docker/docker-compose.yml` runs just `mcp-obsidian`, bind-mounted to a local vault directory (however
   that vault gets onto the machine — LiveSync, Syncthing, iCloud Drive, or Obsidian running right there
   — out of scope for this project), joined to your own external n8n network. Import the workflow, point
   its `obsidian_create_note` call at this `mcp-obsidian` instance, done.
2. **Integrated** — you already have `pebble-index-research-agent` deployed. Skip this repo's
   docker-compose entirely. Import the same workflow JSON into that existing n8n; it reuses the
   already-running `mcp-obsidian` container (same Docker network n8n already talks to it over). Zero new
   containers.

`docs/SETUP.md` documents both paths explicitly, mirroring how the sibling project's own setup guide is
structured (a linear guide with called-out points where your own values differ from the reference
deployment).

## Repo layout

```
pebble-watch-obsidian-notes/
├── README.md              — Delta Notes overview: what it is, how it works, both deploy paths
├── LICENSE                 — AGPLv3 (already in place)
├── CLAUDE.md                — living summary for AI agents picking this up (see that file)
├── PLAN.md                  — this file
├── TODO.md                  — phase-by-phase build checklist
├── watchapp/
│   ├── src/c/                — one module per responsibility, not one big file:
│   │                              main.c              — window lifecycle, wires modules together
│   │                              status_display.*     — the on-screen text + its revert timer
│   │                              note_transport.*      — AppMessage to/from the phone
│   │                              dictation_handler.*    — the DictationSession
│   │                              mic_icon.*              — tap-to-record icon (PBL_TOUCH only)
│   │                            Every function has an @brief/@param/@return comment (user-supplied
│   │                            style) explaining behavior and edge cases, not just what it's named.
│   ├── src/pkjs/              — PebbleKit JS: AppMessage handler, XHR POST, Clay config page
│   ├── resources/images/      — app_icon.png (25x25 menu icon) + mic_icon.png (touch icon bitmap),
│   │                            both rasterized from source art in ../../design/
│   └── package.json — app UUID, display name "Delta Notes", capabilities, resources.media
├── docker/
│   ├── docker-compose.yml     — standalone path: mcp-obsidian only (reused image), external
│   │                            n8n network (N8N_NETWORK_NAME), no host ports published
│   └── .env.example
├── n8n/
│   └── workflows/delta-notes.json — Webhook Trigger (Header Auth) → obsidian_create_note
├── design/                     — right-sized, committable design source (mic_icon.svg, app icon
│                                  master PNG) — the raw drop-off (assets/, huge originals) is
│                                  gitignored, this is the actual versioned source to edit from
└── docs/
    ├── SETUP.md                — both deploy paths, phase-by-phase like the sibling project
    ├── ARCHITECTURE.md          — the why (this file's content, formalized once built/validated)
    ├── TROUBLESHOOTING.md       — real gotchas hit during the build
    └── STORE_LISTING.md         — app-store submission prep: UUID reservation, icon set,
                                    screenshots, listing description (submission itself stays manual)
```

## Explicitly out of scope (for now)

- Editing/correcting dictated text on the watch beyond the Dictation API's own built-in confirm/retry
  dialog.
- Any wiring into the research agent — the workflow is structured so this is easy to add later, but it's
  not part of this build.
- Bundling n8n, MinIO, or LiveSync in this project's own docker-compose.
- Multiple note types / folders / a watch-side menu — one button, one action, per "simplicity is key."
