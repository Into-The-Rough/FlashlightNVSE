#include "Config.h"
#include "FlashlightBridge.h"

namespace {
	template <class T>
	void ReleaseResource(T*& resource) {
		if (resource) {
			resource->Release();
			resource = nullptr;
		}
	}

	bool HasEffectTargets(ShaderManager* manager) {
		return manager->Effects.CombineDepth->Textures.CombinedDepthTexture &&
			manager->Effects.CombineDepth->Textures.CombinedDepthSurface &&
			manager->Effects.Normals->Textures.NormalsTexture &&
			manager->Effects.Normals->Textures.NormalsSurface &&
			manager->Effects.Volumetric->Textures.VolumetricTexture &&
			manager->Effects.Volumetric->Textures.VolumetricSurface;
	}

	bool HasCoreTargets() {
		return TheTextureManager->SourceTexture && TheTextureManager->SourceSurface &&
			TheTextureManager->RenderedTexture && TheTextureManager->RenderedSurface &&
			TheTextureManager->DepthTexture && TheTextureManager->DepthTextureViewModel;
	}
}

void ShaderManager::Initialize() {
	TheShaderManager = new ShaderManager();
	TheShaderManager->FrameVertex = NULL;
	TheShaderManager->DeviceReady = false;

	TheShaderManager->CreateFrameVertex(TheRenderManager->renderer->width, TheRenderManager->renderer->height, &TheShaderManager->FrameVertex);

	std::error_code ec;
	std::filesystem::create_directories("Data\\Shaders\\BetterFlashlightNVSE\\Effects\\Cache", ec);

	//core matrices / camera
	TheShaderManager->RegisterConstant("TESR_ViewTransform",              (D3DXVECTOR4*)&TheRenderManager->ViewMatrix);
	TheShaderManager->RegisterConstant("TESR_InvViewTransform",           (D3DXVECTOR4*)&TheRenderManager->InvViewMatrix);
	TheShaderManager->RegisterConstant("TESR_ProjectionTransform",        (D3DXVECTOR4*)&TheRenderManager->renderer->projMatrix);
	TheShaderManager->RegisterConstant("TESR_InvProjectionTransform",     (D3DXVECTOR4*)&TheRenderManager->InvProjMatrix);
	TheShaderManager->RegisterConstant("TESR_ViewProjectionTransform",    (D3DXVECTOR4*)&TheRenderManager->ViewProjMatrix);
	TheShaderManager->RegisterConstant("TESR_InvViewProjectionTransform", (D3DXVECTOR4*)&TheRenderManager->InvViewProjMatrix);
	TheShaderManager->RegisterConstant("TESR_WorldViewProjectionTransform", (D3DXVECTOR4*)&TheRenderManager->WorldViewProjMatrix);
	TheShaderManager->RegisterConstant("TESR_CameraForward",    &TheRenderManager->CameraForward);
	TheShaderManager->RegisterConstant("TESR_CameraPosition",   &TheRenderManager->CameraPosition);
	TheShaderManager->RegisterConstant("TESR_CameraData",       &TheRenderManager->CameraData);
	TheShaderManager->RegisterConstant("TESR_DepthConstants",   &TheRenderManager->DepthConstants);
	TheShaderManager->RegisterConstant("TESR_ReciprocalResolution", &TheShaderManager->ShaderConst.ReciprocalResolution);
	TheShaderManager->RegisterConstant("TESR_SunDirection",     &TheShaderManager->ShaderConst.SunDir);
	TheShaderManager->RegisterConstant("TESR_SunColor",         &TheShaderManager->ShaderConst.SunColor);
	TheShaderManager->RegisterConstant("TESR_DebugVar",         &TheShaderManager->ShaderConst.DebugVar);
	TheShaderManager->RegisterConstant("TESR_SpotLightPosition",  (D3DXVECTOR4*)&TheShaderManager->SpotLightPosition);
	TheShaderManager->RegisterConstant("TESR_SpotLightColor",     (D3DXVECTOR4*)&TheShaderManager->SpotLightColor);
	TheShaderManager->RegisterConstant("TESR_SpotLightDirection", (D3DXVECTOR4*)&TheShaderManager->SpotLightDirection);
	TheShaderManager->RegisterConstant("TESR_SpotLightToWorldTransform", (D3DXVECTOR4*)&TheShaderManager->SpotLightWorldToLightMatrix[0]);
	TheShaderManager->RegisterConstant("TESR_VolumetricData", &TheShaderManager->VolumetricData);

	TheShaderManager->RegisterEffect<CombineDepthEffect>(&TheShaderManager->Effects.CombineDepth);
	TheShaderManager->RegisterEffect<NormalsEffect>(&TheShaderManager->Effects.Normals);
	TheShaderManager->RegisterEffect<VolumetricEffect>(&TheShaderManager->Effects.Volumetric);
	TheShaderManager->RegisterEffect<FlashlightEffect>(&TheShaderManager->Effects.Flashlight);

	TheShaderManager->InitializeConstants();
	TheShaderManager->DeviceReady = TheShaderManager->FrameVertex && HasCoreTargets() && HasEffectTargets(TheShaderManager);
}

bool ShaderManager::CreateFrameVertex(UInt32 Width, UInt32 Height, IDirect3DVertexBuffer9** FrameVertex) {
	void* VertexData = NULL;
	float OffsetX = (1.0f / (float)Width) * 0.5f;
	float OffsetY = (1.0f / (float)Height) * 0.5f;

	FrameVS FrameVertices[] = {
		{ -1.0f,  1.0f, 1.0f, 0.0f + OffsetX, 0.0f + OffsetY },
		{ -1.0f, -1.0f, 1.0f, 0.0f + OffsetX, 1.0f + OffsetY },
		{  1.0f,  1.0f, 1.0f, 1.0f + OffsetX, 0.0f + OffsetY },
		{  1.0f, -1.0f, 1.0f, 1.0f + OffsetX, 1.0f + OffsetY }
	};
	HRESULT create = TheRenderManager->renderer->device->CreateVertexBuffer(
		4 * sizeof(FrameVS), D3DUSAGE_WRITEONLY, FrameFVF, D3DPOOL_DEFAULT, FrameVertex, NULL);
	if (FAILED(create))
		return false;
	HRESULT lock = (*FrameVertex)->Lock(0, 0, &VertexData, NULL);
	if (FAILED(lock)) {
		ReleaseResource(*FrameVertex);
		return false;
	}
	memcpy(VertexData, FrameVertices, sizeof(FrameVertices));
	(*FrameVertex)->Unlock();
	return true;
}

void ShaderManager::ReleaseDeviceResources() {
	DeviceReady = false;
	FlashlightBridge::Invalidate();
	MaterialPass::BeginFrame(false);

	IDirect3DDevice9* device = TheRenderManager->renderer->device;
	for (UInt32 stage = 0; stage < 16; stage++)
		device->SetTexture(stage, nullptr);
	device->SetStreamSource(0, nullptr, 0, 0);

	for (const auto& [name, effect] : EffectsNames) {
		(*effect)->OnLostDevice();
		(*effect)->ClearSamplerBindings();
	}

	ReleaseResource(Effects.CombineDepth->Textures.CombinedDepthSurface);
	ReleaseResource(Effects.CombineDepth->Textures.CombinedDepthTexture);
	ReleaseResource(Effects.Normals->Textures.NormalsSurface);
	ReleaseResource(Effects.Normals->Textures.NormalsTexture);
	ReleaseResource(Effects.Volumetric->Textures.VolumetricSurface);
	ReleaseResource(Effects.Volumetric->Textures.VolumetricTexture);
	TheTextureManager->ReleaseDeviceResources();
	ReleaseResource(FrameVertex);
}

bool ShaderManager::RestoreDeviceResources() {
	bool success = TheTextureManager->CreateDeviceResources();

	Effects.CombineDepth->RegisterTextures();
	Effects.Normals->RegisterTextures();
	Effects.Volumetric->RegisterTextures();
	if (!HasEffectTargets(this))
		success = false;

	if (!CreateFrameVertex(TheRenderManager->renderer->width, TheRenderManager->renderer->height, &FrameVertex))
		success = false;

	for (const auto& [name, effect] : EffectsNames) {
		if (!(*effect)->OnResetDevice())
			success = false;
	}

	InitializeConstants();
	DeviceReady = success && HasCoreTargets() && HasEffectTargets(this) && FrameVertex;
	return DeviceReady;
}

template <typename T> void ShaderManager::RegisterEffect(T** Pointer) {
	T* effect = new T();
	*Pointer = effect;
	EffectsNames[effect->Name] = (EffectRecord**)Pointer;
	effect->UpdateSettings();
	effect->RegisterConstants();
	effect->RegisterTextures();
	effect->LoadEffect();
	effect->Enabled = effect->IsLoaded() && Config::Flashlight.Enabled;
}

void ShaderManager::RegisterConstant(const char* Name, D3DXVECTOR4* FloatValue) {
	ConstantsTable[Name] = FloatValue;
}

void ShaderManager::InitializeConstants() {
	ShaderConst.ReciprocalResolution.x = 1.0f / (float)TheRenderManager->renderer->width;
	ShaderConst.ReciprocalResolution.y = 1.0f / (float)TheRenderManager->renderer->height;
	ShaderConst.ReciprocalResolution.z = (float)TheRenderManager->renderer->width / (float)TheRenderManager->renderer->height;
	ShaderConst.ReciprocalResolution.w = 0.0f;

	ShaderConst.DebugVar = D3DXVECTOR4(0, 1, 0, 0);
}

//returns a brightness multiplier for warm-up (ramp on toggle-on) * sporadic flicker
//(mostly steady, occasional brief noisy dips). folded into the light colour so the
//surface pool and the volumetric beam flicker together. call once per frame.
static float FlashlightFlickerWarmup(bool active) {
	DWORD now = GetTickCount();
	static UInt32 rng = 0x2545F491u;
	rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;   //xorshift noise, no rand()/includes
	float r = (float)(rng & 0xFFFF) / 65535.0f;            //[0,1)

	//warm-up: smooth ramp 0->1 over WarmupTime seconds after the light turns on
	static DWORD onSince = 0;
	static bool  wasActive = false;
	if (active && !wasActive) onSince = now;
	wasActive = active;
	float warmup = 1.0f;
	float wt = Config::Flashlight.WarmupTime;
	if (wt > 0.0f && active) {
		float w = (float)(now - onSince) * 0.001f / wt;
		w = w < 0.0f ? 0.0f : (w > 1.0f ? 1.0f : w);
		warmup = w * w * (3.0f - 2.0f * w);
	}

	//sporadic flicker: steady, then occasional short bursts of rapid dips
	float amt = Config::Flashlight.Flicker;
	float flicker = 1.0f;
	static DWORD burstEnd = 0, nextBurst = 0;
	if (amt > 0.0f && active) {
		if (nextBurst == 0) nextBurst = now + 2000;
		if (now < burstEnd) {
			flicker = 1.0f - amt * r;                                  //rapid jitter during a burst
		}
		else if (now >= nextBurst) {
			burstEnd = now + 30 + (DWORD)(r * 110.0f);                 //30-140ms burst
			float a = amt < 0.05f ? 0.05f : amt;
			DWORD base = (DWORD)(1500.0f / a);                          //higher amount = more frequent
			nextBurst = burstEnd + base + (DWORD)(r * (float)(base + 500));
		}
	}
	else { burstEnd = 0; nextBurst = 0; }

	return warmup * flicker;
}

void ShaderManager::UpdateConstants() {
	if (!Config::Main.RenderEffects) return;
	if (!Player || !Player->parentCell) return;

	//refresh sun color/direction from sky (flashlight dims during daytime). fallback if sky unavailable
	if (Tes && Tes->sky) {
		Sky* sky = Tes->sky;
		ShaderConst.SunColor.x = sky->sunDirectional.r;
		ShaderConst.SunColor.y = sky->sunDirectional.g;
		ShaderConst.SunColor.z = sky->sunDirectional.b;
		ShaderConst.SunColor.w = 1.0f;

		if (Tes->directionalLight) {
			D3DXVECTOR4 dir = Tes->directionalLight->direction.toD3DXVEC4() * -1.0f;
			dir.w = 0.0f;
			D3DXVec4Normalize(&dir, &dir);
			dir.w = 1.0f;
			ShaderConst.SunDir = dir;
		}
	}
	else {
		ShaderConst.SunColor = D3DXVECTOR4(0.3f, 0.3f, 0.3f, 1.0f);
		ShaderConst.SunDir   = D3DXVECTOR4(0.0f, 0.0f, -1.0f, 1.0f);
	}

	for (const auto& [Name, effect] : EffectsNames) {
		if ((*effect)->Enabled) (*effect)->UpdateConstants();
	}
}

//copy NiSpotLight state into shader constant arrays (what GetNearbyLights does in NVR).
//called once per frame from the frame-start hook - the flicker rng advances per call,
//so a second call within a frame would give the surface pool and the beam different values
void ShaderManager::UpdateSpotLightState() {
	NiSpotLight* sl = Effects.Flashlight->SpotLight;
	bool flActive = Effects.Flashlight->Enabled && Effects.Flashlight->spotLightActive && sl;
	float flickerWarmup = FlashlightFlickerWarmup(flActive);   //call every frame so toggle-on is tracked
	if (flActive) {
		SpotLightPosition[0]  = sl->m_worldTransform.pos.toD3DXVEC4();
		SpotLightPosition[0].w = sl->radius;
		SpotLightDirection[0] = D3DXVECTOR4(
			sl->m_worldTransform.rot.data[0][0],
			sl->m_worldTransform.rot.data[1][0],
			sl->m_worldTransform.rot.data[2][0],
			sl->outerSpotAngle);
		//flicker + warm-up scale the colour (rgb), so pool and beam dim together
		SpotLightColor[0] = D3DXVECTOR4(sl->diffuseColor.r * flickerWarmup, sl->diffuseColor.g * flickerWarmup, sl->diffuseColor.b * flickerWarmup, sl->fadeValue);

		//the volumetric march needs parallax to show a shaft. in 1st person the light
		//sits on the camera (every view ray runs along the beam), so the march uses a
		//virtual light pushed right+down in camera space - the surface pool and cookie
		//keep the true position. w carries the 1st-person strength floor for the shader
		VolumetricData = SpotLightPosition[0];
		VolumetricData.w = 0.0f;
		if (Player && !Player->isThirdPerson && WorldSceneGraph && WorldSceneGraph->camera) {
			NiMatrix33& rot = WorldSceneGraph->camera->m_worldTransform.rot;
			float offR = Config::Flashlight.VolumetricFPOffsetRight;
			float offD = Config::Flashlight.VolumetricFPOffsetDown;
			VolumetricData.x += rot.data[0][2] * offR - rot.data[0][1] * offD;
			VolumetricData.y += rot.data[1][2] * offR - rot.data[1][1] * offD;
			VolumetricData.z += rot.data[2][2] * offR - rot.data[2][1] * offD;
			VolumetricData.w = Config::Flashlight.VolumetricFirstPersonStrength;
		}

	}
	else {
		D3DXVECTOR4 z(0, 0, 0, 0);
		SpotLightPosition[0] = z;
		SpotLightDirection[0] = z;
		SpotLightColor[0] = z;
		VolumetricData = z;
	}
	FlashlightBridge::Update();
}

void ShaderManager::RenderEffectToRT(IDirect3DSurface9* RenderTarget, EffectRecord* Effect, bool clearRenderTarget) {
	IDirect3DDevice9* Device = TheRenderManager->renderer->device;
	Device->SetRenderTarget(0, RenderTarget);
	Effect->Render(Device, RenderTarget, RenderTarget, 0, clearRenderTarget, RenderTarget);
}

void ShaderManager::RenderEffects(IDirect3DSurface9* RenderTarget) {
	if (!Config::Main.RenderEffects) return;
	if (!Player || !Player->parentCell) return;
	if (!Effects.Flashlight->Enabled) return;

	//refresh camera matrices here: vanilla ProcessImageSpaceShaders (which ran
	//before this, post-vanilla) overwrites the engine projMatrix (NiDX9Renderer
	//+0x9C0) with its post-process projection. our toWorld samples
	//TESR_ProjectionTransform (that same field), so without this the cone is
	//stretched to a horizontal oval. NVR refreshes here for the same reason.
	TheRenderManager->UpdateSceneCameraData();
	TheRenderManager->SetupSceneCamera();

	IDirect3DDevice9* Device = TheRenderManager->renderer->device;
	IDirect3DSurface9* SourceSurface  = TheTextureManager->SourceSurface;
	IDirect3DSurface9* RenderedSurface = TheTextureManager->RenderedSurface;

	Device->SetStreamSource(0, FrameVertex, 0, sizeof(FrameVS));
	Device->SetFVF(FrameFVF);

	//depth resolves happen in RenderWorldSceneGraphHook / RenderFirstPersonHook
	//while the device's depth surface is still the world depth — by the time
	//we run post-vanilla here it's been swapped to a smaller HDR target and
	//resolving would only fill a sub-region of DepthTexture (which painted
	//the cone into a small upper-left rectangle).
	//feed depth+normals buffers before the flashlight pass reads them
	RenderEffectToRT(Effects.CombineDepth->Textures.CombinedDepthSurface, Effects.CombineDepth, false);
	RenderEffectToRT(Effects.Normals->Textures.NormalsSurface, Effects.Normals, false);

	//volumetric in-scatter march into the half-res buffer (only when the flashlight is
	//on and volumetric is enabled for the current camera view - Control.x folds the
	//master toggle, the per-view toggle and the strength into one gate)
	if (Effects.Volumetric->Constants.Control.x > 0.0f && Effects.Flashlight->spotLightActive)
		RenderEffectToRT(Effects.Volumetric->Textures.VolumetricSurface, Effects.Volumetric, true);

	Device->SetRenderTarget(0, RenderTarget);
	Device->StretchRect(RenderTarget, NULL, RenderedSurface, NULL, D3DTEXF_NONE);
	Device->StretchRect(RenderTarget, NULL, SourceSurface, NULL, D3DTEXF_NONE);

	//technique 0 = NoShadow. technique 1 (Shadows) needs a scene-from-light
	//depth pass that we don't render — using it produces garbage shadow
	//samples and a black hole in the cone centre.
	Effects.Flashlight->Render(Device, RenderTarget, RenderedSurface, Effects.Flashlight->selectedPass, true, SourceSurface);
}
