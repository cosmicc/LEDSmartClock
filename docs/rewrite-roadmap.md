# LEDSmartClock Rewrite Roadmap

## Why Rewrite

The current firmware works as a single application-wide unit, but most responsibilities are tightly coupled:

- `src/main.cpp` owns startup, device state, scheduling, network requests, JSON parsing, rendering, and utility helpers.
- Several headers contain implementation, which makes reuse and testing harder.
- Global mutable state is shared across hardware access, web configuration, and display logic.
- Platform and hardware assumptions are spread across the codebase instead of being isolated behind interfaces.

This branch starts the version `2.0.0` rewrite by extracting shared support logic out of `main.cpp` into dedicated modules. That is the first step toward a smaller application core with explicit subsystem boundaries.

## Target Architecture

The rewrite should converge on these layers:

1. `core`
   - Application state
   - Shared enums, models, and constants
   - Time and formatting helpers

2. `services`
   - `TimeService`
   - `LocationService`
   - `WeatherService`
   - `AlertService`
   - `AirQualityService`

3. `drivers`
   - LED matrix adapter
   - GPS adapter
   - RTC adapter
   - Light sensor adapter
   - HTTP client adapter
   - Wi-Fi and web configuration adapter

4. `ui`
   - Clock renderer
   - Scroll renderer
   - Status LED renderer
   - Display scheduler

5. `app`
   - Bootstrapping
   - Dependency wiring
   - Main loop / coroutine registration

## Migration Order

1. Move generic support code out of `main.cpp`.
2. Extract HTTP/JSON parsing into service modules that return typed results instead of mutating globals directly.
3. Introduce a single `AppState` object to replace scattered mutable globals.
4. Wrap hardware access behind adapters so rendering and service code stop talking directly to ESP32 libraries.
5. Split display scheduling from display drawing.
6. Move IotWebConf parameter wiring into a configuration module that maps cleanly onto `AppState`.
7. Add a host-buildable test target for pure logic modules.

## Immediate Refactor Rules

- Keep behavior stable unless a bug is obvious and low-risk.
- Prefer moving pure logic first, then stateful services, then hardware-facing code.
- Do not add new global state during the rewrite.
- Avoid putting executable logic into headers unless it must be inline.
- Each extraction should leave `main.cpp` smaller and more orchestration-focused.

## Current Baseline

- `platformio.ini` currently targets `esp32dev` while the README says the hardware is an `ESP32-S3` board. That needs to be confirmed before larger hardware refactors.
- A persistent local PlatformIO install is now available via `/home/ip/.local/bin/platformio`.
- The current build baseline is:
  - `Firmware version`: `2.0.0`
  - `Config schema`: `13`
  - `Build command`: `/home/ip/.local/bin/platformio run`
