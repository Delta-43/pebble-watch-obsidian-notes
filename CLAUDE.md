# CLAUDE.md

Guidance for Claude Code (or any AI coding agent) picking up work on this repository. Written in the
same spirit as the sibling project's own `CLAUDE.md` (`../pebble-index-research-agent/repo/CLAUDE.md`)
— this documents *this specific build's* real decisions and state for continuity across sessions. Read
`PLAN.md` first for the full architecture and rationale; this file is the terse, living summary plus
whatever real-world findings show up once building starts. `TODO.md` tracks phase-by-phase progress.

## What this project is

**Delta Notes** — a Pebble watchapp (targeting the Core Devices **Pebble Time 2**) that lets you dictate
a quick note on your wrist and have it land directly in your Obsidian vault, via a self-hosted n8n
webhook. Sibling project to `pebble-index-research-agent` (the Pebble Index 01 ring → AI research-agent
pipeline) but deliberately simpler and decoupled: no web research, no ring, just watch → text → vault
note. Can run fully standalone, or be wired into an existing `pebble-index-research-agent` deployment.

## Current state (as of this writing)

**Published (2026-09-10): Delta Notes is live on the Pebble Appstore** —
https://apps.repebble.com/c2c541a7bc004712894f8d46. All 10 phases are done; this was the last one. Walked
the actual Core Devices submission portal step-by-step (screen-by-screen, against `docs/STORE_LISTING.md`)
rather than assuming the pre-written checklist covered everything, which surfaced three real gaps the SDK
docs never mentioned: an 80×80 "Small Icon" the repo had no asset for (had 25/144/1024px, nothing at 80),
a separate screenshot per platform at each one's own exact native pixel size rather than one shared set
(basalt/diorite/flint all needed their own 144×168 capture, not just emery's 200×228 and chalk's 180×180),
and an optional 720×320 banner. All three closed with real, committed assets (`design/app_icon_small_80.png`,
per-platform screenshots under `docs/store-assets/screenshots/`, `design/app_banner_720x320.png`) before
submitting, not just noted as follow-up work. `docs/STORE_LISTING.md` has the final, portal-confirmed
version of every field. The Locker icon check from Phase 7 (real phone showed a blank/default icon there)
turned out not to be a bug — confirmed by reading `coredevices/mobileapp`'s own source
(`PbwApp.toLockerEntry()`): a sideloaded app's `LockerEntry` never gets an `iconImageUrl` since that only
ever comes from `appstoreData`, which is `null` for every sideloaded install by design — it was always
going to resolve itself once actually published, which it now has.

Phases 0-5 done, and Phase 6a (end-to-end test of everything *except* the real dictation call) is also
done and genuinely proved, not just code-reviewed: a real scratch vault + real `mcp-obsidian` + real n8n
(workflow imported, Header Auth credential, published/activated) were stood up in throwaway Docker
containers, the emulator was gotten running (needs `libsdl2-2.0-0 libglib2.0-0 libpixman-1-0 zlib1g
libsndio7.0` — see `TODO.md`'s "Local dev environment" section), and — via a temporary test-only bypass
of `dictation_session_start()` (reverted immediately after) — the full watch → AppMessage → PebbleKit JS
→ XHR → n8n Webhook → `obsidian_create_note` path was exercised for real. **Real notes with correct
frontmatter landed in the vault**, confirmed by reading the files back.

That test also found and fixed one real bug (the `Build Note` Set node's expression syntax — bare
`="<expr>"` isn't valid n8n expression syntax, needed `={{ <expr> }}`) and surfaced one known, unfixed,
minor rough edge (a retried AppMessage delivery within the same minute can show "Failed" on the watch
even though the original delivery already saved the note — `obsidian_create_note`'s atomicity prevents
any actual data loss/corruption, it's a cosmetic false-negative only). See `TODO.md` Phase 6a for the
full detail on both.

**Resolved (2026-09-09): dictation works on real Pebble Time 2 hardware.** The user installed the
watchapp for real (`pebble install --phone <ip>` via per-watch LAN Developer Connection — reached from
the Watches list's "⋮" menu + a "Use LAN developer connection" toggle under Settings → Phone →
Connectivity, *not* a generic "Developer" menu, which the public docs describe inaccurately) and
confirmed `dictation_session_start()` returns real transcribed text. This was the single biggest
unverified assumption in the whole project — it's real, not just plausible. Also confirmed: with no
webhook configured, the app correctly shows "Not configured" — the AppMessage leg works on real
hardware too, not just the emulator.

**Real backend now deployed against the user's actual infrastructure (2026-09-09).** For their private
setup, reused the existing `mcp-obsidian` from `pebble-index-research-agent` rather than the standalone
docker-compose — imported a Header Auth credential (id `delta-notes-webhook-token`, header
`X-Auth-Token`) and the workflow (id `delta-notes-watch-inbox`) directly into their real, already-running
`n8n` container via `docker exec ... n8n import:credentials`/`import:workflow`/`publish:workflow`, then
`docker restart n8n`. Confirmed with a real HTTPS POST to `https://$N8N_HOST/webhook/delta-notes` (see
`.env`/`.env.example` at repo root for the real `$N8N_HOST` value — kept out of this file since it's
public) — real note written to the real vault. Coexists cleanly with their existing `Pebble Index →
Research Note` workflow (both show `Activated` after the restart).

**Real incident during this, self-caught and fixed**: testing the *standalone* `docker-compose.yml`
(the public-repo path, unrelated to the private-setup work above) via a literal `docker compose up -d`
recreated/replaced the user's real, already-running `mcp-obsidian` container — because that file's
directory and the sibling project's compose directory are both named `docker/`, and neither set an
explicit project `name:`, so Compose treated them as the same project and saw a service-name collision.
No data loss (stateless service, vault mirror on disk untouched), fully recovered by re-running the
sibling project's own `docker compose up -d mcp-obsidian`. **Root cause fixed**: `docker-compose.yml` now
pins `name: delta-notes` explicitly — never run a `docker compose` command against shared/production
infrastructure without first checking `docker ps -a` for what's already there, project-name collisions
included, regardless of how unrelated the two directories seem.

Only remaining step for the user's own setup: enter the webhook URL + token into the watchapp's Clay
config page on their phone (Locker → tap the app → Settings, since it now has a config page) — full
detail on all of the above in `TODO.md` Phases 6a-6c.

Read `PLAN.md` for the full architecture and rationale; `TODO.md` for the phase-by-phase checklist.

## Key decisions already made (don't re-litigate without new evidence)

- **Device**: Core Devices Pebble Time 2. Has a microphone; classic Pebble SDK **Dictation API**
  (`dictation_session_create()`) is assumed to work via Core Devices' own cloud recognizer backend (the
  same "online transcription" service their official mobile app already uses for the Index 01 ring) —
  **unverified on real hardware, first thing to spike** (see `TODO.md` Phase 1).
- **Phone bridge**: PebbleKit JS, running inside the official `coredevices/mobileapp` (confirmed current
  and supported — it explicitly documents "PebbleKit JS — watchapps can include a JavaScript component
  that runs on the phone... giving watchapps network access and configuration UIs"). No native shim app
  needed.
- **Delivery mechanism**: watch → AppMessage → PebbleKit JS → plain HTTPS POST (`XMLHttpRequest`) to an
  n8n **Webhook Trigger**, protected by n8n's built-in Header Auth (shared secret token). Deliberately
  *not* trying to speak Obsidian LiveSync's own sync protocol (chunking/E2EE/replication) from PebbleKit
  JS — too complex for "dictate a quick note."
- **Config**: webhook URL + auth token are set via an on-phone Pebble config page (not hardcoded) —
  required anyway since this is meant to be a publicly published watchapp other people will install.
- **n8n is never bundled** — same convention as the sibling project. It's always an external prerequisite
  (BYO or use the one from `pebble-index-research-agent`), joined via an `external: true` Docker network
  (`N8N_NETWORK_NAME`), exactly like that project's `docker-compose.yml` does it.
- **Obsidian write tool**: reuse the sibling project's own published `mcp-obsidian` image
  (`ghcr.io/delta-43/pebble-index-research-agent/mcp-obsidian:latest`) rather than building a new one —
  it's already generic (`VAULT_NAME`/`VAULT_MOUNT` env vars). Standalone mode just points it at a local
  vault directory; no MinIO/LiveSync/vault-mirror needed for standalone use.
- **Two deployment paths, one n8n workflow JSON** (`n8n/workflows/delta-notes.json`):
  - **Standalone**: this repo's `docker/docker-compose.yml` runs `mcp-obsidian` only, joined to your own
    (external) n8n network; import the workflow.
  - **Integrated**: if `pebble-index-research-agent` is already deployed, skip this repo's docker-compose
    entirely — just import the same workflow JSON into that existing n8n and reuse its already-running
    `mcp-obsidian`.
- **Note destination**: new `Watch Inbox/` folder (not `Index Inbox/` — deliberately decoupled from the
  ring's research pipeline), tags `[pebble_watch, quick_note]`, title = timestamp (with seconds) + fixed
  "Watch Note" suffix (changed 2026-09-10 from timestamp + first few words of the dictated text, so
  titles stay predictable/sortable rather than content-derived — see TODO.md's "Post-launch" entry).
  Wiring in the research agent later (if ever wanted) is an additive branch on the workflow, not a
  rewrite — e.g. LLM-based tag autogeneration was deliberately left out of *this* workflow (stays
  non-LLM/deterministic per Phase 4's intent, see `TODO.md`'s "Post-launch" entry), but anyone wanting
  heavier note processing can point `Watch Inbox/` notes at `pebble-index-research-agent`'s own pipeline
  instead, rather than adding an LLM call here.
- **License**: AGPLv3 (already in `LICENSE`), matching the sibling project.
- **Repo name**: staying `pebble-watch-obsidian-notes` (not renamed to `delta-notes`) — only the app's
  display name/branding is "Delta Notes".
- **Icon**: minimalist pencil-tip (writing end) silhouette.
- **Publishing**: built with a real `appinfo.json` (UUID, name, icons), `docs/STORE_LISTING.md` covers the
  submission prep, and the app is **now actually published** —
  https://apps.repebble.com/c2c541a7bc004712894f8d46.

## Working conventions for this repo

- Same spirit as the sibling project: this is published documentation of a real, working setup, not just
  code. Keep `docs/SETUP.md` and `docs/TROUBLESHOOTING.md` updated as things get built and validated —
  don't leave stale/aspirational docs.
- `docker/docker-compose.yml` must stay deployable standalone (don't assume it's the only compose file on
  the server) and must never bundle n8n.
- Never commit real secrets — `docker/.env.example` documents required variables; the real `.env` stays
  gitignored.
- Once the n8n workflow is built in the n8n UI, export it as JSON into `n8n/workflows/` so it's versioned.
- Spike the risky/unverified assumption first (Dictation API actually working on real Pebble Time 2
  hardware) before investing in the rest of the build — see `TODO.md` Phase 1. If it turns out not to
  work as assumed, update this file's "Key decisions" and `PLAN.md` together, and record the finding in
  `docs/TROUBLESHOOTING.md` once that file exists.
- Prefer validating against real hardware/a real deployment over trusting SDK docs alone — several
  Pebble SDK doc pages online are over a decade old and may not reflect Core Devices' current backend.
- **`watchapp/src/c/` is one module per responsibility, not one big file** (user request, deliberate —
  see `TODO.md`'s refactor entry): `main.c` only does window lifecycle + wiring; `status_display`,
  `note_transport`, `dictation_handler`, `mic_icon` each own one clearly-scoped job with a `.h` exposing
  only what other modules need. Keep new watchapp functionality in this shape rather than growing any
  one file back into a do-everything file. Every function (including private/static helpers) gets a
  Doxygen-style `@brief`/`@param`/`@return` comment explaining *why*/edge cases, not just restating the
  signature — match the density/tone already in these files, written explicitly to be readable by a
  beginner, not just "technically documented."
