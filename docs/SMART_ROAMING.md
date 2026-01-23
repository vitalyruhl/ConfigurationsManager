# Smart WiFi Roaming

ConfigurationsManager v2.7.0+ includes integrated **Smart WiFi Roaming** functionality that automatically switches to stronger access points in mesh networks.

## Overview

Smart WiFi Roaming solves the common "sticky client" problem where devices remain connected to distant, weak access points instead of switching to nearby, stronger ones. This is particularly useful in:

- **FRITZ!Box Mesh networks** with multiple repeaters
- **Enterprise WiFi deployments** with multiple access points
- **Home mesh systems** (Eero, Google Nest, etc.)
- **WiFi extender setups**

## How It Works

The Smart Roaming system continuously monitors your WiFi connection and intelligently switches to better access points:

1. **Signal Monitoring**: Tracks WiFi signal strength (RSSI) in real-time
2. **Threshold Detection**: Triggers when signal drops below configurable threshold
3. **Network Scanning**: Scans for better access points with the same SSID
4. **Intelligent Decision**: Only switches if significantly better AP is found
5. **Smooth Transition**: Handles reconnection with static IP preservation
6. **Cooldown Protection**: Prevents frequent switching with configurable delays

## Configuration

### Basic Configuration

Smart Roaming is **enabled by default** with sensible settings:

```cpp
#include "ConfigManager.h"

void setup() {
    // Smart Roaming is enabled by default with these settings:
    // - Threshold: -75 dBm
    // - Cooldown: 120 seconds  
    // - Improvement: 10 dBm minimum
    
    ConfigManager.startWebServer(ssid, password);
    // Smart Roaming is now active automatically
}
```

### Custom Configuration

Customize the roaming behavior for your specific environment:

```cpp
void setup() {
    // Configure Smart WiFi Roaming before starting WiFi
    ConfigManager.enableSmartRoaming(true);        // Enable/disable
    ConfigManager.setRoamingThreshold(-70);        // Trigger at -70 dBm (more aggressive)
    ConfigManager.setRoamingCooldown(60);           // Wait 1 minute between attempts
    ConfigManager.setRoamingImprovement(15);       // Require 15 dBm improvement
    
    ConfigManager.startWebServer(ssid, password);
}
```

### Disable Smart Roaming

```cpp
void setup() {
    // Disable Smart Roaming completely
    ConfigManager.enableSmartRoaming(false);
    
    ConfigManager.startWebServer(ssid, password);
}
```

## Configuration Parameters

| Parameter | Type | Default | Range | Description |
| --- | --- | --- | --- | --- |
| **Enabled** | `bool` | `true` | true/false | Enable/disable Smart Roaming |
| **Threshold** | `int` | `-75` | -30 to -90 | Signal strength trigger (dBm) |
| **Cooldown** | `unsigned long` | `120` | 30-600 | Seconds between roaming attempts |
| **Improvement** | `int` | `10` | 5-30 | Minimum signal improvement (dBm) |

### Parameter Guidelines

**Roaming Threshold (-30 to -90 dBm):**

- `-60 dBm`: Very aggressive (excellent signal only)
- `-70 dBm`: Aggressive (good signal preferred)  
- `-75 dBm`: **Default** (balanced performance)
- `-80 dBm`: Conservative (tolerates weak signals)
- `-90 dBm`: Very conservative (emergency roaming only)

**Cooldown Period (30-600 seconds):**

- `30-60 seconds`: Fast switching (may cause instability)
- `120 seconds`: **Default** (balanced stability)
- `300+ seconds`: Slow switching (very stable)

**Signal Improvement (5-30 dBm):**

- `5 dBm`: Switches easily (may cause ping-ponging)
- `10 dBm`: **Default** (good balance)
- `15+ dBm`: Conservative switching (very stable)

## Advanced Usage

### Direct WiFi Manager Access

```cpp
void setup() {
    // Get direct access to WiFi manager
    auto& wifiMgr = ConfigManager.getWiFiManager();
    
    // Check current roaming status
    if (wifiMgr.isSmartRoamingEnabled()) {
        Serial.println("Smart roaming is active");
    }
    
    // Configure with direct access
    wifiMgr.enableSmartRoaming(true);
    wifiMgr.setRoamingThreshold(-80);
    wifiMgr.setRoamingCooldown(180);
    wifiMgr.setRoamingImprovement(12);
}
```

### Runtime Monitoring

Monitor roaming activity through serial logs:

```cpp
void loop() {
    ConfigManager.handleClient();
    
    // Smart Roaming logs appear automatically:
    // [ConfigManager] [WiFi] Current RSSI (-85 dBm) below threshold (-75 dBm), scanning...
    // [ConfigManager] [WiFi] Found better AP: AA:BB:CC:DD:EE:FF (RSSI: -65 dBm, improvement: 20 dBm)
    // [ConfigManager] [WiFi] Initiated roaming to better access point
}
```

## Troubleshooting

### Common Issues

**Frequent Disconnections:**
- Increase cooldown period: `setRoamingCooldown(300)`
- Increase improvement threshold: `setRoamingImprovement(15)`

**No Roaming Despite Weak Signal:**
- Check if threshold is too low: `setRoamingThreshold(-70)`
- Verify multiple APs with same SSID are available
- Check if cooldown period hasn't expired yet

**Roaming to Wrong Access Point:**
- Increase improvement threshold: `setRoamingImprovement(15)`
- Check physical placement of access points

### Debug Information

Enable verbose logging to see detailed roaming decisions:

```cpp
void setup() {
    // Enable detailed WiFi logging
    ConfigManager.setLogger([](const char* msg) {
        Serial.printf("[LOG] %s\n", msg);
    });
}
```

### Network Environment Optimization

**FRITZ!Box Mesh:**
```cpp
// Optimized for FRITZ!Box mesh networks
ConfigManager.setRoamingThreshold(-75);    // Standard threshold
ConfigManager.setRoamingCooldown(120);     // 2 minute cooldown  
ConfigManager.setRoamingImprovement(10);   // 10 dBm improvement
```

**Enterprise WiFi:**
```cpp
// Optimized for enterprise deployments
ConfigManager.setRoamingThreshold(-70);    // More aggressive
ConfigManager.setRoamingCooldown(60);      // Faster switching
ConfigManager.setRoamingImprovement(12);   // Slightly higher threshold
```

**Home Mesh Systems:**
```cpp
// Optimized for consumer mesh systems
ConfigManager.setRoamingThreshold(-78);    // More tolerant
ConfigManager.setRoamingCooldown(180);     // Slower switching
ConfigManager.setRoamingImprovement(8);    // Lower improvement bar
```

## Technical Implementation

### Library Integration

Smart Roaming is implemented directly in the ConfigManager WiFi subsystem:

- **WiFiManager Class**: Contains roaming logic and state management
- **Non-blocking**: Roaming checks run during `handleClient()` calls
- **Static IP Preservation**: Maintains IP configuration during roaming
- **Automatic Scanning**: Periodically scans for better access points
- **State Management**: Tracks connection state and roaming attempts

### Performance Impact

- **CPU Usage**: Minimal (roaming check ~1ms every 30 seconds)
- **Memory Usage**: <100 bytes additional RAM
- **Network Impact**: Brief scan every 2+ minutes when signal is weak
- **Connection Stability**: Improved through intelligent AP selection

## Example Projects

See the examples directory for complete implementations:

- `examples/example_min/src/main.cpp`: Basic setup with default Smart Roaming
- `examples/bme280/src/main.cpp`: Advanced setup with custom roaming configuration

## Version History

- **v2.7.0**: Initial Smart WiFi Roaming implementation
- Integrated into WiFiManager class
- Default enabled with conservative settings
- Configurable thresholds and timing
- Support for static IP preservation during roaming
