#pragma once

#include "io/definitions/ioPinRules.h"
#include "io/definitions/generic-io-definition.h"
#include "io/definitions/esp32-io-definition.h"
#include "io/definitions/arduino-uno-io-definition.h"

namespace cm::io
{
	inline std::unique_ptr<IOPinRules> createPinRulesForMode(GUIMode mode)
	{
		switch (mode)
		{
		case GUIMode::Generic:
			return createGenericPinRules();
		case GUIMode::Esp32:
			return createEsp32PinRules();
		case GUIMode::ArduinoUno:
			return createArduinoUnoPinRules();
		}
		return createEsp32PinRules();
	}
}
