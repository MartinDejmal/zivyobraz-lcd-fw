# Architektura

## Entry point

- Firmware vstupuje přes `setup()` a `loop()` v `src/main.cpp`.
- `setup()` inicializuje: serial, config manager, Wi-Fi manager, protocol service, display, scheduler a web UI.
- `loop()` volá `Scheduler::tick()`, `WebUI::tick()` a periodicky vykreslí status screen, pokud zatím není validní obraz.

## Modulární rozdělení

### Core
- `src/core/boot.*`
- Inicializace serial linky a boot log banneru (FW/API/build/chip/reset reason).

### Configuration
- `src/config/config_manager.*`
- Správa runtime konfigurace (`RuntimeConfig`) a perzistence přes `Preferences` (NVS namespace `zo_cfg`).
- Generuje/validuje 8místný numerický API klíč.

### Networking
- `src/net/wifi_manager.*`
- STA připojení + reconnect logika.
- Fallback do AP+STA režimu s captive DNS (`DNSServer`) při chybějícím SSID nebo neúspěšném connectu.

### Protocol compatibility
- `src/protocol/protocol_compat.*`, `src/protocol/protocol_models.h`, `src/protocol/http_body_stream.*`
- Staví JSON metadata payload, odesílá HTTP/HTTPS POST, parsuje status line + hlavičky.
- Předává body stream callbacku pro dekodér bez plného bufferování payloadu.

### Image pipeline
- `src/image/image_decoder.*`, `src/image/indexed_framebuffer.*`
- Signaturový probe (`PNG/Z1/Z2/Z3`) v okně `probeLimit`.
- Dekódování `Z1/Z2/Z3` do `IndexedFramebuffer`.

### Display
- `src/display/display_hal.h`, `src/display/st7789_display.*`
- HAL rozhraní + ST7789 implementace přes Adafruit ST7789/GFX.
- Render indexovaných barev (0..6) na RGB565 a mapování souřadnic podle rotace.

### Runtime orchestrace
- `src/runtime/scheduler.*`
- Tick-based state machine (interval 90 s), koordinuje sync/decode/render, rozhoduje o commitu timestampu.

### Diagnostics
- `src/diagnostics/*`
- Log makra do serialu + ring bufferu pro web debug.
- Systémové informace (chip/flash/SDK).

### Web UI
- `src/web/web_ui.*`
- HTTP server (port 80), SPA stránka + API endpointy pro status, konfiguraci, preview, logy a restart.

## Datový tok (aktuální implementace)

1. Scheduler při ticku ověří Wi-Fi stav.
2. Při aktivním spojení zavolá `ProtocolCompatService::performSync(...)`.
3. Body callback dekóduje image stream do framebufferu.
4. Při úspěchu se renderuje do display backendu.
5. Timestamp se uloží do konfigurace pouze při úspěšném decode+render scénáři pro změněný obsah.
