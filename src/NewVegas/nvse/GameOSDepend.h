#pragma once

class OSInputGlobals {
public:
	UInt8 pad0000[0x18F8];
	UInt8 currKeyStates[256];
	UInt8 lastKeyStates[256];
	UInt8 pad1AF8[0x34];
	SInt32 currMouseWheelScroll;
	UInt8 currButtonStates[8];
	UInt8 pad1B38[0x0C];
	UInt8 lastButtonStates[8];

	static OSInputGlobals* GetSingleton() {
		return *reinterpret_cast<OSInputGlobals**>(0x11F35CC);
	}
};

assert(offsetof(OSInputGlobals, currKeyStates) == 0x18F8);
assert(offsetof(OSInputGlobals, lastKeyStates) == 0x19F8);
assert(offsetof(OSInputGlobals, currMouseWheelScroll) == 0x1B2C);
assert(offsetof(OSInputGlobals, currButtonStates) == 0x1B30);
assert(offsetof(OSInputGlobals, lastButtonStates) == 0x1B44);
