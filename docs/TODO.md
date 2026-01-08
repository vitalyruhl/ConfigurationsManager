# TODO / Errors / Ideas

## Current Issues to Fix

- **[CURRENT] v3.0.0 cleanup: remove feature flags & pre/post scripts**
  - Goal: Consumer builds without `CM_ENABLE_*` flags and without PlatformIO `extra_scripts`
  - Only logging remains configurable

- **[CHORE] Convert examples to standalone PlatformIO projects** [COMPLETED]
  - Each example now builds via its own `platformio.ini` and `src/main.cpp`
  - Local dev uses `lib_deps = file://../..`
  - `library.json` export excludes `**/.pio/**` to avoid recursive local installs on Windows
  - Each example uses `[platformio] build_dir` + `libdeps_dir` outside the repo to prevent `.pio` folders under `examples/`
  - Verified builds: `pio run -d examples/publish -e usb`, `example_min`, `bme280`, `BME280-Full-GUI-Demo`

### High Priority Bugs (Prio 1)

- ~~**[Bug] Save failed System.OTA Password: can't access property "digest", crypto.subtle is undefined**~~ [COMPLETED] **FIXED** - Improved password transmission security. Passwords are now transmitted as JSON with both the actual password and its SHA-256 hash: `{"password":"ota1234","hash":"954de57..."}`. This prevents casual WiFi packet sniffing (better than no security) while allowing the ESP32 to store and use the actual password. The hash is computed on the client side with a fallback for non-HTTPS contexts. Passwords now display correctly in the GUI (shows "ota1234" not the hash) and work properly with OTA and other features.

### Medium Priority Bugs (Prio 5)

- **[Bug] From project, if i set [\t-DCM_ENABLE_USER_CSS=0 and -DCM_ENABLE_STYLE_RULES=0] the styling injection is stil there** [OBSOLETE in v3.0.0]

  - v3.0.0 removes these feature flags and always enables theming/styling.

### Low Priority Bugs/Features

- **[FEATURE] Automated component testing** (Prio 10)

  - Create script that checks component on/off flags
  - Ensure compilation succeeds for all flag combinations
  - Integrate into test engine for comprehensive validation

- **[FEATURE] add HTTPS support, because its not in core ESP32 WiFi lib yet.** (Prio: not yet, wait for updates)

- **[FEATURE] if there are more cards, the card brocken down is under the longest card from above. It looks not fine -> remake the grid to be more flexible (Prio 10)**

- **[FEATURE] Failover Wifi native support** (Prio 5)
