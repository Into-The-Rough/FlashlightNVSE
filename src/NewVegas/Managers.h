#pragma once

class ShaderManager;	extern ShaderManager*	TheShaderManager;
class TextureManager;	extern TextureManager*	TheTextureManager;
class RenderManager;	extern RenderManager*	TheRenderManager;

class TES;					extern TES*					Tes;
class PlayerCharacter;		extern PlayerCharacter*		Player;
class SceneGraph;			extern SceneGraph*			WorldSceneGraph;
class MenuInterfaceManager;	extern MenuInterfaceManager*	InterfaceManager;

#include "../core/RenderManager.h"
#include "../core/TextureRecord.h"
#include "../core/ShaderManager.h"
#include "../core/TextureManager.h"

void InitializeManagers();
