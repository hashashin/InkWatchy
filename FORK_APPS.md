# Fork-specific apps

Applications and features added on top of [Szybet/InkWatchy](https://github.com/Szybet/InkWatchy).
Each app can be enabled or disabled with its `#define` in `src/defines/config.h` (not versioned).
Secrets such as private endpoints, passwords, and signing keys belong in `src/defines/confidential.h` (not versioned).

## Summary

| App | Flag (`config.h`) | Where it appears | What it does |
|---|---|---|---|
| QR | `QR_APP` | Utilities | Shows QR codes from a filesystem-backed list |
| RSS reader | `RSS_READER` | Utilities | Reads headlines from a private endpoint |
| HA control | `HA_CONTROL` | Utilities | Sends MQTT commands to Home Assistant |
| Stopwatch | `STOPWATCH` | Utilities | Stopwatch with lap tracking |
| Moon / Sun | `MOON_SUN_APP` | Utilities | Compact lunar phase and solar times view |
| Subnet Pocket Calc | `SUBNET_CALC_APP` | Utilities | Offline IPv4 subnet calculator |
| World Clock Tiles | `WORLD_CLOCK_APP` | Utilities | Offline city/timezone tiles |
| Pinout Wallet | `PINOUT_WALLET_APP` | Utilities | Offline black-and-white pinout cards |
| Apple jokes | `APPLE_JOKE` | Utilities | Eating apples and smashing apples joke apps |
| Chess | `CHESS` | Games menu | Chess game against a small AI |
| FS upload | `FS_UPLOAD` | Settings | HTTP server for uploading files to the watch |
| Presence beacon | `PRESENCE_BEACON` | Settings | BLE iBeacon for Home Assistant presence detection |

---

## QR - `QR_APP`

Displays QR codes from a text list stored in the filesystem (`/qrapp/qrlist.txt`).

- **Files:** `src/ui/places/qrApp/`
- **`qrlist.txt` format:** one entry per line, `Title|content`. Without `|`, the whole line is used as both title and content. `#` marks a comment. The app detects the content type from common prefixes and shows a matching icon: `WIFI:`, `tel:`, `http`, `BEGIN:VCARD` / `MECARD:` for contacts, or plain text.
- **Multiline vCards:** supported on a single line using `\n`, which is converted to CRLF while generating the QR. Example: `Me|BEGIN:VCARD\nVERSION:3.0\nFN:Ivan\nTEL:...\nEND:VCARD`.

## RSS reader - `RSS_READER`

Downloads headlines from a private JSON endpoint and displays them, with a QR code for each item URL.

- **Files:** `src/ui/places/rssReader/`
- **Config:** `RSS_READER_ENDPOINT` (URL), `RSS_READER_CACHE_PATH` (filesystem cache).
- **Expected endpoint format:** `{"src":"...","ts":0,"items":[{"t":"title","u":"url"}, ...]}`
- **Security note:** currently uses TLS without certificate validation (`setInsecure`). See pending work.

## HA control - `HA_CONTROL`

Sends MQTT commands to Home Assistant, signed with a shared secret.

- **Files:** `src/ui/places/haControl/`
- **Secret config (`confidential.h`):** `HA_MQTT_HOST`, `HA_MQTT_PORT`, `HA_MQTT_SECRET`.
- **Current commands:** `printer_off`, `roomba_start`, `roomba_dock` (editable in `haControl.cpp`).
- Status and command strings are localized through `HA_S_*` keys in the language files.

## Stopwatch - `STOPWATCH`

Stopwatch with lap markers.

- **Files:** `src/ui/places/stopWatch/`
- Localized strings: `STOPWATCH_*`.

## Moon / Sun - `MOON_SUN_APP`

Compact offline view for the current date, solar times, and lunar phase.

- **Files:** `src/ui/places/moonSun/`
- **Config:** `WEATHER_LATIT` and `WEATHER_LONGTIT` are reused as the location source. If either value is missing, the app shows a setup hint instead of solar times.
- **Shown data:** local date/time, sunrise, sunset, solar noon, daylight duration, moon phase, moon illumination percentage, and configured location.
- **Dependencies:** uses the existing `MoonPhasePlus` library for lunar phase/illumination and a local solar-time calculation adapted from the existing watchface code.
- **Localization:** app labels use `MOON_SUN_*`; lunar phase labels use `MOON_PHASE_*`. Spanish has compact translated phase names; the other non-English languages currently use English fallbacks.
- **Menu icon:** reuses the existing `weather` image key to avoid adding another filesystem image asset.

## Subnet Pocket Calc - `SUBNET_CALC_APP`

Offline IPv4 subnet calculator for quick network planning.

- **Files:** `src/ui/places/subnetCalc/`
- **Input:** four IPv4 octets and CIDR prefix. `MENU` moves between editable fields; `UP`/`DOWN` changes the selected value by 1; long `UP`/`DOWN` changes it by 10; `BACK` exits through the regular manager flow.
- **Shown data:** netmask, network, broadcast, first host, last host, and usable host count.
- **Assets:** reuses the existing `wifiIcon` menu image; no new bitmap assets.
- **Notes:** uses integer-only calculations and keeps no background task running.

## World Clock Tiles - `WORLD_CLOCK_APP`

Compact offline tiles for configured cities/timezones.

- **Files:** `src/ui/places/worldClock/`
- **Config:** `WORLD_CLOCK_TILES(X)` in `config.h`, one entry per `X("LABEL", offset_minutes)`.
- **Input:** `UP`/`DOWN` changes page when more than four tiles are configured.
- **Shown data:** city label, fixed UTC offset, local time, and compact local date.
- **Notes:** offsets are fixed minutes from UTC; daylight saving time is intentionally not calculated.

## Pinout Wallet - `PINOUT_WALLET_APP`

Offline black-and-white pinout cards for common connectors and boards.

- **Files:** `src/ui/places/pinoutWallet/`
- **Cards:** Watchy buttons, ESP32 DevKit 30-pin basics, common AliExpress ESP32-C3/S3 SuperMini clones, I2C 4-pin, UART 4-pin, SPI 6-pin, and JST battery 2-pin.
- **Input:** `UP`/`DOWN` changes card; `MENU` cycles through groups such as power, I2C, SPI, UART, ADC, and GPIO.
- **Assets:** draws connector outlines and pin labels with display primitives; no bitmap assets.
- **Notes:** designed for quick reference, not exhaustive board documentation.

## Chess - `CHESS`

Chess game against an AI, available from the games menu.

- **Files:** `src/ui/places/chessApp/`, split into:
  - `chessApp.cpp` - app flow and UI
  - `chessRules.cpp` - rules and legal moves
  - `chessAi.cpp` - opponent AI
  - `chessRender.cpp` - board rendering

## FS upload - `FS_UPLOAD`

Starts an HTTP server on the watch to upload files to the filesystem over WiFi.

- **Files:** `src/ui/places/fsUpload/`
- **Config:** `FS_UPLOAD_DEFAULT_DIR`, `FS_UPLOAD_PORT`, `FS_UPLOAD_MAX_UPLOAD_BYTES`.
- **Secrets (`confidential.h`):** `FS_UPLOAD_HTTP_USER`, `FS_UPLOAD_HTTP_PASS`, `FS_UPLOAD_SIGN_KEY`.

## Presence beacon - `PRESENCE_BEACON`

Emits a periodic BLE iBeacon so Home Assistant can detect presence.

- **Files:** `src/ui/places/presenceBeacon/` and `src/services/presenceBeaconSvc.*`
- **Config:** `PRESENCE_BEACON_UUID`, `PRESENCE_BEACON_MAJOR`, `PRESENCE_BEACON_MINOR`, `PRESENCE_BEACON_PERIOD_MS`, `PRESENCE_BEACON_BURST_MS`, `PRESENCE_BEACON_TX_PWR_INDEX`, `PRESENCE_BEACON_DEFAULT_ENABLED`.

---

## Fork-specific watchfaces

Additional watchfaces: `WATCHFACE_PULSEPRO`, `WATCHFACE_ANALOG_PULSEPRO`, and `WATCHFACE_BINWATCH` (binary watch). Their flags live in `config.h`.

## Planned app ideas

Ideas intentionally saved for later implementation:

- **RTC Drift Lab:** combined RTC sanity check and drift estimator.
- **ISS Passes:** upcoming ISS passes, preferably from a small endpoint or precomputed data.
- **Watchy Tamagotchi:** a small RTC-driven pet with minimal persistent state.

## Pending work / ideas

- **Host-side tests** for pure logic such as the `qrlist.txt` parser and RSS parsing.
- **CI** that builds on every push to protect upstream merges.
