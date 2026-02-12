# Settings Structure Pattern - Best Practices

## Problem: Static Initialization Order Fiasco

### Das Problem erklärt

In C++ werden globale und statische Objekte in **unbestimmter Reihenfolge** zwischen verschiedenen Übersetzungseinheiten (.cpp Dateien) initialisiert. Dies kann zu schwer nachvollziehbaren Bugs führen.

**Konkret in unserem Fall:**

- `ConfigManagerClass ConfigManager;` (definiert in `ConfigManager.cpp`)
- `SystemSettings systemSettings;` (definiert in `main.cpp`)

**Gefährlicher Code (❌ NICHT verwenden):**

```cpp
struct SystemSettings {
    Config<bool> allowOTA;
    
    SystemSettings() : allowOTA(...) {
        ConfigManager.addSetting(&allowOTA); // ❌ CRASH! ConfigManager existiert möglicherweise noch nicht
    }
};
```

### Warum crasht das?

1. **Unbestimmte Reihenfolge**: `systemSettings` könnte vor `ConfigManager` initialisiert werden
2. **Zugriff auf nicht-initialisiertes Objekt**: `ConfigManager.addSetting()` wird auf ein noch nicht existierendes Objekt aufgerufen
3. **Resultat**: Segmentation Fault, Crash oder stille Fehler

## Die Lösung: Delayed Initialization Pattern

### ✅ Korrekte Implementierung

**1. Struktur mit separater `init()` Methode:**

```cpp
struct SystemSettings {
    Config<bool> allowOTA;
    Config<String> otaPassword;
    Config<String> version;
    
    // Phase 1: Konstruktor (sicher - keine ConfigManager-Zugriffe)
    SystemSettings() : 
        allowOTA(ConfigOptions<bool>{
            .key = "OTAEn", 
            .name = "Allow OTA Updates", 
            .category = "System", 
            .defaultValue = true
        }),
        otaPassword(ConfigOptions<String>{
            .key = "OTAPass", 
            .name = "OTA Password", 
            .category = "System", 
            .defaultValue = String(OTA_PASSWORT), 
            .showInWeb = true, 
            .isPassword = true
        }),
        version(ConfigOptions<String>{
            .key = "P_Version", 
            .name = "Program Version", 
            .category = "System", 
            .defaultValue = String(VERSION)
        })
    {
        // Konstruktor macht NUR Member-Initialisierung - keine ConfigManager-Aufrufe!
    }
    
    // Phase 2: Explizite Initialisierung (sicher - wird in setup() aufgerufen)
    void init() {
        // Hier ist ConfigManager garantiert bereit
        ConfigManager.addSetting(&allowOTA);
        ConfigManager.addSetting(&otaPassword);
        ConfigManager.addSetting(&version);
    }
};
```

```cpp
struct WiFiSettings {
    Config<int> rebootTimeoutMin;
    
    WiFiSettings() :
        rebootTimeoutMin(ConfigOptions<int>{
            .key = "WiFiRb",
            .name = "Reboot if WiFi lost (min)",
            .category = "WiFi",
            .defaultValue = 5,
            .showInWeb = true
        })
    {
    }
    
    void init() {
        ConfigManager.addSetting(&rebootTimeoutMin);
    }
};
```

**2. Instanziierung (global):**

```cpp
SystemSettings systemSettings; // Konstruktor wird aufgerufen, aber init() noch nicht
WiFiSettings wifiSettings;
```

**3. Explizite Initialisierung in setup():**

```cpp
void setup() {
    Serial.begin(115200);
    
    // ConfigManager Logger setzen
    ConfigManagerClass::setLogger([](const char *msg) {
        Serial.print("[ConfigManager] ");
        Serial.println(msg);
    });
    
    // ConfigManager konfigurieren
    ConfigManager.setAppName(APP_NAME);

    ConfigManager.setAppTitle(APP_NAME);
    ConfigManager.setVersion(VERSION);
    // ... weitere ConfigManager-Setup
    
    // JETZT ist ConfigManager bereit - Settings registrieren

    systemSettings.init();    // ✅ Sicher!

    buttonSettings.init();    // ✅ Sicher!
    tempSettings.init();      // ✅ Sicher!
    ntpSettings.init();       // ✅ Sicher!
    wifiSettings.init();      // ✅ Sicher!
    mqttSettings.init();      // ✅ Sicher!
    

    // Weitere Setup-Schritte...
}
```

## Template für neue Settings-Strukturen

**Verwende dieses Template für alle neuen Settings-Strukturen:**

```cpp
struct YourSettings {
    Config<bool> yourBoolSetting;

    Config<String> yourStringSetting;
    Config<int> yourIntSetting;
    
    // Phase 1: Konstruktor - NUR Member-Initialisierung
    YourSettings() : 
        yourBoolSetting(ConfigOptions<bool>{
            .key = "YourBool",
            .name = "Your Bool Setting",
            .category = "YourCategory",
            .defaultValue = true
        }),
        yourStringSetting(ConfigOptions<String>{
            .key = "YourStr",

            .name = "Your String Setting", 
            .category = "YourCategory",
            .defaultValue = String("default")
        }),
        yourIntSetting(ConfigOptions<int>{
            .key = "YourInt",
            .name = "Your Int Setting",
            .category = "YourCategory", 
            .defaultValue = 42
        })
    {
        // ❌ NIEMALS hier ConfigManager.addSetting() aufrufen!
        // ✅ Nur Member-Initialisierung im Konstruktor
    }
    
    // Phase 2: Explizite Initialisierung
    void init() {

        // ✅ Hier ist ConfigManager garantiert bereit
        ConfigManager.addSetting(&yourBoolSetting);
        ConfigManager.addSetting(&yourStringSetting);
        ConfigManager.addSetting(&yourIntSetting);
    }
};

// Globale Instanz
YourSettings yourSettings;

// In setup() aufrufen:
void setup() {
    // ... ConfigManager-Setup ...
    
    yourSettings.init(); // Settings registrieren
    
    // ... weitere Setup-Schritte ...
}
```

## Debugging-Tipps

### Problem diagnostizieren
Wenn Settings nicht in der WebUI erscheinen:

1. **Serial Monitor prüfen**: Erscheinen die `[ConfigManager] Added setting:` Meldungen?
2. **Reihenfolge prüfen**: Wird `init()` nach dem ConfigManager-Setup aufgerufen?
3. **Crash beim Start**: Möglicherweise ConfigManager-Zugriff im Konstruktor

### Typische Fehlermeldungen

```text
Guru Meditation Error: Core 1 panic'ed (LoadProhibited)
```
→ Wahrscheinlich Zugriff auf nicht-initialisiertes ConfigManager-Objekt

### Debug-Ausgabe hinzufügen

```cpp
void init() {
    Serial.println("[DEBUG] Initializing YourSettings...");
    ConfigManager.addSetting(&yourBoolSetting);
    Serial.println("[DEBUG] YourSettings initialized successfully");
}
```

## Migration bestehender Settings-Strukturen

### Alt (❌ Fehlerhaft)

```cpp
struct OldSettings {
    Config<bool> setting;
    
    OldSettings() : setting(...) {
        ConfigManager.addSetting(&setting); // ❌ Timing-Problem
    }
};
```

### Neu (✅ Korrekt)

```cpp
struct NewSettings {
    Config<bool> setting;
    
    NewSettings() : setting(...) {
        // Konstruktor macht nichts kritisches
    }
    
    void init() {
        ConfigManager.addSetting(&setting); // ✅ Sicher
    }
};
```

### Migration Checklist

- [ ] `init()` Methode zur Struktur hinzufügen
- [ ] Alle `ConfigManager.addSetting()` Aufrufe vom Konstruktor in `init()` verschieben
- [ ] `structureName.init()` in `setup()` nach ConfigManager-Setup aufrufen
- [ ] Testen: Alle Settings erscheinen in WebUI
- [ ] Serial Monitor prüfen: Keine Crash-Meldungen beim Start

## Warum ist dieses Pattern besser?

1. **🔒 Timing-Sicherheit**: Garantiert, dass ConfigManager bereit ist
2. **🐛 Einfaches Debugging**: Explizite Initialisierungsreihenfolge sichtbar
3. **📝 Selbstdokumentierend**: Code zeigt klar die zwei Phasen
4. **🔄 Standard-Pattern**: Etabliertes C++ Pattern für dieses Problem
5. **⚡ Performance**: Keine Overhead zur Laufzeit
6. **🛠 Wartbarkeit**: Einfach zu verstehen und zu erweitern

## Zusätzliche Hinweise

### Callbacks und Lambda-Funktionen

```cpp
// ✅ Sicher: Callbacks im Konstruktor sind OK
YourSettings() : 
    setting(ConfigOptions<bool>{
        .key = "test",
        .showIf = [this]() { return otherSetting.get(); } // ✅ OK
    })
{
    // ✅ Sicher: Callbacks setzen
    setting.setCallback([this](bool newValue) {
        // Callback-Logik
    });
}
```

### Abhängigkeiten zwischen Settings

```cpp
void init() {
    // Reihenfolge beachten bei Abhängigkeiten
    ConfigManager.addSetting(&primarySetting);
    ConfigManager.addSetting(&dependentSetting); // Hängt von primarySetting ab
}
```

---

**Fazit**: Das Delayed Initialization Pattern löst das Static Initialization Order Problem elegant und macht den Code robuster und wartbarer. Verwende immer dieses Pattern für Settings-Strukturen!

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| `ConfigManager.begin` | `begin()` | Starts ConfigManager services and web routes. | Used in examples: yes. |

