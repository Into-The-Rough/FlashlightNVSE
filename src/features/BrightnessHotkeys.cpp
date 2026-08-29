#include "BrightnessHotkeys.h"
#include "../core/Config.h"

namespace BrightnessHotkeys {
	constexpr int kUnbound = 0xFF;
	constexpr int kMouseCodeBase = 0x100;
	constexpr int kMouseCodeLast = 0x109;
	static bool s_wasHeld[4];

	static bool IsHeld(OSInputGlobals* apInput, int aiKey) {
		if (aiKey <= 0 || aiKey == kUnbound)
			return false;
		if (aiKey < kMouseCodeBase)
			return apInput->currKeyStates[aiKey] != 0;
		if (aiKey < kMouseCodeBase + 8)
			return apInput->currButtonStates[aiKey - kMouseCodeBase] != 0;
		if (aiKey == kMouseCodeBase + 8)
			return apInput->currMouseWheelScroll > 0;
		if (aiKey == kMouseCodeLast)
			return apInput->currMouseWheelScroll < 0;
		return false;
	}

	static bool IsPressed(OSInputGlobals* apInput, int aiKey, bool& abWasHeld) {
		bool held = IsHeld(apInput, aiKey);
		bool pressed = held && !abWasHeld;
		abWasHeld = held;
		return pressed;
	}

	void MainLoop() {
		auto interfaceManager = *reinterpret_cast<MenuInterfaceManager**>(0x11D8A80);
		if (!Player || !Player->parentCell || !interfaceManager || interfaceManager->currentMode != 1)
			return;

		auto input = OSInputGlobals::GetSingleton();
		if (!input)
			return;

		bool interior = Player->parentCell->IsInterior();
		if (IsPressed(input, Config::Hotkeys.DimmerIncrease, s_wasHeld[0]))
			Config::AdjustDimmer(interior, 0.1f);
		if (IsPressed(input, Config::Hotkeys.DimmerDecrease, s_wasHeld[1]))
			Config::AdjustDimmer(interior, -0.1f);
		if (IsPressed(input, Config::Hotkeys.IntensityIncrease, s_wasHeld[2]))
			Config::AdjustIntensity(interior, 0.05f);
		if (IsPressed(input, Config::Hotkeys.IntensityDecrease, s_wasHeld[3]))
			Config::AdjustIntensity(interior, -0.05f);
	}
}
