# Delta Notes

[![License: AGPL v3](https://img.shields.io/badge/license-AGPL%20v3-blue.svg?style=plastic)](LICENSE)
[![Maintenance](https://img.shields.io/badge/Maintained%3F-yes-green.svg?style=plastic)](https://github.com/Delta-43/pebble-watch-obsidian-notes/graphs/commit-activity)
[![Platform: Pebble](https://img.shields.io/badge/platform-Pebble-e0182a.svg?style=plastic)](https://developer.repebble.com/)
[![Docker Compose](https://img.shields.io/badge/docker-compose-2496ED.svg?style=plastic&logo=docker&logoColor=white)](docker/docker-compose.yml)
[![Built with n8n](https://img.shields.io/badge/built%20with-n8n-EA4B71.svg?style=plastic)](https://n8n.io/)
[![GitHub last commit](https://img.shields.io/github/last-commit/Delta-43/pebble-watch-obsidian-notes.svg?style=plastic)](https://github.com/Delta-43/pebble-watch-obsidian-notes/commits/main)

Dictate a quick note on your **Pebble Time 2** and have it land directly in your Obsidian vault as a
plain Markdown note — no phone typing, no manual copy/paste. Sibling project to
[`pebble-index-research-agent`](https://github.com/Delta-43/pebble-index-research-agent) (the Pebble
Index 01 ring → AI-researched-note pipeline), but deliberately much simpler: no web research, no ring,
just watch → text → vault note. Works standalone, or wired into that project's existing backend.

> **Status: fully validated end-to-end on real hardware** — dictation, the phone bridge, the webhook,
> and the note landing in a real vault have all been confirmed on a real Pebble Time 2 (see
> [Project Status](#project-status)).

**→ [`docs/SETUP.md`](docs/SETUP.md) has the full step-by-step deployment guide.** This README covers
the *what* and *why*; `docs/SETUP.md` is what you actually follow to build and deploy it yourself.

## Contents

- [How it works](#how-it-works)
- [Why this design](#why-this-design)
- [Components](#components)
- [Project status](#project-status)
- [Documentation](#documentation)
- [Prerequisites](#prerequisites)
- [License](#license)

## How it works

```
[Pebble Time 2]
  press SELECT, or tap the mic icon on the touchscreen
  → native on-watch dictation UI (Core Devices' cloud speech recognizer)
  → transcribed text sent to the phone over Bluetooth

[Pebble mobile app, running this watchapp's bundled JavaScript]
  → HTTPS POST { text, timestamp } to your own n8n webhook (Header Auth token)
  → relays the result (saved / failed) back to the watch

[n8n]
  Webhook Trigger → build the note (title, tags, frontmatter) → obsidian_create_note
  → writes into "Watch Inbox/" in your vault, tags [pebble_watch, quick_note]

[Watch shows "Saved!" or a short failure reason]
```

The new note syncs to your phone/desktop the same way any other vault change does — this project
doesn't touch sync itself, it only writes a file into whatever vault directory you point it at.

## Why this design

- **No audio handling anywhere in this project's own code.** The Pebble SDK's Dictation API does the
  mic capture and cloud transcription itself; the watchapp only ever handles plain text.
- **A direct, authenticated webhook — not Obsidian LiveSync's own sync protocol.** Speaking LiveSync's
  chunked/E2EE replication protocol from watch-side JavaScript would be far more complex than "dictate a
  quick note" warrants. A plain HTTPS POST to an n8n Webhook Trigger is the standard-building-blocks
  equivalent.
- **No AI agent in the note-saving path.** Saving a dictated note is a deterministic action, not a
  reasoning task — the workflow calls `obsidian_create_note` directly via n8n's plain MCP Client node,
  not through an LLM-backed agent (unlike the research pipeline, which genuinely needs one).
  Deliberately decoupled from that research pipeline entirely: notes land in their own `Watch Inbox/`
  folder, not the ring's `Index Inbox/`, so wiring research into watch notes later (if ever wanted) is
  an additive branch, not a rewrite.
- **n8n is always external, never bundled.** Same convention as the sibling project — this project's
  own `docker-compose.yml` never tries to run n8n itself, only the pieces specific to it.
- **Touch support is capability-gated, not platform-gated.** The tap-to-record mic icon is compiled in
  wherever `PBL_TOUCH` is defined — a hardware capability check, not a check for "Pebble Time 2"
  specifically — so any future Core Devices touch-capable watch picks it up automatically.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full reasoning, including what was tried,
what broke, and how each real finding was verified rather than assumed.

## Components

| Component | Project | Role |
|---|---|---|
| Watchapp | this repo, `watchapp/` | C app (dictation, result screen, tap-to-record icon) + bundled PebbleKit JS (webhook call, on-phone config page) |
| Obsidian tool | [`StevenStavrakis/obsidian-mcp`](https://github.com/StevenStavrakis/obsidian-mcp) | MCP server that writes the note directly onto the vault directory — reused from the sibling project's own published image, not rebuilt |
| Orchestration | [n8n](https://n8n.io/) | Webhook Trigger → build note → `obsidian_create_note`, no LLM involved |
| Config UI | [Clay](https://github.com/pebble-dev/clay) | Generates the watchapp's on-phone settings page (webhook URL + auth token) — no hosting required |

## Project status

Fully validated end-to-end on real hardware, not just in an emulator: dictation via Core Devices' cloud
recognizer, the watch→phone AppMessage bridge, the phone's webhook call, and a real note landing in a
real Obsidian vault have all been confirmed on an actual Pebble Time 2. Touch (tap-to-record) has been
confirmed working on real hardware too. See [`TODO.md`](TODO.md) for the detailed, phase-by-phase
validation history — including two real bugs caught and fixed along the way, and one real deployment
incident (self-caught, fixed, documented) — if you want the full evidence behind that claim.

## Documentation

| Doc | What's in it |
|---|---|
| [`docs/SETUP.md`](docs/SETUP.md) | Step-by-step deployment guide — building the watchapp, both backend deployment paths, wiring up n8n, configuring the watchapp itself |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | The *why* behind every design decision, and what was verified (and how) rather than assumed |
| [`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md) | Real errors hit while building this, with exact causes and fixes — check here first if something breaks |

## Prerequisites

- A mic-equipped Pebble watch (Pebble Time, Time Steel, Time Round, Pebble 2, Pebble 2 Duo, or Core
  Devices' Pebble Time 2), paired with the [Pebble mobile app](https://github.com/coredevices/mobileapp)
- An Obsidian vault, reachable as a plain directory from wherever you run `mcp-obsidian` — how it gets
  there (Self-hosted LiveSync, Syncthing, iCloud Drive, or Obsidian just running locally) is outside this
  project's scope
- Docker Engine + the Compose plugin
- An existing, self-hosted n8n instance

## License

[GNU AGPLv3](LICENSE) — matches the sibling project: this is self-hosted software, and AGPL's
network-use clause means anyone running a modified version of the backend as a service for others must
also share their modified source, closing the "SaaS loophole" plain GPL leaves open.

---

<p align="center">
  <a href="https://github.com/Delta-43/pebble-watch-obsidian-notes/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=Delta-43/pebble-watch-obsidian-notes" alt="Contributors" />
  </a>
</p>

<p align="center"><sub>Licensed under <a href="LICENSE">GNU AGPLv3</a>.</sub></p>
