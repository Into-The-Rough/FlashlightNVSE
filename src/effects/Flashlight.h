#pragma once

class FlashlightEffect : public EffectRecord
{
public:
	FlashlightEffect() : EffectRecord("Flashlight") {
		spotLightActive = false;
		SpotLight = nullptr;
		EngineLight = nullptr;
		b42ProjectionActive = false;
	};

	struct FlashlightSettingsStruct {
		NiColor		Color;
		float		Dimmer;
		float		Intensity;
		float		SpotSize;
		float		NearFade;
		float		HotspotLimit;
		float		CookieStrength;
		bool		softEdges;
		float		ConeAngle;
		float		Distance;
		bool		renderShadows;
		NiPoint3	Offset;
		bool		attachToWeapon;
		bool		engineLightEnabled;
		float		engineLightDimmer;
		float		engineLightSpecular;
		float		engineLightAtten0;
		float		engineLightAtten1;
		float		engineLightAtten2;
	};
	FlashlightSettingsStruct	Settings;

	struct FlashlightStruct {
		D3DXMATRIX	FlashlightViewProj;
		D3DXVECTOR4	Position;
		D3DXVECTOR4	Direction;
		D3DXVECTOR4	Color;
		D3DXVECTOR4	Tuning;		//x = spot size scale, y = intensity
		D3DXVECTOR4	Hotspot;	//x = hotspot reduction, y = cookie strength
	};
	FlashlightStruct	Constants;

	NiSpotLight* SpotLight;
	NiPointLight* EngineLight;
	bool	spotLightActive;
	bool	b42ProjectionActive;
	int		selectedPass;

	void	UpdateConstants();
	void	RegisterConstants();
	void	UpdateSettings();
	bool	ShouldRender();

	void	GetFlashlightViewProj();
	void	UpdateEngineLight(const NiPoint3& Position, const NiMatrix33& Rotation);
	void	QueueEngineLight();
	void	PrepareB42OpticsRender();
	void	DisableEngineLight();
	void	DestroyEngineLight();
};
