#pragma once

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

	inline const char *toString(GUIMode mode)
	{
		switch (mode)
		{
		case GUIMode::Generic:
			return "generic";
		case GUIMode::Esp32:
			return "esp32";
		case GUIMode::ArduinoUno:
			return "arduinoUno";
		}
		return "generic";
	}

	inline const char *toString(IOPinRole role)
	{
		switch (role)
		{
		case IOPinRole::DigitalOutput:
			return "digitalOutput";
		case IOPinRole::DigitalInput:
			return "digitalInput";
		case IOPinRole::AnalogInput:
			return "analogInput";
		case IOPinRole::AnalogOutput:
			return "analogOutput";
		}
		return "unknown";
	}

	class IOPinRules
	{
	public:
		virtual ~IOPinRules() = default;
		virtual bool isValidDigitalOutputPin(int pin) const = 0;
		virtual bool isValidDigitalInputPin(int pin) const = 0;
		virtual bool isValidAnalogInputPin(int pin) const = 0;
		virtual bool isValidAnalogOutputPin(int pin) const = 0;
		virtual const char *name() const = 0;
	};
}
