#include "Arduino.h"
#include "io/definitions/esp32-io-definition.h"

namespace cm::io
{
	// ------------------------------------------------------------
	// Internal helpers (ESP32 classic: WROOM / DevKit)
	// ------------------------------------------------------------

	static constexpr bool isRealEsp32Gpio(int pin)
	{
		if (pin < 0 || pin > 39)
			return false;

		// SPI Flash pins (not usable)
		if (pin >= 6 && pin <= 11)
			return false;

		// Not bonded / not available
		switch (pin)
		{
		case 20:
		case 24:
		case 28:
		case 29:
		case 30:
		case 31:
			return false;
		default:
			return true;
		}
	}

	static constexpr bool isInputOnlyPin(int pin)
	{
		return pin >= 34 && pin <= 39;
	}

	static constexpr bool isAdc1Pin(int pin)
	{
		return pin >= 32 && pin <= 39;
	}

	static constexpr bool isAdc2Pin(int pin)
	{
		switch (pin)
		{
		case 0:
		case 2:
		case 4:
		case 12:
		case 13:
		case 14:
		case 15:
		case 25:
		case 26:
		case 27:
			return true;
		default:
			return false;
		}
	}

	// ------------------------------------------------------------
	// Digital
	// ------------------------------------------------------------

	bool Esp32PinRules::isValidDigitalOutputPin(int pin) const
	{
		if (!isRealEsp32Gpio(pin))
			return false;

		// GPIO34–39 are input-only
		if (isInputOnlyPin(pin))
			return false;

		// Includes strapping + UART pins (valid but potentially risky)
		return true;
	}

	bool Esp32PinRules::isValidDigitalInputPin(int pin) const
	{
		return isRealEsp32Gpio(pin);
	}

	// ------------------------------------------------------------
	// Analog input
	// ------------------------------------------------------------

	bool Esp32PinRules::isValidAnalogInputPin(int pin) const
	{
		if (!isRealEsp32Gpio(pin))
			return false;

		return isAdc1Pin(pin) || isAdc2Pin(pin);
	}

	bool Esp32PinRules::isValidAnalogInputPin(int pin, bool wifiOrBtActive) const
	{
		if (!isRealEsp32Gpio(pin))
			return false;

		// ADC2 unusable when WiFi/BT is active
		if (wifiOrBtActive)
			return isAdc1Pin(pin);

		return isAdc1Pin(pin) || isAdc2Pin(pin);
	}

	// ------------------------------------------------------------
	// Analog output (DAC)
	// ------------------------------------------------------------

	bool Esp32PinRules::isValidAnalogOutputPin(int pin) const
	{
		// DAC1 = GPIO25, DAC2 = GPIO26
		return pin == 25 || pin == 26;
	}

	PinInfo Esp32PinRules::getPinInfo(int pin) const
	{
		PinInfo info;
		if (!isRealEsp32Gpio(pin))
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

		if (isInputOnlyPin(pin))
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
		if (pin >= 6 && pin <= 11)
		{
			info.constraints |= static_cast<uint32_t>(PinConstraint::FlashPin);
		}
		if (isAdc2Pin(pin))
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
