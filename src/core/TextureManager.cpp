#include <algorithm>

namespace {
	template <class T>
	void ReleaseResource(T*& resource) {
		if (resource) {
			resource->Release();
			resource = nullptr;
		}
	}

	void UnregisterResource(IDirect3DResource9* resource) {
		if (resource)
			NvAPI_D3D9_UnregisterResource(resource);
	}
}

void TextureManager::Initialize() {

	TheTextureManager = new TextureManager();
	TheTextureManager->CreateDeviceResources();

	//cookie texture for the flashlight cone shape. Registering it by sampler
	//name keeps the fallback bind path working even if ResourceName parsing
	//changes with shader preprocessing.
	std::string cookiePath = "Data\\Textures\\Effects\\Flashlight.png";
	TheTextureManager->SpotLightCookie = TheTextureManager->GetFileTexture(cookiePath, TextureRecord::TextureRecordType::PlanarBuffer);
	if (TheTextureManager->SpotLightCookie)
		TheTextureManager->RegisterTexture("TESR_SpotLightTexture", &TheTextureManager->SpotLightCookie);
}

bool TextureManager::CreateDeviceResources() {
	ReleaseDeviceResources();

	IDirect3DDevice9* Device = TheRenderManager->renderer->device;
	UInt32 Width = TheRenderManager->renderer->width;
	UInt32 Height = TheRenderManager->renderer->height;
	bool success = true;

	if (!InitTexture("TESR_SourceBuffer", &SourceTexture, &SourceSurface, Width, Height, D3DFMT_A16B16G16R16F))
		success = false;
	if (!InitTexture("TESR_RenderedBuffer", &RenderedTexture, &RenderedSurface, Width, Height, D3DFMT_A16B16G16R16F))
		success = false;

	HRESULT worldDepth = Device->CreateTexture(Width, Height, 1, D3DUSAGE_DEPTHSTENCIL,
		(D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z'), D3DPOOL_DEFAULT, &DepthTexture, NULL);
	RegisterTexture("TESR_DepthBufferWorld", (IDirect3DBaseTexture9**)&DepthTexture);
	if (SUCCEEDED(worldDepth))
		NvAPI_D3D9_RegisterResource(DepthTexture);
	else
		success = false;

	HRESULT viewDepth = Device->CreateTexture(Width, Height, 1, D3DUSAGE_DEPTHSTENCIL,
		(D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z'), D3DPOOL_DEFAULT, &DepthTextureViewModel, NULL);
	RegisterTexture("TESR_DepthBufferViewModel", (IDirect3DBaseTexture9**)&DepthTextureViewModel);
	if (SUCCEEDED(viewDepth))
		NvAPI_D3D9_RegisterResource(DepthTextureViewModel);
	else
		success = false;

	return success;
}

void TextureManager::ReleaseDeviceResources() {
	UnregisterResource(DepthTextureINTZ);
	UnregisterResource(DepthSurface);
	UnregisterResource(DepthTexture);
	UnregisterResource(DepthTextureViewModel);

	ReleaseResource(SourceSurface);
	ReleaseResource(SourceTexture);
	ReleaseResource(RenderedSurface);
	ReleaseResource(RenderedTexture);
	ReleaseResource(BloomSurface);
	ReleaseResource(BloomTexture);
	ReleaseResource(CombinedDepthSurface);
	ReleaseResource(CombinedDepthTexture);
	ReleaseResource(DepthSurface);
	ReleaseResource(DepthTextureINTZ);
	ReleaseResource(DepthTexture);
	ReleaseResource(DepthTextureViewModel);
}

/*
* Creates a texture of the given size and format and binds a surface to it, so it can be used as render target.
*/
bool TextureManager::InitTexture(const char* Name, IDirect3DTexture9** Texture, IDirect3DSurface9** Surface, int Width, int Height, D3DFORMAT Format, bool mipmaps) {
	IDirect3DDevice9* Device = TheRenderManager->renderer->device;
	// create a texture to receive the surface contents
	HRESULT create;
	if (!mipmaps)
		create = Device->CreateTexture(Width, Height, 1, D3DUSAGE_RENDERTARGET, Format, D3DPOOL_DEFAULT, Texture, NULL);
	else
		create = Device->CreateTexture(Width, Height, 0, D3DUSAGE_RENDERTARGET | D3DUSAGE_AUTOGENMIPMAP, Format, D3DPOOL_DEFAULT, Texture, NULL);

	if (FAILED(create))
		return false;

	// set the surface level to the texture.
	HRESULT surface = (*Texture)->GetSurfaceLevel(0, Surface);
	if (FAILED(surface)) {
		ReleaseResource(*Texture);
		return false;
	}
	RegisterTexture(Name, (IDirect3DBaseTexture9**)Texture);
	return true;
}


/*
* Adds a texture to the list of sampler names recognized in shaders
*/
void TextureManager::RegisterTexture(const char* Name, IDirect3DBaseTexture9** Texture) {
	TextureNames[Name] = Texture;
}

/*
* Gets a texture from the cache based on texture path
*/
IDirect3DBaseTexture9* TextureManager::GetCachedTexture(std::string& pathS) {
	TextureList::iterator t = TextureCache.find(pathS);
	if (t == TextureCache.end()) return nullptr;
	return t->second;
}


/*
* Gets a game dynamic texture by the sampler name
*/
IDirect3DBaseTexture9* TextureManager::GetTextureByName(std::string& Name) {
	TexturePointersList::iterator t = TextureNames.find(Name);
	if (t == TextureNames.end())
		return nullptr;
	return *(t->second);
}


/*
* Loads the actual texture file or get it from cache based on type/Name
*/
IDirect3DBaseTexture9* TextureManager::GetFileTexture(std::string TexturePath, TextureRecord::TextureRecordType Type) {

	IDirect3DBaseTexture9* Texture = GetCachedTexture(TexturePath);
	if (Texture) return Texture;

	switch (Type) {
	case TextureRecord::TextureRecordType::PlanarBuffer:
		D3DXCreateTextureFromFileA(TheRenderManager->renderer->device, TexturePath.data(), (IDirect3DTexture9**)&Texture);
		break;
	case TextureRecord::TextureRecordType::VolumeBuffer:
		D3DXCreateVolumeTextureFromFileA(TheRenderManager->renderer->device, TexturePath.data(), (IDirect3DVolumeTexture9**)&Texture);
		break;
	case TextureRecord::TextureRecordType::CubeBuffer:
		D3DXCreateCubeTextureFromFileA(TheRenderManager->renderer->device, TexturePath.data(), (IDirect3DCubeTexture9**)&Texture);
		break;
	}

	// add texture to cache
	TextureCache[TexturePath] = Texture;
	return Texture;
}


void TextureManager::SetWaterHeightMap(IDirect3DBaseTexture9* WaterHeightMap) {
    if (WaterHeightMapB == WaterHeightMap) return;
    WaterHeightMapB = WaterHeightMap;  //This may cause crashes on certain conditions
	for (WaterMapList::iterator it = TheTextureManager->WaterHeightMapTextures.begin(); it != TheTextureManager->WaterHeightMapTextures.end(); it++){
		 (*it)->Texture = WaterHeightMap;
	}
}


void TextureManager::SetWaterReflectionMap(IDirect3DBaseTexture9* WaterReflectionMap) {
    if (WaterReflectionMapB == WaterReflectionMap) return;
    WaterReflectionMapB = WaterReflectionMap;
	for (WaterMapList::iterator it = TheTextureManager->WaterReflectionMapTextures.begin(); it != TheTextureManager->WaterReflectionMapTextures.end(); it++){
		 (*it)->Texture = WaterReflectionMap;
	}
}




