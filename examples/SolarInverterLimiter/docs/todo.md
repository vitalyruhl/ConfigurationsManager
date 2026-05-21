
# ToDo SolarInverterLimiter

Goal: Refactor the codebase to the new ConfigManager structure, then synchronize HomeAssistant configs, then modernize Logging/MQTT modules and structure them for reuse across projects.

Legend:
- [CURRENT] = currently in progress
- [NEXT] = next reasonable step
- [BLOCKED] = waiting for decision/info
- [COMPLETED] = done

## P0 (critical) – Anglicize the project (repo-wide)

- no critical features

## P0 (critical) – ConfigManager refactor (functional, buildable)

- no critical features for CM

## P1 (high) – Update HomeAssistant YAML to the new structure

## P1 (high) – Reduce firmware flash usage

- [NEXT] Evaluate OLED display stack replacement
	- Current assessment: replacing `Adafruit SSD1306` + `Adafruit GFX` is likely worth about 12-16 KB Flash and 1024 B RAM with a text-only OLED library
	- Alternative assessment: a lighter graphics/text library with similar layout is likely worth about 6-10 KB Flash
	- Current display usage is mostly text plus a simple frame, so a smaller display stack should preserve user-visible behavior
	- Bigger flash savings are more likely in the embedded ConfigManager WebUI, but that is a separate larger effort

## P1 (high) – Add asymmetric inverter setpoint ramp limiting

- [NEXT] Add an asymmetric slew-rate / ramp limiter after the PID or setpoint calculation
	- Apply the limiter after the calculated target setpoint is clamped to `minInverterPower` and `maxInverterPower`
	- Use separate configurable settings instead of hardcoded values:
		- `maxSetpointRiseWattsPerSecond`
		- `maxSetpointFallWattsPerSecond`
	- Rising setpoint should be slower
	- Falling setpoint should be faster
	- This is an output conditioning layer after the PID / setpoint calculation, not a replacement for PID tuning
	- Why this is useful:
		- fast down-ramp reduces the risk of unwanted feed-in when consumption drops
		- slow up-ramp avoids reacting too aggressively to short consumption spikes
	- Suggested flow:
		```text
		calculatedTarget = calculateSetpointOrPidOutput(...)
		clampedTarget = clamp(calculatedTarget, minInverterPower, maxInverterPower)
		limitedSetpoint = applyAsymmetricRampLimit(
		    clampedTarget,
		    previousSetpoint,
		    maxSetpointRiseWattsPerSecond,
		    maxSetpointFallWattsPerSecond,
		    deltaTimeSeconds
		)
		```
	- Validation notes for later:
		- verify that setpoint drops quickly when a large load switches off
		- verify that setpoint rises gradually when load increases
		- verify that short spikes do not immediately pull the inverter setpoint up
		- verify that zero-feed-in behavior is improved or at least not degraded
		- keep the logic non-blocking
		- preserve existing PID / setpoint calculation behavior when both ramp settings are disabled or set high enough
