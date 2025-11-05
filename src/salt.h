#pragma once

// Password Encryption Salt Configuration
// 
// This file defines the encryption salt used for password transmission between
// the WebUI and ESP32. The salt is used for XOR-based encryption to prevent
// casual WiFi packet sniffing.
//
// IMPORTANT SECURITY NOTES:
// 1. Copy this file to your project's src/ folder
// 2. Change ENCRYPTION_SALT to a unique value for your project
// 3. Keep this file OUT of version control (add to .gitignore)
// 4. Use a random string of at least 16 characters
// 5. The salt is embedded in both the WebUI (JavaScript) and ESP32 firmware at compile time
//
// Example of a secure salt:
//   #define ENCRYPTION_SALT "xK9#mP2$nQ7@wR5*zL3^vB8&cF4!hD6"
//
// Default salt (CHANGE THIS!):
#define ENCRYPTION_SALT "DefaultSalt_ChangeMe_4Security!"

// Note: If you don't need password encryption, you can leave this as-is.
// Passwords will still work, but will be transmitted in plaintext over WiFi.
