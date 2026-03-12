# Web UI a HTTP API

## Přehled

`WebUI` startuje `WebServer` na portu 80 a poskytuje SPA stránku i API endpointy.

## UI záložky

- Živý obraz (náhled z `/api/preview.bmp`)
- Konfigurace (Wi-Fi + server)
- Správa (stav zařízení + restart)
- Debug (stream logů z ring bufferu)

## Endpointy

- `GET /` - HTML SPA
- `GET /api/status` - JSON stav (Wi-Fi, IP, RSSI, host, verze, systémové info)
- `GET /api/preview.bmp` - 8bpp BMP z posledního framebufferu
- `GET /api/log?since=N` - inkrementální log stream
- `POST /api/config` - uloží Wi-Fi/server config, následně zkusí reconnect
- `POST /api/restart` - restart zařízení
- `GET /api/wifi/scan` - scan dostupných sítí

## Captive portal routy

Pro klientské OS test URL:
- `/generate_204`, `/gen_204` (Android)
- `/hotspot-detect.html` (iOS/macOS)
- `/connecttest.txt`, `/ncsi.txt` (Windows)
- ostatní požadavky jdou přes `onNotFound()` do captive redirectu při aktivním portálu.

## Preview formát

- Endpoint vrací BMP (`image/bmp`) generovaný za běhu.
- Palette má 8 definovaných barev (indexy 0..7), zbytek 256-entry tabulky je nulový.
- Zdroj dat je poslední framebuffer předaný schedulerem po úspěšném renderu.
