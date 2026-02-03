#pragma once

#include <memory>
#include "ioPinRules.h"

namespace cm::io
{
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

		const char *name() const override;
	};

	std::unique_ptr<IOPinRules> createEsp32PinRules();
}
