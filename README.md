# LED SmartClock

LED SmartClock is an ESP32-based wall clock built around an 8x32 WS2812B LED matrix. It combines a large-format clock display with GPS, NTP, weather, air-quality, and alert data, then exposes everything through a web dashboard, a configuration portal, and OTA firmware updates.

Version `2.5.0` is the current v2 release. It includes the rewritten web interface, OTA flow, display scheduler cleanup, DHCP-aware NTP selection, configuration backup or restore, hardened weather-alert fallback handling, and key-based config storage.

## What It Does

- Displays a large 12-hour or 24-hour clock on an 8x32 LED matrix.
- Keeps time from GPS, NTP, and the onboard DS3231 RTC.
- Resolves timezone automatically from IP geolocation or uses a fixed manual offset.
- Resolves location from GPS, reverse geocoding, or fixed coordinates.
- Shows rotating data blocks for date, current temperature, current weather, daily forecast, AQI, and weather alerts.
- Adjusts brightness automatically from ambient light, with user bias controls.
- Exposes a status dashboard and a full configuration portal over Wi-Fi.
- Supports OTA updates from the web interface using `firmware.bin`.
- Exports and imports configuration backups as JSON so settings can move across firmware versions.

## Web Interface

The firmware serves two main pages:

- `Status page`
  - Current time source, timezone source, uptime, location, weather, AQI, and alert state.
  - Reboot action.
  - Link to the firmware update page.
- `Configuration page`
  - Grouped settings for connectivity, display, clock behavior, weather, AQI, sun events, and location.
  - Cancel and reboot actions.
  - Firmware update entry point.

The OTA update page now matches the rest of the UI and shows upload progress while the browser transfers the image.

The maintenance flow now also includes a themed `Backup & Restore` page:

- `Backup & Restore page`
  - Downloads a JSON snapshot of the current config.
  - Restores recognized settings from an older or newer backup by stable field IDs.
  - Reboots automatically after a successful restore so Wi-Fi, API, and runtime state restart cleanly.

## Required Services

Some features depend on external services:

- `OpenWeather`
  - Used for current weather, forecast, AQI, and reverse geocoding.
  - Requires an API key with access to `One Call 3.0`.
- `ipgeolocation.io`
  - Used for automatic timezone and coarse location fallback.
  - Requires an API key.
- `weather.gov`
  - Used for alert data.
  - No API key required.

Without API keys, the clock still works as a clock, but weather, AQI, reverse geocoding, and automatic timezone/location features will be limited.

## Hardware

The project currently assumes these core parts:

- ESP32 development board
- 8x32 WS2812B LED matrix
- DS3231 RTC module
- TSL2561 light sensor
- NEO-6M GPS module
- 5V power supply sized for the LED matrix

Important:

- The original hardware notes referenced an `ESP32-S3` board.
- The current PlatformIO project is configured for `esp32dev` in [platformio.ini](platformio.ini).
- Confirm the board target for your hardware before flashing or changing pin assignments.

## First-Time Installation

For a first install, use USB flashing.

The safest path is to build and flash from source with PlatformIO so the bootloader, partitions, and firmware stay aligned with the current project configuration.

### Build From Source

1. Install PlatformIO.
2. Clone this repository.
3. Review [platformio.ini](platformio.ini) and confirm the selected board matches your hardware.
4. Build the firmware:

```bash
platformio run
```

5. Flash over USB using PlatformIO:

```bash
platformio run -t upload --upload-port /dev/ttyUSB0
```

If your board is on a different serial device, check:

```bash
ls /dev/ttyUSB* /dev/ttyACM*
```

If automatic reset does not work, manually enter the ESP32 bootloader:

1. Hold `BOOT`.
2. Tap `EN` or `RESET`.
3. Release `BOOT` when the upload starts.

### Manual USB Recovery Flash

If the web UI is unavailable and you want to flash directly over USB/serial, you can write the images with `esptool.py`.

From the build output directory:

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
  0x20000 firmware.bin
```

This is the safest manual recovery method when flashing from a local build because it writes the full image set used by the project.

### Prebuilt Release Notes

If you use release artifacts instead of building locally:

- `firmware.zip` is the release package intended for downloads from GitHub Releases.
- `firmware.zip` contains exactly:
  - `firmware.bin`
  - `bootloader.bin`
  - `partitions.bin`
- `firmware.bin` is suitable for OTA updates after the clock is already installed.
- First-time USB flashing may also require `bootloader.bin` and `partitions.bin`, depending on your flashing workflow.
- If you are not sure, prefer a full PlatformIO flash from source for the first install.
- If you are recovering from a bad OTA and only have the release package, flash the extracted filenames you have over USB. A local build remains the most complete recovery path because it also includes `ota_data_initial.bin`.

## First Boot And Setup

On first boot, or whenever the device is forced back into AP mode, the clock exposes a Wi-Fi access point:

- SSID: `LEDSMARTCLOCK`
- Password: `ledsmartclock`

Initial setup flow:

1. Connect to the `LEDSMARTCLOCK` access point.
2. If you already have a config backup, open `Backup & Restore` and import it first.
3. Otherwise open the configuration portal.
4. Set the following first:
   - `AP Password`
   - `Wi-Fi SSID`
   - `Wi-Fi Password`
   - `OpenWeather API key`
   - `ipgeolocation.io API key`
5. Save the configuration.
6. Let the clock connect to your normal Wi-Fi network.
7. Open the status page on the device IP address to verify time sync, location, weather, AQI, and NTP source selection.

If GPS is unavailable, the clock can still work with fixed coordinates and a manual timezone offset.

## OTA Updates

After the first USB install, updates can be applied from the browser.

OTA behavior in the current firmware:

- Upload `firmware.bin` only.
- Typical firmware size is about `2 MB`.
- The update page now shows upload progress and final success or failure feedback.
- The update page reminds you to export a config backup before flashing.
- The device reboots automatically after a successful OTA update.
- GitHub Releases are intended to include a `firmware.zip` package containing the three flash images with no subdirectories.

Limitations:

- OTA does not replace `bootloader.bin`.
- OTA does not replace `partitions.bin`.
- If a future release changes the bootloader or partition layout, update over USB instead of OTA.

## Configuration Backup And Migration

The firmware can export the full live configuration as a JSON backup and restore it later.

- Export includes Wi-Fi credentials, AP password, API keys, display settings, location overrides, and saved fallback state.
- Restore matches settings by stable JSON field IDs instead of raw storage position.
- Unknown fields are ignored so older backups can still seed newer firmware, and newer backups can partially restore onto older firmware.
- Invalid values for recognized settings are rejected individually instead of failing the whole import.
- Successful imports reboot the clock automatically.

Because these backups contain secrets, store them privately.

## Configuration Notes

The firmware now persists settings in a key-based store instead of relying on IotWebConf's old positional EEPROM layout.

- Firmware version: `2.5.0`
- Adding new settings no longer shifts old settings into the wrong slots.
- Reordering the web configuration page does not corrupt saved config.
- Backup and restore use the same stable setting IDs as normal persistence.

This means normal firmware updates no longer depend on a manually bumped storage version just to keep existing settings safe.

## Status Indicators

The display includes configurable status LEDs that summarize runtime state:

- System status
- Air quality status
- UV / upper-corner status
- Alert indicators for active watches and warnings

The status page in the web UI is the best source of detail when diagnosing why a service is not updating.

## Repository Layout

- `src/`
  - Application code
- `include/`
  - Public headers, data models, and shared interfaces
- `docs/feature-changelog.md`
  - Completed work and planned feature changes for the v2 firmware line

## Development Notes

- `main` is the current release branch.
- `rewrite-foundation` contains the main version 2 rewrite work that has now been merged into `main`.
- `legacy` and `pre-coroutine` are older archive branches.

Useful development commands:

```bash
platformio run
```

```bash
platformio run -t upload
```

If serial debug output is enabled in the configuration, runtime diagnostics are emitted over USB serial.

## Known Caveats

- The current PlatformIO target is `esp32dev`, while earlier project notes referenced `ESP32-S3` hardware. Verify your board choice before flashing.
- OTA updates only cover `firmware.bin`.
- Weather and geolocation features depend on valid external API keys and network access.

## Roadmap

Current feature work and pending upgrades are tracked in [docs/feature-changelog.md](docs/feature-changelog.md).

## Contributing

Bug reports, hardware notes, cleanup patches, and UI improvements are welcome. If you open an issue or pull request, include:

- The hardware variant you are using
- Whether GPS is connected
- Whether API keys are configured
- Whether the problem occurs in AP mode, normal Wi-Fi mode, or both
- Any relevant serial debug output

## Credits

- Project author: Ian Perry ([cosmicc](https://github.com/cosmicc))
- AceTime, AceTimeClock, AceRoutine: Brian Parks ([bxparks](https://github.com/bxparks))
- IotWebConf: Balazs Kelemen ([prampec](https://github.com/prampec))
- FastLED NeoMatrix: Marc Merlin ([marcmerlin](https://github.com/marcmerlin))
