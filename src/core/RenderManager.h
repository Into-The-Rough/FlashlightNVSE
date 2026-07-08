#pragma once

//plugin state lives in our own allocation, the engine NiDX9Renderer is shared
//with other plugins (NVR widens it and overlays its own fields past 0xB80)
class RenderManager {
public:
	void				Initialize(NiDX9Renderer* apRenderer);
	void				ResolveDepthBuffer(IDirect3DTexture9* Buffer);
	void				CreateD3DMatrix(D3DMATRIX* Matrix, NiTransform* Transform);
	void				GetScreenSpaceBoundSize(NiPoint2* BoundSize, NiBound* Bound, float ZeroTolerance = 1e-5f);
	void				UpdateSceneCameraData();
	void				SetupSceneCamera();
	float				GetObjectDistance(NiBound* Bound);
	bool				IsReversedDepth();
	NiDX9Renderer*		renderer;
	D3DXMATRIX			WorldViewProjMatrix;
	D3DXMATRIX			ViewProjMatrix;
	D3DXMATRIX			InvViewProjMatrix;
	D3DXMATRIX			InvProjMatrix;
	D3DXMATRIX			InvViewMatrix;
	D3DXMATRIX			ViewMatrix;
	D3DXVECTOR4			CameraData;
	D3DXVECTOR4			FOVData;
	D3DXVECTOR4			CameraForward;
	D3DXVECTOR4			CameraPosition;
	D3DXVECTOR4			DepthConstants;
	bool				RESZ;
	bool				DXVK;
};
