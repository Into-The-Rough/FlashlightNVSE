#pragma once

class VolumetricEffect : public EffectRecord
{
public:
	VolumetricEffect() : EffectRecord("Volumetric") {};

	struct VolumetricStruct {
		D3DXVECTOR4	Control;	//x=strength, y=anisotropy g, z=height falloff, w=max distance
	};
	VolumetricStruct	Constants;

	struct VolumetricTextures {
		IDirect3DTexture9* VolumetricTexture;
		IDirect3DSurface9* VolumetricSurface;
	};
	VolumetricTextures	Textures{};

	void	UpdateConstants();
	void	UpdateSettings() {};
	void	RegisterConstants();
	void	RegisterTextures();
};
