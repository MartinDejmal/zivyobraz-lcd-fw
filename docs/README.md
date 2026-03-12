# Dokumentace projektu

Tato složka obsahuje podrobnou technickou dokumentaci k firmware.

## Obsah

- [`architecture.md`](architecture.md) - architektura, entry point, moduly a odpovědnosti.
- [`configuration.md`](configuration.md) - konfigurační model, výchozí hodnoty, NVS perzistence.
- [`runtime-and-networking.md`](runtime-and-networking.md) - boot/runtime flow, scheduler, Wi-Fi a serverový sync.
- [`web-ui.md`](web-ui.md) - vestavěné webové rozhraní a endpointy.
- [`build-and-run.md`](build-and-run.md) - build/flash/monitor workflow (PlatformIO).
- [`troubleshooting.md`](troubleshooting.md) - ověřená omezení a diagnostické tipy.

## Poznámka ke zdrojům pravdy

Popis je založen na aktuální implementaci ve `src/` a `include/`, ne na historických návrzích.
