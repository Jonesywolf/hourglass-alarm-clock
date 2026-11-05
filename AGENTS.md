# Project Guidelines

Hour-glass alarm-clock firmware for ESP32-S3 + LVGL 8. Keep modules small so an agent can edit one file without loading the whole tree.

## Physical form and interaction

- The clock sits inside an inverted glass cube candle holder with its bottom open.
- The display is on the front face, the rotary encoder is on the right face, and the
  capacitive touch button is on the top face.
- UI guidance should orient the user to those physical controls: point toward the
  top edge for touch and the right edge for the encoder. Keep the clock's
  personality warm and calm, with restrained motion.

## Architecture

```
main → app → (ui | services | input_router)
               services → hal
               ui → types, theme, fonts, icons only
```

- Public API lives only under `include/hg/`. Implementations live under `src/` with the same names.
- HAL never includes services or UI. Services never include UI. UI never includes HAL or services.
- App pushes POD snapshots from `include/hg/types.h` into screens.
- Includes are always `"hg/..."`. No `"wifi.h"`. No relative `../`.
- Private helpers: `src/<layer>/<name>_internal.h`, included only in that folder.
- Chipset details (MAX98357 I2S, TSL2591, SEN66, LED protocol) stay inside the matching `src/hal/*.cpp`.
- `src/main.cpp` only starts Serial and calls `hg_app_setup` / `hg_app_loop`.

## How to add things

- Swipe page: `include/hg/ui/<name>_screen.h` + `src/ui/screens/<name>_screen.cpp` + one row in the navigator table. Add snapshot fields in `types.h` if the page needs new data.
- Transient UI (alarm-set flash, dialogs): `include/hg/ui/overlays.h` / `src/ui/overlays.cpp`. Do not create a `screens/` file unless it becomes a swipe destination.
- Driver: `include/hg/hal/<name>.h` + `src/hal/<name>.cpp` + pins in `include/hg/hal/pins.h`. Wire it in `src/app/app.cpp` only.
- Domain logic: `include/hg/services/<name>.h` + `src/services/<name>.cpp`.
- Pins that are not wired yet stay `-1` in `pins.h`.
- Weather icon: add an entry to `tools/icons/icons.json`, run `platformio run -t icons`, and map it in `src/ui/weather_icons.cpp`. Screens never reference icon images directly. Glance/outlier rules live in `services/glance`; wording lives in `src/ui/weather_format.cpp`.
- UI icon: register an official Phosphor name with `tools\icons\generate_ui_icons.py --add NAME --sizes SIZE ...`, then run `platformio run -t ui-icons`. Use `hg/ui/ui_icon.h` and generated `HG_UI_ICON_*` IDs, never codepoints or raw image descriptors. `tools/icons/ui_icons.json` selects one of six weights at generation/build time (Bold currently selected), including true Duotone. Keep Meteocons condition art separate and unchanged; see README "UI icons (Phosphor)".

## Edit one module

Change `include/hg/<area>/<mod>.h` and `src/<area>/<mod>.cpp` only. Do not open generated font `.c` files under `src/ui/assets/fonts/` or generated image `.c` files under `src/ui/assets/icons/` and `src/ui/assets/ui_icons/`. Do not open unrelated screens to change Wi-Fi, NTP, or hardware.

`src/app/input_router.cpp` is the only place that interprets touch/encoder. Rewrite it when UX changes; do not leak that policy into HAL or screens.

## Build

```
%USERPROFILE%\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-devkitc-1
```

Keep a single `-Iinclude` in `platformio.ini`. Do not add nested `-Isrc/hal` flags. Add `lib_deps` only when the HAL/service that needs them is implemented.

Secrets stay in gitignored `include/secrets.h`. Copy `include/secrets.example.h`. UI must never include secrets.

## Browser UI preview

- For UI/layout/animation and input-policy changes, use the
  [simulator runbook](simulator/README.md) to preview the actual LVGL 8.3.11
  screens, fonts and production app/alarm/input policy before reporting success.
  Keep the browser build isolated; do not copy/recreate screens in HTML or add
  browser dependencies to the ESP32 PlatformIO environments.
- From the repository root, build with
  `.\simulator\build.ps1 -EmsdkPath <activated-sdk-path>` (Emscripten 4.0.10),
  then run `node .\simulator\serve.cjs` and open `http://127.0.0.1:8080`.
  The runbook covers explicit CMake/Ninja paths and process-local PowerShell
  script policy. Reuse available tools; do not assume a prior session's SDK path.
- Reuse a responding preview server if one is already running. Otherwise start
  one on loopback and verify HTTP readiness. Rebuild and reload after source or
  font changes; there is no hot reload. Stop validation-only servers afterward;
  leave a persistent preview running only when requested.
- Use browser tools to inspect the page and await `window.hgPreview.ready`.
  LVGL widgets are canvas pixels, not DOM elements: interact through accessible
  touch/encoder controls or `hgPreview`, then inspect screenshots and policy
  diagnostics. The canvas is not a touchscreen; do not invent widget-click UX.
- For reproducible captures, pause, await `hgPreview.scenario(name)`, advance a
  specified virtual duration with `hgPreview.advance(ms)`, then capture. Each
  scenario starts a fresh instance. Use `hgPreview.capture()` or the download
  button for native 320 x 240 PNGs; do not substitute arbitrary wall-clock sleeps
  or a screenshot of the scaled page for native-size capture.
- Run the browser regression checks through a browser evaluate call:
  `await window.hgPreview.ready; const { runSimulationTests } = await import("/test.js"); await runSimulationTests();`.
  Also inspect the changed states visually (including night mode and relevant
  animation phases); report assertions and visible/browser errors accurately.
- The suite enforces WCAG 2.2 AA contrast (4.5:1 small text, 3:1 large text/icons)
  in day and nightlight modes. Fix colours via `hg_theme_*` tokens rather than adding
  exemptions; never use `hg_theme_dim` or reduced opacity for text. See the
  [contrast audit](simulator/README.md#contrast-audit-wcag-22-aa).
- Peripherals/time/network are synthetic, settings are memory-only, and audio/
  LEDs are unavailable. Browser checks do not prove ESP32 frame/capture timing,
  DMA, worker concurrency, NVS durability or physical input. Continue to build
  production firmware and follow the device test runbook when applicable; a
  preview never grants upload/audio permission or replaces hardware validation.

## Runtime and tests

- Only the Arduino UI task calls LVGL or mutates navigation/alarm/effect policy.
  Never add sensor/network/flash/audio waits to that task. App connects workers
  through bounded POD handoffs; see [runtime ownership](README.md#runtime-ownership).
- Keep I2C in the sensor worker and HTTPS in the weather worker. Preserve snapshot validity/freshness/error status,
  output generation/stop semantics, and flash-write admission during transitions.
- For runtime, input, persistence, display, or effect-policy changes, follow the
  [test runbook](test/README). It contains exact PlatformIO commands, required
  frame/capture thresholds, failure interpretation, and scratch-NVS safeguards.
- Obtain device/port and upload permission; never assume a previous session's
  COM4 permission still applies. Always restore normal firmware after tests,
  including failures. Report measured timing and restoration, not just build success.
