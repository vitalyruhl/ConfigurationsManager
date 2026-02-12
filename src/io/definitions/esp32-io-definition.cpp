#include "Arduino.h"
#include "io/definitions/esp32-io-definition.h"

namespace cm::io
{
	// ------------------------------------------------------------
	// Digital
	// ------------------------------------------------------------

	bool Esp32PinRules::isValidDigitalOutputPin(int pin) const
	{
		if (!isEsp32RealGpioPin(pin))
			return false;

		// GPIO34–39 are input-only
		if (isEsp32InputOnlyPin(pin))
			return false;

		// Includes strapping + UART pins (valid but potentially risky)
		return true;
	}

	bool Esp32PinRules::isValidDigitalInputPin(int pin) const
	{
		return isEsp32RealGpioPin(pin);
	}

	// ------------------------------------------------------------
	// Analog input
	// ------------------------------------------------------------

	bool Esp32PinRules::isValidAnalogInputPin(int pin) const
	{
		if (!isEsp32RealGpioPin(pin))
			return false;

		return isEsp32AnalogInputPin(pin);
	}

	bool Esp32PinRules::isValidAnalogInputPin(int pin, bool wifiOrBtActive) const
	{
		if (!isEsp32RealGpioPin(pin))
			return false;

		// ADC2 unusable when WiFi/BT is active
		if (wifiOrBtActive)
			return isEsp32Adc1Pin(pin);

		return isEsp32AnalogInputPin(pin);
	}

	// ------------------------------------------------------------
	// Analog output (DAC)
	// ------------------------------------------------------------

	bool Esp32PinRules::isValidAnalogOutputPin(int pin) const
	{
		// DAC1 = GPIO25, DAC2 = GPIO26
		return isEsp32DacPin(pin);
	}

	PinInfo Esp32PinRules::getPinInfo(int pin) const
	{
		PinInfo info;
		if (!isEsp32RealGpioPin(pin))
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
			info.capabilities |= static_cast<uint32_t>(PinCapability::PWMOut);
		}
		if (isValidAnalogInputPin(pin))
		{
			info.capabilities |= static_cast<uint32_t>(PinCapability::AnalogIn);
		}
		if (isValidAnalogOutputPin(pin))
		{
			info.capabilities |= static_cast<uint32_t>(PinCapability::DACOut);
			info.capabilities |= static_cast<uint32_t>(PinCapability::PWMOut);
		}

		if (isEsp32InputOnlyPin(pin))
		{
			info.constraints |= static_cast<uint32_t>(PinConstraint::InputOnly);
			info.constraints |= static_cast<uint32_t>(PinConstraint::NoPull);
		}
		switch (pin)
		{
		case 0:
		case 2:
		case 4:
		case 5:
		case 12:
		case 15:
			info.constraints |= static_cast<uint32_t>(PinConstraint::BootStrap);
			break;
		default:
			break;
		}
		if (pin == 1 || pin == 3)
		{
			info.constraints |= static_cast<uint32_t>(PinConstraint::Serial);
		}
		if (isEsp32Adc2Pin(pin))
		{
			info.constraints |= static_cast<uint32_t>(PinConstraint::ADC2);
		}

		return info;
	}

	String Esp32PinRules::describeConstraints(uint32_t mask) const
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
			{PinConstraint::InputOnly, "input-only pins", "GPIO34-39 accept inputs only"},
			{PinConstraint::NoPull, "pins without pull resistors", "internal pull-ups/pull-downs are unavailable"},
			{PinConstraint::BootStrap, "boot strapping pins", "GPIO0/2/4/5/12/15 influence boot mode"},
			{PinConstraint::Serial, "UART/serial pins", "GPIO0/1 share the console"},
			{PinConstraint::FlashPin, "flash pins", "GPIO6-11 connect to SPI flash"},
			{PinConstraint::ADC2, "ADC2 group", "ADC2 is disabled when WiFi/BT is active"},
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
	// Meta
	// ------------------------------------------------------------

	const char *Esp32PinRules::name() const
	{
		return "ESP32";
	}

	std::unique_ptr<IOPinRules> createEsp32PinRules()
	{
		return std::make_unique<Esp32PinRules>();
	}
}
