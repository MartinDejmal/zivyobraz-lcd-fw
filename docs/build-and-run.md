# Build, flash a spuštění

## Požadavky

- PlatformIO (CLI nebo VS Code extension)
- ESP32 deska kompatibilní s `board = esp32dev`
- ST7789 připojený dle pinů z výchozí konfigurace nebo vlastního nastavení

## PlatformIO konfigurace

`platformio.ini` obsahuje jeden environment:
- `[env:esp32dev]`
- framework: Arduino
- knihovny: `Adafruit GFX`, `Adafruit ST7735 and ST7789`
- C++17

## Typický workflow

```bash
platformio run
platformio run -t upload
platformio device monitor -b 115200
```

## Zprovoznění zařízení

1. Flash firmware.
2. Otevřít serial monitor (`115200`).
3. Pokud není STA konfigurace, zařízení spustí AP portal.
4. Otevřít `http://<ip>` (v AP režimu typicky `http://192.168.4.1/`).
5. Nastavit SSID/heslo a server host + HTTPS.
6. Po úspěšném připojení běží periodický sync přes scheduler.

## Poznámka k tomuto repozitáři

V tomto prostředí nebylo možné ověřit build příkazem `platformio run`, protože `platformio` binárka není nainstalována.
