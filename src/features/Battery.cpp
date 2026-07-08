#include "Battery.h"
#include "../core/Config.h"

namespace Battery {
	float g_charge = 100.0f;
	bool g_depleted = false;
	bool g_wasLightOn = false;
	bool g_wasDialogue = false;
	float g_iconAlpha = 0.0f;
	float g_flashTimer = 0.0f;
	float g_depletionFlickerTimer = 0.0f;
	bool g_depletionFlickerLightOn = false;
	bool g_wasBelowThreshold = false;
	int g_lastFillState = -1;
	Tile* g_rootTile = nullptr;
	Tile* g_iconTile = nullptr;
	bool g_initialized = false;

	constexpr float kFlashDuration = 3.0f;
	constexpr float kFlashRate = 4.0f;
	constexpr float kDepletionFlickerDuration = 1.1f;

	UInt32 GetTraitID(const char* apTraitName) {
		return CdeclCall<UInt32>(0xA00940, apTraitName, 0xFFFFFFFF);
	}

	Tile* GetChild(Tile* apTile, const char* apName) {
		if (!apTile || !apName) return nullptr;
		for (auto node = apTile->firstChild; node; node = node->next) {
			auto child = node->child;
			if (child && child->name.m_data && _stricmp(child->name.m_data, apName) == 0)
				return child;
		}
		return nullptr;
	}

	void SetTileFloat(Tile* apTile, const char* apTraitName, float afValue) {
		if (apTile)
			apTile->SetFloat(GetTraitID(apTraitName), afValue);
	}

	void SetTileString(Tile* apTile, const char* apTraitName, const char* apValue) {
		if (!apTile) return;
		auto value = ThisStdCall<Tile::Value*>(0xA01000, apTile, GetTraitID(apTraitName));
		if (value)
			ThisStdCall(0xA0A300, value, apValue, true);
	}

	void BuildTexturePath(char* apPath, UInt32 auiSize) {
		if (Config::Battery.IconSet >= 1 && Config::Battery.IconSet <= 4)
			sprintf_s(apPath, auiSize, "menus\\set %d\\", Config::Battery.IconSet);
		else
			strcpy_s(apPath, auiSize, "menus\\pipboybattery\\");
	}

	MagicItem* GetPipBoyLightSpell() {
		auto spell = reinterpret_cast<MagicItem**>(0x11C358C);
		return spell ? *spell : nullptr;
	}

	bool IsPipBoyLightOn() {
		auto spell = GetPipBoyLightSpell();
		if (!Player || !spell) return false;

		return ThisStdCall<bool>(0x822B90, &Player->magicTarget, reinterpret_cast<UInt8*>(spell) + 0x18, 1);
	}

	void SetPipBoyLightState(bool abTurnOn) {
		if (IsPipBoyLightOn() == abTurnOn) return;

		auto spell = GetPipBoyLightSpell();
		if (!Player || !spell) return;

		if (abTurnOn) {
			auto caster = reinterpret_cast<void*>(reinterpret_cast<UInt32>(Player) + 0x88);
			auto vtbl = *reinterpret_cast<void***>(caster);
			reinterpret_cast<void(__thiscall*)(void*, void*, UInt32)>(vtbl[0])(caster, spell, 0);
		}
		else {
			ThisStdCall(0x824400, &Player->magicTarget, reinterpret_cast<UInt8*>(spell) + 0x18, nullptr, nullptr);
		}

		auto interfaceManager = *reinterpret_cast<void**>(0x11D8A80);
		if (!interfaceManager) return;
		auto pipboyManager = *reinterpret_cast<void**>(reinterpret_cast<UInt32>(interfaceManager) + 0x174); //FOPipboyManager*
		if (!pipboyManager) return;

		UInt32 state = abTurnOn ? 1 : 0;
		ThisStdCall(0x7FA310, pipboyManager, 1, state, 1); //FOPipboyManager::SetLightEffect
		ThisStdCall(0x7FA310, pipboyManager, 0, state, 1);
	}

	bool IsInDialogue() {
		return *reinterpret_cast<UInt8*>(0x11DCFA4) != 0;
	}

	float GetFrameDelta() {
		return *reinterpret_cast<float*>(0x11F63A0);
	}

	void SetIconVisible(bool abVisible) {
		if (!g_iconTile) return;

		float visible = abVisible ? 1.0f : 0.0f;
		if (g_iconAlpha != visible) {
			g_iconAlpha = visible;
			g_iconTile->SetFloat(Tile::kTileValue_visible, visible);
		}
	}

	void UpdateIconTexture() {
		if (!g_iconTile) return;

		int fillState;
		if (g_charge > 75.0f) fillState = 4;
		else if (g_charge > 50.0f) fillState = 3;
		else if (g_charge > 25.0f) fillState = 2;
		else if (g_charge > 0.0f) fillState = 1;
		else fillState = 0;

		if (fillState == g_lastFillState)
			return;

		g_lastFillState = fillState;
		const char* names[] = { "empty.dds", "25.dds", "50.dds", "75.dds", "full.dds" };
		char texturePath[128];
		char folder[64];
		BuildTexturePath(folder, sizeof(folder));
		sprintf_s(texturePath, "%s%s", folder, names[fillState]);
		SetTileString(g_iconTile, "filename", texturePath);
	}

	void StartFlash(bool abFull) {
		if (!g_iconTile) return;

		g_flashTimer = kFlashDuration;
		g_lastFillState = -1;

		char texturePath[128];
		char folder[64];
		BuildTexturePath(folder, sizeof(folder));
		sprintf_s(texturePath, "%s%s", folder, abFull ? "full.dds" : "empty.dds");
		SetTileString(g_iconTile, "filename", texturePath);
	}

	bool GetDepletionFlickerState(float afTimeRemaining) {
		float elapsed = kDepletionFlickerDuration - afTimeRemaining;
		return elapsed < 0.12f ||
			(elapsed > 0.20f && elapsed < 0.32f) ||
			(elapsed > 0.48f && elapsed < 0.56f) ||
			(elapsed > 0.70f && elapsed < 0.76f);
	}

	void StartDepletionFlicker() {
		g_depletionFlickerTimer = kDepletionFlickerDuration;
		g_depletionFlickerLightOn = true;
		g_lastFillState = -1;
		SetPipBoyLightState(true);
		SetIconVisible(true);
		UpdateIconTexture();
	}

	bool UpdateDepletionFlicker(float afDelta) {
		if (g_depletionFlickerTimer <= 0.0f)
			return false;

		g_depletionFlickerTimer -= afDelta;
		bool flickerOn = g_depletionFlickerTimer > 0.0f && GetDepletionFlickerState(g_depletionFlickerTimer);
		if (flickerOn != g_depletionFlickerLightOn) {
			SetPipBoyLightState(flickerOn);
			g_depletionFlickerLightOn = flickerOn;
		}

		SetIconVisible(flickerOn);
		UpdateIconTexture();

		if (g_depletionFlickerTimer <= 0.0f) {
			g_depletionFlickerTimer = 0.0f;
			g_depletionFlickerLightOn = false;
			SetPipBoyLightState(false);
			g_wasLightOn = false;
			SetIconVisible(false);
			StartFlash(false);
		}

		return true;
	}

	void Reposition() {
		if (!g_rootTile) return;

		SetTileFloat(g_rootTile, "_posX", Config::Battery.IconPosX);
		SetTileFloat(g_rootTile, "_posY", Config::Battery.IconPosY);
		SetTileFloat(g_rootTile, "_fScale", Config::Battery.IconScale);
	}

	void Init() {
		auto hud = *reinterpret_cast<HUDMainMenu**>(0x11D96C0);
		if (!hud || !hud->tile) return;

		g_rootTile = GetChild(hud->tile, "PBB");
		if (!g_rootTile)
			g_rootTile = ThisStdCall<Tile*>(0xA01B00, hud->tile, "menus\\prefabs\\pipboybattery\\battery.xml");
		if (!g_rootTile) return;

		g_iconTile = GetChild(g_rootTile, "PBBIcon");
		if (!g_iconTile) return;

		Reposition();
		g_iconTile->SetFloat(Tile::kTileValue_visible, 0.0f);

		g_charge = 100.0f;
		g_depleted = false;
		g_wasBelowThreshold = false;
		g_wasLightOn = false;
		g_wasDialogue = false;
		g_iconAlpha = 0.0f;
		g_flashTimer = 0.0f;
		g_lastFillState = -1;
		g_initialized = true;
	}

	void MainLoop() {
		if (!Config::Battery.Enabled) return;
		if (!g_initialized) {
			Init();
			if (!g_initialized) return;
		}

		float delta = GetFrameDelta();
		if (delta <= 0.0f || delta > 1.0f) return;

		if (UpdateDepletionFlicker(delta))
			return;

		bool inDialogue = IsInDialogue();
		bool lightOn = IsPipBoyLightOn();

		if (inDialogue && !g_wasDialogue && lightOn) {
			SetPipBoyLightState(false);
			lightOn = false;
		}
		g_wasDialogue = inDialogue;

		if (lightOn && !g_wasLightOn && g_depleted) {
			SetPipBoyLightState(false);
			lightOn = false;
			StartFlash(false);
		}
		g_wasLightOn = lightOn;

		if (g_charge <= Config::Battery.PulseThreshold)
			g_wasBelowThreshold = true;

		if (lightOn) {
			g_charge -= Config::Battery.DrainRate * delta;
			if (g_charge <= 0.0f) {
				g_charge = 0.0f;
				g_depleted = true;
				g_wasLightOn = false;
				g_lastFillState = -1;
				StartDepletionFlicker();
				return;
			}
		}
		else {
			g_charge += Config::Battery.RechargeRate * delta;
			if (g_charge >= 100.0f)
				g_charge = 100.0f;
			if (g_depleted && g_charge >= Config::Battery.RechargeThreshold)
				g_depleted = false;
			if (g_charge >= 100.0f && g_wasBelowThreshold) {
				g_wasBelowThreshold = false;
				StartFlash(true);
			}
		}

		if (g_flashTimer > 0.0f) {
			g_flashTimer -= delta;
			bool flashOn = fmodf(g_flashTimer * kFlashRate, 1.0f) > 0.5f;
			g_iconTile->SetFloat(Tile::kTileValue_visible, flashOn ? 1.0f : 0.0f);
			if (g_flashTimer <= 0.0f) {
				g_flashTimer = 0.0f;
				g_iconTile->SetFloat(Tile::kTileValue_visible, 0.0f);
				g_iconAlpha = 0.0f;
			}
		}
		else {
			SetIconVisible(lightOn);
			UpdateIconTexture();
		}
	}

	void Reset() {
		g_rootTile = nullptr;
		g_iconTile = nullptr;
		g_initialized = false;
		g_charge = 100.0f;
		g_depleted = false;
		g_wasLightOn = false;
		g_wasDialogue = false;
		g_iconAlpha = 0.0f;
		g_flashTimer = 0.0f;
		g_depletionFlickerTimer = 0.0f;
		g_depletionFlickerLightOn = false;
		g_lastFillState = -1;
		g_wasBelowThreshold = false;
	}

	void OnConfigReload() {
		if (!g_initialized && Config::Battery.Enabled)
			Init();

		Reposition();
		g_lastFillState = -1;

		if (!Config::Battery.Enabled) {
			bool wasFlickering = g_depletionFlickerTimer > 0.0f;
			g_depletionFlickerTimer = 0.0f;
			g_depletionFlickerLightOn = false;
			if (wasFlickering)
				SetPipBoyLightState(true);
			SetIconVisible(false);
			g_flashTimer = 0.0f;
		}
		else {
			UpdateIconTexture();
		}
	}
}
