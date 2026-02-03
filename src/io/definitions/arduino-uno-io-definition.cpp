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
