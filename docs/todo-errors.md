# Todo / Errors / Ideas

## Current Issues to Fix

### High Priority Bugs (Prio 1)

- ~~**[Bug] Save failed System.OTA Password: can't access property "digest", crypto.subtle is undefined**~~ ✅ **FIXED** - Improved password transmission security. Passwords are now transmitted as JSON with both the actual password and its SHA-256 hash: `{"password":"ota1234","hash":"954de57..."}`. This prevents casual WiFi packet sniffing (better than no security) while allowing the ESP32 to store and use the actual password. The hash is computed on the client side with a fallback for non-HTTPS contexts. Passwords now display correctly in the GUI (shows "ota1234" not the hash) and work properly with OTA and other features. 

### Medium Priority Bugs (Prio 5)

- **[Bug] From project, if i set [	-DCM_ENABLE_USER_CSS=0 and -DCM_ENABLE_STYLE_RULES=0] the styling injection is stil there** 

### Low Priority Bugs/Features

- **[FEATURE] Automated component testing** (Prio 10)

  - Create script that checks component on/off flags
  - Ensure compilation succeeds for all flag combinations
  - Integrate into test engine for comprehensive validation

- **[FEATURE] add HTTPS support, because its not in core ESP32 WiFi lib yet.** (Prio: not yet, wait for updates)

- **[FEATURE] if there are more cards, the card brocken down is under the longest card from above. It looks not fine -> remake the grid to be more flexible (Prio 10)**

- **[FEATURE] Failover Wifi native support** (Prio 5)