# Build, flash a spuštění

## Požadavky

- PlatformIO (CLI nebo VS Code extension)
- ESP32 deska kompatibilní s `board = esp32dev` nebo `board = esp32-c3-devkitm-1`
- ST7789 připojený dle pinů z výchozí konfigurace nebo vlastního nastavení

## PlatformIO konfigurace

`platformio.ini` obsahuje dva environments:

### `[env:esp32dev]` — ST7789 TFT (výchozí)
- Deska: `esp32dev`
- Framework: Arduino
- Knihovna: `bodmer/TFT_eSPI`
- C++17

### `[env:esp32c3-sharp-ls027b7dh01]` — Sharp LS027B7DH01 400×240 B&W na ESP32-C3
- Deska: `esp32-c3-devkitm-1`
- Framework: Arduino
- Knihovna: `olikraus/U8g2`
- C++17
- Výchozí SPI2 piny: MOSI=GPIO7, SCLK=GPIO6, CS=GPIO10
- Volitelný pin DISP (zapnutí displeje): nastav v `loadDefaults()`, výchozí -1 (vždy zapnuto)

## Typický workflow

```bash
# ST7789 build (výchozí)
platformio run -e esp32dev
platformio run -e esp32dev -t upload

# Sharp LS027B7DH01 build
platformio run -e esp32c3-sharp-ls027b7dh01
platformio run -e esp32c3-sharp-ls027b7dh01 -t upload

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
