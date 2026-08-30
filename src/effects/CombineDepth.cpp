#include "CombineDepth.h"


void CombineDepthEffect::UpdateConstants() {
	NiCamera* Camera = WorldSceneGraph->camera;

	if (!johnnyguitar || !JG_GetClipDist) {
		Constants.viewNearZ = Camera->Frustum.Near;
	}
	else {
		float nearZ = JG_GetClipDist();

		//no floor: clamping here lies to the shader about the plane the viewmodel was drawn with
		Constants.viewNearZ = nearZ > 0.0f ? nearZ : Camera->Frustum.Near;
	}
}

void CombineDepthEffect::RegisterTextures() {
	TheTextureManager->InitTexture("TESR_DepthBuffer", &Textures.CombinedDepthTexture, &Textures.CombinedDepthSurface, TheRenderManager->renderer->width, TheRenderManager->renderer->height, D3DFMT_G32R32F);
}