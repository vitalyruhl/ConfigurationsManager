#pragma once

#include "ioPinRules.h"

namespace cm::io
{
	class GenericPinRules : public IOPinRules
	{
	public:
		bool isValidDigitalOutputPin(int /*pin*/) const override;
		bool isValidDigitalInputPin(int /*pin*/) const override;
		bool isValidAnalogInputPin(int /*pin*/) const override;
		bool isValidAnalogOutputPin(int /*pin*/) const override;
		PinInfo getPinInfo(int pin) const override;
		const char *name() const override;
	};

	std::unique_ptr<IOPinRules> createGenericPinRules();
}
