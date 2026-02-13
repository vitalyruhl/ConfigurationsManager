
# ToDo SolarInverterLimiter

Goal: Refactor the codebase to the new ConfigManager structure, then synchronize HomeAssistant configs, then modernize Logging/MQTT modules and structure them for reuse across projects.

Legend:
- [CURRENT] = currently in progress
- [NEXT] = next reasonable step
- [BLOCKED] = waiting for decision/info
- [COMPLETED] = done

## P0 (critical) – Anglicize the project (repo-wide)



- [COMPLETED] Ensure English-only repository artifacts
	- Review /docs/* and translate remaining German text to English
	- Review /dev-info/* and translate remaining German text to English (keep technical meaning)
	- Review /src/* and translate remaining German comments/strings to English
	- Replace emojis in repository artifacts with plain text tags like [INFO]/[WARNING]/[ERROR]
	- Ensure new documentation is written in English (chat explanations remain German)


- [CURRENT] Rename non-English/typo identifiers (incremental)
	- Replace German variable/function names with English equivalents (update all references)
	- Keep external interfaces stable (MQTT topics/JSON keys) unless explicitly intended


- [COMPLETED] Align project guidance
	- Keep language policy consistent across README, docs, and .github instructions
	- Remove outdated or misleading usage text (e.g. references to non-existing config_example.h)

## P0 (critical) – ConfigManager refactor (functional, buildable)

- [COMPLETED] ConfigManager refactor aligned to ConfigManager 4.0.0
	- Settings migrated to ConfigOptions API (no builder)
	- Runtime/WiFi APIs aligned
	- `pio run -e usb` builds successfully
	- Note: No backward compatibility / migration by explicit decision

## P1 (high) – Update HomeAssistant YAML to the new structure
