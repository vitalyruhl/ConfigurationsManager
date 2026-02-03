#include "Arduino.h"
#include "io/definitions/arduino-uno-io-definition.h"

namespace cm::io
{
	// ------------------------------------------------------------
	// Internal helpers (Arduino Uno / ATmega328P)
	// ------------------------------------------------------------

	static constexpr bool isValidUnoDigitalPinRange(int pin)
	{
		// D0..D13
		return pin >= 0 && pin <= 13;
	}

	static constexpr bool isValidUnoAnalogPinRange(int pin)
	{
		// A0..A5 are commonly represented as 14..19 in the Arduino core
		return pin >= 14 && pin <= 19;
	}

	static constexpr bool isValidUnoAnyIoPin(int pin)
	{
		return isValidUnoDigitalPinRange(pin) || isValidUnoAnalogPinRange(pin);
	}

	static constexpr bool isUnoSerialPin(int pin)
	{
		// D0 = RX, D1 = TX
		return pin == 0 || pin == 1;
	}

	static constexpr bool isUnoPwmPin(int pin)
	{
		// PWM capable pins on Uno (analogWrite)
		return pin == 3 || pin == 5 || pin == 6 || pin == 9 || pin == 10 || pin == 11;
	}

	// ------------------------------------------------------------
	// Digital
	// ------------------------------------------------------------

	bool ArduinoUnoPinRules::isValidDigitalOutputPin(int pin) const
	{
		// On Arduino, A0..A5 (14..19) can also be used as digital I/O.
		// Therefore: allow 0..19. (You can still treat 0/1 as "risky" in higher-level logic.)
		return isValidUnoAnyIoPin(pin);
	}

	bool ArduinoUnoPinRules::isValidDigitalInputPin(int pin) const
	{
		return isValidUnoAnyIoPin(pin);
	}

	// ------------------------------------------------------------
	// Analog input (ADC)
	// ------------------------------------------------------------

	bool ArduinoUnoPinRules::isValidAnalogInputPin(int pin) const
	{
		// True ADC inputs are A0..A5 (14..19)
		return isValidUnoAnalogPinRange(pin);
	}

	// ------------------------------------------------------------
	// "Analog output" on Uno is PWM (no DAC)
	// ------------------------------------------------------------

	bool ArduinoUnoPinRules::isValidAnalogOutputPin(int pin) const
	{
		return isUnoPwmPin(pin);
	}

	PinInfo ArduinoUnoPinRules::getPinInfo(int pin) const
	{
		PinInfo info;
		if (!isValidUnoAnyIoPin(pin))
		{
			return info;
		}

		info.exists = true;
		if (isValidDigitalInputPin(pin))
		{
			info.capabilities |= static_cast<uint32_t>(PinCapability::DigitalIn);
		}
		if (isValidDigitalOutputPin(pin))
		{
			info.capabilities |= static_cast<uint32_t>(PinCapability::DigitalOut);
		}
		if (isValidAnalogInputPin(pin))
		{
			info.capabilities |= static_cast<uint32_t>(PinCapability::AnalogIn);
		}
		if (isValidAnalogOutputPin(pin))
		{
			info.capabilities |= static_cast<uint32_t>(PinCapability::PWMOut);
		}
		if (isUnoSerialPin(pin))
		{
			info.constraints |= static_cast<uint32_t>(PinConstraint::Serial);
		}
		return info;
	}

String ArduinoUnoPinRules::describeConstraints(uint32_t mask) const
{
	if (mask == 0)
	{
		return String();
	}

	static const struct ConstraintInfo
	{
		PinConstraint flag;
		const char *label;
		const char *hint;
	} entries[] = {
		{PinConstraint::Serial, "UART/serial pins", "D0/D1 share the hardware UART"},
	};

	String result;
	for (const auto &entry : entries)
	{
		if (!has(mask, entry.flag))
		{
			continue;
		}
		if (!result.isEmpty())
		{
			result += ", ";
		}
		result += entry.label;
		if (entry.hint && entry.hint[0] != '\0')
		{
			result += " (";
			result += entry.hint;
			result += ")";
		}
	}
	return result;
}

	// ------------------------------------------------------------
	// Optional helpers
	// ------------------------------------------------------------

	bool ArduinoUnoPinRules::isSerialPin(int pin) const
	{
		return isUnoSerialPin(pin);
	}

	bool ArduinoUnoPinRules::isPwmPin(int pin) const
	{
		return isUnoPwmPin(pin);
	}

	// ------------------------------------------------------------
	// Meta
	// ------------------------------------------------------------

	const char *ArduinoUnoPinRules::name() const
	{
		return toString(GUIMode::ArduinoUno);
	}

	std::unique_ptr<IOPinRules> createArduinoUnoPinRules()
	{
		return std::make_unique<ArduinoUnoPinRules>();
	}
}
