# Browser preview

This target compiles **the production LVGL 8.3.11 UI, fonts, app loop, input
router, alarm, brightness, display-mode and effects policy** into WebAssembly.
It does not recreate the screen in HTML or emulate ESP32 instructions.
The original PlatformIO environments and firmware sources are unchanged.

## Windows prerequisites

- Node.js (the local server has no npm dependencies).
- Working Python, CMake 3.20 or newer, Ninja, Git.
- Activated Emscripten SDK **4.0.10**. Install SDK tools outside the repository;
  do not change other agents' global tool configuration.

For example, using a tools directory of your choice:

```powershell
git clone --depth 1 --branch 4.0.10 https://github.com/emscripten-core/emsdk.git C:\dev\emsdk
$python = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
& $python C:\dev\emsdk\emsdk.py install 4.0.10
& $python C:\dev\emsdk\emsdk.py activate 4.0.10
```

If CMake/Ninja are missing, install them in a private tools environment, not the
firmware environment. `build.ps1` accepts explicit executable paths.

## Build and serve

From the repository root:

```powershell
.\simulator\build.ps1 -EmsdkPath C:\dev\emsdk
node .\simulator\serve.cjs
```

If tools are not on PATH:

```powershell
.\simulator\build.ps1 -EmsdkPath C:\dev\emsdk `
    -CMakePath C:\dev\tools\cmake.exe -NinjaPath C:\dev\tools\ninja.exe
```

Open **http://127.0.0.1:8080**. An optional port argument selects another
unprivileged port: `node .\simulator\serve.cjs 8081`.
The server binds only to loopback and serves only explicitly allowed build files,
never repository source or secrets. Build output is ignored by Git.
The first configure fetches the public LVGL v8.3.11 dependency.

If Windows blocks local PowerShell scripts, use a process-local policy (no
persistent policy change):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\simulator\build.ps1 -EmsdkPath C:\dev\emsdk
```

After C/C++/font or web changes, rebuild and reload the page. This is not hot
reload. Stop the server with Ctrl+C when done. Agent validation servers should
be stopped; only keep a persistent server when requested and confirm readiness.

## Preview controls

The display has a **320 x 240** backing canvas and integer-scaled presentation.
Use **Download 320x240 PNG** for native-size output. Backlight off renders black;
nonzero backlight is on, matching the firmware's non-PWM display contract.

The canvas is **not a touchscreen**. Accessible HTML buttons generate the real
clock's touch-pad and encoder events. Encoder/touch down and up support manual
holds; use Play or Advance to run time. Short press and one-second hold buttons
advance virtual time themselves. Focus loss releases held inputs and pauses.
Lost-release recovery cancels the gesture rather than producing a short press.

The simulator starts paused. All app policy and LVGL timers use one virtual
monotonic clock. Advance runs bounded one-millisecond steps, obeys production
loop delays and completes synchronous canvas flushes before returning. Browser
tab throttling cannot advance the clock unexpectedly while paused.

Scenario loading creates a **fresh WebAssembly instance**, resetting static UI,
navigation and service state. Settings persist only in memory within one
instance; no browser storage, device NVS or actual flash writes occur. The
diagnostics show simulated save requests/completions and unavailable outputs.
Weather, indoor air, light, time and Wi-Fi are synthetic; "Preview Wi-Fi" is not a real SSID.
The weather snapshot is built from a condition preset. Hourly temperatures follow
low..high, peaking at 3pm. Wet conditions rain for the next four hours; dry ones
with Rain % >= 50 get rain starting three hours from now. Weather "Unavailable"
models a failed fetch with no data. Units are read once at boot, like the NVS
setting, so changing them reloads the preview.
Stale/paused/error samples retain last-good values, whereas waiting/unavailable
samples have no valid readings.

Scenarios include clock/weather, disabled alarm, boot/sync/fault, unavailable
and stale readings, weather page variants (`weatherNight`, `weatherRain`,
`weatherScroll`, `weatherImperial`, `weatherNightlight`, `weatherNoSensor`,
`fetching`), each clock-face glance line (`glanceStorm`, `glanceSmoke`,
`glanceStuffy`, `glanceGusts`, `glanceHeat`, `glanceNight`), the post-alarm
`briefing` and `briefingOver`, minutes/hours/sound editing, 5/50/55/100% volume, ringing,
snooze/dismiss confirmations, noon/midnight, night/off and mid-slide. They use
actual input/alarm policy, seeded settings and external inputs.
The `colors` scenario is a presentation-only LVGL RGB565 calibration fixture.

## Agent/browser automation

Use browser tools to open the local URL and await `hgPreview.ready`. Accessible
controls can be located by their labels. LVGL widgets are **canvas pixels**, not
DOM elements: inspect screenshots and the policy diagnostics instead.

Via a browser evaluate call:

```javascript
await window.hgPreview.ready;
await window.hgPreview.scenario("sound");
window.hgPreview.advance(100);
const state = window.hgPreview.state();
const pngDataUrl = window.hgPreview.capture(); // native-size PNG
```

Available API:

- `reset({enabled, hour, minute, volume, sound, time, environment, fault})`:
  async fresh instance; alarm defaults to enabled, 07:00, 75%, sound zero.
- `scenario(name)`: async reset plus deterministic scenario inputs.
- `advance(ms)`: 0 through 3,600,000 ms; returns rendered state.
- `input("encoder" | "touch", 0 | 1)`; `input("rotate", signedInt32)`.
  Raw inputs enqueue at the current time; advance at least 5 ms to process.
- `press(duration = 80, kind = "encoder")`: press, advance and release.
- `cancelInputs()`: cancel queued/held inputs using authoritative-state recovery.
- `setTime(hour, minute, second = 0, valid = true)`.
- `setEnvironment({...})`: any subset of `wifi`, `lux`; weather `status`,
  `sky` (condition), `outdoor`, `feels`, `high`, `low` (C), `wind`, `gust` (km/h),
  `uv`, `rain` (%), `aqi`, `airStatus` (defaults to `status`); indoor
  `indoorStatus`, `indoor` (C), `humidity`, `co2`, `pm25`, `voc`; and `units`
  (0 metric, 1 imperial; only via `reset({environment})`). Omitted keys reset to
  defaults: connected/ready, 100 lux, partly cloudy 18 C, AQI 32 and Good indoor air.
  Numeric enum values match [types.h](../include/hg/types.h).
- `state().weather`: `offset` (hourly scroll), `glanceLine` (`hg_glance_line_t`),
  `showWeather`, `indoorLevel` (`hg_air_level_t`).
- `state()`, `capture()`, `pause()`, `play()`, `scenarios`.

For deterministic captures pause, await scenario completion, advance a specified
duration, then capture. No arbitrary wall-clock screenshot sleep is needed.
Capture identical scenario/time combinations using fresh instances for comparison.
Input errors and WASM faults appear in the alert panel and browser console.

## Browser regression checks

Run this in a browser evaluate call on the preview page:

```javascript
await window.hgPreview.ready;
const { runSimulationTests } = await import("/test.js");
await runSimulationTests();
```

The test module asserts actual input/alarm transitions, hold/editor/volume
boundaries, clamps, overlay/navigation gating, night/off/wake, all scenario
rendering, RGB channel order, native viewport size and deterministic PNG output.
Alarm checks also compare arrow pixels at the pulse peak/trough/full cycle,
ensure the digits stay static, verify each instruction nudges toward its
control (snooze up, stop right) in single-pixel steps, rests, and returns after a cycle, and verify confirmation glyphs
render after snooze and stop.
Weather checks cover every glance line and its priority, the briefing window
(stop only, not snooze), Poor indoor rating, hourly scroll clamping/reset, units,
full-color icons in normal mode and red-only pixels in nightlight.
Failures throw with state diagnostics. The suite leaves a fresh clock preview.

## What this does not prove

- ESP32 frame/input-capture thresholds, FreeRTOS concurrency, stack/heap margins.
- SPI/DMA transfers, physical display colors/luminance, GPIO input acquisition.
- I2C/network operation, NVS serialization/durability/garbage collection.
- Audible playback or LED activity: both outputs are explicitly unavailable.

No device tests or uploads are needed to use the browser preview. Firmware
regression testing still follows [the device test runbook](../test/README),
including current port/upload/audio permission and restoring normal firmware.
Settings UI and sunrise scheduling are not invented by the simulator. Real
Open-Meteo/ipapi responses, TLS and SEN66 readings are not exercised.
