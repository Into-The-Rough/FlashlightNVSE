#define RESZ_CODE 0x7FA05000

void RenderManager::CreateD3DMatrix(D3DMATRIX* Matrix, NiTransform* Transform) {

	NiMatrix33* Rot = &Transform->rot;
	NiPoint3* Pos = &Transform->pos;
	float Scale = Transform->scale;

	Matrix->_11 = Rot->data[0][0] * Scale;
	Matrix->_12 = Rot->data[1][0] * Scale;
	Matrix->_13 = Rot->data[2][0] * Scale;
	Matrix->_14 = 0.0f;
	Matrix->_21 = Rot->data[0][1] * Scale;
	Matrix->_22 = Rot->data[1][1] * Scale;
	Matrix->_23 = Rot->data[2][1] * Scale;
	Matrix->_24 = 0.0f;
	Matrix->_31 = Rot->data[0][2] * Scale;
	Matrix->_32 = Rot->data[1][2] * Scale;
	Matrix->_33 = Rot->data[2][2] * Scale;
	Matrix->_34 = 0.0f;
	Matrix->_41 = Pos->x - CameraPosition.x;
	Matrix->_42 = Pos->y - CameraPosition.y;
	Matrix->_43 = Pos->z - CameraPosition.z;
	Matrix->_44 = 1.0f;

}

void RenderManager::GetScreenSpaceBoundSize(NiPoint2* BoundSize, NiBound* Bound, float ZeroTolerance) {
		
	NiCamera* Camera = WorldSceneGraph->camera;
	NiMatrix33* WorldRotate = &Camera->m_worldTransform.rot;
	NiPoint3 BoundPos = { Bound->Center.x - Camera->m_worldTransform.pos.x, Bound->Center.y - Camera->m_worldTransform.pos.y, Bound->Center.z - Camera->m_worldTransform.pos.z};
	float BoundViewDist = BoundPos * BoundPos;
	float Ratio = Bound->Radius;

	if (BoundViewDist < ZeroTolerance) {
		if (BoundViewDist > -ZeroTolerance) {
			BoundSize->x = FLT_MAX;
			BoundSize->y = FLT_MAX;
			return;
		}
		BoundViewDist = -BoundViewDist;
	}

	if (!Camera->Frustum.Ortho) Ratio /= BoundViewDist;
	BoundSize->x = Ratio * 2.0f / (Camera->Frustum.Right - Camera->Frustum.Left);
	BoundSize->y = Ratio * 2.0f / (Camera->Frustum.Top - Camera->Frustum.Bottom);

}

float RenderManager::GetObjectDistance(NiBound* Bound){
	NiCamera* Camera = WorldSceneGraph->camera;
	NiMatrix33* WorldRotate = &Camera->m_worldTransform.rot;
	NiPoint3 BoundPos = { Bound->Center.x - Camera->m_worldTransform.pos.x, Bound->Center.y - Camera->m_worldTransform.pos.y, Bound->Center.z - Camera->m_worldTransform.pos.z };
    return sqrt(BoundPos * BoundPos);
}

bool RenderManager::IsReversedDepth() {
	return NiDX9Renderer::GetSingleton()->m_fZClear < 1.0f;
}

//the scene renders into an offscreen target, so identity against the back buffer is no
//use. anything projecting the world into a texture - B42 Optics scopes, the realtime
//pip-boy map, location discovery - uses a small square instead of a frame-sized one.
bool RenderManager::RenderingToSceneTarget() {
	if (!renderer)
		return false;
	NiRenderTargetGroup* current = renderer->GetCurrentRT();
	if (!current)
		return true;
	return current->GetWidth(0) == renderer->width && current->GetHeight(0) == renderer->height;
}

void RenderManager::UpdateSceneCameraData() {

	NiCamera* Camera = WorldSceneGraph->camera;

	if (Camera) {
		NiMatrix33* WorldRotate = &Camera->m_worldTransform.rot;
		NiPoint3* WorldTranslate = &Camera->m_worldTransform.pos;
		
		CameraForward.x = WorldRotate->data[0][0];
		CameraForward.y = WorldRotate->data[1][0];
		CameraForward.z = WorldRotate->data[2][0];
		CameraPosition = WorldTranslate->toD3DXVEC4();
	}

}

void RenderManager::SetupSceneCamera() {

	NiCamera* Camera = WorldSceneGraph->camera;

	if (Camera) {
		//the engine matrix slots in NiDX9Renderer, refreshed by the engine per draw batch
		D3DXMATRIXA16& worldMatrix = renderer->worldMatrix;
		D3DXMATRIX& viewMatrix = renderer->viewMatrix;
		D3DXMATRIX& invViewMatrix = renderer->invViewMatrix;
		D3DXMATRIX& projMatrix = renderer->projMatrix;
		NiPoint3 Loc = *Pointers::Generic::CameraLocation;
		NiPoint3 Forward = { 0.0f, 0.0f, 0.0f };
		NiPoint3 Up = { 0.0f, 0.0f, 0.0f };
		NiPoint3 Right = { 0.0f, 0.0f, 0.0f };
		NiMatrix33* WorldRotate = &Camera->m_worldTransform.rot;
		NiPoint3* WorldTranslate = &Camera->m_worldTransform.pos;
		NiFrustum* Frustum = &Camera->Frustum;
		float FrustumWidth = Frustum->Right - Frustum->Left;
		float FrustumHeight = Frustum->Top - Frustum->Bottom;
		
		// projection transform built according to this:
		// https://learn.microsoft.com/en-us/windows/win32/direct3d9/projection-transform
		float aspectRatio = FrustumWidth / FrustumHeight;
		float nearZ = Frustum->Near;
		float farZ = Frustum->Far;
		float fInvFmN = 1.0f / (farZ - nearZ);

		float RpL = Frustum->Right + Frustum->Left; // always 0 for a viewport centered on 0 ?
		float TpB = Frustum->Top + Frustum->Bottom; // always 0 for a viewport centered on 0 ?

		// FOVData will contain x/y fov in degrees (x and y) and in radians (z and w);
		FOVData.y = (WorldSceneGraph->cameraFOV); // cameraFOV seems to be the vertical FOV?
		FOVData.w = D3DXToRadian(WorldSceneGraph->cameraFOV);
		FOVData.z = 2 * atan(tan(FOVData.w * 0.5) * aspectRatio);
		FOVData.x = D3DXToDegree(FOVData.z);

		memcpy(Pointers::Generic::CameraWorldTranslate, WorldTranslate, 0x0C);

		Forward.x = WorldRotate->data[0][0];
		Forward.y = WorldRotate->data[1][0];
		Forward.z = WorldRotate->data[2][0];
		Up.x = WorldRotate->data[0][1];
		Up.y = WorldRotate->data[1][1];
		Up.z = WorldRotate->data[2][1];
		Right.x = WorldRotate->data[0][2];
		Right.y = WorldRotate->data[1][2];
		Right.z = WorldRotate->data[2][2];

		// world translation
		worldMatrix._11 = 1.0f;
		worldMatrix._12 = 0.0f;
		worldMatrix._13 = 0.0f;
		worldMatrix._14 = 0.0f;
		worldMatrix._21 = 0.0f;
		worldMatrix._22 = 1.0f;
		worldMatrix._23 = 0.0f;
		worldMatrix._24 = 0.0f;
		worldMatrix._31 = 0.0f;
		worldMatrix._32 = 0.0f;
		worldMatrix._33 = 1.0f;
		worldMatrix._34 = 0.0f;
		worldMatrix._41 = -WorldTranslate->x;
		worldMatrix._42 = -WorldTranslate->y;
		worldMatrix._43 = -WorldTranslate->z;
		worldMatrix._44 = 1.0f;

		viewMatrix._11 = Right.x;
		viewMatrix._12 = Up.x;
		viewMatrix._13 = Forward.x;
		viewMatrix._14 = 0.0f;
		viewMatrix._21 = Right.y;
		viewMatrix._22 = Up.y;
		viewMatrix._23 = Forward.y;
		viewMatrix._24 = 0.0f;
		viewMatrix._31 = Right.z;
		viewMatrix._32 = Up.z;
		viewMatrix._33 = Forward.z;
		viewMatrix._34 = 0.0f;
		viewMatrix._41 = -(Right * Loc);
		viewMatrix._42 = -(Up * Loc);
		viewMatrix._43 = -(Forward * Loc);
		viewMatrix._44 = 1.0f;

		invViewMatrix._11 = Right.x;
		invViewMatrix._12 = Right.y;
		invViewMatrix._13 = Right.z;
		invViewMatrix._14 = 0.0f;
		invViewMatrix._21 = Up.x;
		invViewMatrix._22 = Up.y;
		invViewMatrix._23 = Up.z;
		invViewMatrix._24 = 0.0f;
		invViewMatrix._31 = Forward.x;
		invViewMatrix._32 = Forward.y;
		invViewMatrix._33 = Forward.z;
		invViewMatrix._34 = 0.0f;
		invViewMatrix._41 = Loc.x;
		invViewMatrix._42 = Loc.y;
		invViewMatrix._43 = Loc.z;
		invViewMatrix._44 = 1.0f;

		InvViewMatrix = invViewMatrix;
		ViewMatrix = viewMatrix;

		projMatrix._11 = 2.0f / FrustumWidth;
		projMatrix._12 = 0.0f;
		projMatrix._13 = 0.0f;
		projMatrix._14 = 0.0f;
		projMatrix._21 = 0.0f;
		projMatrix._22 = 2.0f / FrustumHeight;
		projMatrix._23 = 0.0f;
		projMatrix._24 = 0.0f;
		projMatrix._31 = -RpL / FrustumWidth;
		projMatrix._32 = -TpB / FrustumHeight;
		projMatrix._34 = 1.0f;
		projMatrix._41 = 0.0f;
		projMatrix._42 = 0.0f;
		projMatrix._44 = 0.0f;
		if (IsReversedDepth()) {
			projMatrix._33 = -(nearZ * fInvFmN);
			projMatrix._43 = nearZ * farZ * fInvFmN;
		}
		else {
			projMatrix._33 = farZ * fInvFmN;
			projMatrix._43 = -(nearZ * farZ * fInvFmN);
		}

		WorldViewProjMatrix = worldMatrix * viewMatrix * projMatrix;
		ViewProjMatrix = viewMatrix * projMatrix;
		ProjMatrix = projMatrix;
		D3DXMatrixInverse(&InvProjMatrix, NULL, &projMatrix);
		InvViewProjMatrix = InvProjMatrix * invViewMatrix;

		CameraForward = Forward.toD3DXVEC4();
		CameraPosition = WorldTranslate->toD3DXVEC4();

		// depth reconstruction constants
		//DepthConstants.x = - nearZ; //NearZ: TESR_ProjectionTransform._43 / TESR_ProjectionTransform._33
		DepthConstants.x = TheShaderManager->Effects.CombineDepth->Constants.viewNearZ; //NearZ: TESR_ProjectionTransform._43 / TESR_ProjectionTransform._33
		DepthConstants.y = 0.0f; // FarZ: (TESR_ProjectionTransform._33 * nearZ) / (TESR_ProjectionTransform._33 - 1.0f);
		DepthConstants.z = IsReversedDepth() ? 1.0f : 0.0f; // detect inverted depth buffer
		//CombineDepth viewmodel-buffer enable: 0 in 3rd person, no viewmodel is rendered
		//and the buffer holds stale data, so the merge uses pure world depth
		DepthConstants.w = (Player && Player->isThirdPerson) ? 0.0f : 1.0f;

		CameraData.x = nearZ;
		CameraData.y = farZ;
		CameraData.z = FrustumWidth / FrustumHeight;
		CameraData.w = FOVData.x;
	}

}

// detect if dxvk is present
bool getDXVKPresent() {
	DWORD  verHandle = 0;
	LPBYTE lpBuffer = NULL;

	DWORD  verSize = GetFileVersionInfoSizeA("d3d9.dll", &verHandle);
	if (verSize == NULL) return false;

	LPSTR verData = new char[verSize];
	if (!GetFileVersionInfoA("d3d9.dll", verHandle, verSize, verData)) {
		delete[] verData;
		return false;
	}

	struct LANGANDCODEPAGE {
		WORD wLanguage;
		WORD wCodePage;
	} *lpTranslate;

	UINT size = 0;
	if (!VerQueryValueA(verData, "\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &size)) {
		delete[] verData;
		return false;
	}

	if (!size) {
		delete[] verData;
		return false;
	}

	for (UINT i = 0; i < (size / sizeof(struct LANGANDCODEPAGE)); i++)
	{
		char subblock[256];
		StringCchPrintfA(subblock, 256, "\\StringFileInfo\\%04x%04x\\ProductName", lpTranslate[i].wLanguage, lpTranslate[i].wCodePage);

		char* description = NULL;
		UINT dwBytes = 0;
		if (VerQueryValueA(verData, subblock, (LPVOID*)&description, &dwBytes) &&
			description && dwBytes >= 4 && !memcmp(description, "DXVK", 4)) {
			delete[] verData;
			return true;
		}
	}

	delete[] verData;
	return false;
}

void RenderManager::Initialize(NiDX9Renderer* apRenderer) {

	IDirect3D9* D3D = NULL;
	D3DDISPLAYMODE currentDisplayMode;

	renderer = apRenderer;
	CameraForward = { 0.0f, 0.0f, 0.0f, 0.0f };
	CameraPosition = { 0.0f, 0.0f, 0.0f, 0.0f };
	renderer->device->GetDirect3D(&D3D);
	D3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &currentDisplayMode);
	RESZ = D3D->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, currentDisplayMode.Format, D3DUSAGE_RENDERTARGET, D3DRTYPE_SURFACE, (D3DFORMAT)MAKEFOURCC('R','E','S','Z')) == D3D_OK;
	D3D->Release();
	DXVK = false;

	//RESZ = AMD/Intel/DXVK depth resolve, otherwise fall back to NVAPI StretchRectEx
	if (RESZ)
		DXVK = getDXVKPresent();
	else
		NvAPI_Initialize();
}

void RenderManager::ResolveDepthBuffer(IDirect3DTexture9* Buffer) {

	IDirect3DDevice9* device = renderer->device;
	if (RESZ) {
		IDirect3DBaseTexture9 *pCurrTX = NULL;
		IDirect3DVertexShader9 *pCurrVS = NULL;
		IDirect3DPixelShader9 *pCurrPS = NULL;
		IDirect3DVertexBuffer9 *pCurrVX = NULL;
		UINT32 dCurrVO;
		UINT32 dCurrVS;
		DWORD dCurrZE;
		DWORD dCurrZW;
		DWORD dCurrCW;
		DWORD dCurrFVF;
		IDirect3DVertexDeclaration9* pCurrDecl = nullptr;

		D3DXVECTOR3 vDummyPoint(0.0f, 0.0f, 0.0f);
		device->GetFVF(&dCurrFVF);
		device->GetVertexDeclaration(&pCurrDecl);
		device->GetTexture(0, &pCurrTX);
		device->GetVertexShader(&pCurrVS);
		device->GetPixelShader(&pCurrPS);
		device->GetStreamSource(0, &pCurrVX, &dCurrVO, &dCurrVS);

		device->GetRenderState(D3DRS_ZENABLE, &dCurrZE);
		device->GetRenderState(D3DRS_ZWRITEENABLE, &dCurrZW);
		device->GetRenderState(D3DRS_COLORWRITEENABLE, &dCurrCW);
		device->SetVertexShader(NULL);
		device->SetPixelShader(NULL);
		device->SetFVF(D3DFVF_XYZ);
		device->SetTexture(0, Buffer);
		device->SetRenderState(D3DRS_ZENABLE, FALSE);
		device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, NULL);
		device->DrawPrimitiveUP(D3DPT_POINTLIST, 1, vDummyPoint, sizeof(D3DXVECTOR3));
		device->SetRenderState(D3DRS_ZENABLE, dCurrZE);
		device->SetRenderState(D3DRS_ZWRITEENABLE, dCurrZW);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, dCurrCW);
		device->SetRenderState(D3DRS_POINTSIZE, RESZ_CODE);
		device->SetRenderState(D3DRS_POINTSIZE, NULL);

		device->SetFVF(dCurrFVF);
		device->SetVertexDeclaration(pCurrDecl);
		device->SetTexture(0, pCurrTX);
		device->SetVertexShader(pCurrVS);
		device->SetPixelShader(pCurrPS);
			device->SetStreamSource(0, pCurrVX, dCurrVO, dCurrVS);
			
			if (pCurrDecl) pCurrDecl->Release();
			if (pCurrTX) pCurrTX->Release();
			if (pCurrVS) pCurrVS->Release();
			if (pCurrPS) pCurrPS->Release();
			if (pCurrVX) pCurrVX->Release();
		}
	else {
		if (!TheTextureManager->DepthSurface) {
			device->GetDepthStencilSurface(&TheTextureManager->DepthSurface);
            D3DSURFACE_DESC desc;
            TheTextureManager->DepthSurface->GetDesc(&desc);
			if (desc.Format == (D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z')) {
				void* Container = NULL;
				TheTextureManager->DepthSurface->GetContainer(IID_IDirect3DTexture9, &Container);
				TheTextureManager->DepthTextureINTZ = (IDirect3DTexture9*)Container;
				NvAPI_D3D9_RegisterResource(TheTextureManager->DepthTextureINTZ);
			}
			else {
				NvAPI_D3D9_RegisterResource(TheTextureManager->DepthSurface);
			}
		}

		if (TheTextureManager->DepthTextureINTZ)
			NvAPI_D3D9_StretchRectEx(device, TheTextureManager->DepthTextureINTZ, NULL, Buffer, NULL, D3DTEXF_NONE);
        else 
			NvAPI_D3D9_StretchRectEx(device, TheTextureManager->DepthSurface, NULL, Buffer, NULL, D3DTEXF_NONE);
	}
}

