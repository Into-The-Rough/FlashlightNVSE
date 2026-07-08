#pragma once

class CombineDepthEffect : public EffectRecord
{
public:
	CombineDepthEffect() : EffectRecord("CombineDepth") {
		johnnyguitar = GetModuleHandleA("johnnyguitar.dll");
		if (johnnyguitar)
			JG_GetClipDist = (float(__cdecl*)())GetProcAddress(johnnyguitar, "JGGetViewmodelClipDistance");
	};

	HMODULE johnnyguitar = nullptr;
	float(__cdecl* JG_GetClipDist)() = nullptr;

	struct CombineDepthStruct {
		float viewNearZ;
	};
	CombineDepthStruct	Constants;

	struct CombineDepthTextures {
		IDirect3DTexture9* CombinedDepthTexture;
		IDirect3DSurface9* CombinedDepthSurface;
	};
	CombineDepthTextures	Textures{};

	void	UpdateConstants();
	void	RegisterTextures();
};
