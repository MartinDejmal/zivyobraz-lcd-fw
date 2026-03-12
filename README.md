# Koncepce firmwaru pro ESP32 + TFT SPI klienta služby Živý obraz

Tento dokument navrhuje architekturu firmwaru pro zařízení s **ESP32-xxx** a **TFT displejem přes SPI**, které bude fungovat jako koncový klient služby **Živý obraz** při zachování 100% kompatibility se stávajícím serverem (historicky určeným pro ePaper firmware).

---

## 1. Cíle návrhu

1. **Plná serverová kompatibilita**
   - Zachovat stejný transport, endpointy, hlavičky, payload i význam odpovědí jako u původního klienta.
   - Implementovat podporu formátů obrazu PNG/Z1/Z2/Z3 a všech řídicích HTTP hlaviček.

2. **Konfigurovatelný hardware**
   - Umožnit volbu typu desky, typu TFT kontroleru, rozlišení, barevnosti i pin-mapy bez změn serveru.

3. **Webové rozhraní na zařízení**
   - Lokální UI pro základní konfiguraci.
   - Náhled aktuálně zobrazeného obsahu displeje (framebuffer preview).

4. **Provozní robustnost**
   - Bezpečné fallbacky při chybách sítě/dat.
   - OTA aktualizace (včetně respektování serverové hlavičky `X-OTA-Update`).

---

## 2. Kompatibilita se serverem Živý obraz (100%)

Firmware musí implementovat **stejný komunikační kontrakt** jako původní ePaper klient:

- HTTP/HTTPS komunikace (POST).
- Endpoint: `POST /index.php?timestampCheck={0|1}`.
- Hlavička `X-API-Key` jako stabilní identifikátor zařízení.
- JSON payload se sekcemi `fwVersion`, `apiVersion`, `board`, `system`, `network`, `display` (+ volitelné senzory).
- Zpracování hlaviček odpovědi: `Timestamp`, `PreciseSleep`, `Rotate`, `PartialRefresh`, `ShowNoWifiError`, `X-OTA-Update`.
- Dekódování obrazového body streamu: PNG, Z1, Z2, Z3.

> TFT klient se vůči serveru bude chovat jako drop-in náhrada: server nemusí poznat, že cílem není ePaper.

### 2.1 Klíčové pravidlo kompatibility

Síťová vrstva, payload schema a interpretační logika odpovědí budou implementovány jako samostatný modul **`protocol_compat`**, který se nemění dle typu displeje. Odlišnosti TFT/ePaper se řeší až v render vrstvě.

---

## 3. Vysoká architektura firmwaru

Navržená modulární struktura:

1. **`core/boot`**
   - start zařízení, init NVS, watchdog, logování, reset reason.

2. **`net/wifi_manager`**
   - STA připojení, reconnect logika, RSSI, IP stav.
   - AP fallback (volitelně) pro prvotní konfiguraci.

3. **`protocol_compat`**
   - tvorba requestů (hlavičky + JSON metadata),
   - parsování odpovědí a řídicích hlaviček,
   - timestamp rozhodovací logika,
   - retry/timeout policy.

4. **`image_decoder`**
   - detekce signatury streamu,
   - dekódování PNG/Z1/Z2/Z3 do interního framebufferu,
   - toleranční režim při částečně poškozeném streamu.

5. **`display_hal`**
   - abstrakce nad TFT knihovnou/driverem,
   - převod palety barev Živý obraz -> fyzické barvy panelu,
   - push framebufferu přes SPI,
   - rotace, orientace, případné „partial refresh“ mapované jako dirty regions.

6. **`config_manager`**
   - perzistence konfigurace v NVS,
   - validace hodnot,
   - schema verzování konfigurace.

7. **`web_ui`**
   - HTTP server na zařízení,
   - stránky pro konfiguraci,
   - endpointy pro náhled framebufferu,
   - základní diagnostika.

8. **`ota_manager`**
   - lokální OTA (upload firmware),
   - vzdálená OTA přes URL z `X-OTA-Update`.

9. **`scheduler/power`**
   - plánování cyklu fetch-render-sleep,
   - respektování `PreciseSleep`,
   - light/deep sleep strategie dle typu zařízení.

---

## 4. Datový tok (runtime)

1. Boot + načtení konfigurace.
2. Připojení k Wi-Fi.
3. Odeslání `timestampCheck=1` requestu.
4. Parsování hlaviček:
   - pokud stejný `Timestamp` => bez refresh, přejít do sleep dle `PreciseSleep`.
   - pokud nový `Timestamp` => stáhnout/načíst obrazová data (podle zvoleného režimu 1- nebo 2-fázově).
5. Dekódování PNG/Z1/Z2/Z3 do framebufferu.
6. Render na TFT přes SPI.
7. Uložení runtime metrik (`lastDownloadDuration`, `lastRefreshDuration`) pro další request.
8. Sleep a opakování.

---

## 5. Konfigurace desky a displeje

Konfigurace bude rozdělena na:

### 5.1 Build-time profil (volitelné)

- `board_profile` (např. esp32dev, esp32-s3, custom),
- výchozí pinout SPI,
- výchozí driver displeje.

Výhoda: rychlá kompilace připravených variant.

### 5.2 Runtime konfigurace (NVS + Web UI)

Povinné položky:

- **Deska**: model MCU/board profilu.
- **Displej**:
  - řadič (např. ILI9341/ST7789/GC9A01),
  - šířka, výška,
  - orientace/rotace,
  - barevný režim (interně mapovaný na `BW|3C|GRAYSCALE|7C` pro serverový payload).
- **SPI parametry**:
  - piny MOSI/SCLK/CS/DC/RST/BL,
  - frekvence SPI,
  - invertace barev (pokud panel vyžaduje).
- **Síť**:
  - SSID, heslo,
  - server host (default `cdn.zivyobraz.eu`),
  - HTTP/HTTPS mód.

### 5.3 Validace konfigurace

- kontrola konfliktů pinů,
- kontrola rozsahů rozlišení,
- test inicializace displeje při uložení,
- rollback na poslední funkční konfiguraci při boot failu.

---

## 6. Webové rozhraní na desce

Minimální web UI (SPA nebo jednoduché server-rendered stránky):

1. **Dashboard**
   - stav Wi-Fi, IP, RSSI,
   - API key,
   - poslední timestamp,
   - poslední časy download/refresh,
   - uptime, reset reason.

2. **Konfigurace**
   - síťové parametry,
   - board/display/SPI konfigurace,
   - intervaly a provozní volby.

3. **Náhled displeje**
   - endpoint `/preview` vracející JPEG/PNG z interního framebufferu,
   - endpoint `/preview/live` (periodické obnovování),
   - volitelně překryv diagnostiky (FPS renderu, velikost posledního payloadu).

4. **Servis**
   - restart,
   - factory reset konfigurace,
   - OTA upload.

### 6.1 Bezpečnost web UI

- výchozí AP provisioning režim chráněný heslem,
- v STA režimu volitelná HTTP Basic Auth,
- CSRF token pro POST operace,
- možnost úplně vypnout web UI po dokončení instalace.

---

## 7. Render strategie pro TFT

Aby byla zachována kompatibilita se serverem, ale zároveň efektivní provoz na TFT:

- interní framebuffer bude mít logické barvy služby Živý obraz (indexy 0..6),
- render backend převede indexové barvy do RGB565 dle konkrétního panelu,
- `Rotate` hlavička se aplikuje před push na displej,
- `PartialRefresh` se interpretuje jako preferovaný režim aktualizace dirty regions (když HW/driver umí).

Pozn.: Oproti ePaper není nutná stejná fyzická semantika partial refresh; důležité je, aby vizuální výsledek odpovídal dodanému obrazu.

---

## 8. Spolehlivost a provoz

- Retry připojení k serveru minimálně 3x.
- Timeouty kompatibilní s původním klientem.
- Tolerance rušivých dat před signaturou obrázku (scan prvních 4 KB).
- Watchdog pro síťové i render operace.
- Ring buffer logů dostupný přes web UI.

---

## 9. Doporučená implementační roadmapa

### Fáze 1: Kompatibilní síťové jádro
- Implementovat `protocol_compat` a validovat proti test serveru.
- Ověřit hlavičky, timestamp logiku, sleep intervaly.

### Fáze 2: Decoder + TFT render
- Přidat PNG/Z1/Z2/Z3 dekodéry.
- Integrace s jedním referenčním TFT (např. ST7789).

### Fáze 3: Konfigurace + Web UI
- NVS schema, validační pravidla.
- UI stránky + preview endpoint.

### Fáze 4: Multi-board/multi-display
- Rozšířit driver tabulky o další kontrolery.
- Profilový systém a kompatibilitní test matrix.

### Fáze 5: Produkční hardening
- OTA, autentizace UI, burn-in testy, telemetrie chyb.

---

## 10. Testovací strategie

1. **Protokolová kompatibilita**
   - golden testy HTTP request/response vůči referenčním dumpům.
2. **Dekódování formátů**
   - unit testy pro PNG/Z1/Z2/Z3 na známých vzorcích.
3. **Render korektnost**
   - porovnání referenčního framebufferu a screenshotu displeje.
4. **Konfigurace**
   - validační testy NVS schématu a migration testy.
5. **End-to-end**
   - scénáře „nový timestamp“, „stejný timestamp“, „server error“, „OTA trigger“.

---

## 11. Shrnutí

Navržený firmware odděluje **serverově-kompatibilní komunikační vrstvu** od **hardware-specifické render vrstvy TFT**. Tím je možné zachovat 100% zpětnou kompatibilitu se službou Živý obraz, a současně nabídnout moderní správu zařízení přes vestavěné webové rozhraní, včetně náhledu aktuálního obsahu displeje a konfigurace desky/displeje.

---

## 10. Firmware scaffold (PlatformIO, ESP32 DevKit + ST7789) - Phase 1 baseline

Tento repozitář nyní obsahuje inicializační firmware scaffold v C++ pro PlatformIO (`esp32dev`, Arduino framework), připravený pro postupné doplnění plné kompatibility se serverovým protokolem Živý obraz.

### Aktuální účel

- Spustitelný základ s jasně oddělenými vrstvami:
  - boot/core
  - config manager
  - wifi manager
  - protocol compatibility facade
  - image decoder interface
  - display HAL (ST7789)
  - runtime scheduler
  - diagnostics
- Po flashnutí se inicializuje Serial i ST7789, vykreslí se diagnostická obrazovka a periodicky běží scheduler tick.

### Struktura modulů

- `src/core` - start systému, boot banner, reset reason helper.
- `src/config` - runtime konfigurace a výchozí nastavení pro `esp32dev + ST7789 240x240`.
- `src/net` - Wi-Fi manager abstraction (zatím compile-safe stub).
- `src/protocol` - modely request/response + scaffold pro server kompatibilní tok.
- `src/image` - detekce formátu + decoder rozhraní pro PNG/Z1/Z2/Z3.
- `src/display` - display HAL a ST7789 backend, status screen + test primitives.
- `src/runtime` - jednoduchý periodický state machine připravený pro fetch-render-sleep.
- `src/diagnostics` - log makra, systémové info helpery a kruhový log buffer pro web UI.
- `src/web` - vestavěný HTTP server (port 80) s webovým rozhraním (4 záložky).

### Doporučené další kroky

1. Implementovat reálný HTTPS POST flow (`X-API-Key`, `/index.php?timestampCheck=`) v `protocol_compat`.
2. Doplnit parsování odpovědních hlaviček (`Timestamp`, `PreciseSleep`, `Rotate`, `PartialRefresh`, `ShowNoWifiError`, `X-OTA-Update`).
3. Napojit HTTP body stream na `image_decoder` facade.
4. Implementovat reálné dekodéry PNG/Z1/Z2/Z3 do `PixelSink`.
5. Přidat NVS perzistenci do `config_manager` a STA/AP fallback režimy ve `wifi_manager`.
6. Rozšířit scheduler o řízený fetch-render-sleep cyklus včetně deep sleep strategie.

## 11. Milestone update: stream decode + indexed framebuffer render (current step)

Nově je implementováno:

- Streamové předání HTTP body z `protocol_compat` do dekódovací pipeline přes `HttpBodyStream` bez ukládání celého payloadu do velkého `String` bufferu.
- Tolerantní signaturový probe (okno až 4096 B) pro `PNG`, `Z1`, `Z2`, `Z3` v `image_decoder` s reportem formátu i offsetu signatury.
- Interní `IndexedFramebuffer` (`uint8_t` indexy, contiguous storage, bounds-checked access) nezávislý na ST7789 driveru.
- Dekódování formátů `Z1`, `Z2`, `Z3` do interního framebufferu (row-major), včetně robustního hlášení chyb:
  - truncated stream
  - zero-length run
  - unsupported color index
  - pixel overflow
- `PNG` je nyní korektně rozpoznáno a vrací stav „recognized but not implemented yet“.
- Render cesta z indexového framebufferu do ST7789:
  - deterministické mapování barev 0..6 -> RGB565
  - aplikace `Rotate` až ve fázi renderu
  - `PartialRefresh` je propisováno jako hint (TODO pro další optimalizace)
- Timestamp commit policy v scheduleru:
  - beze změny timestampu: bez decode/render/commit
  - při chybě decode/render: bez commit
  - commit do NVS pouze po úspěšném decode + render
- Rozšířená diagnostická obrazovka: Wi-Fi, API key, host, protocol status, detected format, signature offset, decode result, decoded pixels, stored/candidate timestamp, render result.

### Aktuální omezení

- PNG dekodér není implementován (jen detekce + explicitní stav).
- OTA download/execution, deep sleep politika, AP provisioning a web UI nejsou v tomto kroku implementovány.
- Render běží jako full redraw (bez dirty-region optimalizace).

### Přesně doporučený další krok

1. Přidat PNG dekodér do stejného `ImageDecoderFacade` kontraktu.
2. Po stabilizaci PNG doplnit sleep policy (`PreciseSleep` + deep sleep) a navázat partial refresh optimalizace v render backendu.
3. Následně řešit OTA execution flow s bezpečným rollbackem.

## 12. Milestone update: protocol wire diagnostics + missing Timestamp classification

V tomto kroku byla doplněna cílená diagnostika protokolu (bez uvolnění produkčních pravidel):

- Přidán debug přepínač `PROTOCOL_WIRE_DEBUG` (build flag) / runtime `protocolDebug.wireDebug` pro zapnutí detailních wire logů.
- V debug režimu firmware loguje:
  - raw HTTP status line,
  - raw response headers (řádek po řádku),
  - parsed interpretaci známých hlaviček,
  - počet hlaviček a detekci oddělovače header/body,
  - request payload JSON + host/path/transport/content-length/API key/`timestampCheck`.
- Přidán bezpečný body preview (max 256 B) v HEX + ASCII a diagnostická klasifikace signatury (`PNG`, `Z1`, `Z2`, `Z3`, `HTML`, `text`, `unknown`) včetně offsetu.
- Stav `HTTP 200 + body + chybějící Timestamp` je nově explicitně klasifikován jako protokolová nekompatibilita (`missing_ts`), nikoli jako běžný no-change scénář.
- Debug-only probe při chybějícím `Timestamp` je ne-destruktivní: necommituje timestamp a nezměkčuje produkční rozhodnutí.
- Přidán volitelný diagnostický fallback mód: po `timestampCheck=1` odpovědi bez `Timestamp` lze vynutit následující cyklus s `timestampCheck=0` (`forceTimestampCheckZeroOnMissingTimestamp`).
- Diagnostická obrazovka zobrazuje navíc: protocol debug on/off, protokolovou klasifikaci, missing Timestamp flag, body probe klasifikaci a offset.

### Pravděpodobný další krok

Porovnat skutečný wire dump (raw status/headers/body preview) s očekávaným serverovým kontraktem a potvrdit, zda problém vzniká na straně serverové odpovědi nebo ve formátu hlaviček/flow režimu `timestampCheck`.

## 13. Milestone update: request schema hardening + API key preflight

V tomto kroku byl zpřísněn request kontrakt směrem k serverovému protokolu:

- Payload `board` je nyní posílán jako skalární string (ne jako objekt).
- `network.ip` byl nahrazen `network.ipAddress` a payload obsahuje i `network.apRetries`.
- `display` metadata jsou rozšířena na `type`, `width`, `height`, `colorType` (pro ST7789 nyní `"7C"`).
- `system` již neposílá nekompatibilní `name`; posílají se jen protokolově kompatibilní pole (aktuálně `resetReason`, pokud je k dispozici).
- API key lifecycle byl zpřesněn:
  - validace přesně 8 číslic,
  - při chybě/missing stavu regenerace + persistence,
  - jasné logování, zda byl key načten nebo regenerován.
- Přidána preflight validace requestu před odesláním (`apiKey`, `board`, `network.ipAddress`, `display.width/height/colorType`).
  - Při validační chybě se request neodešle.
- Wire debug log nově obsahuje kompaktní schema summary (`boardKind`, `systemFields`, `networkFields`, `displayFields`, `apiKeyValid`).

---

## 14. Milestone update: webové rozhraní (Živý obraz / Konfigurace / Správa / Debug)

Nově je implementováno kompletní vestavěné webové rozhraní (`src/web/web_ui.h`, `src/web/web_ui.cpp`) běžící na portu **80** přímo v ESP32 HTTP serveru (Arduino `WebServer`).

### Záložky webového rozhraní

| Záložka | URL fragment | Popis |
|---------|-------------|-------|
| 📷 **Živý obraz** | `#live` | Náhled aktuálního obrazu zobrazeného na LCD (BMP, automaticky se obnovuje každých 30 s) |
| ⚙️ **Konfigurace** | `#cfg` | Editace WiFi SSID/hesla, adresy serveru a HTTPS přepínače; změny jsou okamžitě uloženy do NVS |
| 🔧 **Správa** | `#adm` | Přehled verzí FW/API, čip, flash, SDK, uptime, volná RAM; tlačítko restart |
| 🐛 **Debug** | `#dbg` | Real-time výpis všech log zpráv (polling 1 s); zachovává i výstup na sériový port |

### HTTP API endpointy

| Metoda | Endpoint | Popis |
|--------|----------|-------|
| `GET` | `/` | SPA HTML stránka |
| `GET` | `/api/status` | JSON se stavem WiFi, IP, RSSI, konfigurací, systémovými informacemi |
| `GET` | `/api/preview.bmp` | Aktuální obraz jako 8bpp Windows BMP (velikost závisí na rozlišení displeje) |
| `GET` | `/api/log?since=N` | JSON `{ "lines": [...], "next": N }` s novými log řádky od indexu N |
| `POST` | `/api/config` | Uloží konfiguraci WiFi/server (`application/x-www-form-urlencoded`) |
| `POST` | `/api/restart` | Restartuje zařízení |

### Přístup k rozhraní

Po připojení zařízení do WiFi otevřete v prohlížeči:

```
http://<IP_ADRESA_ZARIZENI>/
```

IP adresu zařízení zjistíte na LCD diagnostické obrazovce nebo ze sériového portu (115 200 Bd).

### Screenshot webového rozhraní

```
┌────────────────────────────────────────────────────────────────┐
│ 📺 Živý obraz – LCD panel                                      │
├────────────────────────────────────────────────────────────────┤
│ WiFi: connected | IP: 192.168.1.42 | RSSI: -67 dBm | API: ... │
├──────────┬──────────────┬──────────┬──────────────────────────┤
│📷Živý ob.│⚙️Konfigurace │🔧Správa  │🐛Debug                   │
├──────────┴──────────────┴──────────┴──────────────────────────┤
│                                                                │
│         [  aktuální obraz z LCD 240×240 px  ]                 │
│                                                                │
│                    [ 🔄 Obnovit nyní ]                        │
└────────────────────────────────────────────────────────────────┘
```

> Skutečný screenshot je dostupný v prohlížeči po připojení k běžícímu zařízení na `http://<IP>/`.

### Technické detaily implementace

- **`src/web/web_ui.h` / `web_ui.cpp`** – třída `WebUI` (ESP32 Arduino `WebServer` na portu 80).
- **`src/diagnostics/log_buffer.h` / `log_buffer.cpp`** – kruhový buffer posledních 100 log řádků; `gLogBuffer` je globální singleton dostupný z log makra.
- **`src/diagnostics/log.h` / `log.cpp`** – log makra nyní zapisují do Serialu **i** do kruhového bufferu; sériový výstup je beze změny.
- Náhled obrazu (`/api/preview.bmp`) je generován jako 8bpp Windows BMP přímo z posledního `IndexedFramebuffer` (volá `Scheduler::tick()` po každém úspěšném renderu přes `WebUI::updatePreview()`).
- Konfigurace WiFi a serveru uložená přes web UI se projeví po restartu zařízení.
