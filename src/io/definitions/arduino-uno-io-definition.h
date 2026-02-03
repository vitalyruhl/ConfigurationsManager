#pragma once

#include <memory>
#include "ioPinRules.h"

namespace cm::io
{
	class ArduinoUnoPinRules : public IOPinRules
	{
	public:
		// Digital
		bool isValidDigitalOutputPin(int pin) const override;
		bool isValidDigitalInputPin(int pin) const override;

		// Analog
		// NOTE: Arduino UNO has ADC inputs on A0..A5, usually mapped to 14..19.
		bool isValidAnalogInputPin(int pin) const override;

		// "Analog output" on UNO is PWM (not a real DAC).
		bool isValidAnalogOutputPin(int pin) const override;

		// Optional helpers (non-breaking)
		bool isSerialPin(int pin) const;       // 0/1
		bool isPwmPin(int pin) const;          // 3,5,6,9,10,11

		const char *name() const override;
	};

	std::unique_ptr<IOPinRules> createArduinoUnoPinRules();
}
