#pragma once
#define FrameFVF D3DFVF_XYZ | D3DFVF_TEX1

#include "ShaderRecord.h"
#include "EffectRecord.h"
#include "../effects/Flashlight.h"
#include "../effects/Normals.h"
#include "../effects/CombineDepth.h"
#include "../effects/Volumetric.h"

struct ShaderConstants {
	D3DXVECTOR4		ReciprocalResolution;
	D3DXVECTOR4		SunDir;
	D3DXVECTOR4		SunColor;
	D3DXVECTOR4		DebugVar;
};

typedef std::map<std::string, EffectRecord**> EffectsList;
typedef std::map<std::string, D3DXVECTOR4>    CustomConstants;

struct FrameVS { float x, y, z, u, v; };

__declspec(align(16)) class ShaderManager : public ShaderManagerBase {
public:
	static void Initialize();

	void* operator new(size_t i) { return _mm_malloc(i, 16); }

	template <typename T> void RegisterEffect(T** Pointer);
	void   RegisterConstant(const char* Name, D3DXVECTOR4* FloatValue);
	bool   CreateFrameVertex(UInt32 Width, UInt32 Height, IDirect3DVertexBuffer9** FrameVertex);
	void   ReleaseDeviceResources();
	bool   RestoreDeviceResources();
	void   InitializeConstants();
	void   UpdateConstants();
	void   UpdateSpotLightState();
	void   RenderEffects(IDirect3DSurface9* RenderTarget);
	void   RenderEffectToRT(IDirect3DSurface9* RenderTarget, EffectRecord* Effect, bool clearRenderTarget);

	struct EffectsStruct {
		FlashlightEffect*   Flashlight;
		NormalsEffect*      Normals;
		CombineDepthEffect* CombineDepth;
		VolumetricEffect*   Volumetric;
	};

	EffectsStruct           Effects{};
	EffectsList             EffectsNames;
	ShaderConstants         ShaderConst;
	CustomConstants         CustomConst;
	std::map<std::string, D3DXVECTOR4*> ConstantsTable;
	IDirect3DVertexBuffer9* FrameVertex = nullptr;
	bool                    DeviceReady = false;

	D3DXVECTOR4   SpotLightPosition[SpotLightsMax];
	D3DXVECTOR4   SpotLightColor[SpotLightsMax];
	D3DXVECTOR4   SpotLightDirection[SpotLightsMax];
	D3DXVECTOR4   VolumetricData;   //xyz = march light pos (offset off-camera in 1st person), w = 1st-person strength floor
	D3DXMATRIX    SpotLightWorldToLightMatrix[SpotLightsMax];
};
