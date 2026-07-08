#pragma once
#pragma warning (disable: 4244)

#define DETOURS_INTERNAL
#define assert(a) static_assert(a, "Assertion failed")
#define DIRECTINPUT_VERSION 0x0800
#define D3DXFX_LARGEADDRESS_HANDLE

#include <windows.h>
#include <CommCtrl.h>
#include <string>
#include <regex>
#include <vector>
#include <stack>
#include <map>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <d3d9.h>
#include <d3dx9math.h>
#include <dinput.h>
#include <dsound.h>
#include "../../lib/Detours/detours.h"
#include "../../lib/Nvidia/nvapi.h"
#include "../base/Types.h"
#include "../base/SafeWrite.h"
#include "../base/PluginVersion.h"
#include "../base/Utils.h"
#include "Plugin.h"
#include "nvse/GameNi.h"
#include "nvse/GameHavok.h"
#include "nvse/Game.h"
#include "Defines.h"
#include "Base.h"
#include "Managers.h"
#include "../core/Hooks/GameCommon.h"
#include "Hooks/Game.h"
#include "Hooks/Render.h"
#include "Hooks/MaterialPass.h"
#include "Hooks/Hooks.h"
