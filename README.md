# TideLantern — Tides. Weather. Time.

**Your window to the water.** TideLantern is an open-source, dual-display ESP32 marine information station featuring a tide monitor, marine forecast and international clock. **Created and maintained by John Christensen** (GitHub [@Tikishark777](https://github.com/Tikishark777)).

**Status: v1.0.0 — initial hardware-tested release.** The firmware has compiled successfully through GitHub Actions and has been tested on a physical, dual-GC9A01 ESP32 setup. The clock, tide display, marine forecast, Wi-Fi network scanner, setup portal and local settings dashboard have been demonstrated. As a hobby project that depends on external data services, coverage and forecasts are not guaranteed.

### [Open the TideLantern installer and illustrated build guide](https://tikishark777.github.io/TideLantern/)

## What TideLantern does

- **Two independent circular displays:** left is the international clock; right shows tides, marine conditions or alternates between them.
- **NOAA tide predictions:** next high/low events, predicted heights in feet or meters and a graphical curve with a NOW marker. The smooth curve is a **schematic interpolation** of official high/low events, **not** a measured or independently predicted continuous water level.
- **Marine conditions:** modeled wind speed, direction and gusts, wave height and sea-surface temperature **where the forecast models provide them**. Choose marine-only, tide-only or automatic alternating mode.
- **Search for locations worldwide:** look up a harbor, marina, city or waterway, or enter coordinates manually. Nearby NOAA tide stations are offered where supported.
- **International clock:** automatic location-aware time zone; 12/24-hour format, optional seconds, leading zero and full, U.S., day-first or ISO date formats.
- **Easy Wi-Fi configuration:** scan nearby 2.4 GHz networks, filter/search the list, choose your router, then configure the device in your phone's web browser.
- **Browser-based installer:** use desktop Chrome or Edge to install firmware directly over USB; no programming tools required.

**Coverage note:** Location searching is worldwide; official tide predictions in this release are available only for supported **U.S. NOAA CO-OPS stations**. International marine forecasts may be available even when tide predictions are not. **Waves and sea-temperature values can be absent** for certain locations; `--` means unavailable, not zero.

## Required hardware

| Item | Quantity | Description |
| --- | :---: | --- |
| ESP32 | 1 | ESP-WROOM-32, 30-pin development board |
| Round SPI displays | 2 | GC9A01, 1.28-inch, 240×240, seven-pin modules |
| USB data cable | 1 | To install firmware from a computer |
| Jumper wires / breakout | As needed | Connect displays to ESP32 |
| Enclosure | Optional | The existing SkyOrb-style printed housing can be reused (see credits) |

### Wiring — compatible with the original SkyOrb dual-display hardware

| GC9A01 module pin | Left — clock | Right — tides and weather |
| --- | --- | --- |
| GND | GND | GND |
| VCC | **3.3 V** | **3.3 V** |
| SCL / CLK | GPIO 18 | GPIO 18 |
| SDA / MOSI | GPIO 23 | GPIO 23 |
| RES / RST | GPIO 21 | GPIO 4 |
| DC | GPIO 17 | GPIO 2 |
| CS | GPIO 16 | GPIO 5 |

Both displays share CLK and MOSI. **Do not change these GPIO assignments** if using this wiring. Supply **3.3 V** to the seven-pin displays used in this project; confirm the electrical specifications of your own modules. No GPS receiver, AIS receiver or external tide sensor is required.

## Install TideLantern

1. Assemble the hardware using the wiring table above.
2. Open the **[TideLantern web installer](https://tikishark777.github.io/TideLantern/)** in **desktop Chrome or Edge**.
3. Connect the ESP32 with a known-good USB **data** cable. Click **Install TideLantern firmware**, select its USB/serial port and follow the prompts.
4. Flashing may overwrite existing firmware and erase saved Wi-Fi settings. **Do not flash over a working SkyOrb unless you intend to replace its firmware.**

## Connect Wi-Fi and open the settings dashboard

1. On first setup, connect your phone to the Wi-Fi network **`TideLantern-Setup`**, password **`Tides2026!`**.
2. Open **`http://192.168.4.1/setup`**. Tap **Scan nearby Wi-Fi**, filter the list if necessary, select your home **2.4 GHz** network, enter its password and select **Save Wi-Fi & restart**. A hidden network SSID can be entered manually.
3. **Switch your phone back to your regular home Wi-Fi** after TideLantern connects.
4. At startup, the **left clock temporarily shows `SETTINGS PAGE` and the device's local IP address for approximately 30 seconds**. Enter that address in the browser with `http://` in front, for example `http://192.168.1.123/` (your address will differ).
5. The clock's IP label **disappears after the startup interval by design**. To reopen settings later, try **[http://tidelantern.local/](http://tidelantern.local/)** from the same home network; if that address does not work, check your router's connected-device list or reboot TideLantern to show the IP again briefly.
6. In the settings dashboard, search your waterway, choose an appropriate nearby NOAA tide station when available, select tide/weather rotation and adjust measurement, clock and date preferences.

**Troubleshooting:** The phone and ESP32 must be able to reach each other on the same local network. Some mesh, guest or isolated Wi-Fi networks prevent this even if both devices have Internet access. A router DHCP reservation can keep the address consistent. If the device is still in hotspot setup mode, its setup address remains displayed until it is configured.

## Understanding the readings

- **Tide heights and times** come from NOAA CO-OPS tide-event predictions for the selected station. Predictions refresh periodically. Station choice matters: a distant station may not be representative of your harbor.
- **The tide graph is illustrative.** Its connecting line is constructed from predicted high and low events, not observed water level. Don't derive safe depth or passage timing from this graphic.
- **Wind and marine conditions** come from Open-Meteo forecast models, not an on-site anemometer or water sensor. Wind, gusts, waves and temperature have differing availability; `--` indicates unavailable data.
- **Time zones** depend on location-based data. Around daylight-saving transitions, NOAA event times shown in local time may differ by an hour because this version converts predictions using the currently known UTC offset; consult official NOAA predictions for critical planning.
- **Offline operation:** Tide/weather data are held in RAM, not guaranteed to survive a reboot without Internet. Time synchronization also requires initial network access.

Data references: [NOAA CO-OPS tide predictions](https://api.tidesandcurrents.noaa.gov/api/dev/), [NOAA station metadata](https://api.tidesandcurrents.noaa.gov/mdapi/prod/), [Open-Meteo forecast](https://open-meteo.com/en/docs), [Open-Meteo Marine](https://open-meteo.com/en/docs/marine-weather-api), [OpenStreetMap / Nominatim](https://operations.osmfoundation.org/policies/nominatim/).

**Safety:** TideLantern is a hobby information display, **not a marine navigation or safety instrument**. Never rely on it for route planning, crossing bars, tide-dependent passage, or weather-safety decisions. Check official local tide/current tables, National Weather Service marine forecasts and navigational guidance.

## Source code and automation

GitHub Actions generates a NOAA tide-station catalog, compiles the PlatformIO ESP32 firmware, stages the boot files and publishes the website/installer via GitHub Pages. The firmware source and the public website are separate parts of this repository.

| Path | Purpose |
| --- | --- |
| `src/main.cpp` | ESP32 firmware, displays, network and data retrieval |
| `include/web_ui.h` | Local Wi-Fi setup and settings dashboard |
| `platformio.ini` | Board configuration and dependencies |
| `scripts/gen_stations.py` | Build-time NOAA tide-station catalog generator |
| `scripts/prepare_installer.py` | Copies compiled binaries for the browser installer |
| `.github/workflows/build.yml` | Build and GitHub Pages deployment |
| `web-installer/index.html` | Public lighthouse-themed installer and build guide |
| `web-installer/manifest.json` | ESP Web Tools firmware/version metadata and offsets |

Website text edits do **not** require replacing ESP32 firmware. A successful GitHub Actions build is required before installing any future source-code changes. The latest firmware remains in your existing `src/` and `include/` directories.

## Credits and licenses

**TideLantern creator and maintainer:** **John Christensen** — [GitHub @Tikishark777](https://github.com/Tikishark777).

**Hardware heritage:** TideLantern is an independent marine firmware project designed for the dual-round-display layout and pin wiring popularized by the original [SkyOrb project by GulfCoastMaker](https://github.com/GulfCoastMaker/SkyOrb). TideLantern is not affiliated with or endorsed by that project.

**Printable enclosure:** The SkyOrb-compatible enclosure used for development is based on the [Aviation Desk Clock – Project SkyOrb model on MakerWorld](https://makerworld.com/en/models/2959706-aviation-desk-clock-project-skyorb#profileId-3317442), published by [TopherPrints](https://makerworld.com/en/@Topherprints_45). The printable design retains its creator's attribution and applicable MakerWorld license.

**Data and software:** NOAA CO-OPS, Open-Meteo, [© OpenStreetMap contributors](https://www.openstreetmap.org/copyright) (Nominatim), Adafruit GFX, Adafruit GC9A01A, ArduinoJson, PlatformIO and ESP Web Tools. Their respective service terms, data attribution requirements and software licenses continue to apply. TideLantern's own repository is distributed under its [MIT LICENSE](LICENSE).

**Security:** The local settings dashboard is intended for a trusted home LAN; do not expose it directly to the public Internet. This release uses TLS without certificate validation for read-only external API requests (`setInsecure()`), which limits data-integrity assurance. Reset or erase stored ESP32 Wi-Fi credentials before giving the device to someone else. Verify the commercial-use terms of external data services before commercial redistribution.
