# Password Encryption Security

## Overview

Starting with version 2.7.5, ConfigurationsManager includes XOR-based password encryption for HTTP transmission between the WebUI and the ESP32. This prevents casual WiFi packet sniffing while maintaining compatibility with embedded systems that don't support HTTPS.

## Security Model

### What is Protected
- **HTTP transmission**: Passwords are encrypted before being sent over WiFi
- **Casual packet sniffing**: Encrypted hex strings prevent easy password interception
- **WiFi network exposure**: Passwords are not transmitted in plaintext

### What is NOT Protected
- **Storage**: Passwords are stored in **plaintext** in ESP32 flash (NVS Preferences)
- **Physical access**: Anyone with physical access to the ESP32 can read passwords from flash
- **Advanced attacks**: XOR encryption is not cryptographically strong

### Design Philosophy

This is "weak encryption" - sufficient for embedded IoT devices where:
1. HTTPS is not available (standard ESP32 WiFi library limitation)
2. Physical access is required to read firmware
3. The primary threat is casual WiFi packet sniffing
4. Passwords need to be used directly by the ESP32 (OTA, MQTT, etc.)

## Usage

### Simple Setup (One File Configuration)

**Step 1: Copy salt.h to your project**

Copy the salt configuration file from the library:

```bash
cp .pio/libdeps/your_env/ESP32\ Configuration\ Manager/src/salt.h src/
```

**Step 2: Customize the salt**

Edit `src/salt.h` in your project:

```cpp
#pragma once

// CHANGE THIS to a unique random string for your project!
// Use at least 16 characters, mix of letters, numbers, and symbols
#define ENCRYPTION_SALT "xK9#mP2$nQ7@wR5*zL3^vB8&cF4!hD6"
```

**Step 3: Add to .gitignore**

Keep your salt out of version control:

```gitignore
# .gitignore
src/salt.h
```

**Step 4: Use normally**

Encryption is automatic - no code changes needed:

```cpp
#include <ConfigManager.h>

ConfigManagerClass cfg;

void setup() {
    // Encryption is automatically initialized from src/salt.h!
    
    Config<String> otaPassword(ConfigOptions<String>{
        .key = "ota_pwd",
        .category = "OTA",
        .defaultValue = "admin123",
        .name = "OTA Password",
        .isPassword = true
    });
    
    cfg.addSetting(&otaPassword);
    cfg.loadAll();
    cfg.startWebServer("MyNetwork", "wifi-password");
}
```

### How It Works

1. **Build time**: The build script reads `ENCRYPTION_SALT` from `src/salt.h`
2. **WebUI**: The salt is injected into the JavaScript during compilation
3. **ESP32**: The salt is included via `#include "salt.h"` automatically
4. **Runtime**: Encryption/decryption happens transparently

### Important Notes

1. **One file configuration**: `src/salt.h` configures both WebUI and ESP32
2. **Automatic**: Encryption is enabled automatically when `salt.h` exists
3. **Optional**: If you don't copy `salt.h`, the library's default is used (plaintext transmission)
4. **Storage is plaintext**: Passwords are always stored as plaintext in ESP32 flash
5. **Choose a unique salt**: Use a different salt for each project
6. **Keep secret**: Add `src/salt.h` to `.gitignore`

## How It Works

### Frontend (JavaScript)

```javascript
// In webui/src/App.vue
const ENCRYPTION_SALT = "__ENCRYPTION_SALT__"; // Placeholder for build-time injection

function encryptPassword(password, salt) {
    const encoder = new TextEncoder();
    const pwdBytes = encoder.encode(password);
    const saltBytes = encoder.encode(salt);
    const encrypted = new Uint8Array(pwdBytes.length);
    
    for (let i = 0; i < pwdBytes.length; i++) {
        encrypted[i] = pwdBytes[i] ^ saltBytes[i % saltBytes.length];
    }
    
    // Convert to hex string
    return Array.from(encrypted)
        .map(b => b.toString(16).padStart(2, '0'))
        .join('');
}
```

### Backend (C++)

```cpp
// In src/web/WebServer.cpp
String ConfigManagerWeb::decryptPassword(const String& encryptedHex, const String& salt) {
    if (encryptedHex.isEmpty() || salt.isEmpty()) {
        return encryptedHex;
    }
    
    String result;
    for (size_t i = 0; i < encryptedHex.length(); i += 2) {
        // Parse 2 hex chars to byte
        char hexByte[3] = {encryptedHex[i], encryptedHex[i+1], '\0'};
        uint8_t encrypted = (uint8_t)strtol(hexByte, nullptr, 16);
        
        // XOR with salt
        uint8_t saltByte = (uint8_t)salt[(i/2) % salt.length()];
        uint8_t decrypted = encrypted ^ saltByte;
        
        result += (char)decrypted;
    }
    
    return result;
}
```

### Flow

1. User enters password in WebUI
2. JavaScript encrypts password with XOR using salt → produces hex string (e.g., `3f2a1c0d...`)
3. Encrypted hex string transmitted over HTTP
4. ESP32 receives encrypted hex string
5. Backend detects hex format (even length, only hex chars)
6. Backend decrypts with matching salt → plaintext password
7. Plaintext password stored in NVS Preferences
8. Password available for OTA, MQTT, etc. in plaintext

## Security Considerations

### Advantages
- ✅ Prevents casual WiFi packet sniffing
- ✅ No HTTPS overhead (embedded systems)
- ✅ Passwords stored in usable plaintext format
- ✅ Simple implementation with low memory footprint
- ✅ Works in HTTP-only environments

### Limitations
- ⚠️ Not cryptographically strong encryption
- ⚠️ Physical access to ESP32 reveals passwords
- ⚠️ Salt is embedded in firmware (readable with flash dump)
- ⚠️ XOR is reversible if salt is known
- ⚠️ Vulnerable to determined attackers

### Best Practices

1. **Use unique salts per project**: Don't reuse the same salt across different projects
2. **Keep salts private**: Don't publish your salt in public repositories
3. **Long salts are better**: Use at least 16-20 characters
4. **Mix characters**: Use letters, numbers, and special characters
5. **Consider your threat model**: This protects against casual sniffing, not targeted attacks

### Example Salts (in `src/salt.h`)

```cpp
// Good: Long, unique, mixed characters
#define ENCRYPTION_SALT "Xk9$mP2#nQ7@rT4&vZ8!"

// Bad: Short, predictable
#define ENCRYPTION_SALT "abc123"

// Bad: Project name (too obvious)
#define ENCRYPTION_SALT "MyIoTProject"
```

## Future Enhancements (Optional)

### Automatic Salt Injection

You can modify the build script to automatically inject a unique salt:

1. Generate random salt at build time
2. Replace `__ENCRYPTION_SALT__` placeholder in compiled JavaScript
3. Pass same salt to firmware via build flag

This ensures each build has a unique salt without manual configuration.

### HTTPS Support

For production environments requiring stronger security, consider:
- Using an HTTPS reverse proxy (e.g., nginx, Caddy)
- External hardware with SSL/TLS support
- ESP32-S3 with sufficient resources for mbedTLS

## API Reference

### ConfigManagerClass

No public API is required to set the salt. The library automatically includes `src/salt.h` and initializes the encryption during `startWebServer()`.

### ConfigManagerWeb

#### `static String encryptPassword(const String& password, const String& salt)`
Encrypts a password using XOR with the provided salt.

**Returns:** Hex string representation of encrypted password

#### `static String decryptPassword(const String& encryptedHex, const String& salt)`
Decrypts a hex-encoded password using XOR with the provided salt.

**Returns:** Plaintext password

## FAQ

### Q: Why not use proper encryption like AES?
**A:** AES requires significant resources (CPU, RAM) and key management complexity. For embedded systems with HTTP-only interfaces, XOR encryption provides adequate protection against casual threats while remaining lightweight.

### Q: Can I disable encryption?
**A:** Yes. If you don't provide a `src/salt.h` with `ENCRYPTION_SALT`, the WebUI will transmit passwords in plaintext (same behavior as versions < 2.7.5).

### Q: What happens if I change the salt?
**A:** Nothing bad! Passwords are stored in plaintext on the ESP32, so changing the salt only affects new password transmissions. Existing stored passwords remain unaffected.

### Q: Is this secure enough for my use case?
**A:** It depends on your threat model:
- **Casual WiFi sniffing**: Yes, sufficient
- **Home automation**: Yes, good balance
- **Public/untrusted networks**: No, use HTTPS proxy
- **High-security applications**: No, use proper TLS/SSL

### Q: Can someone reverse-engineer my salt?
**A:** Yes, if they have physical access to the ESP32, they can dump the firmware and extract the salt. This is why XOR encryption is "weak" - it protects against network sniffing, not physical access.

## Related Documentation

- [Settings Configuration](SETTINGS.md)
- [Feature Flags](FEATURE_FLAGS.md)
- [Smart WiFi Roaming](SMART_ROAMING.md)
