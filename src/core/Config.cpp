#include "Config.h"

namespace Config {
	FlashlightSettings Flashlight{};
	MainSettings Main{};
	BatterySettings Battery{};

	static float GetF(const char* section, const char* key, float fallback) {
		char buf[32];
		GetPrivateProfileStringA(section, key, "", buf, sizeof(buf), IniPath);
		if (buf[0] == 0) {
			char w[32]; sprintf_s(w, "%f", fallback);
			WritePrivateProfileStringA(section, key, w, IniPath);
			return fallback;
		}
		return (float)atof(buf);
	}

	static int GetI(const char* section, const char* key, int fallback) {
		int v = GetPrivateProfileIntA(section, key, INT_MIN, IniPath);
		if (v == INT_MIN) {
			char w[16]; sprintf_s(w, "%d", fallback);
			WritePrivateProfileStringA(section, key, w, IniPath);
			return fallback;
		}
		return v;
	}

	static bool GetB(const char* section, const char* key, bool fallback) {
		return GetI(section, key, fallback ? 1 : 0) != 0;
	}

	void Load() {
		Flashlight.ColorR         = GetF("Flashlight", "ColorR",         2.0f);
		Flashlight.ColorG         = GetF("Flashlight", "ColorG",         1.85f);
		Flashlight.ColorB         = GetF("Flashlight", "ColorB",         1.65f);
		Flashlight.Dimmer         = GetF("Flashlight", "Dimmer",         1.0f);
		Flashlight.Intensity      = GetF("Flashlight", "Intensity",      0.35f);
		Flashlight.SpotSize       = GetF("Flashlight", "SpotSize",       1.5f);
		Flashlight.NearFade       = GetF("Flashlight", "NearFade",       150.0f);
		Flashlight.HotspotLimit   = GetF("Flashlight", "HotspotLimit",   1.0f);
		Flashlight.CookieStrength = GetF("Flashlight", "CookieStrength", 0.25f);
		Flashlight.Flicker        = GetF("Flashlight", "Flicker",        0.25f);
		Flashlight.WarmupTime     = GetF("Flashlight", "WarmupTime",     0.4f);
		Flashlight.Angle          = GetF("Flashlight", "Angle",          18.0f);
		Flashlight.Distance       = GetF("Flashlight", "Distance",       1800.0f);
		Flashlight.OffsetX        = GetF("Flashlight", "OffsetX",        0.0f);
		Flashlight.OffsetY        = GetF("Flashlight", "OffsetY",        0.0f);
		Flashlight.OffsetZ        = GetF("Flashlight", "OffsetZ",        0.0f);
		Flashlight.RenderShadows  = GetB("Flashlight", "RenderShadows",  false);
		Flashlight.AttachToWeapon = GetB("Flashlight", "AttachToWeapon", true);
		Flashlight.Enabled        = GetB("Flashlight", "Enabled",        true);
		Flashlight.SoftEdges      = GetB("Flashlight", "SoftEdges",      true);
		Flashlight.DisableNVRFlashlight = GetB("Flashlight", "DisableNVRFlashlight", true);

		Flashlight.EngineLightEnabled = GetB("EngineLight", "Enabled",  false);
		Flashlight.EngineLightDimmer  = GetF("EngineLight", "Dimmer",   0.12f);
		Flashlight.EngineLightSpecular = GetF("EngineLight", "Specular", 0.5f);
		Flashlight.EngineLightAtten0  = GetF("EngineLight", "Atten0",   1.0f);
		Flashlight.EngineLightAtten1  = GetF("EngineLight", "Atten1",   0.0f);
		Flashlight.EngineLightAtten2  = GetF("EngineLight", "Atten2",   0.000004f);

		Flashlight.MaterialLightEnabled     = GetB("MaterialLight", "Enabled",     true);
		Flashlight.MaterialLightRender      = GetB("MaterialLight", "Render",      true);
		Flashlight.MaterialLightIntensity   = GetF("MaterialLight", "Intensity",   0.3f);
		Flashlight.MaterialLightSpecular    = GetF("MaterialLight", "Specular",    0.5f);
		Flashlight.MaterialLightSpecularPower = GetF("MaterialLight", "SpecularPower", 160.0f);
		Flashlight.MaterialLightSpecularBoost = GetF("MaterialLight", "SpecularBoost", 16.0f);
		Flashlight.MaterialLightNormalStrength = GetF("MaterialLight", "NormalStrength", 2.5f);
		Flashlight.MaterialLightMaxGeometry = GetI("MaterialLight", "MaxGeometry", 2048);
		Flashlight.MaterialLightDebugMode   = GetI("MaterialLight", "DebugMode",   0);

		Flashlight.VolumetricEnabled       = GetB("Volumetric", "Enabled",       true);
		Flashlight.VolumetricFirstPerson   = GetB("Volumetric", "FirstPersonEnabled", true);
		Flashlight.VolumetricThirdPerson   = GetB("Volumetric", "ThirdPersonEnabled", true);
		Flashlight.VolumetricStrength      = GetF("Volumetric", "Strength",      1.0f);
		Flashlight.VolumetricG             = GetF("Volumetric", "Anisotropy",    0.45f);
		Flashlight.VolumetricHeightFalloff = GetF("Volumetric", "HeightFalloff", 1.0f);
		Flashlight.VolumetricMaxDistance   = GetF("Volumetric", "MaxDistance",   1500.0f);
		Flashlight.VolumetricFirstPersonStrength = GetF("Volumetric", "FirstPersonStrength", 0.15f);
		Flashlight.VolumetricFPOffsetRight = GetF("Volumetric", "FirstPersonOffsetRight", 8.0f);
		Flashlight.VolumetricFPOffsetDown  = GetF("Volumetric", "FirstPersonOffsetDown",  4.0f);

		Battery.Enabled           = GetB("Battery", "Enabled",           true);
		Battery.DrainRate         = GetF("Battery", "DrainRate",         2.0f);
		Battery.RechargeRate      = GetF("Battery", "RechargeRate",      2.5f);
		Battery.RechargeThreshold = GetF("Battery", "RechargeThreshold", 10.0f);
		Battery.IconPosX          = GetF("Battery", "IconPosX",          0.97f);
		Battery.IconPosY          = GetF("Battery", "IconPosY",          0.87f);
		Battery.IconScale         = GetF("Battery", "IconScale",         0.6f);
		Battery.PulseThreshold    = GetF("Battery", "PulseThreshold",    25.0f);
		Battery.IconSet           = GetI("Battery", "IconSet",           2);

		Main.RenderEffects = GetB("Main", "RenderEffects", true);
	}

	void Reload() { Load(); }
}
