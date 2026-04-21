# LED SmartClock

LED SmartClock is an ESP32-based wall clock built around an 8x32 WS2812B LED matrix. It combines large-format time display with GPS, NTP, weather, air quality, and alert data, then exposes everything through a web dashboard, diagnostics page, live console, configuration portal, backup and restore, and OTA updates.

Current firmware release: `2.6.1`

## What It Does

- Displays a large 12-hour or 24-hour clock on an 8x32 matrix.
- Keeps time from GPS, NTP, and the DS3231 RTC.
- Handles DST automatically when a real timezone is known, or uses a fixed manual offset when desired.
- Resolves location from GPS, reverse geocoding, IP geolocation fallback, or fixed coordinates.
- Shows rotating display blocks for date, current temperature, current weather, daily forecast, AQI, and weather alerts.
- Adjusts brightness automatically from ambient light, with user controls.
- Supports first-boot onboarding, optional web password protection, dark mode, and Basic or Advanced config views.
- Supports configuration backup and restore with version-tolerant JSON imports.
- Supports first-time installs from a browser-based web installer and later OTA updates using `update.bin`.

## Web Interface

The firmware exposes these main web pages:

- `Dashboard`
  - Current time source, timezone, location, weather, AQI, alert state, uptime, and service health summary.
  - Reboot action.
- `Diagnostics`
  - Per-service health for Wi-Fi, timekeeping, NTP, GPS, weather, AQI, alerts, IP geolocation, and reverse geocoding.
  - GPS raw NMEA view and GPS recovery actions.
  - One-shot display-test buttons for weather, AQI, alerts, and temp/icon output.
- `Console`
  - Live in-memory debug log tail.
  - Downloadable console log.
  - Web-triggered debug commands.
  - Clear-console action.
- `Configuration`
  - Grouped settings for connectivity, display, clock behavior, weather, alerts, status indicators, sun events, location, and GPS.
  - Basic and Advanced modes.
  - Optional dark mode.
  - Cancel and reboot actions.
- `Firmware Update`
  - OTA upload page with progress, result feedback, and instructions.
- `Backup & Restore`
  - JSON export and import of saved settings.

## Required Services

Some features depend on external services:

- `OpenWeather`
  - Current weather
  - Daily forecast
  - AQI
  - Reverse geocoding
  - Requires an API key with access to `One Call 3.0`
- `ipgeolocation.io`
  - Automatic timezone
  - Coarse location fallback
  - Requires an API key
- `weather.gov`
  - Weather alerts
  - No API key required

Without API keys, the clock still works as a clock, but weather, AQI, reverse geocoding, and automatic timezone or location features will be limited.

## Hardware

The current project assumes these core parts:

- ESP32 development board
- 8x32 WS2812B LED matrix
- DS3231 RTC module
- TSL2561 light sensor
- NEO-6M GPS module
- 5V power supply sized for the LED matrix

Important:

- The PlatformIO target is currently `esp32dev`.
- Earlier project notes referenced `ESP32-S3` hardware.
- Confirm your actual board target and pinout before flashing.

## First-Time Installation

The easiest first install is the browser-based web installer:

- `https://cosmicc.github.io/LEDSmartClock/`
- Uses the release `firmware.bin` image for a full blank-device install
- Requires Chrome or Edge on desktop with Web Serial support
- After the clock is installed, later OTA updates use `update.bin`

If browser flashing is unavailable, use one of the USB paths below.

### Build From Source

1. Install PlatformIO.
2. Clone this repository.
3. Review [platformio.ini](platformio.ini) and confirm the selected board matches your hardware.
4. Build the firmware:

```bash
platformio run
```

5. Flash over USB:

```bash
platformio run -t upload --upload-port /dev/ttyUSB0
```

If your board is on a different device, check:

```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

If automatic reset does not work:

1. Hold `BOOT`.
2. Tap `EN` or `RESET`.
3. Release `BOOT` when upload starts.

### Manual USB Flash

If browser flashing or OTA is unavailable, you can flash directly with `esptool.py`.

#### From A Release

If you downloaded `firmware.zip` from GitHub Releases, flash the merged first-install image like this:

```bash
esptool.py \
  --chip esp32 \
  --port /dev/ttyUSB0 \
  --baud 460800 \
  --before default_reset \
  --after hard_reset \
  write_flash \
  0x0 firmware.bin
```

That single `firmware.bin` already includes the bootloader, partition table, OTA data, and application image for a fresh install.

#### From A Local Build

From the local build output directory:

```bash
cd .pio/build/esp32dev
esptool.py \
  --chip esp32 \
  --port /dev/ttyUSB0 \
  --baud 460800 \
  --before default_reset \
  --after hard_reset \
  write_flash \
  --flash_mode dio \
  --flash_freq 80m \
  --flash_size 4MB \
  0x1000 bootloader.bin \
  0x11000 partitions.bin \
  0x18000 ota_data_initial.bin \
  0x20000 update.bin
```

This is the safest recovery method from a local build because it writes the full image set used by the project.

### Prebuilt Release Artifacts

GitHub releases include a `firmware.zip` package containing:

- `firmware.bin`
- `update.bin`
- `installer.txt`

Notes:

- `firmware.bin` is the merged first-install image used by the web installer and by manual USB flashing at offset `0x0`.
- `update.bin` is the OTA application image used by the clock web UI after the device is already installed.
- `installer.txt` is a short explainer that tells users which file is for first-time installs and which file is for OTA updates.
- A local PlatformIO build remains the most complete recovery path because it also includes `bootloader.bin`, `partitions.bin`, and `ota_data_initial.bin`.

## First Boot And Setup

On first boot, or whenever the device is forced back into AP mode, the clock exposes:

- SSID: `LEDSMARTCLOCK`
- Password: `ledsmartclock`

The onboarding flow walks through:

1. Wi-Fi credentials
2. Optional web password protection
3. API keys
4. Timezone behavior
5. Final self-test

Useful setup notes:

- If you already have a config backup, restore it before entering everything by hand.
- Web password protection is optional.
- If GPS is unavailable, the clock can still operate with fixed coordinates and a fixed manual timezone offset.
- If you forget the web password, return the device to setup mode and reconfigure it there.

## OTA Updates

After the clock is installed, updates can be applied from the browser.

OTA behavior:

- Upload `update.bin` only.
- Typical firmware size is about `2 MB`.
- The firmware page shows upload progress and final success or failure status.
- The page reminds you to export a config backup before flashing.
- The device reboots automatically after a successful OTA update.

Limitations:

- OTA does not replace `bootloader.bin`.
- OTA does not replace `partitions.bin`.
- If a release changes bootloader or partition layout, update over USB instead of OTA.

## Configuration Storage And Backups

The firmware persists settings in a key-based non-volatile store instead of relying on positional EEPROM-style layout.

That means:

- Adding new settings does not shift old settings into the wrong slots.
- Reordering config-page sections does not corrupt saved settings.
- Backup and restore use stable setting IDs instead of raw storage offsets.

Backup behavior:

- Export includes Wi-Fi credentials, web and AP passwords, API keys, display settings, location overrides, and saved fallback state.
- Restore matches recognized settings by stable JSON field IDs.
- Unknown fields are ignored.
- Invalid values for recognized settings are rejected individually instead of failing the whole import.
- Successful imports reboot the clock automatically.

Because backups contain secrets, store them privately.

## Status Indicators

The matrix includes configurable corner indicators for:

- Bottom-left system or time status
- Top-left AQI status
- Top-right UV status

These can be turned off entirely from the configuration page.

The display also uses side indicators for active watches and warnings.

## Debugging

If serial debug output is enabled in configuration, runtime diagnostics are emitted over USB serial.

Useful paths and tools:

- `Diagnostics` for service health, GPS state, and raw NMEA
- `Console` for live runtime log output and debug commands
- `Backup & Restore` before risky changes or firmware updates

Useful build commands:

```bash
platformio run
```

```bash
platformio run -t upload
```

## Repository Layout

- `src/`
  - Application code
- `include/`
  - Public headers, data models, and shared interfaces
- `docs/feature-changelog.md`
  - Completed work and planned feature changes for the v2 firmware line

## Known Caveats

- The current PlatformIO target is `esp32dev`, while older project notes referenced `ESP32-S3` hardware.
- OTA updates only cover `update.bin`.
- Weather and geolocation features depend on valid API keys and network access.
- Some GPS problems are hardware, wiring, antenna, or sky-visibility issues rather than firmware issues.

## Roadmap

Current feature work and planned upgrades are tracked in [docs/feature-changelog.md](docs/feature-changelog.md).

## Contributing

Bug reports, hardware notes, cleanup patches, and UI improvements are welcome. Include:

- The hardware variant you are using
- Whether GPS is connected
- Whether API keys are configured
- Whether the problem occurs in AP mode, normal Wi-Fi mode, or both
- Relevant serial or web-console output

## Credits

- Project author: Ian Perry ([cosmicc](https://github.com/cosmicc))
- AceTime, AceTimeClock, AceRoutine: Brian Parks ([bxparks](https://github.com/bxparks))
- IotWebConf: Balazs Kelemen ([prampec](https://github.com/prampec))
- FastLED NeoMatrix: Marc Merlin ([marcmerlin](https://github.com/marcmerlin))
