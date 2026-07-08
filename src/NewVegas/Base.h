#pragma once

class Hooks {
public:
	static const UInt32 NewMain						= 0x0086C160;
	static const UInt32 InitializeRenderer			= 0x0086D500;
	static const UInt32 NewTES						= 0x0044FB20;
	static const UInt32 NewPlayerCharacter			= 0x00938180;
	static const UInt32 NewSceneGraph				= 0x00878610;
	static const UInt32 NewMenuInterfaceManager		= 0x0070A130;
	static const UInt32 NewQueuedModelLoader		= 0x00442650;
	static const UInt32 Render						= 0x008706B0;
	static const UInt32 ProcessImageSpaceShaders	= 0x00B55AC0;
	static const UInt32 RenderWorldSceneGraph		= 0x00873200;
	static const UInt32 RenderFirstPerson			= 0x00875110;
};

class ShaderManagerBase {
public:
};
