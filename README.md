# Živý obraz LCD firmware (ESP32 + ST7789)

Firmware pro ESP32 (Arduino / PlatformIO), který se periodicky připojuje k serveru Živý obraz, stahuje obrazová data a vykresluje je na TFT displej ST7789. Projekt obsahuje i lokální webové rozhraní pro konfiguraci, stav zařízení a náhled posledního framebufferu.

Aktuálně jde o funkční základ se serverovou synchronizací, dekódováním formátů Z1/Z2/Z3 a renderem na ST7789. Některé části protokolu a roadmap položky zatím nejsou dokončené (viz omezení níže).

## Hlavní funkce

- Periodický sync přes HTTP/HTTPS `POST /index.php?timestampCheck={0|1}` s hlavičkou `X-API-Key`.
- Parsování řídicích hlaviček (`Timestamp`, `PreciseSleep`, `Rotate`, `PartialRefresh`, `ShowNoWifiError`, `X-OTA-Update`).
- Dekódování obrazových streamů `Z1`, `Z2`, `Z3` do interního indexovaného framebufferu.
- Render framebufferu do ST7789 (včetně rotace podle hlavičky `Rotate`).
- Wi-Fi STA režim + fallback do captive AP portálu při chybějící/selhané konfiguraci.
- Web UI na portu 80 (`/`, `/api/status`, `/api/config`, `/api/preview.bmp`, `/api/log`, `/api/wifi/scan`, `/api/restart`).

## Architektura (high-level)

- `main.cpp`: boot, inicializace modulů, hlavní `loop()`.
- `src/runtime/scheduler.*`: periodický orchestration cyklu Wi-Fi -> protocol sync -> decode -> render -> timestamp commit.
- `src/protocol/*`: request/response kompatibilita a streamování HTTP body.
- `src/image/*`: signaturový probe a dekódování `Z1/Z2/Z3`.
- `src/display/*`: HAL + ST7789 backend.
- `src/net/wifi_manager.*`: STA připojení, reconnect, AP portal a captive DNS.
- `src/config/config_manager.*`: runtime konfigurace + perzistence v NVS (`Preferences`).
- `src/web/web_ui.*`: SPA + REST endpointy pro správu zařízení.

## Konfigurace

Hlavní konfigurační oblasti:
- Wi-Fi (`ssid`, `password`)
- Server (`host`, `useHttps`, endpoint path)
- Display (`width`, `height`, piny, rotace)
- Protokol (`apiKey`, `wireDebug`, diagnostický fallback `timestampCheck=0`)

Výchozí hodnoty jsou v `ConfigManager::loadDefaults()` a část konfigurace je persistována do NVS.

## Getting started

1. Otevřete projekt v PlatformIO.
2. Zkontrolujte `platformio.ini` environment `esp32dev`.
3. Připojte ESP32 (default serial/upload 115200).
4. Nahrajte firmware.
5. Po startu sledujte Serial monitor.
   - Pokud není nastavena Wi-Fi, zařízení spustí captive AP (SSID ve tvaru `<device>-<hex>`, heslo `zivyobraz-setup`).
6. Otevřete web UI na `http://<ip-adresa-zarizeni>/` a nastavte Wi-Fi + server.

## Build / flash / run

Podrobný postup je v `docs/build-and-run.md`.

## Dokumentace

- Rozcestník: [`docs/README.md`](docs/README.md)
- Architektura: [`docs/architecture.md`](docs/architecture.md)
- Konfigurace: [`docs/configuration.md`](docs/configuration.md)
- Runtime + networking: [`docs/runtime-and-networking.md`](docs/runtime-and-networking.md)
- Web UI a API: [`docs/web-ui.md`](docs/web-ui.md)
- Build/flash: [`docs/build-and-run.md`](docs/build-and-run.md)
- Troubleshooting a omezení: [`docs/troubleshooting.md`](docs/troubleshooting.md)

## Aktuální omezení

- PNG je rozpoznán, ale není implementován dekodér.
- `PreciseSleep` je parsován, ale scheduler aktuálně běží na fixním intervalu 90 s.
- `PartialRefresh` je předán do renderu jen jako hint; optimalizovaný partial render není implementován.
- OTA flow není implementován (`ZO_FEATURE_OTA=0`; hlavička `X-OTA-Update` se jen čte).
- `lastTimestamp` se při bootu nenačítá z NVS (po restartu se vždy začíná novou synchronizací).
