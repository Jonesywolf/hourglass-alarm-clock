# Project Guidelines

Hour-glass alarm-clock firmware for ESP32-S3 + LVGL 8. Keep modules small so an agent can edit one file without loading the whole tree.

## Architecture

```
main → app → (ui | services | input_router)
               services → hal
               ui → types, theme, fonts only
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

## Edit one module

Change `include/hg/<area>/<mod>.h` and `src/<area>/<mod>.cpp` only. Do not open generated font `.c` files under `src/ui/assets/fonts/`. Do not open unrelated screens to change Wi-Fi, NTP, or hardware.

`src/app/input_router.cpp` is the only place that interprets touch/encoder. Rewrite it when UX changes; do not leak that policy into HAL or screens.

## Build

```
%USERPROFILE%\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-devkitc-1
```

Keep a single `-Iinclude` in `platformio.ini`. Do not add nested `-Isrc/hal` flags. Add `lib_deps` only when the HAL/service that needs them is implemented.

Secrets stay in gitignored `include/secrets.h`. Copy `include/secrets.example.h`. UI must never include secrets.
