#pragma once

typedef std::map<std::string, IDirect3DBaseTexture9*> TextureList;
typedef std::map<std::string, IDirect3DBaseTexture9**> TexturePointersList;
typedef std::vector<TextureRecord*> WaterMapList;

class TextureManager { // Never disposed
public:
	static void				Initialize();
	bool					CreateDeviceResources();
	void					ReleaseDeviceResources();
	bool					InitTexture(const char* Name, IDirect3DTexture9** Texture, IDirect3DSurface9** Surface, int Width, int Height, D3DFORMAT format, bool mipmaps = false);
	void					RegisterTexture(const char* Name, IDirect3DBaseTexture9** Texture);
	void					SetWaterHeightMap(IDirect3DBaseTexture9* WaterHeightMap);
    void                    SetWaterReflectionMap(IDirect3DBaseTexture9* WaterReflectionMap);
	IDirect3DBaseTexture9*	GetFileTexture(std::string TexturePath, TextureRecord::TextureRecordType type);
	IDirect3DBaseTexture9* 	GetCachedTexture(std::string& pathS);
	IDirect3DBaseTexture9*	GetTextureByName(std::string& Name);

	IDirect3DTexture9*		SourceTexture = nullptr;
	IDirect3DSurface9*		SourceSurface = nullptr;
	IDirect3DTexture9* 		RenderedTexture = nullptr;
	IDirect3DSurface9*		RenderedSurface = nullptr;
	IDirect3DTexture9* 		BloomTexture = nullptr;
	IDirect3DSurface9*		BloomSurface = nullptr;
	IDirect3DTexture9*		DepthTexture = nullptr;
	IDirect3DTexture9*		DepthTextureViewModel = nullptr;
	IDirect3DSurface9*		CombinedDepthSurface = nullptr;
	IDirect3DTexture9*		CombinedDepthTexture = nullptr;
	IDirect3DTexture9*		DepthTextureINTZ = nullptr;
	IDirect3DSurface9*		DepthSurface = nullptr;
	IDirect3DBaseTexture9*	SpotLightCookie = nullptr;
	TextureList				TextureCache;
	TexturePointersList		TextureNames;
    WaterMapList         	WaterHeightMapTextures;
    WaterMapList         	WaterReflectionMapTextures;

    IDirect3DBaseTexture9*  WaterHeightMapB = nullptr;
    IDirect3DBaseTexture9*  WaterReflectionMapB = nullptr;
};
