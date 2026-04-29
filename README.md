# LED Smart Clock

LED Smart Clock is an ESP32-based wall clock built around an 8x32 WS2812B LED matrix. It combines large-format time display with GPS, NTP, weather, air quality, and alert data, then exposes everything through a web dashboard, diagnostics page, live console, configuration portal, backup and restore, and OTA updates.

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

The recommended first install is the browser-based web installer.

### Web Installer

You have two supported ways to open the installer:

- Hosted installer: `https://cosmicc.github.io/LEDSmartClock/`
- Local installer included inside the release `firmware.zip`: extract the zip, then open `web-installer/index.html`

Do not use the GitHub repository root page as the installer page. Use the hosted installer URL above or the extracted `web-installer/index.html` file from the release package.

What you need:

- A desktop browser with Web Serial support: Google Chrome or Microsoft Edge
- A USB data cable
- An ESP32 connected directly over USB

How to use it:

1. Open the hosted installer page, or extract `firmware.zip` and open `web-installer/index.html` locally.
2. Plug the ESP32 into your computer with a data-capable USB cable.
3. Close any serial monitor, terminal, or other app that might already be using the ESP32 USB port.
4. Click `Install`.
5. When the browser asks for a device, select your ESP32 serial port and confirm.
6. Wait for the install to finish and the board to reboot.
7. On a fresh install, join the `LEDSMARTCLOCK` setup access point and complete onboarding in the browser.

Notes:

- The web installer flashes the release `firmware.bin` image for a full first-time install.
- The local installer inside `firmware.zip` includes its own `manifest.json` and `styles.css`, and it reads `firmware.bin` and `update.bin` from the root of the extracted zip.
- After the clock is installed, later browser-based firmware updates use `update.bin` on the clock's own OTA page.
- If automatic reset does not work, hold `BOOT`, tap `EN` or `RESET`, then release `BOOT` when the install begins.
- If browser flashing is unavailable, use the USB release-flash method below.

### Manual USB Flash

If browser flashing is unavailable, you can flash the release image directly with `esptool.py`.

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

That single `firmware.bin` is the full first-install image from the release package.

### Prebuilt Release Artifacts

GitHub releases include a `firmware.zip` package containing:

- `firmware.bin`
- `update.bin`
- `release-metadata.json`
- `web-installer/`
  - `index.html`
  - `manifest.json`
  - `styles.css`

Notes:

- `firmware.bin` is the merged first-install image used by the web installer and by manual USB flashing at offset `0x0`.
- `update.bin` is the OTA application image used by the clock web UI after the device is already installed.
- `release-metadata.json` includes release version, git commit, board target, and package build date.
- `web-installer/index.html` can be opened locally after extracting the zip and provides the same first-install flow without needing to browse the repository.
- The `web-installer` folder does not contain duplicate firmware binaries.

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

- OTA updates only replace the application image.
- If a release ever requires a full reinstall, use the web installer or the USB release-flash method instead of OTA.

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
