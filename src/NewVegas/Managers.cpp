ShaderManager*  TheShaderManager  = NULL;
TextureManager* TheTextureManager = NULL;
RenderManager*  TheRenderManager  = NULL;

TES*                Tes              = NULL;
PlayerCharacter*    Player           = NULL;
SceneGraph*         WorldSceneGraph  = NULL;
MenuInterfaceManager* InterfaceManager = NULL;

namespace {
	bool resetPending = false;

	bool __cdecl DeviceResetCallback(bool beforeReset, void*) {
		if (!TheShaderManager || !TheTextureManager)
			return true;

		if (beforeReset) {
			if (!resetPending) {
				TheShaderManager->ReleaseDeviceResources();
				resetPending = true;
			}
			return true;
		}

		if (resetPending) {
			TheShaderManager->RestoreDeviceResources();
			resetPending = false;
		}
		return true;
	}
}

void InitializeManagers() {
	TextureManager::Initialize();
	ShaderManager::Initialize();
	TheRenderManager->renderer->AddResetNotificationFunc(DeviceResetCallback, nullptr);
}
