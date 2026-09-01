#include "Flashlight.h"
#include "../core/Config.h"

constexpr UInt32 kVtbl_LightEffect = 0x107A2CC;

static void SetVanillaPipboyLightCulled(bool abCulled);
static constexpr UInt32 kBSUtilities_GetObjectByName = 0x4AAE30;




void FlashlightEffect::RegisterConstants() {
	TheShaderManager->RegisterConstant("TESR_FlashLightViewProjTransform", (D3DXVECTOR4*)&Constants.FlashlightViewProj);
	TheShaderManager->RegisterConstant("TESR_FlashLightPosition",  (D3DXVECTOR4*)&Constants.Position);
	TheShaderManager->RegisterConstant("TESR_FlashLightDirection", (D3DXVECTOR4*)&Constants.Direction);
	TheShaderManager->RegisterConstant("TESR_FlashLightColor",     (D3DXVECTOR4*)&Constants.Color);
	TheShaderManager->RegisterConstant("TESR_FlashLightTuning",    (D3DXVECTOR4*)&Constants.Tuning);
	TheShaderManager->RegisterConstant("TESR_FlashLightHotspot",   (D3DXVECTOR4*)&Constants.Hotspot);
};

void FlashlightEffect::UpdateSettings() {
	Settings.attachToWeapon = Config::Flashlight.AttachToWeapon;
	Settings.renderShadows  = Config::Flashlight.RenderShadows;
	selectedPass = Settings.renderShadows; //0 = NoShadow technique, 1 = Shadows

	Settings.Offset    = NiPoint3(Config::Flashlight.OffsetX, Config::Flashlight.OffsetY, Config::Flashlight.OffsetZ);
	Settings.Color     = NiColor(Config::Flashlight.ColorR, Config::Flashlight.ColorG, Config::Flashlight.ColorB);
	Settings.SpotSize  = Config::Flashlight.SpotSize;
	Settings.NearFade  = Config::Flashlight.NearFade;
	Settings.HotspotLimit = Config::Flashlight.HotspotLimit;
	Settings.CookieStrength = Config::Flashlight.CookieStrength;
	Settings.softEdges = Config::Flashlight.SoftEdges;
	Settings.ConeAngle = Config::Flashlight.Angle;
	Settings.Distance  = Config::Flashlight.Distance;
	Settings.engineLightEnabled = Config::Flashlight.EngineLightEnabled;
	Settings.engineLightDimmer  = Config::Flashlight.EngineLightDimmer;
	Settings.engineLightSpecular = Config::Flashlight.EngineLightSpecular;
	Settings.engineLightAtten0  = Config::Flashlight.EngineLightAtten0;
	Settings.engineLightAtten1  = Config::Flashlight.EngineLightAtten1;
	Settings.engineLightAtten2  = Config::Flashlight.EngineLightAtten2;

	if (!SpotLight)
		SpotLight = NiSpotLight::CreateObject();
	if (!Config::Flashlight.Enabled || !Settings.engineLightEnabled)
		DestroyEngineLight();
	if (!Config::Flashlight.Enabled)
		SetVanillaPipboyLightCulled(false);
};

//currentMode 1 is gameplay, anything higher means a menu owns the screen - terminals,
//lockpicking, dialogue and the pip-boy all render their own view over the world
static bool MenuOwnsScreen() {
	return InterfaceManager && InterfaceManager->currentMode > 1;
}

static MagicItem* GetPipBoyLightItem() {
	return (MagicItem*)(ThisStdCall<uint8_t*>(0x93CCD0, NULL) + 0x18);
}

//the pip-boy light belongs to a LightEffect on the player's magic target, which owns the
//NiPointLight itself. matching on RTTI instead culls every point light parented to the
//player, including ones other mods attach - that blanked Realtime Pip-Boy Map's screen.
static void SetVanillaPipboyLightCulled(bool abCulled) {
	if (!Player) return;
	MagicItem* pipboyLight = GetPipBoyLightItem();
	if (!pipboyLight) return;

	struct EffectNode { void* effect; EffectNode* next; };
	for (EffectNode* node = (EffectNode*)Player->magicTarget.GetEffectList(); node; node = node->next) {
		uint8_t* effect = (uint8_t*)node->effect;
		if (!effect || *(void**)effect != (void*)kVtbl_LightEffect) continue;
		if (*(MagicItem**)(effect + 0x08) != pipboyLight) continue; //0x08 magic item
		NiAVObject* light = *(NiAVObject**)(effect + 0x48); //0x48 the light the effect owns
		if (!light) continue;
		if (abCulled)
			light->m_flags |= NiAVObject::APP_CULLED;
		else
			light->m_flags &= ~NiAVObject::APP_CULLED;
	}
}

void FlashlightEffect::UpdateConstants() {

	if (!SpotLight) return;

	// based on JIP https://github.com/jazzisparis/JIP-LN-NVSE/blob/5a30ac4356ea0e93b9ff357b5031b1e420240a4d/functions_jip/jip_fn_ui.h#L1469
	UInt32 lightIsOn = ThisStdCall<bool>(0x822B90, &Player->magicTarget, GetPipBoyLightItem(), 1);

	spotLightActive = Enabled && lightIsOn;
	if (!spotLightActive) {
		// disable light by setting it to 0 dimmer & radius
		SpotLight->fadeValue = 0.0f;
		SpotLight->radius = 0.0f;
		SpotLight->resetTraits = false;
		DisableEngineLight();
		SetVanillaPipboyLightCulled(false);
		return;
	}

	if (MenuOwnsScreen()) {
		spotLightActive = false;
		DisableEngineLight();
		//the cone is skipped while the menu is up, the vanilla light has to take over
		SetVanillaPipboyLightCulled(false);
		return;
	}

	NiPoint3 WeaponPos;
	NiMatrix33 WeaponRot;
	bool melee = false;
	if (Player->process->IsWeaponOut()) {
		melee = !Player->ActorSkinInfo->WeaponForm ||
			Player->ActorSkinInfo->WeaponForm->weaponType == TESObjectWEAP::WeaponType::kWeapType_HandToHandMelee ||
			Player->ActorSkinInfo->WeaponForm->weaponType == TESObjectWEAP::WeaponType::kWeapType_OneHandMelee ||
			Player->ActorSkinInfo->WeaponForm->weaponType == TESObjectWEAP::WeaponType::kWeapType_TwoHandMelee ||
			Player->ActorSkinInfo->WeaponForm->weaponType == TESObjectWEAP::WeaponType::kWeapType_OneHandGrenade ||
			Player->ActorSkinInfo->WeaponForm->weaponType == TESObjectWEAP::WeaponType::kWeapType_OneHandMine ||
			Player->ActorSkinInfo->WeaponForm->weaponType == TESObjectWEAP::WeaponType::kWeapType_OneHandLunchboxMine ||
			Player->ActorSkinInfo->WeaponForm->weaponType == TESObjectWEAP::WeaponType::kWeapType_OneHandThrown;
	}

	if (Player->isThirdPerson) {
		if (Settings.attachToWeapon && !melee && Player->process->IsWeaponOut() && Player->ActorSkinInfo && Player->ActorSkinInfo->WeaponNode) {
			WeaponPos = Player->ActorSkinInfo->WeaponNode->m_worldTransform.pos;
			WeaponRot = Player->ActorSkinInfo->WeaponNode->m_worldTransform.rot;
		}
		else {
			// matrix that will rotate 90 degrees on the Z then X axis
			NiMatrix33 rotation = NiMatrix33();
			rotation.data[0][0] = 0;
			rotation.data[0][1] = -1;
			rotation.data[0][2] = 0;
			rotation.data[1][0] = 0;
			rotation.data[1][1] = 0;
			rotation.data[1][2] = 1;
			rotation.data[2][0] = -1;
			rotation.data[2][1] = 0;
			rotation.data[2][2] = 0;

			WeaponPos = Player->ActorSkinInfo->HeadNode->m_worldTransform.pos;
			WeaponRot = Player->ActorSkinInfo->HeadNode->m_worldTransform.rot;
			rotation = rotation * WeaponRot; // we place the rotation matrix in the referential of the bone
			WeaponRot = WeaponRot * rotation; // we apply it
		}
	}
	else {
		if (Settings.attachToWeapon && !melee && Player->process->IsWeaponOut() && Player->firstPersonSkinInfo && Player->firstPersonSkinInfo->WeaponNode) {
			WeaponPos = Player->firstPersonSkinInfo->WeaponNode->m_worldTransform.pos;
			WeaponRot = Player->firstPersonSkinInfo->WeaponNode->m_worldTransform.rot;
		}
		else {
			WeaponPos = WorldSceneGraph->camera->m_worldTransform.pos;
			WeaponRot = WorldSceneGraph->camera->m_worldTransform.rot;
		}
	}

	// rotate offset in the direction of the cone
	NiPoint3 offset = WeaponRot * Settings.Offset;
	WeaponPos.x += offset.x;
	WeaponPos.y += offset.y;
	WeaponPos.z += offset.z;

	SetVanillaPipboyLightCulled(true);
	const auto& brightness = Player->parentCell && Player->parentCell->IsInterior() ? Config::Interior : Config::Exterior;

	SpotLight->resetTraits = false;
	SpotLight->diffuseColor = Settings.Color;
	SpotLight->fadeValue = brightness.Dimmer * 10.0f;
	SpotLight->m_worldTransform.pos = WeaponPos;
	SpotLight->m_worldTransform.rot = WeaponRot;
	SpotLight->m_worldTransform.scale = 1.0f;
	SpotLight->outerSpotAngle = Settings.ConeAngle;
	SpotLight->radius = Settings.Distance;

	Constants.Tuning = D3DXVECTOR4(Settings.SpotSize, brightness.Intensity, Settings.NearFade, Settings.softEdges ? 1.0f : 0.0f);
	Constants.Hotspot = D3DXVECTOR4(Settings.HotspotLimit, Settings.CookieStrength, 0.0f, 0.0f);

	UpdateEngineLight(WeaponPos, WeaponRot, brightness.Dimmer);
	GetFlashlightViewProj();
};

bool FlashlightEffect::ShouldRender() { return SpotLight && spotLightActive; };


void FlashlightEffect::GetFlashlightViewProj() {
	if (!SpotLight) return;

	D3DXVECTOR3 Up = D3DXVECTOR3(0, 0, 1);
	D3DXVECTOR3 Eye = SpotLight->m_worldTransform.pos.toD3DXVEC3();
	D3DXVECTOR3 Direction = D3DXVECTOR3(SpotLight->m_worldTransform.rot.data[0][0], SpotLight->m_worldTransform.rot.data[1][0], SpotLight->m_worldTransform.rot.data[2][0]);
	D3DXVECTOR3 At = Eye + Direction;

	float Radius = SpotLight->radius;
	D3DXMATRIX View, Proj;
	D3DXMatrixPerspectiveFovRH(&Proj, D3DXToRadian(SpotLight->outerSpotAngle * 2), 1.0f, 0.1f, Radius);
	D3DXMatrixLookAtRH(&View, &Eye, &At, &Up);

	Constants.FlashlightViewProj = View * Proj;
}

void FlashlightEffect::UpdateEngineLight(const NiPoint3& Position, const NiMatrix33& Rotation, float afDimmer) {
	if (!Settings.engineLightEnabled || !WorldSceneGraph) {
		DestroyEngineLight();
		return;
	}

	if (!EngineLight) {
		EngineLight = NiPointLight::CreateObject();
		if (EngineLight) {
			EngineLight->m_uiRefCount = 2;
			EngineLight->SetName("BetterFlashlightNVSEEngineLight");
		}
	}
	if (!EngineLight)
		return;

	EngineLight->m_flags &= ~EngineLight->APP_CULLED;
	EngineLight->m_flags |= EngineLight->DISPLAY_OBJECT | EngineLight->IS_POINTLIGHT | EngineLight->DONE_INIT_LIGHTS;
	EngineLight->switchState = true;
	EngineLight->effectType = NiDynamicEffect::POINT_LIGHT;
	EngineLight->resetTraits = false;
	EngineLight->extraFlags = 0;

	EngineLight->m_localTransform.pos = Position;
	EngineLight->m_localTransform.rot = Rotation;
	EngineLight->m_localTransform.scale = 1.0f;
	EngineLight->m_worldTransform = EngineLight->m_localTransform;

	EngineLight->ambientColor = NiColor(0, 0, 0);
	EngineLight->diffuseColor = NiColor(
		Settings.Color.r * Settings.engineLightSpecular,
		Settings.Color.g * Settings.engineLightSpecular,
		Settings.Color.b * Settings.engineLightSpecular
	);
	EngineLight->fadeValue = afDimmer * Settings.engineLightDimmer;
	EngineLight->radius = Settings.Distance;
	EngineLight->radius0E4 = 1.0f;
	EngineLight->baseLight = nullptr;
	EngineLight->obj0EC = nullptr;
	EngineLight->constAtten = Settings.engineLightAtten0;
	EngineLight->linearAtten = Settings.engineLightAtten1;
	EngineLight->quadAtten = Settings.engineLightAtten2;
}

void FlashlightEffect::QueueEngineLight() {
	if (!EngineLight || !Settings.engineLightEnabled || !spotLightActive)
		return;

	ShadowSceneNode* sceneNode = BSShaderManager::GetShadowSceneNode(0);
	if (!sceneNode)
		return;

	if (EngineLight->m_parent)
		EngineLight->m_parent->RemoveObject(EngineLight);

	//queueing a light already in the list leaks the new ShadowSceneLight, refresh in place
	for (auto entry = sceneNode->lights.start; entry; entry = entry->next) {
		if (entry->data && entry->data->sourceLight == EngineLight) {
			sceneNode->UpdateQueuedLight(entry->data);
			return;
		}
	}
	if (ShadowSceneLight* lightData = ShadowSceneLight::CreatePointLight(EngineLight, sceneNode->portalGraph))
		sceneNode->QueueLightData(lightData);
}

void FlashlightEffect::DisableEngineLight() {
	if (!EngineLight) return;

	EngineLight->switchState = false;
	EngineLight->fadeValue = 0.0f;
	EngineLight->resetTraits = false;
	EngineLight->m_flags |= EngineLight->APP_CULLED;
	if (EngineLight->m_parent)
		EngineLight->m_parent->RemoveObject(EngineLight);
}

void FlashlightEffect::DestroyEngineLight() {
	if (!EngineLight) return;

	DisableEngineLight();
	// The renderer may still hold same-frame ShadowSceneLight references to this
	// object. Keep the plugin-owned light allocated for the process lifetime.
}
