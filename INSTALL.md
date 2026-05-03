# Installing LED Smart Clock

For normal installs and updates, use the published release files instead of building from source.

## What You Need

- Google Chrome or Microsoft Edge on a desktop computer
- USB data cable
- ESP32 connected directly over USB
- Latest release package from `https://github.com/cosmicc/LEDSmartClock/releases`

Close any serial monitor, terminal, or other app that might already be using the ESP32 USB port before flashing.

## First-Time Install With The Web Installer

The recommended first install is the browser-based web installer.

You can open it in either of these ways:

- Hosted installer: `https://cosmicc.github.io/LEDSmartClock/`
- Local installer: download `firmware.zip` from GitHub Releases, extract it, then open `web-installer/index.html`

Do not use the GitHub repository root page as the installer. Use the hosted installer URL above or the extracted `web-installer/index.html` file from the release package.

Steps:

1. Open the hosted installer page, or extract `firmware.zip` and open `web-installer/index.html` locally.
2. Plug the ESP32 into your computer with a data-capable USB cable.
3. Click `Install`.
4. When the browser asks for a device, select your ESP32 serial port and confirm.
5. Wait for the install to finish and the board to reboot.
6. On a fresh install, join the `LEDSMARTCLOCK` setup access point and complete onboarding in the browser.

Notes:

- The web installer flashes the release `firmware.bin` image for a full first-time install.
- The local installer inside `firmware.zip` includes its own `manifest.json` and `styles.css`.
- The local installer reads `firmware.bin` and `update.bin` from the root of the extracted zip.
- If automatic reset does not work, hold `BOOT`, tap `EN` or `RESET`, then release `BOOT` when the install begins.

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
- If GPS is unavailable, the clock can still operate with fixed coordinates, a manual timezone name such as `America/New_York`, or a fixed GMT offset.
- If you forget the web password, return the device to setup mode and reconfigure it there.

## Updating An Installed Clock

After the clock is installed, updates are applied from the clock web UI.

1. Download the release `firmware.zip`.
2. Extract the zip.
3. Open the clock dashboard in a browser.
4. Export a fresh configuration backup from `Backup & Restore`.
5. Open `Firmware Update`.
6. Upload `update.bin`.
7. Wait for the upload to finish and the clock to reboot.

OTA notes:

- Upload `update.bin` only.
- Do not upload `firmware.bin` to the OTA page.
- The firmware page validates the image name and OTA image header.
- If a release ever requires a full reinstall, use the web installer or manual USB flash with `firmware.bin`.

## Manual USB Flash

If browser flashing is unavailable, flash the release image directly with `esptool.py`.

After extracting `firmware.zip`, flash the merged first-install image like this:

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

## Release Package Contents

GitHub releases include a `firmware.zip` package containing:

- `firmware.bin`
- `update.bin`
- `release-metadata.json`
- `web-installer/index.html`
- `web-installer/manifest.json`
- `web-installer/styles.css`

File meanings:

- `firmware.bin` is the merged first-install image used by the web installer and by manual USB flashing at offset `0x0`.
- `update.bin` is the OTA application image used by the clock web UI after the device is already installed.
- `release-metadata.json` includes release version, git commit, board target, and package build date.
- `web-installer/index.html` can be opened locally after extracting the zip and provides the same first-install flow without needing to browse the repository.
- The `web-installer` folder does not contain duplicate firmware binaries.
