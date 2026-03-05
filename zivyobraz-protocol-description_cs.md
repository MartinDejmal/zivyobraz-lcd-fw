# Komunikační protokol firmware ↔ server (Živý obraz FW 3.0)

Tento dokument popisuje síťovou komunikaci firmware tak, aby šla implementovat na jiném zařízení při zachování existující serverové části.

## 1) Transportní vrstva

- Protokol: HTTP/1.1 přes TCP.
- Port:
  - `443` při HTTPS (výchozí),
  - `80` při HTTP (`USE_CLIENT_HTTP`).
- Cílový host ve FW: `cdn.zivyobraz.eu`.
- Cesta endpointu: `POST /index.php?timestampCheck={0|1}`.
- TLS: FW používá `setInsecure()` (neověřuje certifikát serveru).

## 2) Autorizace zařízení

- Každé zařízení posílá hlavičku `X-API-Key`.
- Hodnota je 8místné číslo (bez úvodních nul), generované lokálně při prvním startu, uložené v NVS (`Preferences`, klíč `apikey`) a cachované přes deep sleep v RTC RAM.
- Prakticky je to identifikátor/autentizační token zařízení, který musí server akceptovat.

## 3) Dvoufázový režim stahování

Firmware používá dvě varianty požadavku:

1. **Kontrola změny (`timestampCheck=1`)**
   - Cíl: zjistit, jestli je novější obsah.
   - Server má vracet hlavičky včetně `Timestamp`.
   - Pokud je obsah nový, FW může:
     - buď spojení zavřít a udělat druhý request,
     - nebo (v direct-streaming režimu) nechat spojení otevřené a číst body rovnou jako obrazová data.

2. **Stažení obrazu (`timestampCheck=0`)**
   - Cíl: stáhnout image payload (PNG/Z1/Z2/Z3).
   - V paged režimu se tento request opakuje pro každou stránku displeje.

> Důležité: při `timestampCheck=1` musí server při změně obsahu vrátit nejen hlavičky, ale i validní image body (pro direct-streaming cestu).

## 4) Request (co zařízení posílá)

### 4.1 HTTP metoda a hlavičky

- Metoda: `POST`
- URL: `/index.php?timestampCheck=1` nebo `/index.php?timestampCheck=0`
- Povinné hlavičky:
  - `Host: <server>`
  - `X-API-Key: <8digit>`
  - `Content-Type: application/json`
  - `Content-Length: <len>`
  - `Connection: close`

### 4.2 JSON payload

FW posílá JSON s metadaty zařízení. Pole `lastDownloadDuration`, `lastRefreshDuration` a `sensors` jsou volitelná (posílají se pouze pokud jsou k dispozici).

```json
{
  "fwVersion": "3.0",
  "apiVersion": "3.0",
  "buildDate": "<BUILD_DATE>",
  "board": "<boardType>",
  "system": {
    "cpuTemp": <float>,
    "resetReason": "<string>",
    "vccVoltage": <float>
  },
  "network": {
    "ssid": "<wifi-ssid>",
    "rssi": <int>,
    "mac": "AA:BB:CC:DD:EE:FF",
    "apRetries": <uint8>,
    "ipAddress": "<ipv4>",
    "lastDownloadDuration": <ms>
  },
  "display": {
    "type": "<displayType>",
    "width": <int>,
    "height": <int>,
    "colorType": "BW|3C|GRAYSCALE|7C",
    "lastRefreshDuration": <ms>
  },
  "sensors": [
    {
      "type": "SHT4X|BME280|SCD4X|STCC4",
      "temp": <float>,
      "hum": <int>,
      "pres": <int>,
      "co2": <int>
    }
  ]
}
```

Poznámky k senzorům:
- `pres` a `co2` se vzájemně vylučují dle typu senzoru.
- Celé pole `sensors` je přítomné pouze pokud je FW kompilovaný se `SENSOR` a měření je validní.

## 5) Response (co server musí vracet)

## 5.1 Status

- Úspěch: `HTTP/1.1 200 OK` (akceptováno i `HTTP/1.0 200 OK`).
- Jiný status FW bere jako chybu a použije fallback sleep.

## 5.2 Řídicí hlavičky

Hlavičky jsou case-sensitive podle implementace (`startsWith(...)`), proto je doporučen přesný zápis:

- `Timestamp: <unix/int>`
  - Používá se pro rozhodnutí, zda je nový obsah.
  - Pokud je stejný jako poslední uložený timestamp, FW refresh neprovede.

- `PreciseSleep: <seconds>`
  - Nastavuje interval dalšího probuzení (i když není nový obsah).

- `Rotate: <int>`
  - Pokud přítomné, FW nastaví rotaci displeje (ve FW je prakticky použita rotace o 180°).

- `PartialRefresh: <libovolná hodnota>`
  - Samotná přítomnost hlavičky aktivuje požadavek na partial refresh.

- `ShowNoWifiError: 0|1`
  - Řídí, zda má FW při ztrátě Wi-Fi zobrazit chybovou obrazovku.

- `X-OTA-Update: <url>`
  - Pokud přítomné, FW po kontrole update spustí OTA update z dané URL místo renderu obrazu.

## 5.3 Body odpovědi

- Body je stream obrazových dat bez dalšího JSON wrapperu.
- FW po hlavičkách čte raw bytes a hledá signaturu formátu.
- Pokud body začíná textem/chybou (např. PHP warning), FW prohledá až prvních 4096 B a hledá validní signaturu.

Podporované formáty:

1. **PNG**
   - Standardní PNG stream.
   - Signatura detekce: první dva byty `0x89 0x50`.

2. **Z1** (RLE)
   - Signatura: ASCII `Z1` (`0x5A 0x31`).
   - Následuje sekvence dvojic:
     - `color` (1 B),
     - `count` (1 B).

3. **Z2** (RLE packed)
   - Signatura: ASCII `Z2` (`0x5A 0x32`).
   - Každý běh je 1 B:
     - b7..b6 = `color` (2 bity),
     - b5..b0 = `count` (6 bitů).

4. **Z3** (RLE packed)
   - Signatura: ASCII `Z3` (`0x5A 0x33`).
   - Každý běh je 1 B:
     - b7..b5 = `color` (3 bity),
     - b4..b0 = `count` (5 bitů).

Mapování barev ve FW:
- `0` = bílá,
- `1` = černá,
- `2` = druhá barva (red/yellow nebo lightgrey podle typu displeje),
- `3` = třetí barva (yellow/darkgrey podle typu displeje),
- u 7C navíc `4`=green, `5`=blue, `6`=orange.

Dekódování probíhá po pixelech v pořadí řádků (zleva doprava, shora dolů).

## 6) Rozhodovací logika klienta

- Uložený timestamp je per-device v RTC RAM.
- Pokud `Timestamp` v odpovědi == uložený timestamp, neprovádí se refresh obrazu.
- Pokud je timestamp nový, FW uloží nový timestamp a stáhne/renderuje image.
- Počet neúspěšných Wi-Fi pokusů (`apRetries`) se posílá serveru a zároveň ovlivňuje fallback sleep na klientovi.

## 7) Timeouty, retry a robustnost

- Připojení na server: až 3 pokusy.
- Čekání na první data odpovědi: ~10 s.
- Čtení streamu:
  - idle timeout bez dat: 5 s,
  - celkový timeout: 30 s.
- Při nekompletním image FW v některých cestách akceptuje >=95 % pixelů jako validní obraz.

## 8) Minimální serverový kontrakt (pro implementaci jiného klienta)

Pokud chcete vytvořit jiné zařízení kompatibilní se stávajícím serverem, zachovejte minimálně:

1. `POST /index.php?timestampCheck=...`.
2. Hlavičku `X-API-Key` (stabilní identita zařízení).
3. JSON body se základními poli (`fwVersion`, `apiVersion`, `board`, `system`, `network`, `display`).
4. Response `200 OK` + hlavičky `Timestamp` a `PreciseSleep`.
5. Response body jako raw PNG nebo Z1/Z2/Z3 stream.

Doporučené pro plnou kompatibilitu:
- hlavičky `Rotate`, `PartialRefresh`, `ShowNoWifiError`, `X-OTA-Update`.
- schopnost vracet image body i při `timestampCheck=1` (kvůli direct streamingu).

## 9) Ukázka response z pohledu serveru

```http
HTTP/1.1 200 OK
Content-Type: application/octet-stream
Timestamp: 1739999999
PreciseSleep: 300
Rotate: 2
PartialRefresh: 1
ShowNoWifiError: 1

<raw image bytes: PNG nebo Z1/Z2/Z3>
```

## 10) Poznámky k interoperabilitě

- FW neočekává chunk metadata na aplikační úrovni, jen čistý HTTP body stream.
- Při chybách vygenerovaných před image payloadem (PHP warning) má klient omezenou toleranci díky skenu prvních 4 KB.
- Server by měl vracet konzistentní timestamp per obsah; timestamp je hlavní mechanismus cache/refresh rozhodnutí.
