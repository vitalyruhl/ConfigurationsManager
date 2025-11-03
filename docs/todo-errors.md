# Todo / Errors / Ideas

## Current Issues to Fix

### High Priority Bugs (Prio 1)

- ~~i've deaktivated // ConfigManager.setSettingsPassword(SETTINGS_PASSWORD); to deaktivate the password protection for settings, but the webui still asks for a password.~~ **FIXED**: Changed default password to empty string; only enforces password if explicitly set via `setSettingsPassword()`.
- ~~if i deaktivate the System information (not all need it allways on), the Flash button in webui does not work anymore. (The Button is deactivated, ota itself works fine).~~ **FIXED**: Flash button now works when system provider is disabled by checking config/meta fallback and runtime metadata as alternatives.

### Medium Priority Bugs (Prio 5)

- ~~on upload the firmware via webui, the ui says restart esp, but it does not restart.~~ **FIXED**: Wired up reboot callback in `setupOTA()` so device restarts after successful HTTP OTA upload.

### Low Priority Bugs/Features

- **[FEATURE] Automated component testing** (Prio 10)

  - Create script that checks component on/off flags
  - Ensure compilation succeeds for all flag combinations
  - Integrate into test engine for comprehensive validation

- **[FEATURE] add HTTPS support, because its not in core ESP32 WiFi lib yet.** (Prio: not yet, wait for updates)

- **[FEATURE] if there are more cards, the card brocken down is under the longest card from above. It looks not fine -> remake the grid to be more flexible (Prio 10)**
