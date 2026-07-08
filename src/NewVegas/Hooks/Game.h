#pragma once

extern TES* (__thiscall* NewTES)(TES*, char*, NiNode*, NiNode*, Sky*, NiNode*);
TES* __fastcall NewTESHook(TES* This, UInt32 edx, char* RootData, NiNode* ObjectLODRoot, NiNode* LandLOD, Sky* Sky, NiNode* WaterLOD);

extern MenuInterfaceManager* (__thiscall* NewMenuInterfaceManager)(MenuInterfaceManager*);
MenuInterfaceManager* __fastcall NewMenuInterfaceManagerHook(MenuInterfaceManager* This, UInt32 edx);
