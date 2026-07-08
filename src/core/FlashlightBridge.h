#pragma once

#define FLASHLIGHT_RENDER_STATE_VERSION 1

struct FlashlightRenderStateV1 {
	UInt32 size;
	UInt32 version;
	UInt64 frameId;
	UInt32 active;
	UInt32 firstPerson;
	float lightPosition[3];
	float volumePosition[3];
	float lightDirection[3];
	float colour[3];
	float range;
	float outerAngleDegrees;
	float dimmer;
	float volumetricStrength;
	float anisotropy;
	float heightFalloff;
	float maxDistance;
	UInt32 generation;
};

namespace FlashlightBridge {

void Update();
void Invalidate();
void DetectMRC();
bool MRCVolumeActive();

}

extern "C" {
__declspec(dllexport) bool __cdecl BetterFlashlightNVSE_GetRenderState(
	UInt32 version, FlashlightRenderStateV1* outState);
}
