# LED Smart Clock

LED Smart Clock is an ESP32-based wall clock for an 8x32 WS2812B LED matrix. It combines a large time display with GPS, NTP, RTC fallback, weather, air quality, weather alerts, automatic brightness, a web dashboard, diagnostics, live console logging, configuration backup and restore, and browser-based firmware updates.

<p align="center">
  <a href="https://www.pcbway.com/" target="_blank">
    <img src="docs/pcbway-logo.png" alt="PCBWay logo" width="420">
  </a>
</p>

## Project Supporter: PCBWay

This project is supported by [PCBWay](https://www.pcbway.com/), a PCB fabrication and prototype manufacturing service that helps bring hardware projects from breadboard or hand-wired prototypes into cleaner, more repeatable physical designs.

PCBWay helps this LED Smart Clock project and many other maker, hobby electronics, student, and open-source hardware projects by making custom PCB manufacturing more accessible. Their cost-effective prototyping services make it practical to test a board, revise the design, and continue improving the hardware without needing a large production run.

For a project like this, a professionally manufactured PCB helps reduce loose wiring, makes the build easier to assemble, improves long-term reliability, and gives other builders a clearer hardware reference to follow. That support benefits this project directly, but it also helps the broader community by making shared designs easier to reproduce, modify, and improve.

PCBWay offers a useful path for electronics projects that need more than a one-off breadboard build, including prototype PCBs, small-batch board runs, and manufacturing services that can support projects as they move from early testing toward a finished design.

Thank you to PCBWay for supporting this project and for helping makers, developers, and open-source hardware projects turn working ideas into real hardware.

## Highlights

- Large 12-hour or 24-hour clock display on an 8x32 LED matrix.
- Hybrid timekeeping from GPS, NTP, and a DS3231 RTC.
- Automatic DST when a named timezone is available, with manual zone-name selection plus fixed-offset and GPS-longitude fallbacks.
- Location from GPS, saved coordinates, reverse geocoding, or IP geolocation fallback.
- Rotating matrix messages for date, current temperature, current weather, daily forecast, AQI, weather alerts, sunrise, and sunset.
- Ambient-light brightness control with user-configurable display behavior.
- Web dashboard with service health, build metadata, GPS state, weather, AQI, alerts, and sun-event source.
- Diagnostics page with live service status, GPS recovery actions, raw NMEA, and one-shot display tests.
- Live web console with downloadable in-memory logs and web-triggered debug commands.
- First-boot onboarding, optional password protection, dark mode, Basic and Advanced config views, factory-default reset, OTA updates, and version-tolerant config backup and restore.

## Quick Links

- Install or update the clock: [INSTALL.md](INSTALL.md)
- Hosted web installer: `https://cosmicc.github.io/LEDSmartClock/`
- Latest releases: `https://github.com/cosmicc/LEDSmartClock/releases`
- Feature changelog and roadmap: [docs/feature-changelog.md](docs/feature-changelog.md)

## Hardware

The current project assumes:

- ESP32 development board
- 8x32 WS2812B LED matrix
- DS3231 RTC module
- TSL2561 light sensor
- NEO-6M GPS module
- 5V power supply sized for the LED matrix
- Custom PCB revision manufactured with support from PCBWay for cleaner module interconnects and power wiring

The PlatformIO target is currently `esp32dev`. Earlier project notes referenced ESP32-S3 hardware, so confirm your actual board target and pinout before flashing.

## Web Interface

The firmware exposes these main pages:

| Page | Purpose |
| --- | --- |
| Dashboard | Current clock state, service health, firmware version, git commit, board target, build date, location, GPS, weather, AQI, alerts, and sun events. |
| Diagnostics | Per-service health, retry state, last errors, GPS raw NMEA, GPS parser/UART recovery, and display-test actions. |
| Console | Live in-memory debug output, downloadable logs, clear-console action, and web-triggered debug commands. |
| Configuration | Detailed grouped settings for Wi-Fi, display, clock behavior, current weather, daily weather, air quality, weather alerts, status indicators, sun events, location, GPS, Basic/Advanced mode, dark mode, and maintenance. |
| Firmware Update | OTA upload for `update.bin` with progress, validation, success/failure feedback, and backup reminders. |
| Backup & Restore | JSON export and import of saved settings using stable field IDs. |

## External Services

The clock works as a clock without API keys, but these services unlock online features:

- `OpenWeather`: current weather, daily forecast, AQI, sunrise/sunset API data, and reverse geocoding. Requires an API key with One Call 3.0 access.
- `ipgeolocation.io`: automatic timezone and coarse location fallback. Requires an API key.
- `weather.gov`: weather alerts. No API key required.

## Configuration And Backups

Settings are stored by stable key instead of positional EEPROM-style layout. New firmware can add, remove, or reorder settings without shifting older saved values into the wrong slots.

Backup and restore behavior:

- Exports include Wi-Fi credentials, web and AP passwords, API keys, display settings, location overrides, and saved fallback state.
- Restore matches known settings by stable JSON field IDs.
- Unknown fields are ignored.
- Invalid recognized values are rejected individually instead of failing the entire import.
- Successful imports reboot the clock so network, portal, and derived state restart cleanly.

Backups contain secrets, so store them privately.

## Status Indicators

The matrix includes configurable corner indicators for:

- Bottom-left system or time status
- Top-left AQI status
- Top-right UV status

These indicators can be disabled from the configuration page. The display also uses side indicators for active watches and warnings.

## Debugging

Useful troubleshooting paths:

- `Diagnostics` for service health, GPS state, raw NMEA, and display tests.
- `Console` for live runtime output and debug commands.
- `Backup & Restore` before firmware updates or major configuration changes.

If serial debug output is enabled in configuration, the same runtime diagnostics are also emitted over USB serial.

## Repository Layout

- `src/` - application code
- `include/` - public headers, data models, and shared interfaces
- `scripts/` - build metadata, release package, and artifact helpers
- `web-installer/` - hosted/local browser installer template
- `docs/feature-changelog.md` - completed work and planned feature changes for the v2 firmware line
- `docs/pcbway-logo.png` - PCBWay sponsor logo used in this README
- `INSTALL.md` - end-user install, first boot, update, and release artifact notes

## Known Caveats

- The current PlatformIO target is `esp32dev`, while older project notes referenced ESP32-S3 hardware.
- OTA updates use `update.bin` only. First-time installs and full recovery flashes use `firmware.bin`.
- Weather and geolocation features depend on valid API keys and network access.
- GPS issues can be caused by hardware, wiring, antenna placement, or sky visibility rather than firmware alone.

## Contributing

Bug reports, hardware notes, cleanup patches, and UI improvements are welcome. Include:

- The hardware variant you are using
- Whether GPS is connected
- Whether API keys are configured
- Whether the problem occurs in AP mode, normal Wi-Fi mode, or both
- Relevant serial or web-console output

## Credits

- Project author: Ian Perry ([cosmicc](https://github.com/cosmicc))
- Project supporter and PCB manufacturing partner: [PCBWay](https://www.pcbway.com/)
- AceTime, AceTimeClock, AceRoutine: Brian Parks ([bxparks](https://github.com/bxparks))
- IotWebConf: Balazs Kelemen ([prampec](https://github.com/prampec))
- FastLED NeoMatrix: Marc Merlin ([marcmerlin](https://github.com/marcmerlin))
