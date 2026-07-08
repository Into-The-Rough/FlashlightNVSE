#include "Render.h"
#include "../../core/Config.h"
#include "../../core/FlashlightBridge.h"

//frame-start hook, as NVR: set up the scene camera + constants before the world
//renders, while WorldSceneGraph->camera is the real view camera.
void (__thiscall* Render)(Main*, BSRenderedTexture*, int, int) =
	(void (__thiscall*)(Main*, BSRenderedTexture*, int, int))Hooks::Render;
void __fastcall RenderHook(Main* This, UInt32 edx, BSRenderedTexture* RenderedTexture, int Arg2, int Arg3) {
	FlashlightEffect* flashlight = TheShaderManager ? TheShaderManager->Effects.Flashlight : nullptr;
	if (TheRenderManager && TheShaderManager && Player && Player->parentCell &&
		Config::Main.RenderEffects && TheShaderManager->DeviceReady && flashlight && flashlight->Enabled) {
		TheRenderManager->UpdateSceneCameraData();
		TheRenderManager->SetupSceneCamera();
		TheShaderManager->UpdateConstants();
		TheShaderManager->UpdateSpotLightState();
		flashlight->QueueEngineLight();
		MaterialPass::BeginFrame(flashlight->spotLightActive);
	} else {
		MaterialPass::BeginFrame(false);
		FlashlightBridge::Invalidate();
	}
	(*Render)(This, RenderedTexture, Arg2, Arg3);
}

void(__cdecl* ProcessImageSpaceShaders)(NiDX9Renderer*, BSRenderedTexture*, BSRenderedTexture*) =
	(void(__cdecl*)(NiDX9Renderer*, BSRenderedTexture*, BSRenderedTexture*))Hooks::ProcessImageSpaceShaders;

//world depth is resolved inside RenderWorldSceneGraph (FinalizeWorldRenderHook,
//patched in at 0x874418), so it happens before the function returns and therefore
//before any post-hook Z clear. NVR chains this same detour and clears the Z buffer
//in its 3rd-person viewmodel-depth step; grabbing depth in the function body instead
//of this post-hook makes us immune to that clear no matter which detour runs first.
void (__thiscall* RenderWorldSceneGraph)(Main*, Sun*, UInt8, UInt8, UInt8) =
	(void (__thiscall*)(Main*, Sun*, UInt8, UInt8, UInt8))Hooks::RenderWorldSceneGraph;
void __fastcall RenderWorldSceneGraphHook(Main* This, UInt32 edx, Sun* SkySun, UInt8 IsFirstPerson, UInt8 WireFrame, UInt8 Arg4) {
	(*RenderWorldSceneGraph)(This, SkySun, IsFirstPerson, WireFrame, Arg4);

	if (TheRenderManager && TheTextureManager && WorldSceneGraph) {
		MaterialPass::CaptureScene(WorldSceneGraph);
		MaterialPass::RenderWorld();
	}
}

//0x874418 is the last call in RenderWorldSceneGraph before its epilogue (a trivial flag
//setter); the whole world scene graph, incl the 3rd-person player body, is in the depth
//buffer by now. resolve here so we own a correct world-depth copy before any chained
//detour's post-hook clears the engine Z. the engine Z is left untouched here, so NVR's
//own depth resolve can't be clobbered - both flashlights coexist either load order.
//3rd-person viewmodel buffer is left stale and ignored in-shader via TESR_DepthConstants.w.
void (__cdecl* FinalizeWorldRender)(char, char) = (void (__cdecl*)(char, char))0x00872DD0;
void __cdecl FinalizeWorldRenderHook(char a1, char a2) {
	FinalizeWorldRender(a1, a2);

	FlashlightEffect* flashlight = TheShaderManager ? TheShaderManager->Effects.Flashlight : nullptr;
	if (TheRenderManager && TheTextureManager && TheShaderManager && Player && Player->parentCell &&
		Config::Main.RenderEffects && TheShaderManager->DeviceReady && flashlight && flashlight->Enabled &&
		!(InterfaceManager && InterfaceManager->IsPipBoyOpen()))
		TheRenderManager->ResolveDepthBuffer(TheTextureManager->DepthTexture);
}

void (__thiscall* RenderFirstPerson)(Main*, NiDX9Renderer*, NiGeometry*, Sun*, BSRenderedTexture*) =
	(void (__thiscall*)(Main*, NiDX9Renderer*, NiGeometry*, Sun*, BSRenderedTexture*))Hooks::RenderFirstPerson;
void __fastcall RenderFirstPersonHook(Main* This, UInt32 edx, NiDX9Renderer* Renderer, NiGeometry* Geo, Sun* SkySun, BSRenderedTexture* RenderedTexture) {
	if (TheRenderManager) TheRenderManager->renderer->Clear(NULL, NiRenderer::kClear_ZBUFFER);
	(*RenderFirstPerson)(This, Renderer, Geo, SkySun, RenderedTexture);
	if (TheRenderManager && TheTextureManager && TheShaderManager && TheShaderManager->DeviceReady)
		TheRenderManager->ResolveDepthBuffer(TheTextureManager->DepthTextureViewModel);
}

static void ApplyRenderState(NiDX9RenderState* RenderState) {
	RenderState->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE, RenderStateArgs);
	RenderState->SetRenderState(D3DRS_ZWRITEENABLE, D3DZB_FALSE, RenderStateArgs);
	RenderState->SetRenderState(D3DRS_STENCILENABLE, D3DZB_FALSE, RenderStateArgs);
	RenderState->SetRenderState(D3DRS_STENCILMASK, D3DZB_FALSE, RenderStateArgs);
	RenderState->SetRenderState(D3DRS_STENCILWRITEMASK, 255, RenderStateArgs);
	RenderState->SetRenderState(D3DRS_STENCILREF, D3DZB_FALSE, RenderStateArgs);
	RenderState->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP, RenderStateArgs);
	RenderState->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS, RenderStateArgs);
	RenderState->SetRenderState(D3DRS_COLORWRITEENABLE, 15, RenderStateArgs);
	RenderState->SetRenderState(D3DRS_ALPHATESTENABLE, D3DZB_FALSE, RenderStateArgs);
	RenderState->SetRenderState(D3DRS_ALPHABLENDENABLE, D3DZB_FALSE, RenderStateArgs);
	RenderState->SetRenderState(D3DRS_ALPHAREF, 0, RenderStateArgs);
	RenderState->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE, RenderStateArgs);
	RenderState->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE, RenderStateArgs);
	RenderState->SetRenderState(D3DRS_NORMALIZENORMALS, D3DZB_FALSE, RenderStateArgs);
	RenderState->SetRenderState(D3DRS_POINTSIZE, 810365505, RenderStateArgs);
}

void __cdecl ProcessImageSpaceShadersHook(NiDX9Renderer* Renderer, BSRenderedTexture* SourceTarget, BSRenderedTexture* DestinationTarget) {
	if (!TheRenderManager || !TheShaderManager) { ProcessImageSpaceShaders(Renderer, SourceTarget, DestinationTarget); return; }
	if (!Player || !Player->parentCell)          { ProcessImageSpaceShaders(Renderer, SourceTarget, DestinationTarget); return; }
	if (!TheShaderManager->DeviceReady)          { ProcessImageSpaceShaders(Renderer, SourceTarget, DestinationTarget); return; }
	if (!TheShaderManager->Effects.Flashlight->Enabled) { ProcessImageSpaceShaders(Renderer, SourceTarget, DestinationTarget); return; }
	//pipboy is rendered as a near-camera mesh into the same target. our cone
	//math illuminates it because we forced viewmodel into the depth buffer.
	//easiest fix: skip the entire effect chain while the pipboy menu is up.
	if (InterfaceManager && InterfaceManager->IsPipBoyOpen()) { ProcessImageSpaceShaders(Renderer, SourceTarget, DestinationTarget); return; }

	NiDX9RenderState* RenderState = TheRenderManager->renderer->renderState;

	TheRenderManager->UpdateSceneCameraData();
	TheRenderManager->SetupSceneCamera();
	TheShaderManager->UpdateConstants();

	//draw the flashlight onto the game's HDR scene buffer BEFORE vanilla post-
	//process runs, so the beam is tonemapped + bloomed + colour-graded together
	//with the scene (NVR's RenderPreTonemapping path). drawing it post-vanilla on
	//currentRTGroup instead pastes a flat, unfiltered beam onto the finished
	//frame. the Combine pass works in linear space because this buffer is HDR-
	//linear, not the sRGB LDR backbuffer.
	if (SourceTarget) {
		IDirect3DTexture9* GameTexture = SourceTarget->GetD3DTexture(0);
		IDirect3DSurface9* GameSurface = NULL;
		if (GameTexture && GameTexture->GetSurfaceLevel(0, &GameSurface) == D3D_OK && GameSurface) {
			ApplyRenderState(RenderState);
			TheShaderManager->RenderEffects(GameSurface);
			GameSurface->Release();
		}
	}

	ProcessImageSpaceShaders(Renderer, SourceTarget, DestinationTarget);
}
