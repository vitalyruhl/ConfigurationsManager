#pragma once

#include <memory>
#include "ioPinRules.h"

namespace cm::io
{
	// Shared constexpr helpers for ESP32 pin classification.
	// Kept in header so examples and static checks can use the same source of truth.
	constexpr bool isEsp32ReservedPin(int pin)
	{
		return ((pin >= 6 && pin <= 11) || pin == 20 || pin == 24 || (pin >= 28 && pin <= 31));
	}

	constexpr bool isEsp32RealGpioPin(int pin)
	{
		return (pin >= 0 && pin <= 39) && !isEsp32ReservedPin(pin);
	}

	constexpr bool isEsp32InputOnlyPin(int pin)
	{
		return pin >= 34 && pin <= 39;
	}

	constexpr bool isEsp32Adc1Pin(int pin)
	{
		return pin >= 32 && pin <= 39;
	}

	constexpr bool isEsp32Adc2Pin(int pin)
	{
		return (pin == 0 || pin == 2 || pin == 4 || (pin >= 12 && pin <= 15) || (pin >= 25 && pin <= 27));
	}

	constexpr bool isEsp32AnalogInputPin(int pin)
	{
		return isEsp32Adc1Pin(pin) || isEsp32Adc2Pin(pin);
	}

	constexpr bool isEsp32DacPin(int pin)
	{
		return pin == 25 || pin == 26;
	}

	class Esp32PinRules : public IOPinRules
	{
public:
		// Digital
		bool isValidDigitalOutputPin(int pin) const override;
		bool isValidDigitalInputPin(int pin) const override;

		// Analog
		bool isValidAnalogInputPin(int pin) const override;          // ADC1 + ADC2
		bool isValidAnalogOutputPin(int pin) const override;         // DAC only (25,26)

		// Optional: recommended variant when WiFi/BT is active
		bool isValidAnalogInputPin(int pin, bool wifiOrBtActive) const;
		PinInfo getPinInfo(int pin) const override;
		String describeConstraints(uint32_t mask) const override;

		const char *name() const override;
	};

	std::unique_ptr<IOPinRules> createEsp32PinRules();
}
