#include "src/NewVegas/Framework.h"
#include "src/core/Config.h"
#include "src/core/FlashlightBridge.h"
#include "src/features/Battery.h"

static PluginHandle g_pluginHandle = 0xFFFFFFFF;
static NVSEMessagingInterface* g_messaging = nullptr;
static constexpr UInt32 kJG_OnRenderGameModeUpdate = 10014;

//community/MCM convention: a "reload config" message so INI edits (incl. MCM, which
//writes our config\BetterFlashlightNVSE.ini directly) apply instantly
#define kMessage_ReloadConfig 25

static void DestroyEngineLight() {
	if (TheShaderManager && TheShaderManager->Effects.Flashlight)
		TheShaderManager->Effects.Flashlight->DestroyEngineLight();
}

static void JohnnyMessageHandler(NVSEMessagingInterface::Message* msg) {
	if (msg->type == kJG_OnRenderGameModeUpdate && TheShaderManager && TheShaderManager->Effects.Flashlight)
		TheShaderManager->Effects.Flashlight->PrepareB42OpticsRender();
}

static void MessageHandler(NVSEMessagingInterface::Message* msg) {
	if (msg->type == kMessage_ReloadConfig) {
		Config::Load();
		if (TheShaderManager) {
			//every effect shares the master toggle (as in RegisterEffect) - the depth,
			//normals and volumetric passes must re-enable with the flashlight
			for (const auto& [name, effect] : TheShaderManager->EffectsNames) {
				(*effect)->UpdateSettings();
				(*effect)->Enabled = (*effect)->IsLoaded() && Config::Flashlight.Enabled;
			}
		}
		Battery::OnConfigReload();
		return;
	}
	switch (msg->type) {
	case NVSEMessagingInterface::kMessage_SaveGame:
		DestroyEngineLight();
		break;
	case NVSEMessagingInterface::kMessage_LoadGame:
	case NVSEMessagingInterface::kMessage_PreLoadGame:
	case NVSEMessagingInterface::kMessage_ExitToMainMenu:
	case NVSEMessagingInterface::kMessage_ExitGame:
	case NVSEMessagingInterface::kMessage_ExitGame_Console:
		Battery::Reset();
		DestroyEngineLight();
		break;
	case NVSEMessagingInterface::kMessage_PostLoadGame:
	case NVSEMessagingInterface::kMessage_NewGame:
		Battery::Init();
		break;
	case NVSEMessagingInterface::kMessage_MainGameLoop:
		Battery::MainLoop();
		break;
	case NVSEMessagingInterface::kMessage_PostLoad:
		//all plugins are loaded now - safe to look for NewVegasReloaded.dll and flag its
		//flashlight for suppression. hooks attach at NVSEPlugin_Load; render init runs later
		DetectNVR();
		FlashlightBridge::DetectMRC();
		if (g_messaging)
			g_messaging->RegisterListener(g_pluginHandle, "JohnnyGuitarNVSE", JohnnyMessageHandler);
		break;
	}
}

extern "C" {

__declspec(dllexport) bool NVSEPlugin_Query(const PluginInterface* Interface, PluginInfo* Info) {
	Info->InfoVersion = PluginInfo::kInfoVersion;
	Info->Name = "BetterFlashlightNVSE";
	Info->Version = 100;
	return true;
}

__declspec(dllexport) bool NVSEPlugin_Load(const PluginInterface* Interface) {
	Config::Load();

	if (!Interface->IsEditor) {
		NVSEMessagingInterface* msg = (NVSEMessagingInterface*)Interface->QueryInterface(kInterface_Messaging);
		if (msg) {
			g_messaging = msg;
			g_pluginHandle = Interface->GetPluginHandle();
			msg->RegisterListener(g_pluginHandle, "NVSE", MessageHandler);
		}
		AttachHooks();
	}
	return true;
}

}

BOOL WINAPI DllMain(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved) {
	return TRUE;
}
