# TODO.md — Delta Notes build phases

Spike the riskiest assumption first, same methodology as `pebble-index-research-agent`. Update the
checkboxes as work lands; when a phase surfaces a real gotcha, record it in `docs/TROUBLESHOOTING.md`
(create it once Phase 6 starts) and reflect any changed assumption back into `PLAN.md`/`CLAUDE.md`.

- [x] **Phase 0 — repo scaffolding**
  `watchapp/`, `docker/`, `n8n/workflows/`, `docs/` created. `watchapp/` scaffolded via
  `pebble new-project watchapp --c --javascript --ai` (pebble-tool 5.0.40, SDK 4.33.1, installed into a
  scratch venv for this session — not committed to the repo; whoever builds this for real needs their
  own `pebble-tool` install, see `docs/SETUP.md` once written).

- [x] **Phase 1 (code) — watchapp C: dictation → AppMessage → result screen**
  `watchapp/src/c/watchapp.c` written: SELECT → `dictation_session_start()` (buffer 512B, confirmation
  dialog enabled) → on success, sends `noteText` to the phone via AppMessage → waits (15s timeout) for a
  `resultStatus`/`resultMessage` reply → shows a result screen, auto-returns to idle after 3s. **Builds
  clean with zero warnings** across basalt/chalk/diorite/emery/flint (`pebble build`, confirmed in this
  session). `targetPlatforms` trimmed to mic-equipped devices only (dropped aplite/gabbro).
  **Still unverified**: real Dictation cloud-recognizer behavior on actual Pebble Time 2 hardware — the
  sandbox this was built in has no SDL2 (no root to install it) so the emulator can't run, and dictation
  fundamentally needs a real phone + Core Devices' cloud backend anyway. **This is the first thing to
  test on real hardware** before trusting anything downstream.

- [x] **Phase 2 (code) — watch → phone via AppMessage**
  Folded into Phase 1 above (`noteText` out, `resultStatus`/`resultMessage` in) rather than done as a
  separate throwaway spike, since the message-passing code is trivial once the dictation callback exists.

- [x] **Phase 3 (code) — PebbleKit JS networking + config page**
  `watchapp/src/pkjs/index.js`: `appmessage` listener → `XMLHttpRequest` POST (JSON body
  `{text, timestamp}`, `X-Auth-Token` header) → relays `resultStatus`/`resultMessage` back to the watch.
  Config via **Clay** (`@rebble/clay`, installed as a real Pebble package) instead of a hand-hosted HTML
  page — current recommended approach per developer.repebble.com, needs no hosting at all.
  `src/pkjs/config.json` defines two fields (webhook URL, auth token); `autoHandleEvents: false` so Clay
  stores them in phone-side `localStorage` only (the watch itself never needs them). Bundles clean
  (`pebble build`'s `merge_js` step succeeded, confirming `require()` resolution for both Clay and
  `config.json`). **Still unverified**: real XHR behavior from inside `coredevices/mobileapp` on an
  actual phone (iOS vs Android networking differences noted as a risk in `PLAN.md`, not yet tested).

- [x] **Phase 4 — n8n workflow**
  `n8n/workflows/delta-notes.json`: Webhook Trigger (`POST /webhook/delta-notes`, Header Auth,
  `responseMode: lastNode` so the phone's response reflects whether the note actually got written) →
  Set node (builds `Watch Inbox/<timestamp> - <first few words>.md` + frontmatter content) → **MCP
  Client** node (`@n8n/n8n-nodes-langchain.mcpClient`, *not* MCP Client Tool/AI Agent — deliberately no
  LLM in this path, it's a deterministic save) calling `obsidian_create_note` directly on `mcp-obsidian`.
  Node types/params/versions confirmed against real n8n source (spun up a throwaway local n8n container
  in this session to read `dist/nodes/mcp/**` and `dist/nodes/Webhook/**`, and to inspect
  `StevenStavrakis/obsidian-mcp`'s actual `obsidian_create_note` zod schema on GitHub — matches the `@2`
  version pinned in the sibling project's own Dockerfile). **The JSON was actually imported into a real
  n8n instance via `n8n import:workflow` in this session and succeeded** — strong validation the schema
  is right, though full runtime correctness (a real webhook POST → real note written) is still Phase 6.
  Credential intentionally left unattached in the committed JSON (never commit secrets) — attach the
  Header Auth credential manually after import, per convention.

- [x] **Phase 5 — docker-compose (standalone path)**
  `docker/docker-compose.yml`: `mcp-obsidian` only, reusing
  `ghcr.io/delta-43/pebble-index-research-agent/mcp-obsidian:latest` (confirmed pullable, publicly, in
  this session — no new image built), bind-mounted to `VAULT_PATH`, joined to an `external: true` n8n
  network (`N8N_NETWORK_NAME`, same pattern as the sibling project). `docker/.env.example` documents
  required vars. No host ports published. `docker compose config` validated clean in this session.

- [x] **Phase 6a — end-to-end test of everything except real dictation (done, this sandbox)**
  Installed the watchapp to the `pebble` emulator (needs `libsdl2-2.0-0 libglib2.0-0 libpixman-1-0
  zlib1g libsndio7.0` — see "Getting the emulator running" below) and confirmed the idle screen renders
  correctly. Confirmed empirically that Dictation cannot be exercised here at all: `pypkjs` (the
  emulator's real PebbleKit JS runtime) has zero dictation-simulation code, and `dictation_session_start()`
  just hangs waiting for a phone/cloud recognizer that doesn't exist in this environment — expected,
  matches `PLAN.md`'s risk assessment, not a bug.
  To validate everything *downstream* of dictation for real, temporarily patched in a test-only UP
  button (bypassing `dictation_session_start()` with a canned string) and hardcoded test webhook
  config in `index.js` (both reverted immediately after — see git history if you need the exact diff).
  Stood up a real scratch vault + `mcp-obsidian` + a real n8n (imported `delta-notes.json`, Header Auth
  credential, published/activated) in throwaway Docker containers, all torn down after.
  **Result: real notes were written to the vault with correct frontmatter/tags/content**, via the actual
  watch → AppMessage → PebbleKit JS → XHR → n8n Webhook (Header Auth) → Set → MCP Client →
  `obsidian_create_note` path — confirmed by reading the files back, not just trusting a 200 response.
  Two bugs found and fixed by this test (both were real bugs, not test artifacts):
    - The n8n workflow's `Build Note` Set node used bare `="<expr>"` syntax instead of `={{ <expr> }}`
      — n8n doesn't evaluate the former as an expression, so the literal unparsed text was being sent
      as the note path/content, tripping `obsidian_create_note`'s `PATH_REJECTED` validation. Fixed in
      `n8n/workflows/delta-notes.json` (see the `{{ }}`-wrapped `value` fields).
    - A throwaway-test-only issue, not a repo bug: the scratch `mcp-obsidian` container needed a Docker
      network alias matching the hostname (`mcp-obsidian`) the workflow's `endpointUrl` hardcodes —
      noted here only because the same requirement applies to real deployments (see
      `docker/docker-compose.yml`'s `container_name`).
  **One real, minor finding, not fixed (documented instead)**: under a lost/retried AppMessage delivery
  (observed here as emulator-serial packet loss — `[PHONESIM] Exception decoding QemuInboundPacket.footer`
  in the logs, but the watch's AppMessage protocol retries the same way over a flaky real BLE link), a
  duplicate delivery within the same minute hits `obsidian_create_note`'s atomicity guard
  (`ALREADY_EXISTS`) and correctly does *not* corrupt/overwrite the already-saved note — but the watch
  shows "Failed to save note" on that duplicate even though the original delivery actually succeeded.
  Cosmetic (no data loss), not worth fixing for v1; revisit if it turns out to be common on real BLE.

- [x] **Phase 6b — the one thing that still needed your real Pebble Time 2 + phone**
  **Confirmed on real hardware (2026-09-09): dictation works.** Installed via `pebble install --phone
  <ip>` (LAN Developer Connection, enabled per-watch from the Watches list's "⋮" menu, not a global
  "Developer" settings screen — the generic docs page was stale on this point), pressed SELECT on a real
  Pebble Time 2, and `dictation_session_start()` returned real transcribed text via Core Devices' cloud
  recognizer. This was the single biggest unverified assumption in the whole project (`PLAN.md`) — it's
  real. Confirmed on the same real hardware: with no webhook configured, the app correctly showed
  "Not configured" after dictation — proves the AppMessage leg (watch → phone → PebbleKit JS) also works
  on real hardware, not just the emulator.

- [x] **Phase 6c — real backend deployed and tested against the user's actual n8n/vault**
  Reused the user's existing `mcp-obsidian` (from `pebble-index-research-agent`, already running) — no
  standalone docker-compose needed for their private setup. Imported a Header Auth credential + the
  `delta-notes.json` workflow directly into their real, already-running `n8n` container (`docker exec
  n8n n8n import:credentials` / `import:workflow` / `publish:workflow`, then `docker restart n8n` to
  register the webhook — same restart-required quirk noted in the sibling project's own `CLAUDE.md`).
  Confirmed via a real HTTPS POST to `https://$N8N_HOST/webhook/delta-notes` (see `.env`/`.env.example`
  at repo root for the real `$N8N_HOST` value — kept out of this file since it's public): real note
  written to the real vault, read back to confirm exact content. Real credential id: `delta-notes-webhook-token`
  (header `X-Auth-Token`); real workflow id: `delta-notes-watch-inbox`. Coexists cleanly with the
  existing `Pebble Index → Research Note` workflow — both showed `Activated` on the same restart.
  **Real incident during this phase, self-caught and fixed**: the standalone `docker-compose.yml`
  (public-repo path) was first tested by literally running `docker compose up -d` from its own
  directory — but that directory is named `docker/`, same as the sibling project's own compose
  directory, and neither file set an explicit project `name:`. Compose defaulted both to project name
  "docker", saw a service-name collision (`mcp-obsidian` defined in both), and recreated/replaced the
  user's real, already-running `mcp-obsidian` container with this project's (different volume, pointed
  at a scratch test vault) — silently, with only a generic "orphan containers" notice for unrelated
  services, no explicit warning about the one it was about to touch. No data loss (the service is
  stateless; the real vault mirror on disk was untouched) — recovered by re-running the sibling
  project's own `docker compose up -d mcp-obsidian`, which recreated it correctly from its own file.
  **Fixed the root cause**: `docker/docker-compose.yml` now sets an explicit top-level `name: delta-notes`
  so this can never happen regardless of parent directory naming — re-tested clean after the fix (no
  orphan warning, real container's uptime uninterrupted). See the comment above `name:` in that file.
  Still open: the actual watch-side config (Clay page, entering the real URL/token) hasn't been done by
  the user yet — that's the very last step before a real dictated note has gone through the full path.

- [x] **Confirmed by the user (2026-09-10): full path works end-to-end on real hardware.** Real
  dictation → real webhook → real note, on the user's actual Pebble Time 2 and vault. The project's core
  premise is proven, not just plausible.

- [x] **Touch support (added post-launch, user request): mic icon for tap-to-dictate**
  Pebble Time 2 has a touchscreen; SELECT-only felt like it'd age poorly given Core Devices is clearly
  leaning into touch on new hardware. Added a mic icon at the bottom of the screen that does the same
  thing as SELECT, gated entirely by `#ifdef PBL_TOUCH` (a compile-time *capability* define, not a
  platform-name check) — genuinely future-proof: applies automatically to any future Core Devices
  touch-capable platform, zero effect on today's non-touch platforms. Confirmed via symbol inspection
  (`arm-none-eabi-nm`) that touch code is compiled into the `emery` binary only, absent from
  basalt/chalk/diorite/flint — interesting/useful finding along the way: `flint` (Pebble 2 Duo) does
  *not* have `PBL_TOUCH`, so it doesn't have a touchscreen either, despite being the other new Core
  Devices hardware.
  Implementation: plain `TouchService` + manual `grect_contains_point()` hit-testing against the icon's
  layer frame (not a tap `GestureRecognizer` — simpler, and this only ever needs one tappable region,
  not gesture disambiguation). Icon is hidden if `touch_service_is_enabled()` is false (touch present but
  user-disabled) — SELECT still always works regardless. Shares the exact same `prv_try_start_dictation()`
  path already proven correct via SELECT, so no new state-machine risk.
  **Verified**: builds clean on all platforms; installed to the `emery` emulator (which, unlike basalt,
  actually launches for this SDK) and confirmed the icon renders correctly — well-positioned, reads
  clearly as a mic, no clipping. **Not verified**: the actual tap-to-trigger interaction. Tried sending a
  real click via VNC (`vncdotool`) at the icon's exact coordinates — no visible effect (screen unchanged,
  no evidence the emulated touch controller is wired to VNC pointer input for this QEMU machine model).
  Inconclusive, not a negative result — same category of limitation as dictation itself: needs the user's
  real Pebble Time 2 to confirm tapping the icon actually starts dictation. Code-review confidence is
  high (well-established, standard hit-testing pattern, reuses already-proven trigger logic), but
  untested-on-hardware is untested-on-hardware.

- [x] **Confirmed by the user (2026-09-10) on real hardware: tap-to-trigger works.** Resolves the "not
  verified" item above — the emulator's VNC-click inconclusiveness turned out to just be an emulator
  limitation, not a real bug.

- [x] **Icon iteration 1 (superseded): hand-drawn Graphics-primitive approximation of a mic glyph**
  Got reasonably close (outlined capsule via nested rounded-rects, cradle via `graphics_fill_radial`,
  stand) but user correctly judged it "not accurate" against their actual reference design. Superseded
  by iteration 2 below, which uses the real artwork instead of approximating it.

- [x] **Icon iteration 2 (current): real bitmap assets from user-supplied SVG + PNG**
  User provided the actual design files (added to `assets/`, gitignored — that's the raw drop-off, not
  the versioned source): `mic_bitmap.svg` (the touch icon's real vector artwork) and a 9600x9600 PNG
  app logo (blue circle, pencil, red squiggle — the "pencil tip" mark from `PLAN.md`'s original branding
  call). Both are now real bitmap resources, not hand-drawn approximations:
  - Rasterized `mic_bitmap.svg` → `watchapp/resources/images/mic_icon.png` (52x52, alpha-transparent)
    using `cairosvg` (pip-installable, no system `rsvg-convert`/root needed — verified the SDK's own
    build tooling *can* take raw `.svg` resource files directly via `rsvg-convert` internally, but the
    public docs don't document that path and it'd add a system dependency for every future contributor,
    so pre-rasterizing to a committed PNG was the more portable choice).
  - Downsampled the 9600x9600 logo → `watchapp/resources/images/app_icon.png` at 25x25 (the standard
    Pebble menu/launcher icon size — confirmed via the real app-metadata docs, which specify 25x25 for
    the equivalent AppGlance icon and give no reason to expect the classic menuIcon differs) using
    Lanczos resampling. Still legible at that size (circle + pencil silhouette reads fine; the fine
    squiggle detail is naturally lost, which is expected/fine at 25px).
  - Kept right-sized design sources under `design/`: `mic_icon.svg` (copy of the original), a 1024x1024
    master PNG for the logo (the original 9600x9600/15MB file itself was never going in git), and a
    144x144 preview. `design/app_icon_master_1024.png` is what Phase 9's store listing assets should be
    cropped/exported from later, not the raw `assets/` drop-off.
  - `package.json`: `APP_ICON` (`menuIcon: true`) and `MIC_ICON` bitmap resources added.
  - `watchapp.c`: mic icon layer now does `gbitmap_create_with_resource(RESOURCE_ID_MIC_ICON)` +
    `graphics_draw_bitmap_in_rect` with `GCompOpSet` compositing (respects the PNG's own alpha) instead
    of the hand-drawn primitives from iteration 1 — all of that drawing code is gone now.
  **Verified**: builds clean, resource compilation succeeded for both bitmaps across all platforms,
  installed to the `emery` emulator and confirmed the rendered icon is now a pixel-accurate reproduction
  of the user's actual SVG (not an approximation). **Not verified in this sandbox**: how `APP_ICON`
  actually looks in the phone app's Locker/launcher list — that needs the user's real phone, same
  category of limitation as everything else that needs real hardware/a real phone UI.

- [x] **Refactor: split the single watchapp.c into one module per responsibility (user request)**
  `watchapp.c` had grown to ~280 lines mixing five distinct jobs (display, AppMessage transport,
  dictation, touch input, window lifecycle) in one file with no function-level documentation. Split into
  `main.c` (window lifecycle + wiring only) + four focused modules, each with a `.h`/`.c` pair and a
  module-level comment explaining its one job: `status_display` (owns the text layer + its
  revert-to-idle timer), `note_transport` (owns the "busy sending" state + AppMessage handlers),
  `dictation_handler` (owns the `DictationSession`), `mic_icon` (owns the touch icon, `PBL_TOUCH`-gated
  as before). Every function now has an `@brief`/`@param`/`@return` doc comment in the style the user
  provided as a reference — including private/static helpers, not just the public API — explaining
  *why*, not just restating the signature (e.g. why `dictation_handler_start()` calls
  `status_display_cancel_pending_revert()` specifically rather than `status_display_show_idle()`).
  **Caught one real behavioral bug during the refactor, before it shipped**: the original single-file
  `default` case in the dictation callback (user cancels/no-speech) called a function that did two
  things at once — reset the displayed text to the idle prompt *and* cancel the pending revert timer.
  Splitting into separate `status_display_show_idle()` (does both) vs
  `status_display_cancel_pending_revert()` (cancel only) functions, it would have been easy to reach for
  the wrong one; caught by re-deriving the original behavior line-by-line rather than assuming the
  refactor was equivalent, and fixed by using `status_display_show_idle()` in that specific spot — using
  `cancel_pending_revert()` there would have left a stale result message on screen indefinitely after a
  cancelled retry (e.g. retry from "Failed to save note", then cancel the new dictation — the old
  "Failed" text would never clear).
  **Verified**: clean build (zero warnings) across all 5 platforms; reinstalled to the `emery` emulator
  and confirmed the idle screen and SELECT→dictation-UI behavior are pixel-identical to pre-refactor.

- [x] **Phase 7 — watch UI polish + icon assets**
  Verified for real via the emulator rather than assumed from the code: built, installed, and
  screenshotted the idle screen, "Saved!", and a 3-line "Failed:\nNo phone\nconnection" result on
  **emery** (Pebble Time 2, the primary target, touch), **chalk** (round — the layout risk case), and
  **flint** (Pebble 2 Duo). All render cleanly with no text clipping. Used the same temporary
  `select_click_handler()` bypass technique as Phase 6a (preview the result screens without a real
  dictation call, reverted immediately after — `git diff` confirmed clean before moving on).
  The chalk result screen looked clipped by the round bezel on first glance; pixel-row analysis of the
  screenshot showed that was the round mask's corner vignette (which `pebble screenshot` does render),
  not the text — the actual glyphs sit entirely within rows ~70–131 of a 180px-tall display, well
  clear of the mask. No layout bug, no fix needed.
  Also discovered (from the waf build cache's actual per-platform `#define`s, not assumption): **flint
  does not define `PBL_TOUCH`** — it's black & white and non-touch, unlike emery. The mic icon's
  `#ifdef PBL_TOUCH` gate in `mic_icon.c`/`main.c` already handles this correctly (icon only compiles in
  and shows on emery among the five built targets); this just confirms the capability-gated design
  decision (see `CLAUDE.md`) was already right, no code change required.
  Icon sizing confirmed against current SDK docs (`developer.repebble.com/guides/app-resources/images.md`):
  the menu icon must be exactly 25x25 — "icons that are larger will be rejected by the SDK" — the same
  size for every platform, no per-platform variants needed. `watchapp/resources/images/app_icon.png` is
  already exactly 25x25.
  **The real-phone Locker icon check came back blank/default — investigated, and it's not a bug.**
  Read `coredevices/mobileapp`'s actual source (`libpebble3/.../locker/Locker.kt`,
  `disk/pbw/PbwApp.kt`, `database/entity/LockerEntry.kt`) rather than guess: a sideloaded app's
  `LockerEntry` is built entirely by `PbwApp.toLockerEntry()`, which only ever populates
  `pbwIconResourceId` (the on-**watch** icon, sent to the watch itself over Bluetooth via
  `AppMetadata.icon` — this is the `APP_ICON`/`menuIcon` resource, and it's what Phase 7's emulator
  screenshots already confirmed renders correctly). It explicitly leaves `iconImageUrl` (and
  `listImageUrl`/`screenshotImageUrl`) at their `null` default — those three fields only ever get set
  from `appstoreData` for apps synced from the actual app store backend, and `appstoreData = null` for
  every sideloaded entry, unconditionally. The phone's own Locker *list* screen renders from
  `iconImageUrl`, not from the PBW's bundled bitmap, so a blank icon there is the official mobile app's
  own designed behavior for any sideloaded app — not specific to Delta Notes, and nothing in this repo
  controls it. Confirms the watch-side icon path is unaffected and already verified; this should resolve
  on its own once actually published (Phase 10), since the store backend will host an `iconImageUrl` for
  it. Also traced (same source read) that `LockerAppScreen.kt`'s `hasSettings()` gates the Settings
  button on exactly `LockerEntry.configurable`, which for a sideloaded app resolves to
  `info.capabilities.any { it == "configurable" }` — i.e. the Phase 9 `capabilities` addition is real,
  functionally-read metadata in the *current* `coredevices/mobileapp` source, not just documentation.
  (`CLAUDE.md` already recorded the Settings button working on 2026-09-09, before that field existed —
  most likely a slightly different app version at the time, or another path this read didn't need to
  chase since the end result either way is already confirmed real; noted rather than investigated
  further, since it doesn't change what to do here.)

- [x] **Phase 8a — docs/SETUP.md and docs/TROUBLESHOOTING.md**
  Both written and reflect real, tested behavior (not aspirational): `SETUP.md` covers both deploy paths
  (standalone `docker-compose.yml`, and integrated/reuse-existing) plus the Clay config page step;
  `TROUBLESHOOTING.md` captures every real gotcha hit during Phase 6a-6c (the n8n `={{ }}` expression
  syntax bug, the `mcp-obsidian` DNS-alias requirement, the publish-then-restart quirk, the AppMessage
  retry/duplicate-note edge case, the emulator's SDL2 dependency, the docker-compose project-name
  collision incident, and the stale-docs Developer Connection navigation).

- [x] **Phase 8b — README.md and docs/ARCHITECTURE.md**
  `README.md` rewritten, styled directly after the sibling project's own (badges, how it works, why this
  design, components table, project status, documentation table, prerequisites, license, contributors
  footer). `docs/ARCHITECTURE.md` written: formalizes `PLAN.md`'s original reasoning now that it's
  validated against reality (real findings table mirroring the sibling's own spike-validation table,
  plus the docker-compose incident writeup) rather than just stated intent. `docs/SETUP.md` updated with
  a one-line mention of the touch/mic-icon path (was SELECT-only in its wording, code already supported
  both). Full repo review pass otherwise found `PLAN.md`/`CLAUDE.md`/`TODO.md` still accurate as the
  dev-facing (not public) design record — no changes needed there beyond what's already been kept
  current phase-by-phase throughout the build.

- [x] **Phase 9 — app store submission prep**
  [`docs/STORE_LISTING.md`](../docs/STORE_LISTING.md) written: manifest identity fields cross-checked
  against `package.json`, both icon sizes identified (25×25 in-app menu icon, already SDK-max per Phase
  7; 1024×1024 store icon from `design/`), four curated screenshots captured and committed under
  `docs/store-assets/screenshots/` (idle/saved/a failure state on emery, plus idle on chalk for
  cross-shape coverage — a *curated* subset of the full 9-state review set from this session, not all of
  it), and short/full store description copy written to lead with the self-hosting requirement rather
  than bury it post-install (see `docs/STORE_LISTING.md`'s own reasoning for why).
  Also added `"capabilities": ["configurable"]` to `package.json` — a real, documented manifest field the
  app was missing despite already having a working Clay config page; rebuilt and confirmed it reaches
  `build/appinfo.json` correctly.
  **Genuinely unverifiable from here, flagged rather than guessed**: exact store-icon pixel size,
  screenshot limits, and category taxonomy the Core Devices submission *portal* itself expects — nothing
  fetchable describes that UI (only the SDK's manifest docs), so `docs/STORE_LISTING.md` lists these as
  open items to confirm at submission time rather than inventing numbers.
  Still the user's own: the real-phone Locker icon check carried over from Phase 7, and actual submission
  (Phase 10) itself, which needs their developer account.

- [ ] **Phase 10 — publish**
  Repo pushed to `main` (6 commits: C fixes, README, Phase 7, idle-prompt copy, Phase 9, the Locker icon
  finding). Remaining: the actual submission click-through, walking `docs/STORE_LISTING.md`'s checklist —
  needs the user's own Core Devices developer account, which this repo/session has no access to.

## Local dev environment (how Phase 1-6a were actually done)

Not committed to the repo (tooling, not project code) — whoever picks this up needs their own copy:

```bash
# Toolchain (works without root)
python3 -m venv .venv && ./.venv/bin/pip install pebble-tool
source .venv/bin/activate
pebble sdk install 4.33.1   # or `pebble sdk list` for current latest

# Emulator's native deps (needs root — this is the only part that does)
sudo apt install -y libsdl2-2.0-0 libglib2.0-0 libpixman-1-0 zlib1g libsndio7.0
```

Then from `watchapp/`: `pebble build`, `pebble install --emulator basalt --vnc` (add `--vnc` to every
emulator-interacting command in a headless/no-X11 environment — installs, screenshots, `emu-button`,
`logs`), `pebble screenshot --emulator basalt --vnc --no-open out.png`.

`pebble repl --emulator basalt --vnc` is a Python console into `pypkjs` internals, **not** a JS console
into the running app — not useful for poking at `localStorage`/app state directly; don't reach for it
for that.
