# Smart LED Clock

## Summary
LEDSmartClock is a ESP32 powered GPS/Wifi enabled wall clock with a 8x32 WS2812B LED matrix display.  It has many features, including multiple ways to obtain and keep accurate date & time, timezone, location, local weather conditions, local weather alerts, and outdoor air quality.  It has a web interface to display additional information, and many configuration options. 

## Features
-  Auto time/date synchronization with GPS and NTP.
-  Large Font 12/24 Hour LED Clock
-  Clock color can be automatic based on time of day (yellow morning to blue night) or custom fixed color.
-  Automatic timezone based on IP Geolocation or custom fixed timezone.
-  Automatic location based on GPS coordinates and IP Geolocation.
-  Automatic LED brightness based on room brightness with adjustable high/med/low settings.
-  Current & daily weather forecast based on automatic location: Conditions, Temp, Hi/Lo, Humidity, Wind, Air Quality
-  Weather Temperature color can be automatic (purple freezing to red hot) or custom fixed color.
-  Weather forecast display includes animated icons based on conditions.
-  Weather watch and warning alert display
-  All display intervals and colors can be customized.
-  Adjustable text scroll speed.
-  AP mode for initial setup.
-  Reset button to reset lost password and force clock in AP mode.
-  Full web interface to change settings, view additional info, and OTA wifi firmware updates.

## Download
If this is a first time installation, you must flash your ESP32 manually. Please follow the instructions in the *initial install* section below.
Latest version can be downloaded in the [Releases](https://github.com/cosmicc/LEDSmartClock/releases) section.  Firmware in the releases section can only be applied via the web interface after initial installation.  

## Components

 1. ESP32-S3 **30 Pin Version!** (NOT the 38 Pin Version) | Link: https://tinyurl.com/4nxevwau
 2. 8x32 (256 Pixel) WS2812B LED Matrix | Link: https://tinyurl.com/4wyw3hsw
 3. DS3231 RTC Module | Link: https://tinyurl.com/3mbe5wsn
 4. TSL2561 Light Luminosiy Sensor | Link: https://tinyurl.com/3jmt8h34  
 5. NEO-6M GPS Module | Link: https://tinyurl.com/4wyw3hsw
 6. 5v (2amp minimum) Power Supply | Link: https://tinyurl.com/2xem9tnv

## Circuit Board & Schematics
A schematic and circuit board will be created with KiCad and shared here so you can order your own board from PCBWay to connect all the components.

## Enclosure 
3D Printing STL files will also be created and shared here for enclosures.

## Installation
These steps are for the very first initial installation only.  If you already have LEDSmartClock running on your ESP32, please follow the instructions in the Upgrading section below.
The initial installation requires you to manually flash the ESP32 with platformio.  You need platformio setup in your editor (ie vscode) to compile and upload the code to the ESP32.  I may switch to a Arduino IDE compatible structure in the future for more user friendly initial installation.
(I don't have good installation instructions yet)
 1. Clone the `main` repository for the latest release.
 2. All libraries needed are located in the platformio.txt file.
 3. 


## Configuration

**Initial Configuration**
 - All settings are changed through the web interface over the network or directly, when the clock is in AP mode.
 - Initial configuration is done by connecting to the clock's wifi hotspot with the name `LEDSMARTCLOCK` using the wifi password `ledsmartclock`.
 - After connecting to the clock's `LEDSMARTCLOCK` wifi hotspot, you will be taken to the clock's web interface configuration page automatically.
 - Required settings you must change are:
   - `AP Password` - This password will be the admin password for the web interface, and the new Wifi hotspot password when the clock is in AP mode
   - `Wifi SSID` - This is the name of your wireless network (SSID) that the clock will use to get Internet access.
   - `Wifi Password` - This password of your wireless network that the clock will use to get Internet access.
 - Click `Apply` at the bottom of the configuration page and disconnect from the clock's wifi hotspot to continue.
 - The clock will now disable its wifi hotspot, and connect to the wireless network with the information you provided. It will scroll the IP address it obtained across the display upon successful connection.
 - You can then connect to the web interface again to continue configuration, but now over the network with he IP address that was displayed, instead of directly to the clock's wifi hotspot as before.

**Optional Configuration**
In order to get all the features of the smartclock (weather, geolocation, air quality), you will need sign up for some API keys:
 - LEDSmartClock uses [https://app.ipgeolocation.io](https://app.ipgeolocation.io) for location services.  Sign up for their free service to obtain an API key, and enter that key in the configuration page.  The clock will then be able to automatically determine your timezone and location.  These features will not work without an API key.
 - LEDSmartClock also uses [https://openweathermap.org](https://openweathermap.org/) for weather conditions, forecast, and air quality.  The clock will then be able to get weather and air quality information. These features will not work without an API key.
 - All other configuration settings are fairly self explanatory.  Change display colors, notification intervals, etc.  
   - The `clock colon` can be set to always on, normal blink, and fast blink.  
   - The `Alert Flash` is a full screen led display flash before any notification.

## Upgrading
Upgrading to a new version is easy!
1. Download the new firmware in the [Releases](https://github.com/cosmicc/LEDSmartClock/releases) section.
2. Connect to the web interface of the LEDSmartClock (Either over the network or directly in AP mode)
3. Click on the Configure Settings link to navigate to the configure settings page.
4. At the bottom of the configure setting page, click browse and select the new firmware file downloaded from releases.
5. Click the update button to upload the new firmware, and begin the upgrade process.  The clock will reboot when finished.
6. Connect to the web interface after it reboots to verify the new firmware version.

*The current firmware version running on the clock is displayed on the web interface's info page.*

> WARNING: Some new major and minor versions may require you to reconfigure the settings on your clock. This will be fairly infrequent (only if new configuration settings are added in release) and will be clearly announced if the upgrade will casue the settings will be reset.


## Status Indicators
- The display uses the lower left corner LED as a system status indicator that can be disabled in the settings, details below.
- The top left corner LED is the air quality status indicator, details below.  
- During a weather alert, 2 LED's in the middle of each side of the clock will illuminate yellow for an active watch, and red for an active warning.  The watch/warning details will scroll across the display at set intervals.

*The overall idea here was if everything is working optimally, no alerts are active, and air quality is good, then all status LED's are off.*

***Air Quality Status LED colors:***
*OFF/BLACK* - Air Quality (AQI) "Good"
*YELLOW* - Air Quality (AQI) "Fair"
*ORANGE* - Air Quality (AQI) "Fair" 
*RED* - Air Quality (AQI) "Fair"
*PURPLE* - Air Quality (AQI) "Fair"

***System Status LED colors:***
*RED* -  Wifi offline, or no NTP or GPS source available (Last sync over an hour)  
*MAGENTA* - AP mode.  Connect to LEDSmartClock SSID with ledsmartclock password (or the password you have set)
*YELLOW* - Connecting to wifi
*BLUEGREEN* - Wifi connected, no NTP or GPS time sync (Last sync under an hour)
*GREEN* - NTP sync (Last sync under an hour)  
*PURPLE* - No GPS fix (Last GPS sync over 10 minutes)
*BLUE* - No GPS fix (Last GPS sync under 10 minutes)
*OFF/BLACK* - GPS fix (Last GPS sync under 10 minutes)

*When the status LED is disabled, it will still show red if time is not updated by any source in over an hour*

## TODO / Feature Requests
Feel free to suggest any new features, changes, and ideas in the [discussions](https://github.com/cosmicc/LEDSmartClock/discussions) section.

## Known Issues
Please post any issues in the [issues](https://github.com/cosmicc/LEDSmartClock/issues) section.

## Developers / Contributions
I create raspberry-pi and micro-controller projects as a hobby, and far from being a decent embedded programmer. (Python was my normal go-to, but I'm creating projects like this to learn C to use in all my future projects) 

PLEASE, feel free to contribute any bug fixes, code optimizations, updates, security fixes, etc.  Your contributions will be much appreciated and you will be included in the project credits.

-Use the `dev` branch for development and pull requests.  The `main` branch is only for merges from `dev` for new version releases.  
-The `legacy` and `pre-coroutine` branches were used to to archive major rewrites during development and are now locked for reference.
-Release version numbers are formatted Major.Minor.Patch.  Some major and minor versions may require the LEDSmartClock settings to be reconfigured as all settings will be lost after upgrading due to IotWebConf options updates and the IotWebConf config version changing.
-weather images used to create the animated bitmaps are located in the `icons` directory.
-A Jtag connection will be available on the circuit board for easy debugging.
-An extra I2C connection will be available on the circuit board for optional expansion.
-If `debug to serial` is checked in settings, debug information will be sent to the USB serial port.  Make sure `DCORE_DEBUG_LEVEL` is set to `4` in platformio.txt for full debug log output.
-Single character commands can be sent to the Serial to get debug information:
`d` - Displays all current information
`s` - Displays all coroutine states
`f` - Displays all coroutine execution times (Only if `#define COROUTINE_PROFILER` is set in main.h)
`r` - Forces air quality to display
`w` - Forces current weather to display
`q` - Forces daily weather to display
`e` - Forces date to display

## Credits
Created by Ian Perry (cosmicc) https://github.com/cosmicc
Thanks to Brian Parks (bxparks) https://github.com/bxparks for creating some amazing AceTime, AceTimeClock, and AceRoutines libraries.
Thanks to Balázs Kelemen (prampec) https://github.com/prampec for the glorious IotWebConf library.
Thanks to Marc Merlin (marcmerlin) https://github.com/marcmerlin for creating the great FastLED NeoMatrix library, making interfacing with the LED matrix incredibly easy.
