#pragma once

namespace Config {
	struct FlashlightSettings {
		float ColorR;
		float ColorG;
		float ColorB;
		float Dimmer;
		float Intensity;
		float SpotSize;
		float NearFade;
		float HotspotLimit;
		float CookieStrength;
		float Flicker;
		float WarmupTime;
		float Angle;
		float Distance;
		float OffsetX;
		float OffsetY;
		float OffsetZ;
		bool RenderShadows;
		bool AttachToWeapon;
		bool Enabled;
		bool SoftEdges;
		bool DisableNVRFlashlight;
		bool EngineLightEnabled;
		float EngineLightDimmer;
		float EngineLightSpecular;
		float EngineLightAtten0;
		float EngineLightAtten1;
		float EngineLightAtten2;
		bool MaterialLightEnabled;
		bool MaterialLightRender;
		float MaterialLightIntensity;
		float MaterialLightSpecular;
		float MaterialLightSpecularPower;
		float MaterialLightSpecularBoost;
		float MaterialLightNormalStrength;
		int MaterialLightMaxGeometry;
		int MaterialLightDebugMode;
		bool VolumetricEnabled;
		bool VolumetricFirstPerson;
		bool VolumetricThirdPerson;
		float VolumetricStrength;
		float VolumetricG;
		float VolumetricHeightFalloff;
		float VolumetricMaxDistance;
		float VolumetricFirstPersonStrength;
		float VolumetricFPOffsetRight;
		float VolumetricFPOffsetDown;
	};

	struct MainSettings {
		bool RenderEffects;
	};

	struct BatterySettings {
		bool Enabled;
		float DrainRate;
		float RechargeRate;
		float RechargeThreshold;
		float IconPosX;
		float IconPosY;
		float IconScale;
		float PulseThreshold;
		int IconSet;
	};

	extern FlashlightSettings Flashlight;
	extern MainSettings Main;
	extern BatterySettings Battery;

	void Load();
	void Reload();
}
