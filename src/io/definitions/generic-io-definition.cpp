#include "io/definitions/generic-io-definition.h"

#include <memory>

namespace cm::io
{
	bool GenericPinRules::isValidDigitalOutputPin(int /*pin*/) const
	{
		return true;
	}

	bool GenericPinRules::isValidDigitalInputPin(int /*pin*/) const
	{
		return true;
	}

	bool GenericPinRules::isValidAnalogInputPin(int /*pin*/) const
	{
		return true;
	}

	bool GenericPinRules::isValidAnalogOutputPin(int /*pin*/) const
	{
		return true;
	}

	PinInfo GenericPinRules::getPinInfo(int /*pin*/) const
	{
		PinInfo info;
		info.exists = true;
		info.capabilities = static_cast<uint32_t>(PinCapability::DigitalIn)
			| static_cast<uint32_t>(PinCapability::DigitalOut)
			| static_cast<uint32_t>(PinCapability::AnalogIn)
			| static_cast<uint32_t>(PinCapability::DACOut)
			| static_cast<uint32_t>(PinCapability::PWMOut);
		info.constraints = 0;
		return info;
	}
	const char *GenericPinRules::name() const
	{
		return toString(GUIMode::Generic);
	}

	std::unique_ptr<IOPinRules> createGenericPinRules()
	{
		return std::make_unique<GenericPinRules>();
	}
}
