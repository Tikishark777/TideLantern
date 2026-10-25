# TideLantern — Tides. Weather. Time.

**Your window to the water.** An open-source ESP32 desk instrument with two 1.28-inch round displays: an international clock and a tide / marine-conditions display. Developed as a distinct marine project by **John Christensen ([@Tikishark777](https://github.com/Tikishark777))** with AI-assisted design and implementation.

**STATUS: v0.1.x hardware-test preview.** GitHub Actions firmware build #2 and the installer deployment completed successfully; this is still early firmware and is being tested on real ESP32 hardware. Wi-Fi setup now scans nearby networks and displays the settings-page address on the clock. Protect any working SkyOrb device by using a separate ESP32 for experiments.

## Website and browser installer

Once this repository is created and GitHub Pages successfully deploys:

**https://tikishark777.github.io/TideLantern/**

Open on **desktop Chrome or Edge**, connect ESP32 via USB **data** cable, and click **Install TideLantern firmware**.

The landing page has a CSS-animated lighthouse beacon that respects reduced-motion preferences. The installer uploads the precompiled PlatformIO binary files published by GitHub Actions.

## Features included in this preview

- **Left clock:** synchronized NTP time, waterway-based time zone from Open-Meteo, familiar North American/selected international abbreviations and precise UTC offsets elsewhere, 12/24-hour time, optional seconds, leading zero and four date formats.
- **Right tide mode:** NOAA CO-OPS high/low tide predictions and heights for **a chosen NOAA U.S. tide station**, plus a curved *schematic* display between official high/low events. The interpolated graph and its NOW marker are **not observed or independently calculated accurate water levels**.
- **Right marine mode:** Open-Meteo modeled wind/gust forecast, wave heights and sea-surface temperature where available. Choose permanent tide, permanent marine or alternate every 20 seconds.
- **World location search:** user-initiated search by port, city, marina or waterway using OpenStreetMap Nominatim. Search results populate coordinates. You can also type coordinates directly.
- **NOAA station selector:** an automatically generated NOAA tide-station catalog on the GitHub Pages website. The phone UI shows nearby stations within 200 km, with an 80 km initial selection threshold. Manual station ID override also available.
- **International limitations:** You can select a location anywhere, but **tide predictions are currently limited to NOAA-supported stations**. An overseas search may show marine weather without tide predictions. Adding a licensed worldwide tide provider is future work.
- **No AIS, vessel radar or additional sensors.** All forecast information comes over Wi-Fi.

## Original SkyOrb-compatible wiring — unchanged

30-pin ESP-WROOM-32, dual 7-pin 240×240 SPI GC9A01 displays:

| GC9A01 module | Clock, **left** | Tides/Marine, **right** |
| --- | --- | --- |
| GND | GND | GND |
| VCC | **3.3V** | **3.3V** |
| SCL / CLK | GPIO 18 | GPIO 18 |
| SDA / MOSI | GPIO 23 | GPIO 23 |
| RES / RST | GPIO 21 | GPIO 4 |
| DC | GPIO 17 | GPIO 2 |
| CS | GPIO 16 | GPIO 5 |

Keep the supply at 3.3V as required by the modules used in the original build. Confirm your own module electrical specifications. No wiring, module CS/DC or reset pins have been changed.

## Setup from a phone

1. Flash the firmware using the installer, after the new GitHub Actions build succeeds.
2. Join Wi-Fi network **`TideLantern-Setup`** with password **`Tides2026!`** (only when the device is not yet connected to home Wi-Fi).
3. Open **http://192.168.4.1/setup** on your phone. In the Wi-Fi setup page, tap **Scan nearby Wi-Fi** to see a list of reachable **2.4 GHz** hotspots. Use the search box to filter the list, tap your router, enter the password, and select **Save Wi-Fi & restart**. You can also type a hidden network's SSID manually.
4. **After the ESP32 restarts, return your phone to your normal home Wi-Fi.** Look at the **bottom of the left round clock display**: the **SETTINGS PAGE** label shows TideLantern's IP address. Open `http://<that IP>/` in your phone browser (for example `http://192.168.1.123/`, using *your own* displayed address). You can optionally try **http://tidelantern.local/**, but mDNS is not supported on every network.
5. If the setup hotspot reappears instead of the normal clock, verify the password and the 2.4 GHz network. If the dashboard will not open, ensure your phone and TideLantern can reach one another on the same local network (guest networks and some mesh/router subnets may isolate devices).
6. Search for a waterway or enter latitude and longitude, select an appropriate NOAA station, and press **Save waterway & station**.
7. Select the right display's tide/marine mode, your measurement units, and international clock options.

The phone and ESP32 must be able to reach one another on the same local network. TideLantern's local HTTP dashboard is intended for a **trusted home Wi-Fi network**, not the public Internet. If your router assigns TideLantern a new IP later, use the current address shown on the clock; you can reserve its IP in your router settings for a permanent address. Avoid guest networks or phone cellular switching during setup.

## Tide and weather data handling

**NOAA CO-OPS Data API:** `api.tidesandcurrents.noaa.gov/api/prod/datagetter`, `product=predictions`, `interval=hilo`, `time_zone=gmt`, `datum=MLLW`; feet or meters depending on settings. Predictions are refreshed approximately every six hours (more frequently on errors). The high/low events are official predictions; the display's connecting line is **cosine interpolation between those events**, not an official height curve or live gauge reading. We deliberately avoid substituting model tide levels for unavailable official local tides. See [NOAA API documentation](https://api.tidesandcurrents.noaa.gov/api/dev/).

**NOAA Metadata API:** GitHub Actions downloads NOAA's tide-prediction station listing when building the site. If NOAA is unreachable during building, the generator falls back to a small, bundled seed station list. The dashboard displays nearby NOAA stations and explicitly warns if none are available. A selected NOAA station may not represent a physically nearby or safely navigable waterway—verify the station yourself. [NOAA station metadata](https://api.tidesandcurrents.noaa.gov/mdapi/prod/).

**Open-Meteo:** Weather forecast endpoint supplies forecast wind and temperature. Marine endpoint supplies *modeled*, not observed, wave and sea-surface-temperature values. Coastal accuracy and coverage vary. On landlocked areas the marine model may return no data or the closest sea grid cell, not local lake conditions. Providers: [Weather forecast](https://open-meteo.com/en/docs) and [Marine forecast](https://open-meteo.com/en/docs/marine-weather-api). Respect their [licensing and commercial-use terms](https://open-meteo.com/en/terms).

**Nominatim/OpenStreetMap:** Location searches are only performed when the user presses Search, with no autocomplete and a rate limit / short cache. The remote request identifies the project in its User-Agent. OSM © contributors, licensed ODbL. This prototype is for small-scale personal use; large deployments must move to a policy-compliant paid/self-hosted search service. [Nominatim usage policy](https://operations.osmfoundation.org/policies/nominatim/) and [attribution](https://www.openstreetmap.org/copyright).

**Time zone:** the weather endpoint's automatically identified IANA time zone and UTC offset drive the clock. Popular regions are displayed using familiar abbreviations where clear; otherwise the accurate current UTC offset appears. Time zone refreshes with the weather forecast. Upcoming NOAA event times use the *current* UTC offset and can be off by an hour when displayed across a forthcoming daylight-saving change; consult official NOAA local times for decisions.

**No offline forecast guarantee:** tide events and marine forecasts currently live in RAM. An offline power-on cannot reconstruct reliable tide/weather data until the network and services return. The clock needs initial NTP sync after a fresh boot.

**Security:** This preview retrieves read-only public API data using TLS without validating the server certificates (`setInsecure()`). It does not transmit Wi-Fi passwords to third-party APIs, but data integrity cannot be cryptographically guaranteed. Future releases should implement validated trust stores. Do not expose the dashboard to untrusted networks; Wi-Fi credentials are saved in ESP32 NVS. Reset/erase flash to remove stored Wi-Fi credentials when handing the unit to someone else.

**Safety:** NOT FOR NAVIGATION, TIDE-DEPENDENT PASSAGE TIMING, CROSSING BARS, OR MARINE WEATHER SAFETY DECISIONS. Predictions and model forecasts may be unavailable, delayed or wrong. Consult official NOAA/NWS or corresponding local maritime authorities and carry proper onboard equipment.

## GitHub setup (first-time maintainer instructions)

1. Create a **new public GitHub repository** named **`TideLantern`**, under account `Tikishark777`. **Do not modify or rename your SkyOrb repository**.
2. Upload the **contents inside** this starter ZIP into the root of that new repository, keeping `.github/workflows/build.yml` and every directory path intact. When using GitHub's browser **Add file → Upload files**, hidden `.github` folder uploads may require uploading files through its folder path or using GitHub Desktop. Avoid nesting everything inside `TideLantern-Starter/`.
3. Open **Actions**. The build workflow should first generate a station catalog, compile via PlatformIO, then stage installer binaries. On the first run, resolve any compile/library issues before flashing.
4. In the repository **Settings → Pages**, choose **Build and deployment → Source: GitHub Actions**. GitHub Actions will deploy the `web-installer` directory. The website should open at `https://tikishark777.github.io/TideLantern/`.
5. **Do not flash a functioning SkyOrb** until TideLantern builds successfully and you've explicitly decided to replace its firmware (a separate test ESP32 is recommended).
6. Inspect the GitHub Actions run log for successes/errors; if anything fails, send its screenshot or relevant error lines for repair.

## Project structure

```text
TideLantern/
├─ .github/workflows/build.yml   # Build firmware, NOAA catalog & GitHub Pages site
├─ src/main.cpp                 # Standalone marine firmware
├─ include/web_ui.h             # Local phone dashboard
├─ platformio.ini               # ESP32 board and dependencies
├─ scripts/gen_stations.py      # NOAA tide station catalog generator
├─ scripts/prepare_installer.py # Stage built binaries for Web Serial flashing
├─ data/seed_stations.json      # Limited offline build fallback
├─ web-installer/index.html     # Lighthouse-themed installation website
├─ web-installer/manifest.json  # ESP Web Tools flash layout
├─ web-installer/tide-stations.json # Generated/seed catalog
└─ LICENSE
```

## Credits

**Project and testing:** John Christensen (GitHub [@Tikishark777](https://github.com/Tikishark777)). TideLantern is a separate marine application created with AI-assisted coding/design, not a branch or renamed version of SkyOrb.

**Hardware compatibility:** The pinout is deliberately compatible with the original [SkyOrb](https://github.com/GulfCoastMaker/SkyOrb) dual-display design by GulfCoastMaker; this attribution is for the original hardware concept and wiring, not ownership of TideLantern firmware.

**Marine data:** [NOAA CO-OPS](https://tidesandcurrents.noaa.gov/) and [Open-Meteo](https://open-meteo.com/), subject to their usage/licensing terms. **Geocoding:** © [OpenStreetMap contributors](https://www.openstreetmap.org/copyright), via [Nominatim](https://nominatim.org/). **Third-party libraries:** Adafruit GFX, Adafruit GC9A01A, ArduinoJson, ESP Web Tools and PlatformIO, under their own licenses. This project's own new code is MIT licensed; upstream content remains governed by its licenses.
