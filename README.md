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

## Effects and indoor-air policy

`include/hg/services/effects.h` is UI-task-owned, deterministic policy, not a
playback engine. Current speaker/LED HAL implementations remain unavailable stubs;
feedback requests do not pretend those outputs are active.

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

Sunrise scheduling, ramp duration, animation content, sound format, and actual
LED/audio/SEN66/weather integration are not implemented here. Future sunrise
progress should use elapsed monotonic time, not UI frame counts. Use hardware RMT
for SK6812 output and I2S DMA/buffered audio; do not bit-bang LEDs or decode/play
audio on the UI task. Recheck shared internal-memory, peripheral, and CPU budgets.

## Persistence

Alarm edits retain their debounce; enable changes request an asynchronous save.
The store worker coalesces desired settings and tracks requested/saved generations,
failures, pending/writing state, and write timing. An old completion cannot clear a
newer pending save. Failures remain pending and retry with backoff.

New saves use one validated `alarm_v1` blob in the existing `hg-store` namespace,
avoiding partially updated hour/minute/enabled keys. Existing legacy settings are
read and migrate on the next save.

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

## Build and tests

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e esp32-s3-devkitc-1
```

See the [agent test runbook](test/README) for hardware permission, exact test/upload
commands, output interpretation, regression thresholds, scratch-NVS safety, and
mandatory restoration of normal firmware. A successful build alone does not
verify UI smoothness. Actual future output drivers require another combined-load
run; synthetic load is not a substitute for measuring DMA underruns or LED timing.
