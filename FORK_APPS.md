# Apps propias de este fork

Aplicaciones y features añadidas sobre [Szybet/InkWatchy](https://github.com/Szybet/InkWatchy).
Cada una se activa/desactiva con su `#define` en `src/defines/config.h` (no versionado).
Los secretos (endpoints privados, contraseñas, claves de firma) van en `src/defines/confidential.h` (no versionado).

## Resumen

| App | Flag (`config.h`) | Dónde aparece | Qué hace |
|---|---|---|---|
| QR | `QR_APP` | Menú principal | Muestra QRs desde una lista en el filesystem |
| RSS reader | `RSS_READER` | Menú principal | Lee titulares desde un endpoint propio |
| HA control | `HA_CONTROL` | Menú principal | Manda comandos a Home Assistant por MQTT |
| Cronómetro | `STOPWATCH` | Menú principal | Cronómetro con vueltas |
| Ajedrez | `CHESS` | Menú de juegos | Partida contra una IA |
| FS upload | `FS_UPLOAD` | Ajustes | Servidor HTTP para subir archivos al reloj |
| Presence beacon | `PRESENCE_BEACON` | Ajustes | iBeacon BLE para detección de presencia en HA |

---

## QR — `QR_APP`

Muestra códigos QR a partir de una lista de texto en el filesystem (`/qrapp/qrlist.txt`).

- **Archivos:** `src/ui/places/qrApp/`
- **Formato de `qrlist.txt`:** una entrada por línea, `Título|contenido`. Sin `|`, la línea entera es título y contenido. `#` = comentario. Detecta el tipo por el prefijo del contenido y muestra icono: `WIFI:`, `tel:`, `http`, `BEGIN:VCARD` / `MECARD:` (contacto), o texto.
- **vCard multilínea:** se admite en una sola línea usando `\n` (se convierte a CRLF al generar el QR). Ej: `Yo|BEGIN:VCARD\nVERSION:3.0\nFN:Ivan\nTEL:...\nEND:VCARD`.

## RSS reader — `RSS_READER`

Descarga titulares desde un endpoint JSON propio y los muestra (con QR a la URL de cada item).

- **Archivos:** `src/ui/places/rssReader/`
- **Config:** `RSS_READER_ENDPOINT` (URL), `RSS_READER_CACHE_PATH` (caché en FS).
- **Formato esperado del endpoint:** `{"src":"...","ts":0,"items":[{"t":"titulo","u":"url"}, ...]}`
- ⚠️ Usa TLS sin validar certificado (`setInsecure`). Ver pendientes.

## HA control — `HA_CONTROL`

Envía comandos a Home Assistant por MQTT (firmados con un secreto compartido).

- **Archivos:** `src/ui/places/haControl/`
- **Config (secretos, `confidential.h`):** `HA_MQTT_HOST`, `HA_MQTT_PORT`, `HA_MQTT_SECRET`.
- **Comandos actuales:** `printer_off`, `roomba_start`, `roomba_dock` (editables en `haControl.cpp`).
- Los textos de estado/comandos están localizados (`HA_S_*` en los archivos de idioma).

## Cronómetro — `STOPWATCH`

Cronómetro con marcas de vuelta.

- **Archivos:** `src/ui/places/stopWatch/`
- Textos localizados: `STOPWATCH_*`.

## Ajedrez — `CHESS`

Partida de ajedrez contra una IA, en el menú de juegos.

- **Archivos:** `src/ui/places/chessApp/` — separado en:
  - `chessApp.cpp` — flujo/UI de la app
  - `chessRules.cpp` — reglas y movimientos
  - `chessAi.cpp` — IA del rival
  - `chessRender.cpp` — dibujado del tablero

## FS upload — `FS_UPLOAD`

Levanta un servidor HTTP en el reloj para subir archivos al filesystem por WiFi.

- **Archivos:** `src/ui/places/fsUpload/`
- **Config:** `FS_UPLOAD_DEFAULT_DIR`, `FS_UPLOAD_PORT`, `FS_UPLOAD_MAX_UPLOAD_BYTES`.
- **Secretos (`confidential.h`):** `FS_UPLOAD_HTTP_USER`, `FS_UPLOAD_HTTP_PASS`, `FS_UPLOAD_SIGN_KEY`.

## Presence beacon — `PRESENCE_BEACON`

Emite un iBeacon BLE periódico para que Home Assistant detecte presencia.

- **Archivos:** `src/ui/places/presenceBeacon/` + `src/services/presenceBeaconSvc.*`
- **Config:** `PRESENCE_BEACON_UUID`, `PRESENCE_BEACON_MAJOR/MINOR`, `PRESENCE_BEACON_PERIOD_MS`, `PRESENCE_BEACON_BURST_MS`, `PRESENCE_BEACON_TX_PWR_INDEX`, `PRESENCE_BEACON_DEFAULT_ENABLED`.

---

## Watchfaces propias

Además de las apps: `WATCHFACE_PULSEPRO`, `WATCHFACE_ANALOG_PULSEPRO` y `WATCHFACE_BINWATCH` (reloj binario) — flags en `config.h`.

## Pendientes / ideas

- **Seguridad:** validar el certificado TLS en RSS reader (hoy `setInsecure`); auto-apagar el servidor de FS upload tras inactividad.
- **Batería:** auditar que WiFi/BLE no queden activos entre wakes.
- **Tests host-side** para la lógica pura (parser de `qrlist.txt`, RSS, etc.).
- **CI** que compile en cada push (protege los merges con upstream).
- **App nueva candidata:** autenticador TOTP/2FA offline (encaja con el vault + RTC).
