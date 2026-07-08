#include "Game.h"

TES* (__thiscall* NewTES)(TES*, char*, NiNode*, NiNode*, Sky*, NiNode*) = (TES* (__thiscall*)(TES*, char*, NiNode*, NiNode*, Sky*, NiNode*))Hooks::NewTES;
TES* __fastcall NewTESHook(TES* This, UInt32 edx, char* RootData, NiNode* ObjectLODRoot, NiNode* LandLOD, Sky* Sky, NiNode* WaterLOD) {
	Tes = (*NewTES)(This, RootData, ObjectLODRoot, LandLOD, Sky, WaterLOD);
	return Tes;
}

MenuInterfaceManager* (__thiscall* NewMenuInterfaceManager)(MenuInterfaceManager*) = (MenuInterfaceManager* (__thiscall*)(MenuInterfaceManager*))Hooks::NewMenuInterfaceManager;
MenuInterfaceManager* __fastcall NewMenuInterfaceManagerHook(MenuInterfaceManager* This, UInt32 edx) {
	InterfaceManager = (*NewMenuInterfaceManager)(This);
	return InterfaceManager;
}

NiNode* TESObjectCELL::GetChildNode(CellNodes aeNode) {
	NiNode* pMaster = GetNode();
	return pMaster ? static_cast<NiNode*>(pMaster->m_children.data[aeNode]) : nullptr;
}
