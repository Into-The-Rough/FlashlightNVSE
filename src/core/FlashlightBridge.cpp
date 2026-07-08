#include "Config.h"
#include "FlashlightBridge.h"

namespace {

typedef UInt32 (__cdecl* GetBuiltinFeatureStateFn)(UInt32 featureId);

FlashlightRenderStateV1 g_state = {};
UInt64 g_frameId = 0;
UInt32 g_generation = 1;
bool g_previousActive = false;
bool g_previousFirstPerson = false;
GetBuiltinFeatureStateFn g_getBuiltinState = nullptr;
bool g_mrcResolved = false;

}

namespace FlashlightBridge {

void Update()
{
	FlashlightRenderStateV1 next = {};
	next.size = sizeof(next);
	next.version = FLASHLIGHT_RENDER_STATE_VERSION;
	next.frameId = ++g_frameId;

	if (TheShaderManager && TheShaderManager->Effects.Flashlight) {
		FlashlightEffect* flashlight = TheShaderManager->Effects.Flashlight;
		bool active = flashlight->Enabled && flashlight->spotLightActive &&
			flashlight->SpotLight;
		next.active = active ? 1 : 0;
		next.firstPerson = Player && !Player->isThirdPerson ? 1 : 0;
		if (active) {
			const D3DXVECTOR4& position = TheShaderManager->SpotLightPosition[0];
			const D3DXVECTOR4& volume = TheShaderManager->VolumetricData;
			const D3DXVECTOR4& direction = TheShaderManager->SpotLightDirection[0];
			const D3DXVECTOR4& colour = TheShaderManager->SpotLightColor[0];
			next.lightPosition[0] = position.x;
			next.lightPosition[1] = position.y;
			next.lightPosition[2] = position.z;
			next.volumePosition[0] = volume.x;
			next.volumePosition[1] = volume.y;
			next.volumePosition[2] = volume.z;
			next.lightDirection[0] = direction.x;
			next.lightDirection[1] = direction.y;
			next.lightDirection[2] = direction.z;
			next.colour[0] = colour.x;
			next.colour[1] = colour.y;
			next.colour[2] = colour.z;
			next.range = position.w;
			next.outerAngleDegrees = direction.w;
			next.dimmer = colour.w;
			bool viewEnabled = next.firstPerson
				? Config::Flashlight.VolumetricFirstPerson
				: Config::Flashlight.VolumetricThirdPerson;
			next.volumetricStrength =
				Config::Flashlight.VolumetricEnabled && viewEnabled
				? Config::Flashlight.VolumetricStrength : 0.0f;
			if (next.firstPerson && volume.w > 0.0f)
				next.volumetricStrength *= volume.w;
			next.anisotropy = Config::Flashlight.VolumetricG;
			next.heightFalloff = Config::Flashlight.VolumetricHeightFalloff;
			next.maxDistance = Config::Flashlight.VolumetricMaxDistance;
		}
	}
	bool active = next.active != 0;
	bool firstPerson = next.firstPerson != 0;
	if (active != g_previousActive)
		++g_generation;
	next.generation = g_generation;
	g_previousActive = active;
	g_previousFirstPerson = firstPerson;
	g_state = next;
}

void Invalidate()
{
	FlashlightRenderStateV1 next = {};
	next.size = sizeof(next);
	next.version = FLASHLIGHT_RENDER_STATE_VERSION;
	next.frameId = ++g_frameId;
	if (g_previousActive)
		++g_generation;
	next.generation = g_generation;
	g_previousActive = false;
	g_previousFirstPerson = false;
	g_state = next;
}

void DetectMRC()
{
	if (g_mrcResolved)
		return;
	g_mrcResolved = true;
	HMODULE module = GetModuleHandleA("MojaveRenderCore.dll");
	if (!module)
		return;
	g_getBuiltinState = (GetBuiltinFeatureStateFn)GetProcAddress(
		module, "MRC_GetBuiltinFeatureState");
}

bool MRCVolumeActive()
{
	if (!g_mrcResolved)
		DetectMRC();
	return g_getBuiltinState && g_getBuiltinState(3) == 2;
}

}

extern "C" {

bool __cdecl BetterFlashlightNVSE_GetRenderState(
	UInt32 version, FlashlightRenderStateV1* outState)
{
	if (version != FLASHLIGHT_RENDER_STATE_VERSION || !outState ||
		outState->size < sizeof(FlashlightRenderStateV1))
		return false;
	*outState = g_state;
	return true;
}

}
