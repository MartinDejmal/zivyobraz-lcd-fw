# Konfigurace

## RuntimeConfig struktura

Definice je v `include/project_config.h`:
- `boardName`
- `display` (`type`, `width`, `height`, `pins`, `rotation`)
- `wifi` (`ssid`, `password`)
- `server` (`host`, `useHttps`, `endpointPath`)
- `versions` (`fwVersion`, `apiVersion`)
- `apiKey`
- `lastTimestamp`
- `protocolDebug` (`wireDebug`, `forceTimestampCheckZeroOnMissingTimestamp`)

## Výchozí hodnoty

Nastavuje `ConfigManager::loadDefaults()`:
- board: `esp32dev`
- display: ST7789, 240x240, rotace 0, výchozí SPI pinout (MOSI 23, SCLK 18, DC 16, RST 17, BL 4, CS -1)
- server: `cdn.zivyobraz.eu`, HTTPS zapnuto, path `/index.php`
- Wi-Fi: prázdné SSID/password
- API key: při startu validace; pokud chybí/invalid, vygeneruje se nový 8-digit key

## NVS perzistence

Ukládá se namespace `zo_cfg`, klíče:
- `wifi_ssid`, `wifi_pass`, `host`, `https`, `api_key`, `last_ts`

## Důležité chování

- `lastTimestamp` se z NVS při `load()` záměrně nenačítá.
- `setWifi()` vyžaduje neprázdné SSID; prázdné heslo znamená „ponechat stávající“.
- `setServer()` ponechá původní host, pokud je nový host prázdný.

## Build-time přepínače

`platformio.ini` definuje:
- `ZO_FW_VERSION`, `ZO_API_VERSION`, `ZO_BUILD_DATE`
- feature flagy (`ZO_FEATURE_WIFI`, `ZO_FEATURE_HTTPS`, `ZO_FEATURE_OTA`)
- `PROTOCOL_WIRE_DEBUG`

`PROTOCOL_WIRE_DEBUG=1` zapíná wire debug default už v konfiguraci.
