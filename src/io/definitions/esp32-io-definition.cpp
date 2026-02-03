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
