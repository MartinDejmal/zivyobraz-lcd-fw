# Troubleshooting a známá omezení

## Známá omezení (ověřeno v kódu)

1. PNG není dekódován
   - Signatura PNG je detekována, ale dekodér vrací stav „recognized but not implemented yet“.

2. PreciseSleep není použit pro plánování
   - `PreciseSleep` je parsován z hlavičky, ale scheduler používá fixní interval 90 s.

3. OTA není implementováno
   - Přítomnost `X-OTA-Update` se eviduje, ale není žádný download/flash OTA flow.

4. Partial refresh není optimalizován
   - `partialRefresh` se předává do renderu jako hint, ale render běží full redraw.

5. HTTPS certifikáty nejsou validovány
   - `WiFiClientSecure` používá `setInsecure()`.

6. `lastTimestamp` není obnovován po restartu
   - Config manager jej při `load()` z NVS nenačítá.

## Diagnostické body

- Serial log: obsahuje boot banner, Wi-Fi stav, HTTP status a decode/render výsledky.
- Web Debug tab: čte logy z ring bufferu (`/api/log`).
- Protocol wire debug:
  - build flag `PROTOCOL_WIRE_DEBUG=1` nebo runtime `protocolDebug.wireDebug`
  - zobrazí raw status line, headers a body preview.

## Časté problémy

- **Zařízení se nepřipojí do Wi-Fi:**
  - Ověřte SSID/heslo přes web konfiguraci.
  - Pokud connect selhává, zařízení zůstane v AP portálovém režimu.

- **Server vrací HTTP 200, ale bez aktualizace:**
  - Zkontrolujte, zda je přítomen `Timestamp` header.
  - Při missing timestampu je odpověď klasifikována jako protokolová nekompatibilita.

- **Preview není dostupný (`404 No preview available yet`):**
  - Preview se naplní až po prvním úspěšném decode+render cyklu.
