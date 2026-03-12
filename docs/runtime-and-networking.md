# Runtime a networking flow

## Boot a hlavní smyčka

- `setup()` inicializuje moduly v pořadí: serial -> config -> wifi -> protocol -> display -> scheduler -> web UI.
- Scheduler provede při startu i okamžitý pokus o Wi-Fi connect.
- `loop()` volá scheduler a web server tick, plus fallback status screen.

## Scheduler

- Tick interval je pevný: `kTickIntervalMs = 90000` (90 s).
- Při každém ticku:
  1. aktualizace Wi-Fi stavu,
  2. pokud není Wi-Fi connected: sync se přeskočí,
  3. pokud je connected: spustí se protocol sync + decode/render callback,
  4. podle výsledku se rozhodne o `lastTimestamp` commitu.

### Timestamp commit policy

- `SuccessChanged` + `bodyPresent` + `decode.success` + `renderSuccess` -> timestamp se uloží.
- Při decode/render chybě nebo chybějícím body se timestamp neukládá.
- `SuccessUnchanged` bez body -> render skip.

## Wi-Fi flow

`WifiManager`:
- Pokud je SSID prázdné: přechod do captive AP režimu.
- Pokud STA connect selže: fallback do captive AP.
- AP režim používá:
  - SSID `<deviceName>-<last4hex>`
  - heslo `zivyobraz-setup`
  - IP `192.168.4.1`
  - DNS wildcard redirect přes `DNSServer`
- Reconnect pokusy běží periodicky (`kReconnectIntervalMs = 10000`).

## Protocol sync

`ProtocolCompatService::performSync(...)`:
- Vyžaduje aktivní Wi-Fi.
- Validuje request metadata a API key.
- Odesílá `POST` na `<endpointPath>?timestampCheck=...`.
- Hlavičky requestu: `Host`, `X-API-Key`, `Content-Type: application/json`, `Content-Length`.
- Pro HTTPS používá `WiFiClientSecure::setInsecure()`.

### Parsované response hlavičky

- `Timestamp`
- `PreciseSleep`
- `Rotate`
- `PartialRefresh`
- `ShowNoWifiError`
- `X-OTA-Update`

### Diagnostika a klasifikace

- Podporován wire debug log (raw status line, raw headers, payload summary, body preview).
- `HTTP 200 + body + missing Timestamp` je explicitní protocol error (`missing_ts`).
- Volitelný diagnostický fallback: jednorázové `timestampCheck=0` po chybějícím timestampu.

## Decode + render

- Probe signatury až do limitu 4096 B (`kProbeLimit`).
- Podporované dekódy: `Z1`, `Z2`, `Z3`.
- `PNG` je rozpoznán, ale vrací `RecognizedNotImplemented`.
- Render path: indexovaný framebuffer -> ST7789 backend (`renderIndexed`).
- `PartialRefresh` je aktuálně pouze logovaný hint.
