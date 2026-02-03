#pragma once

#include <Arduino.h>
#include <cstdint>
#include <memory>

namespace cm::io
{
	enum class GUIMode
	{
		Generic,
		Esp32,
		ArduinoUno
	};

	enum class IOPinRole
	{
		DigitalOutput,
		DigitalInput,
		AnalogInput,
		AnalogOutput
	};

	// --- NEW: what a pin CAN do (capabilities) ---
	enum class PinCapability : uint32_t
	{
		None        = 0,
		DigitalIn   = 1u << 0,
		DigitalOut  = 1u << 1,
		AnalogIn    = 1u << 2,
		DACOut      = 1u << 3,   // true DAC (ESP32 25/26)
		PWMOut      = 1u << 4    // "analog out" via PWM (UNO/ESP32)
	};

	// --- NEW: what can be problematic (constraints / warnings) ---
	enum class PinConstraint : uint32_t
	{
		None        = 0,
		InputOnly   = 1u << 0,   // ESP32 34-39
		NoPull      = 1u << 1,   // ESP32 34-39
		BootStrap   = 1u << 2,   // ESP32 0,2,4,5,12,15
		Serial      = 1u << 3,   // UNO 0/1, ESP32 1/3 typically
		FlashPin    = 1u << 4,   // ESP32 6-11
		ADC2        = 1u << 5    // ESP32 ADC2 group
	};

	// Bitops helpers
	inline constexpr uint32_t operator|(PinCapability a, PinCapability b)
	{
		return static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
	}
	inline constexpr uint32_t operator|(PinConstraint a, PinConstraint b)
	{
		return static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
	}
	inline constexpr bool has(uint32_t mask, PinCapability c)
	{
		return (mask & static_cast<uint32_t>(c)) != 0;
	}
	inline constexpr bool has(uint32_t mask, PinConstraint c)
	{
		return (mask & static_cast<uint32_t>(c)) != 0;
	}

	struct PinInfo
	{
		bool exists = false;
		uint32_t capabilities = 0; // PinCapability bits
		uint32_t constraints  = 0; // PinConstraint bits
	};

	inline const char *toString(GUIMode mode)
	{
		switch (mode)
		{
		case GUIMode::Generic:    return "generic";
		case GUIMode::Esp32:      return "esp32";
		case GUIMode::ArduinoUno: return "arduinoUno";
		}
		return "generic";
	}

	inline const char *toString(IOPinRole role)
	{
		switch (role)
		{
		case IOPinRole::DigitalOutput: return "digitalOutput";
		case IOPinRole::DigitalInput:  return "digitalInput";
		case IOPinRole::AnalogInput:   return "analogInput";
		case IOPinRole::AnalogOutput:  return "analogOutput";
		}
		return "unknown";
	}

	class IOPinRules
	{
	public:
		virtual ~IOPinRules() = default;

		// existing API stays
		virtual bool isValidDigitalOutputPin(int pin) const = 0;
		virtual bool isValidDigitalInputPin(int pin) const = 0;
		virtual bool isValidAnalogInputPin(int pin) const = 0;
		virtual bool isValidAnalogOutputPin(int pin) const = 0;
		virtual const char *name() const = 0;

		// --- NEW: single source of truth ---
		virtual PinInfo getPinInfo(int pin) const = 0;
		virtual String describeConstraints(uint32_t mask) const { (void)mask; return String(); }
	};
}
