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

	const char *GenericPinRules::name() const
	{
		return toString(GUIMode::Generic);
	}

	std::unique_ptr<IOPinRules> createGenericPinRules()
	{
		return std::make_unique<GenericPinRules>();
	}
}
