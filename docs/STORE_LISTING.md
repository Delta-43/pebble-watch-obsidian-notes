# App store listing — Delta Notes

Phase 9 deliverable: everything needed to submit Delta Notes to the Core Devices/Rebble app store, up to
the actual submission click-through — that step needs the user's own developer account and is
deliberately not automated here. See [`TODO.md`](../TODO.md) Phase 9/10 for where this sits in the
overall build.

**A note on sourcing**: Core Devices' publishing docs (`developer.repebble.com`) are thin on the actual
submission portal — the SDK's own `app-metadata` guide covers manifest fields, but nothing fetchable
describes the portal's upload UI (exact store-icon pixel size, screenshot count limits, category
taxonomy). Rather than guess those, this doc separates what's **verified** (the manifest, our own
assets) from what's an **open item for the portal itself** at submission time.

## App identity

Already set in [`watchapp/package.json`](../watchapp/package.json)'s `pebble` block — nothing here needs
creating, only confirming it matches what gets typed into the portal:

| Field | Value |
|---|---|
| Display name | Delta Notes |
| npm-safe short name | `delta-notes` |
| UUID | `6bc558aa-cf0a-4cf6-81bb-3dcdf38599fd` |
| Author | Delta-43 |
| Version | `1.0.0` (already `major.minor.0`, matches the SDK's required format) |
| SDK version | 3 |
| Type | watchapp (not a watchface) |
| Target platforms | basalt, chalk, diorite, emery, flint |
| Capabilities | `configurable` (added this phase — the app has a Clay config page; see below) |

**`capabilities: ["configurable"]` added to `package.json` this phase.** The SDK's `app-metadata` guide
lists `configurable` as a real, recognized capability value for apps with a settings page. Delta Notes
already has one (Clay-generated, confirmed showing up as a real **Settings** button in the Locker on
real hardware — see `CLAUDE.md`, 2026-09-09); this capability was missing from the manifest despite the
feature existing. Rebuilt and confirmed it propagates correctly into `build/appinfo.json`.

**UUID reservation**: the UUID above was generated once, at project init, by `pebble` CLI tooling, and
must never be regenerated (changing it would make store updates register as a new, different app). Open
item: whether the Core Devices developer portal requires a separate "reserve this UUID" step before
upload, or just registers whatever UUID is embedded in the uploaded `.pbw` — not documented anywhere
fetchable; confirm on the actual portal at submission time.

## Icons

Two different icons, already committed, sized for two different jobs — don't confuse them:

- **In-app menu icon** (shown in the watch's own app list): [`watchapp/resources/images/app_icon.png`](../watchapp/resources/images/app_icon.png),
  exactly 25×25px. Confirmed against current SDK docs in Phase 7 — 25×25 is the *required and enforced*
  size for every platform ("icons that are larger will be rejected by the SDK"), not a choice.
- **Store listing icon**: [`design/app_icon_master_1024.png`](../design/app_icon_master_1024.png),
  1024×1024px, the same pencil-tip artwork at full resolution. Use this as the upload if the portal asks
  for a high-res store icon; [`design/app_icon_preview_144.png`](../design/app_icon_preview_144.png)
  (144×144, already this repo's own README logo) if it asks for something mid-size instead.
  Open item: the exact pixel size the store listing icon field actually wants isn't specified anywhere
  fetchable from the current SDK docs — Core Devices' publishing flow is newer than most of what's
  indexed. Have both sizes ready and match whatever the upload field asks for.

**If a sideloaded install shows a blank/default icon in the phone app's Locker, that's expected, not a
bug** — confirmed by reading `coredevices/mobileapp`'s own source. A sideloaded app's `LockerEntry` (see
`PbwApp.toLockerEntry()` in `libpebble3/.../disk/pbw/PbwApp.kt`) only ever populates `pbwIconResourceId`
(the on-**watch** icon — this repo's `APP_ICON`, already confirmed correct in Phase 7) and leaves
`iconImageUrl` (what the phone's own Locker *list* actually renders from) at its `null` default, because
that field only ever comes from `appstoreData`, which is unconditionally `null` for every sideloaded
entry. This isn't something this repo's assets or manifest can fix — it resolves itself once the app is
actually published and the store backend has a hosted icon URL to serve.

## Screenshots

Four curated shots, captured on the emulator (see [`docs/SETUP.md`](SETUP.md) for the real-hardware
validation these already match) and committed under
[`docs/store-assets/screenshots/`](store-assets/screenshots/) — a *curated* set for the store listing,
not the full state-by-state review set (that lives only in this session's scratch, per the earlier
review pass):

| File | Shows | Why this one |
|---|---|---|
| `emery_idle.png` | Idle screen, "Let's take a note. (Press SELECT)" | The first thing every installer sees — sets the tone before they've even used it |
| `emery_saved.png` | "Saved!" result | The golden path actually working |
| `emery_failure.png` | "No phone connection" result | Shows the app fails legibly instead of silently — a genuine differentiator worth putting in the listing itself |
| `chalk_idle.png` | Idle screen on chalk (round, Pebble Time Round) | Signals multi-shape support instead of only showing Pebble Time 2 |

All four are native 200×228 (emery) / 180×180 (chalk) captures, unscaled, unedited. Re-capture with
`pebble screenshot --emulator <platform> --vnc <output>.png` (see `docs/SETUP.md`'s local dev environment
notes) if the watchapp's screens change before submission. Open item: the portal's exact screenshot count
limit/aspect-ratio requirements aren't documented anywhere fetchable — these four are a reasonable
minimum, not a confirmed maximum.

## Store description copy

**Short description** (leads with the hard requirement — see the reasoning below; don't cut this for
length, it's the whole point):

> Dictate a note on your wrist and have it saved straight into your own self-hosted Obsidian vault.
> Requires your own n8n instance — not a standalone note-taking app.

**Full description:**

> Delta Notes turns your Pebble into a voice inbox for Obsidian. Press SELECT (or tap the mic icon on a
> touchscreen model), say your note, and it lands as a plain Markdown file in your vault's `Watch Inbox/`
> folder within seconds — no phone typing, no manual copy-paste.
>
> **This requires infrastructure you run yourself**: a self-hosted [n8n](https://n8n.io) instance and an
> Obsidian vault reachable from wherever you run the companion `mcp-obsidian` tool (Docker Compose
> included). If you don't already run n8n, budget 15–20 minutes for the one-time setup —
> [full guide here](https://github.com/Delta-43/pebble-watch-obsidian-notes/blob/main/docs/SETUP.md).
> If you're not planning to self-host a backend, this app won't do anything useful for you.
>
> What it does do, once set up: dictation via your watch's built-in cloud speech recognizer, a direct
> authenticated webhook to your n8n (no third-party service in between), and a note that syncs to your
> other devices the same way any other vault change does. Open source, AGPLv3 licensed.

Why the requirement leads the copy rather than living only in the setup docs: a store browser who installs
expecting a working note-taking app out of the box — the requirement buried past install — is the
predictable source of "doesn't work" reviews for an audience that was never going to be able to use this
app in the first place. Putting it in the first line filters for the actual audience (people who already
self-host or are willing to) before they install, not after.

## Category

Likely **Tools** (or the closest current equivalent in whatever taxonomy the portal presents) — open item,
not confirmed against the actual current portal, which wasn't reachable from here.

## Submission checklist (the user's own click-through)

- [ ] Confirm/create a Core Devices developer account
- [ ] Build the release `.pbw`: `pebble build` (produces `watchapp/build/watchapp.pbw`)
- [ ] Upload the `.pbw`, confirm the UUID above is what registers
- [ ] Upload `design/app_icon_master_1024.png` (or `_preview_144.png`, per what the field asks for)
- [ ] Upload the four screenshots under `docs/store-assets/screenshots/`
- [ ] Paste in the short + full description above
- [ ] Pick the closest category
- [ ] Submit for review

## Not done in this phase

- Actual submission is Phase 10, and stays manual — it needs the user's own developer account
  credentials, which this repo/session has no access to and shouldn't.
