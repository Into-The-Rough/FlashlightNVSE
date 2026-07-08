#include "Volumetric.h"
#include "../core/Config.h"
#include "../core/FlashlightBridge.h"

void VolumetricEffect::RegisterConstants() {
	TheShaderManager->RegisterConstant("TESR_VolumetricControl", &Constants.Control);
}

void VolumetricEffect::RegisterTextures() {
	//half-res HDR buffer for the in-scatter march; the Combine pass upsamples it
	TheTextureManager->InitTexture("TESR_VolumetricBuffer", &Textures.VolumetricTexture, &Textures.VolumetricSurface,
		TheRenderManager->renderer->width / 2, TheRenderManager->renderer->height / 2, D3DFMT_A16B16G16R16F);
}

void VolumetricEffect::UpdateConstants() {
	//strength is forced to 0 when disabled - globally or for the current camera view -
	//so both the march and the Combine gate add nothing (a stale buffer can never bleed in)
	bool viewEnabled = (Player && Player->isThirdPerson)
		? Config::Flashlight.VolumetricThirdPerson
		: Config::Flashlight.VolumetricFirstPerson;
	float strength = (Config::Flashlight.VolumetricEnabled && viewEnabled &&
		!FlashlightBridge::MRCVolumeActive())
		? Config::Flashlight.VolumetricStrength : 0.0f;
	Constants.Control = D3DXVECTOR4(strength, Config::Flashlight.VolumetricG,
		Config::Flashlight.VolumetricHeightFalloff, Config::Flashlight.VolumetricMaxDistance);
}
