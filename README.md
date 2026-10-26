# hourglass-alarm-clock

ESP32-S3 / Arduino-ESP32 / LVGL 8 firmware.

## Runtime ownership

The Arduino loop is the **only LVGL and UI-policy owner**. It consumes copies of
background results without waiting for them. Adding a slow operation to a service
does not give permission to call it from a screen or the UI loop.

| Context | Core / priority | Responsibility |
| --- | --- | --- |
| Arduino loop | 1 / 1 | LVGL, navigation, alarm/display/effect policy, applying snapshots |
| Input capture | 1 / 2 | Touch/button debounce and encoder capture, nominally every 5 ms |
| Sensor worker | 0 / 1 | All shared `Wire` access, light and indoor-air acquisition |
| Network worker | 0 / 1 | Wi-Fi maintenance, weather/phone service work, bounded diagnostic output |
| Persistence worker | 0 / 1 | Coalesced Preferences saves and acknowledgements |
| Audio worker | 0 / 3 | I2S playback of the commanded sound; exists only if the speaker initialized |

Workers start behind a readiness gate. Allocation/start failures produce a visible
fault rather than falling back to blocking I/O on the UI task. Input calibration
and a bounded initial light-read grace period happen before the first paint.
Alarm checks and background work continue during slides and the boot overlay.
The overlay blocks ordinary navigation/editing but not alarm dismissal.

- Public runtime API: `include/hg/app/runtime.h`; small workers: `src/app/`.
- Services never include app runtime. App connects services, HAL, and UI.
- Measurements use length-one latest-value mailboxes; input uses a bounded queue.
  The UI never drains an unbounded backlog or waits for a worker.
- Input events carry capture time and a navigation generation. Expired/blocked
  input is discarded, not replayed after boot. Encoder movement is coalesced;
  overflow/expiry counters and authoritative pressed-state diagnostics are exposed.
  Button transitions also carry a sequence, with a current-state handoff for
  recovery: lost releases cancel gestures rather than leaving a stuck hold.
- A sample's `valid` field means a last-good value exists, **not** that it is fresh.
  Inspect `status` and `sampled_ms`; distinguish waiting, ready, stale, paused,
  unavailable hardware, and acquisition error.
- Only the sensor worker initializes I2C. Drivers must use finite transactions
  and readiness/state-machine acquisition, not sleep with the bus owned.
- Network operations must have finite request deadlines and retry backoff.
  Network-service getters remain worker-owned; UI reads the runtime cache.
- Time snapshots use the system clock without waiting for NTP and retain the
  software-RTC fallback. A future physical RTC belongs behind the I2C worker.
- Diagnostic submission is bounded and nonblocking. Debug logging defaults off;
  errors are still queued. Check `hg_log_dropped()` if output is missing.

The display retains the existing library's ST7789 initialization and uses a private
ESP-IDF SPI transport with **two internal DMA-capable 40-line buffers** and RGB565
wire byte order. LVGL renders the next stripe while DMA transmits the previous one.
Word-aligned rectangles avoid bounce allocations. The SPI ISR only timestamps
completion; flush acknowledgement and all LVGL calls remain on the UI task.
The UI polls completion or waits only for its own finite display transfer, never
for background work. SPI2 is exclusively owned by this display; do not share it
with another driver. The two buffers total 51,200 bytes of internal RAM; include
that in future audio/DMA budgets. This is not a guarantee of 60 fps.
Volume-panel position updates ready the LVGL display-refresh timer so animation
steps are not delayed or coalesced by its independent refresh phase. Rendering
and DMA acknowledgement still run through the ordinary UI-task LVGL handler.

## Effects and indoor-air policy

`include/hg/services/effects.h` is UI-task-owned, deterministic policy, not a
playback engine. The speaker is implemented (see "Alarm and audio" below); the LED
HAL is still an unavailable stub. Feedback requests do not pretend an output is active.

- Ringing takes precedence over sunrise; either alarm phase reserves **both**
  LED/audio channels over interaction feedback.
- Conflicting feedback is discarded, not queued for after the alarm.
  Ordinary feedback is rate-limited, expires, and can be replaced.
- Desired state carries a generation. Future output workers must reject outdated
  commands; report applied state back through the UI owner, not by calling policy
  functions concurrently. Stop acknowledgement gates new feedback after cancellation.
- Applied LED/audio activity is distinct from requested feedback. Output
  availability/activity is not inferred from a successful request.
- LED-related AQI suppression is **opt-in, off by default**. When enabled,
  LED-active/start-interlock state pauses acquisition requests and immediately marks
  retained indoor data paused/stale. Acquisition resumes without catch-up bursts.
- Before a future LED backend turns on, publish `leds_starting`, let app publish
  the sampling-policy generation, and wait for that generation in the sensor
  snapshot. This acknowledges that any old acquisition has finished/discarded.
  Do not acknowledge an LED-start interlock by pretending LEDs are already off.
- Host-side suppression does **not** stop a SEN66 internally measuring. Physical
  stop/restart, warm-up, thermal cooldown, and fresh-data rules need actual driver
  support. No thermal-control guarantee is made by this policy.

Sunrise scheduling, ramp duration, animation content, and actual
LED/SEN66/weather integration are not implemented here. Future sunrise
progress should use elapsed monotonic time, not UI frame counts. Use hardware RMT
for SK6812 output; do not bit-bang LEDs. Recheck shared internal-memory, peripheral,
and CPU budgets, and merge any LED worker's applied state into the same
`hg_effects_report_applied()` call the audio path uses.

## Alarm and audio

**Setting an alarm (clock page only):**

- Hold the encoder button for one second to enter setting mode. The alarm can be
  enabled or disabled; editing does not change that state. Minutes flash first.
- Rotate for one-minute steps, carrying/borrowing hours and wrapping at midnight.
  Short-press to cycle minutes -> hours -> sound -> minutes; hour steps preserve minutes
  and cycle through all 24 hours, including the am/pm change.
- While editing, a music-note icon and small label above the alarm time show the sound. Both flash
  when sound is selected; rotate to cycle Warm mallets (1A, default), Wooden call
  (1F), and Plucked dawn (1H), wrapping in either direction. Choice changes are
  silent, and the current sound is copied into the draft on entry.
- Release the entry hold before pressing again. Another one-second hold saves,
  or ten seconds without encoder activity saves automatically. Touch is ignored
  and does not extend the editor timeout.
- Alarm scheduling is suppressed while editing. Detents change only a draft;
  only the committed time and sound are sent through the asynchronous save path.
  Saving a time matching the current minute does not ring immediately.
- Outside editing, a short press on the clock page toggles alarm enabled on
  **release**, not on button-down. Other pages do not edit or toggle the alarm
  with the encoder.

**Alarm volume (clock page only, outside time editing):**

- Rotate clockwise/counterclockwise to raise/lower the saved volume by 5 points
  per detent, clamped to 5–100% (no mute). This works with the alarm enabled,
  disabled, or snoozed, but not while ringing or a boot/alarm confirmation
  overlay is visible. Snoozed adjustment resumes after confirmation clears.
- A vertical bar with a speaker icon slides in from the right edge, then slides
  out after three seconds without a turn. Turns at either limit refresh it too.
  The icon has one wave at 5–50% and multiple waves at 55–100%, without extra animation.
  Leaving the clock, entering time editing, or showing a blocking overlay
  immediately cancels it. The panel follows nighttime colors and is not touchable.
- Turning is silent: there is no preview or extra chime. All existing button,
  time-editing, stop/snooze, and first-gesture-only-wakes behavior is unchanged.
- Volume scales the whole 30-second alarm ramp. At 100%, speaker output ramps
  from 5% to the new 80% cap. The initial 75% setting ramps from 3.75% to 60%,
  retaining the prior gain policy, not the old beep's perceived loudness. The new
  clips preserve their approved, quieter audition levels; check wake-up strength
  on the speaker before relying on the alarm. Alarm volume does not affect UI chimes.
- Volume is saved asynchronously after adjustment stops and flash writes are
  admitted; allow the save to complete before disconnecting power.

**Quiet alarm feedback:**

- Enter editing: a soft rounded tap; switch fields: a shorter, higher tap.
- Hold-save and ten-second timeout-save: the same two-note warm confirmation.
  This confirms the accepted time, even if unchanged, not completion of the
  asynchronous flash write. Save failures still follow the logged retry path.
- Enable: a higher warm chime; disable: a lower warm chime.
- Detents, touch navigation, ignored inputs, wake-only gestures, snooze, and
  dismissal remain silent. Consumed holds/releases do not produce extra cues.
- The latest cue replaces the previous cue; no sound history is queued. Alarm
  phases preempt feedback without replay. Ordinary silent feedback cannot cut
  off an alarm-action cue, and real action cues bypass the ordinary rate limit.

**Nighttime display and wake:**

- At or below 5 lux the display becomes red; after 60 minutes it blanks the clock
  labels and switches the active-high GPIO13 backlight off. Wake restores the
  backlight; at or above 50 lux it returns to normal.
  Intermediate light levels do not cancel the initial countdown.
- Touch, encoder press, or rotation while off wakes into red mode. The first
  interaction only wakes: it cannot navigate, toggle, or edit. A waking button
  hold and its release are consumed; release and press again for another action.
- After an input wake, one minute without further input returns to off. Further
  interactions restart that timer, and a held input keeps the display awake.
  Reaching 50 lux cancels temporary wake; a later dark transition gets the usual
  initial 60-minute countdown.
- An enabled alarm within 60 minutes, ringing, or snoozed prevents off mode.
  Alarm stop/snooze controls therefore remain visible, including during the
  temporary-wake timeout.

**Interaction while the alarm rings** (`src/app/input_router.cpp` is the only place
that interprets it; all of it works during slides and the boot overlay):

- Touch tap: snooze for `HG_ALARM_SNOOZE_MIN` (15) minutes. Snoozing is temporary and
  in-memory: the alarm label shows the snoozed time, nothing is saved, and a reboot
  during a snooze forgets it.
- Encoder press: stop. This dismisses the ring or cancels a snooze and restores the
  saved time; the alarm stays enabled for tomorrow. Rotation is ignored while
  ringing or snoozed.
- The press that stops the alarm is consumed, and the enable toggle is ignored for
  `HG_ALARM_TOGGLE_LOCKOUT_MS` afterward, so mashing the knob cannot disable the
  alarm. The entire stopping press/hold is consumed. Idle clock-page short presses
  toggle enabled and save on release.
- The alarm screen is a transient overlay (`ui/overlays.*`, LVGL top layer) with
  snooze/stop hints. After an action it briefly confirms "Snoozed until", "Next
  alarm" or "Alarm off".

**Audio path.** `effects` generation + phase → `hg_sound_for_effects()` → an
`hg_audio_command_t` in a length-one mailbox → the audio worker → `hal/speaker`
(legacy ESP-IDF I2S driver, 16 kHz 16-bit, mono duplicated into both slots,
MAX98357A SD_MODE held low until playback). The worker reports the generation it
applied; the UI feeds that into `hg_effects_report_applied()`. A stop is always
acknowledged, which clears `stop_pending` and re-admits flash writes. While audio is
active or stopping, effects are timing-sensitive, so flash writes are deferred.

- `services/sound` is hardware-free: a descriptor table, looping PCM/finite tone rendering, and the
  volume ramp (`HG_ALARM_VOLUME_START_PCT` → `HG_ALARM_VOLUME_MAX_PCT` over
  `HG_ALARM_RAMP_MS`, measured in rendered audio frames, plus a short fade-in).
  Tune the cap on the device; automated test builds use 1-2 % so they stay quiet.
- The alarm offers three original six-second PCM loops: Warm mallets (1A),
  Wooden call (1F), and Plucked dawn (1H). They live in read-only flash, not
  three RAM buffers, and share the same ramp, saved volume and startup fade.
  Five semantic alarm-action sounds are
  synthesized from original tone descriptors in `services/sound`: rounded,
  decaying taps and layered chimes with smooth attack/release, no pitch sweeps.
  They last 40-182 ms and peak below `HG_UI_CUE_VOLUME_PCT` (2% full scale),
  independently of the alarm ramp. No external recording or new dependency is
  used. `HG_SOUND_UI_NAVIGATE` / `HG_SOUND_UI_ALARM_EDIT` remain silent.
- Finite sounds pad their final chunk and flush bounded silence through the DMA
  ring before muting. The worker checks commands between chunks during this drain,
  so replacement and alarm interruption remain prompt. Semantic feedback has a
  450 ms policy lifetime including playback/drain; ordinary feedback retains its
  existing expiry. One-shot start/write failures are logged, not retried later.
- The DMA ring is 6 × 256 frames (~96 ms, ~6 KB internal RAM). A write error or
  timeout stops playback; looping alarms retry after `HG_AUDIO_RETRY_MS`. `underruns` is
  inferred from refill gaps longer than the ring can cover; check it with
  `hg_runtime_get_diagnostics()` (cumulative, not cleared by reset).
- A flash write already in flight when the alarm starts is covered by the DMA ring
  (measured NVS writes are tens of ms); new writes are then deferred.
- The clips are `assets/audio/warm_mallets.wav`, `wooden_call.wav` and
  `plucked_dawn.wav` (original synthesis; see `assets/audio/ATTRIBUTION.md` for
  provenance, approved PCM hashes and reproducible WAV generation).
  Regenerate their embedded constants after changing them:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools\audio\wav_to_pcm.py
```

`src/services/sound_alarm_clip.cpp` is generated; do not edit or open it. The WAV must
be six seconds, mono, 16-bit, `HG_AUDIO_SAMPLE_RATE`. Preserve continuous tails
across loop points rather than adding a repeated fade to silence. The approved
clips have matched RMS, not guaranteed equal perceived loudness.

## Persistence

Alarm time/sound/volume edits retain their debounce; enable changes request an asynchronous save.
The store worker coalesces desired settings and tracks requested/saved generations,
failures, pending/writing state, and write timing. An old completion cannot clear a
newer pending save. Failures remain pending and retry with backoff.

New saves use one validated twelve-byte `alarm_v2` blob in `hg-store`, atomically
storing time, enabled state, volume and sound. Existing eight-byte `alarm_v1`
records and separate legacy keys remain readable and migrate on the next save.
Missing sound defaults to Warm mallets. A legacy zero volume means 75%. Invalid
sound/volume logs an error and defaults only that field, preserving valid time
and enabled settings. Malformed v2 base records log an error and do not fall back
to stale v1 settings. No keys are erased and saves do not dual-write schemas.
Downgraded firmware sees its last pre-v2 settings, not later v2 edits.
All saves during snooze retain the base alarm time, never the temporary snooze time.

Flash-write admission is disabled during slides, recent interaction, and declared
timing-sensitive effects. Flash/cache activity can stall **both cores**, so moving
NVS to core 0 alone is insufficient. Admission cannot undo a write already started.
Continuous critical activity can defer durability; unsaved changes may be lost on
power loss. Consult `hg_store_get_status()` rather than assuming a request is durable.

## Touch Serial diagnostics

Touch diagnostics are currently enabled independently of general debug logging by
`HG_LOG_TOUCH_DEBUG` in `include/hg/config.h`. They run in regular firmware; no
diagnostic test image or on-screen prompt is needed. Open the PlatformIO Serial
Monitor on the device's port (currently COM4) at **115200 baud**:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor --port COM4 --baud 115200
```

Close the monitor before uploading firmware, then reopen it afterward.

The `touch` tag prints its GPIO, calibrated baseline/threshold, and debounce time
at startup. Every 500 ms it reports `raw`, window `min`/`max`, `thr` (threshold),
`hi` (samples at/above threshold divided by total samples), `down` (debounced held
state), and the cumulative number of detected `taps`. A second line reports
`fresh` (polls that observed a completed hardware scan, including calibration),
`age` (milliseconds since the last fresh reading), and `timeouts`.

The touch HAL owns the ESP-IDF driver directly, configuring GPIO7 before starting
the scan FSM. Calibration uses 16 completed scans, not repeated reads of a cached
register. Voltage, measurement timing, denoise, the +2000 threshold delta, and
30 ms debounce retain the previous settings. Hardware interrupts remain disabled;
the capture worker polls raw completion/timeout flags without waiting. A timeout
is logged and resumed using the driver API; failed reads do not count as input.
Calibration must complete within one second or startup reports an input fault.

- `PRESSED` / `RELEASED` identify debounced state changes.
- `input tap action=next` means the router accepted navigation.
- `action=dismiss` means the press dismissed an alarm.
- `action=blocked` reports navigation gating and capture/current generations.
- `tap expired` means a captured press exceeded the input-age limit.
- `measurement timeout` means the hardware scan halted and recovery was requested.
- `no completed scan` or a growing `age` means readings are stale; constant raw
  values alone do not establish that the hardware is still acquiring samples.

Compare a few idle lines with lines while holding and releasing the pad. Raw
readings failing to cross `thr` indicate a different problem from crossings that
never produce `PRESSED`, or accepted navigation that fails to appear on screen.
Logging uses the bounded queue/background Serial writer, not synchronous output
on the capture/UI tasks. Set `HG_LOG_TOUCH_DEBUG` to `0` after diagnosis.

Physical touch is not validated by the automated tests' injected input events.
After a driver change, verify actual pad press/release and navigation in regular
firmware, then unplug/replug and repeat on multiple cold boots with the pad
untouched during calibration. Require `fresh` to keep increasing, low `age`,
and no unexplained timeout/stall errors. A successful upload or warm reset alone
does not verify the cold-boot failure.

## Fonts

Generated LVGL fonts under `src/ui/assets/fonts/` are described by
[`tools/fonts/fonts.json`](tools/fonts/fonts.json): each entry names a source font
(Google Fonts / Material Icons, pinned by commit URL and SHA-256), a pixel size, and
the glyphs to keep. Select glyphs with `icons` (Material icon names), `codepoints`,
`ranges`, or `text`; variable fonts take `variations` (e.g. Inter `opsz`/`wght`).

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -t fonts                 # regenerate everything
$env:HG_FONTS = "icons_36"; & $pio run -t fonts   # only some fonts (comma-separated)
& $pio run -t fonts-force           # also re-download sources and reinstall converters
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools\fonts\generate_fonts.py --list
```

Source font files are downloaded on demand into the gitignored `.fonts-cache/`
(`*.ttf` / `*.otf` are also ignored); only the generated `.c` files are committed.
Node.js is required for `lv_font_conv`; `fonttools` is installed into the cache for
variable-font instances. After adding a font, declare it in `include/hg/ui/fonts.h`.
The existing Wi-Fi icon fonts were first generated from Google's TTF; the official
repository only ships OTF, so regenerating them can change anti-aliasing slightly.

## Build and tests

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e esp32-s3-devkitc-1
```

See the [agent test runbook](test/README) for hardware permission, exact test/upload
commands, output interpretation, regression thresholds, scratch-NVS safety, and
mandatory restoration of normal firmware. A successful build alone does not
verify UI smoothness. Actual future output drivers require another combined-load
run; synthetic load is not a substitute for measuring DMA underruns or LED timing.
