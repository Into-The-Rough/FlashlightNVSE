#include "Hooks.h"
#include "../../core/Config.h"
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)

//if NewVegasReloaded.dll is loaded, its flashlight effect duplicates ours. NVR gates its
//cone on a single engine call - 0x822B90 (MagicTarget::HasActiveMagicItem) - to read the
//pip-boy light toggle, and that is the ONLY site NVR calls 0x822B90 from. returning false
//for calls whose return address lands inside NVR's module makes NVR read the light as off,
//so its cone, spotlight and engine light all skip rendering (EffectRecord::Render bails on
//!ShouldRender). our own call and the engine's own ~10 callers live outside NVR's module,
//so our flashlight and the real pip-boy light are untouched.
static UInt8* g_nvrBase = nullptr;
static UInt8* g_nvrEnd  = nullptr;

void DetectNVR() {
	HMODULE h = GetModuleHandleA("NewVegasReloaded.dll");
	if (!h) return;
	IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)h;
	IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)((UInt8*)h + dos->e_lfanew);
	g_nvrBase = (UInt8*)h;
	g_nvrEnd  = g_nvrBase + nt->OptionalHeader.SizeOfImage;
}

char (__thiscall* HasActiveMagicItem)(void*, void*, char) = (char (__thiscall*)(void*, void*, char))0x00822B90;
char __fastcall HasActiveMagicItemHook(void* This, void* edx, void* magicItem, char flag) {
	void* ret = _ReturnAddress();
	if (g_nvrBase && Config::Flashlight.DisableNVRFlashlight && ret >= g_nvrBase && ret < g_nvrEnd)
		return 0;
	return HasActiveMagicItem(This, magicItem, flag);
}

void AttachHooks() {
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());

	//capture singletons the render path needs
	DetourAttach(&(PVOID&)InitializeRenderer,      &InitializeRendererHook);
	DetourAttach(&(PVOID&)NewPlayerCharacter,      &NewPlayerCharacterHook);
	DetourAttach(&(PVOID&)NewSceneGraph,           &NewSceneGraphHook);
	DetourAttach(&(PVOID&)NewTES,                  &NewTESHook);
	DetourAttach(&(PVOID&)NewMenuInterfaceManager, &NewMenuInterfaceManagerHook);

	//frame-start hook - set up scene camera + constants before the world renders,
	//while WorldSceneGraph->camera is the real view camera (NVR does this in Render)
	DetourAttach(&(PVOID&)Render, &RenderHook);
	DetourAttach(&(PVOID&)RenderCamera, &RenderCameraHook);

	//main frame hook - runs our effect chain before vanilla post-process
	DetourAttach(&(PVOID&)ProcessImageSpaceShaders, &ProcessImageSpaceShadersHook);

	//capture depth at world-render time so post-vanilla draws can sample it
	DetourAttach(&(PVOID&)RenderWorldSceneGraph,    &RenderWorldSceneGraphHook);
	DetourAttach(&(PVOID&)RenderFirstPerson,        &RenderFirstPersonHook);

	//suppress NVR's duplicate flashlight when it's loaded (gated by DetectNVR + config)
	DetourAttach(&(PVOID&)HasActiveMagicItem,       &HasActiveMagicItemHook);

	MaterialPass::InitHooks();

	DetourTransactionCommit();

	//keep viewmodel in depth buffer. NVR does this to prevent the flashlight
	//ignoring the 1st-person weapon. idempotent if multiple mods apply it.
	SafeWrite8(0x008751C0, 0);

	//without this jump, post-vanilla draws on currentRTGroup go to a stale /
	//partial viewport and the flashlight shows up as a small rectangle in the
	//upper-left instead of full-screen. NVR/NVRShadows both apply it; we get
	//the same fix when running standalone. idempotent across plugins.
	SafeWriteJump(0x00C03F49, 0x00C03F5A);

	//resolve world depth inside RenderWorldSceneGraph (last call before its epilogue)
	//instead of in the chained post-hook, so it lands before NVR's 3rd-person Z clear
	//regardless of detour order. replaces the 5-byte call to sub_872DD0 in place.
	SafeWriteCall(0x00874418, (UInt32)&FinalizeWorldRenderHook);
}
