# v3 TODO / Roadmap

Stand: 2026-01-09

## [COMPLETED] v3.0.0 Stabilisierung
- Apply all / Save all: End-to-end getestet (UI -> Firmware -> Persistenz) inkl. Browser Reload.
- Config Endpoints: robustes JSON Parsing für `/config/auth`, `/config/save`, `/config/apply_all`, `/config/save_all`.
- Password Security: `/config.json` maskiert Passwörter; Reveal nur nach erfolgreicher Auth via Token-Header.
- OTA: stabilisiert durch sofortigen Reboot am OTA-Ende (Workaround für lwIP assert).
- Examples: Secrets entfernt; `example_min` ist ein minimaler GUI-Skeleton (AP Mode wenn SSID leer).

## [CURRENT] v3.1.0 Stabilisierung (Bugfixes)
- WebSocket: Ursache für Disconnects beim Settings-Laden prüfen (Payload-Größe, Timing, Reconnect-Loop, Memory).
- WiFi Reboot: Verhalten dokumentieren (Timeout vs. phased reconnect restart) und im UI klar machen.

## [NEXT] UI/UX (Settings & Runtime)
- Analog: separate Eingabe (NumberInput) vs Slider; Slider-Current-Value Styling anpassen.
- GUI Display Mode Toggle: “Current” (Karten) vs “Categories” (Map/Listenansicht).
- `order`/Sortierung: Metadaten für Live-Cards und Settings-Kategorien (stable ordering).

## [NEXT] Architektur / API
- Settings-Auth: Legacy-Endpoint `/config/settings_password` entfernen oder hart deaktivieren.
- Einheitliche JSON Handler: alle POST/PUT Config-Endpunkte über robuste JSON Parser (keine manuellen Body-Akkus).
- Settings-Schema Versionierung/Migration (wenn neue Felder/Struktur kommen).

## [LATER] Test & CI
- Minimal: Unit-/Integration-Test für Auth + password reveal + bulk save/apply.
- PlatformIO CI: Build-Matrix für mindestens ein ESP32 env + WebUI build step.
