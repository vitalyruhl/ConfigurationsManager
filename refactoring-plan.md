# Refactoring-Plan zum TODO

Die geplanten Schritte basieren auf der in [docs/TODO.md](docs/TODO.md) beschriebenen Vision (Abschnitte A–H und die "Implementation Sequence"). Für jeden Schritt notiere ich den konkreten Scope, wichtige Abhängigkeiten und was danach geprüft werden muss.

## 1. Terminologie & Default-Layout festlegen
- Beziehe dich auf A) und die Begriffsdefinitionen im TODO: SettingsPage/Card/Group sowie LivePage/Card/Group sollen intern eindeutig verwaltet werden.
- Prüfe vorhandene Layoutdaten und entscheide, wie unbekannte Namen (z. B. Tippfehler) auf eine Default-Card oder -Page fallen (sie sollen eine Warnung werfen, aber nicht scheitern).
- Dokumentiere die finalen Defaults (z. B. SettingsCard = PageName, LiveCard = "Live Values") für weitere Schritte.

## 2. Layout-Registries für Settings + Live bauen
- Implementiere registrierende APIs (addSettingsPage/Card/Group und addLivePage/Card/Group) wie in Abschnitt A beschrieben.
- Sorge für case-insensitive Lookup, einmalige Warnungen bei unbekannten Layout-Namen und Zugriff auf Ordnung/Zuweisung.
- Stelle sicher, dass Registries nur Layout-Informationen sammeln (keine Settings/IOs erzeugen) und dass die Reihenfolge beim Rendern aus `order` gezogen wird.
- Ergänze Unit-Tests oder Logging, damit Fehlkonfigurationen früh auffallen.

## 3. Fluent Settings-Builder einführen
- Ersetze bestehende `ConfigOptions<T>` in `ConfigManager` durch `addSettingX(...).name(...).defaultValue(...).persist(...).build()` gemäß Abschnitt B.
- Definiere intern die stabile hashed key - Logik (sanitisiert, lowercase, 8 Zeichen) und stelle sicher, dass `.name()` den selben human-readable alias liefert.
- Default für `.persist()` soll `true` sein; optional `.persist(false)` für temporäre Werte.
- Kümmere dich um Speicher: Builder-Objekte bleiben auf dem Stack, ohne unnötige heap-allocation.

## 4. Generic Placement-Methoden für Settings + Live implementieren
- Baue `ConfigManager.addToSettings(...)`, `addToSettingsGroup(...)` und `addToLive(...)`, `addToLiveGroup(...)` mit den gewünschten Überladungen (s. TODO B).
- Nutze die Layout-Registries aus Schritt 2 zur Validierung der Page/Card/Group-Argumente und zur Order-Verwaltung.
- Stelle sicher, dass Settings nur angezeigt werden, wenn sie zuvor über den Builder erstellt und bei Bedarf in Settings/Live geroutet wurden.

## 5. IOManager: Definition und UI-Platzierung trennen
- Definiere digitale und analoge IOs wie in Abschnitt C mit Parameterliste, klaren Bezeichnern und `persistSettings`-Flag.
- Entferne bisherige `settingsCategory`-Mechanik und ersetze sie durch die neuen Layout-Aufrufe aus Schritt 4 (Abschnitt D).
- Implementiere `add*ToSettings*` (mit persist-check) und `add*ToLive*`, wobei letzterer ein Handle zurückgibt.
- Stelle sicher, dass nur `persistSettings=true` Settings-UI hinzugefügt werden (ansonsten Warnung).

## 6. Live-Callback-Builder und UI-Handles implementieren
- Realisiere `RuntimeControlType` (ValueField, Slider, Button, StateButton, Checkbox, PressButton) und trenne digitale vs. analoge Aufrufe (Abschnitt E).
- Die `add*ToLive(...)`-Methoden geben einen Builder/Handle zurück, über den Events wie `onChange()`, `onClick()`, `onPress()`, `onRelease()`, `onLongPress()`, `onReleaseAfterLongPress()`, `onRepeatWhilePressed()`, `onLongPressAtStartup()` und `onMultiClick()` konfiguriert werden können (Abschnitt F).
- Sorge für Fallbacks: z. B. Slider-Request auf Bool -> Checkbox, Momentary-Press-Button, mögliche Label/Color-Overrides.
- Definiere Timing-Defaults (Debounce, long press) und stelle sicher, dass manuelle Overrides möglich sind.

## 7. Generic Alarm-Registry aufbauen
- Implementiere `addAlarm(const AlarmConfig&)` und ggf. helper `addAlarmAnalog(...)` wie in Abschnitt G beschrieben.
- Trenne Alarm-Trigger-Definitionen von Live-Platzierung; letzteres erfolgt über neue Layout-APIs (Live-Seite/Card/Group).
- Unterstütze callback hooks (onAlarmCome/Gone/Stay) und dokumentiere die Erwartung, dass Alarme per default nur in Live angezeigt werden (keine Settings-Toggles).

## 8. MQTTManager analog umstrukturieren
- Richte APIs zur Topic-Definition (`addTopicReceiveInt/Float`) ein, die nur das Abonnement beschreiben (Abschnitt H).
- Füge `addMqttTopicToSettingsGroup(...)` und `addMqttTopicToLiveGroup(...)` hinzu, die die Layout-Registries nutzen.
- Kläre Schnittstellen mit ConfigManager (z. B. MQTT-Host/User/Pass gehören zu ConfigManager-Settings) und ermögliche konfigurierbare Button/Page-Namen in der Attach-Hilfe.

## 9. Beispiele, Docs & Validierung aktualisieren
- Migriere alle Beispiele/Docs auf die neuen APIs:
  - Schritte 1–8 für jedes Beispiel durchgehen (IOManager, ConfigManager, MQTTManager, Live/Settings-Platzierung).
  - Passe WebUI/Dokumentation an (API-Namen, Layout-Verhalten, Live-Events).
- Führe mindestens einmal `pio run -e <passender env>` durch; für das Refactoring bietet sich `examples/Full-GUI-Demo` an.
- Ergänze ggf. `docs/TODO.md` bzw. andere Dokumente mit finalen Entscheidungen und neuen Instruktionen.

---
**Verifikation:** Nach jedem Schritt sollten Regressionsrisiken überprüft werden (Unit-Tests, Logging, Build). Bei größeren Änderungen bitte vor Commit Benutzer nach Bestätigung fragen.
