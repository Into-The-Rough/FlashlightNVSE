#include "MaterialPass.h"
#include "../../core/Config.h"
#include <d3dx9shader.h>

namespace MaterialPass {
	static constexpr UInt32 kRTTI_NiNode = 0x11F4428;
	static constexpr UInt32 kRTTI_NiGeometry = 0x11F4ACC;
	static constexpr UInt32 kRTTI_NiSwitchNode = 0x11F5EB4;
	static constexpr UInt32 kRenderTriShapeAlt = 0xE745A0;
	static constexpr UInt32 kRenderTriStripsAlt = 0xE74840;
	static constexpr UInt32 kSavedStreamCount = 8;
	static constexpr UInt32 kSavedTextureCount = 2;
	static constexpr UInt32 kSavedVertexConstantCount = 8;
	static constexpr UInt32 kSavedPixelConstantCount = 7;
	static constexpr D3DRENDERSTATETYPE kSavedRenderStates[] = {
		D3DRS_ZENABLE,
		D3DRS_ZWRITEENABLE,
		D3DRS_ZFUNC,
		D3DRS_ALPHABLENDENABLE,
		D3DRS_SRCBLEND,
		D3DRS_DESTBLEND,
		D3DRS_BLENDOP,
		D3DRS_SEPARATEALPHABLENDENABLE,
		D3DRS_ALPHATESTENABLE,
		D3DRS_STENCILENABLE,
		D3DRS_CULLMODE,
		D3DRS_COLORWRITEENABLE,
		D3DRS_FOGENABLE,
		D3DRS_MULTISAMPLEMASK,
		D3DRS_SCISSORTESTENABLE,
		D3DRS_CLIPPLANEENABLE
	};
	static constexpr D3DSAMPLERSTATETYPE kSavedSamplerStates[] = {
		D3DSAMP_ADDRESSU,
		D3DSAMP_ADDRESSV,
		D3DSAMP_MAGFILTER,
		D3DSAMP_MINFILTER,
		D3DSAMP_MIPFILTER
	};
	static constexpr UInt32 kSavedRenderStateCount = sizeof(kSavedRenderStates) / sizeof(kSavedRenderStates[0]);
	static constexpr UInt32 kSavedSamplerStateCount = sizeof(kSavedSamplerStates) / sizeof(kSavedSamplerStates[0]);

	static const char* kVertexShaderSource =
		"row_major float4x4 gWorldViewProj : register(c0);\n"
		"row_major float4x4 gWorld : register(c4);\n"
		"struct VS_IN { float4 pos : POSITION0; float3 normal : NORMAL0; float4 uv : TEXCOORD0; };\n"
		"struct VS_OUT { float4 pos : POSITION; float2 uv : TEXCOORD0; float3 worldRel : TEXCOORD1; float3 normalRel : TEXCOORD2; };\n"
		"VS_OUT main(VS_IN IN) {\n"
		"    VS_OUT OUT;\n"
		"    float4 localPos = float4(IN.pos.xyz, 1.0);\n"
		"    float4 worldPos = mul(localPos, gWorld);\n"
		"    OUT.pos = mul(localPos, gWorldViewProj);\n"
		"    OUT.uv = IN.uv.xy;\n"
		"    OUT.worldRel = worldPos.xyz;\n"
		"    OUT.normalRel = normalize(mul(float4(IN.normal.xyz, 0.0), gWorld).xyz);\n"
		"    return OUT;\n"
		"}\n";

	static const char* kPixelShaderSource =
		"float4 gLightColorIntensity : register(c0);\n"
		"float4 gLightDirAngle : register(c1);\n"
		"float4 gTuning : register(c2);\n"
		"float4 gLightPosRadius : register(c3);\n"
		"float4 gSpecular : register(c4);\n"
		"float4 gSpecularTuning : register(c5);\n"
		"float4 gHotspot : register(c6);\n"
		"sampler2D DiffuseMap : register(s0);\n"
		"struct PS_IN { float2 uv : TEXCOORD0; float3 worldRel : TEXCOORD1; float3 normalRel : TEXCOORD2; };\n"
		"float invlerp(float a, float b, float v) { return saturate((v - a) / max(0.0001, b - a)); }\n"
		"float luma(float3 color) { return dot(color, float3(0.2126, 0.7152, 0.0722)); }\n"
		"float4 main(PS_IN IN) : COLOR0 {\n"
		"    float debugStrength = 4.0 * max(gLightColorIntensity.w, 0.25);\n"
		"    if (gTuning.z > 0.5 && gTuning.z < 1.5) return float4(debugStrength, 0.0, 0.0, 1.0);\n"
		"    float4 base = tex2D(DiffuseMap, IN.uv);\n"
		"    float3 toLight = gLightPosRadius.xyz - IN.worldRel;\n"
		"    float distToLight = length(toLight);\n"
		"    float3 lightVector = toLight / max(distToLight, 0.001);\n"
		"    float radius = max(gLightPosRadius.w, 1.0);\n"
		"    float s = saturate((distToLight / radius) * (distToLight / radius));\n"
		"    float atten = saturate(((1.0 - s) * (1.0 - s)) / (1.0 + 5.0 * s));\n"
		"    float angle = max(1.0, gLightDirAngle.w * max(gTuning.x, 0.05));\n"
		"    float angleCosMax = cos(radians(angle));\n"
		"    float angleCosMin = cos(radians(angle * 0.5));\n"
		"    float cone = pow(invlerp(angleCosMax, angleCosMin, dot(normalize(gLightDirAngle.xyz), -lightVector)), 2.0);\n"
		"    float nearFade = (gTuning.y > 0.0) ? smoothstep(0.0, gTuning.y, distToLight) : 1.0;\n"
		"    float contribution = atten * cone * nearFade;\n"
		"    float3 normal = normalize(IN.normalRel);\n"
		"    float diffuse = saturate(dot(normal, lightVector));\n"
		"    float3 viewVector = normalize(-IN.worldRel);\n"
		"    float3 halfVector = normalize(lightVector + viewVector);\n"
		"    float nDotH = saturate(dot(normal, halfVector));\n"
		"    float gloss = smoothstep(0.45, 0.95, saturate(gSpecular.z));\n"
		"    float specPower = max(gSpecularTuning.x, gSpecular.y * 2.0);\n"
		"    float specExponent = max(specPower / 32.0, 1.0);\n"
		"    float specLobe = pow(saturate((nDotH - 0.72) / 0.28), specExponent);\n"
		"    float specular = gSpecular.x * gloss * specLobe;\n"
		"    specular *= (diffuse <= 0.2) ? saturate(diffuse + 0.5) : 1.0;\n"
		"    float3 diffuseLight = base.rgb * diffuse * gLightColorIntensity.rgb * gLightColorIntensity.w * contribution;\n"
		"    float specularMask = saturate(specular * gSpecularTuning.y);\n"
		"    float3 specularLight = saturate(specularMask * contribution * gLightColorIntensity.rgb);\n"
		"    if (gTuning.z > 6.5) return float4(saturate(specLobe * 8.0), gloss, saturate(specLobe * gloss * 16.0), 1.0);\n"
		"    if (gTuning.z > 5.5) return float4(0.0, 0.0, 0.0, 1.0);\n"
		"    if (gTuning.z > 4.5) return float4(pow(nDotH, 16.0) * debugStrength * contribution, pow(nDotH, 16.0) * debugStrength * contribution, pow(nDotH, 16.0) * debugStrength * contribution, 1.0);\n"
		"    if (gTuning.z > 3.5) return float4(0.0, 0.0, 0.0, 1.0);\n"
		"    if (gTuning.z > 2.5) return float4(specularLight * 4.0, 1.0);\n"
		"    if (gTuning.z > 1.5) return float4(0.0, 0.0, debugStrength * contribution, 1.0);\n"
		"    float3 outLight = diffuseLight + specularLight;\n"
		"    if (gHotspot.x > 0.0) {\n"
		"        float limit = 1.0 / gHotspot.x;\n"
		"        float outLum = luma(outLight);\n"
		"        outLight *= limit / max(limit, outLum);\n"
		"    }\n"
		"    return float4(outLight, 1.0);\n"
		"}\n";

	static const char* kNormalMapVertexShaderSource =
		"row_major float4x4 gWorldViewProj : register(c0);\n"
		"row_major float4x4 gWorld : register(c4);\n"
		"struct VS_IN { float4 pos : POSITION0; float3 tangent : TANGENT0; float3 binormal : BINORMAL0; float3 normal : NORMAL0; float4 uv : TEXCOORD0; };\n"
		"struct VS_OUT { float4 pos : POSITION; float2 uv : TEXCOORD0; float3 worldRel : TEXCOORD1; float3 tangentRel : TEXCOORD2; float3 binormalRel : TEXCOORD3; float3 normalRel : TEXCOORD4; };\n"
		"VS_OUT main(VS_IN IN) {\n"
		"    VS_OUT OUT;\n"
		"    float4 localPos = float4(IN.pos.xyz, 1.0);\n"
		"    float4 worldPos = mul(localPos, gWorld);\n"
		"    OUT.pos = mul(localPos, gWorldViewProj);\n"
		"    OUT.uv = IN.uv.xy;\n"
		"    OUT.worldRel = worldPos.xyz;\n"
		"    OUT.tangentRel = normalize(mul(float4(IN.tangent.xyz, 0.0), gWorld).xyz);\n"
		"    OUT.binormalRel = normalize(mul(float4(IN.binormal.xyz, 0.0), gWorld).xyz);\n"
		"    OUT.normalRel = normalize(mul(float4(IN.normal.xyz, 0.0), gWorld).xyz);\n"
		"    return OUT;\n"
		"}\n";

	static const char* kNormalMapPixelShaderSource =
		"float4 gLightColorIntensity : register(c0);\n"
		"float4 gLightDirAngle : register(c1);\n"
		"float4 gTuning : register(c2);\n"
		"float4 gLightPosRadius : register(c3);\n"
		"float4 gSpecular : register(c4);\n"
		"float4 gSpecularTuning : register(c5);\n"
		"float4 gHotspot : register(c6);\n"
		"sampler2D DiffuseMap : register(s0);\n"
		"sampler2D NormalMap : register(s1);\n"
		"struct PS_IN { float2 uv : TEXCOORD0; float3 worldRel : TEXCOORD1; float3 tangentRel : TEXCOORD2; float3 binormalRel : TEXCOORD3; float3 normalRel : TEXCOORD4; };\n"
		"float invlerp(float a, float b, float v) { return saturate((v - a) / max(0.0001, b - a)); }\n"
		"float luma(float3 color) { return dot(color, float3(0.2126, 0.7152, 0.0722)); }\n"
		"float4 main(PS_IN IN) : COLOR0 {\n"
		"    float debugStrength = 4.0 * max(gLightColorIntensity.w, 0.25);\n"
		"    if (gTuning.z > 0.5 && gTuning.z < 1.5) return float4(debugStrength, 0.0, 0.0, 1.0);\n"
		"    float4 base = tex2D(DiffuseMap, IN.uv);\n"
		"    float3 toLight = gLightPosRadius.xyz - IN.worldRel;\n"
		"    float distToLight = length(toLight);\n"
		"    float3 lightVector = toLight / max(distToLight, 0.001);\n"
		"    float radius = max(gLightPosRadius.w, 1.0);\n"
		"    float s = saturate((distToLight / radius) * (distToLight / radius));\n"
		"    float atten = saturate(((1.0 - s) * (1.0 - s)) / (1.0 + 5.0 * s));\n"
		"    float angle = max(1.0, gLightDirAngle.w * max(gTuning.x, 0.05));\n"
		"    float angleCosMax = cos(radians(angle));\n"
		"    float angleCosMin = cos(radians(angle * 0.5));\n"
		"    float cone = pow(invlerp(angleCosMax, angleCosMin, dot(normalize(gLightDirAngle.xyz), -lightVector)), 2.0);\n"
		"    float nearFade = (gTuning.y > 0.0) ? smoothstep(0.0, gTuning.y, distToLight) : 1.0;\n"
		"    float contribution = atten * cone * nearFade;\n"
		"    float3 tangent = normalize(IN.tangentRel);\n"
		"    float3 binormal = normalize(IN.binormalRel);\n"
		"    float3 normal = normalize(IN.normalRel);\n"
		"    float4 normalSample = tex2D(NormalMap, IN.uv);\n"
		"    float3 mapNormal = normalSample.xyz * 2.0 - 1.0;\n"
		"    mapNormal.xy *= max(gSpecular.w, 0.0);\n"
		"    mapNormal = normalize(mapNormal);\n"
		"    float3 worldNormal = normalize(mapNormal.x * tangent + mapNormal.y * binormal + mapNormal.z * normal);\n"
		"    float vertexDiffuse = saturate(dot(normal, lightVector));\n"
		"    float diffuse = saturate(dot(worldNormal, lightVector));\n"
		"    float3 viewVector = normalize(-IN.worldRel);\n"
		"    float3 halfVector = normalize(lightVector + viewVector);\n"
		"    float glossInput = saturate(normalSample.a);\n"
		"    float gloss = smoothstep(0.45, 0.95, glossInput);\n"
		"    float nDotH = saturate(dot(worldNormal, halfVector));\n"
		"    float specPower = max(gSpecularTuning.x, gSpecular.y * 2.0);\n"
		"    float specExponent = max(specPower / 32.0, 1.0);\n"
		"    float specLobe = pow(saturate((nDotH - 0.72) / 0.28), specExponent);\n"
		"    float specular = gSpecular.x * gloss * specLobe;\n"
		"    specular *= (diffuse <= 0.2) ? saturate(diffuse + 0.5) : 1.0;\n"
		"    float3 diffuseLight = base.rgb * diffuse * gLightColorIntensity.rgb * gLightColorIntensity.w * contribution;\n"
		"    float specularMask = saturate(specular * gSpecularTuning.y);\n"
		"    float3 specularLight = saturate(specularMask * contribution * gLightColorIntensity.rgb);\n"
		"    if (gTuning.z > 6.5) return float4(saturate(specLobe * 8.0), gloss, saturate(specLobe * gloss * 16.0), 1.0);\n"
		"    if (gTuning.z > 5.5) {\n"
		"        float normalDelta = diffuse - vertexDiffuse;\n"
		"        float posDelta = saturate(normalDelta * 8.0) * debugStrength * contribution;\n"
		"        float negDelta = saturate(-normalDelta * 8.0) * debugStrength * contribution;\n"
		"        return float4(posDelta, negDelta, negDelta, 1.0);\n"
		"    }\n"
		"    if (gTuning.z > 4.5) return float4(pow(nDotH, 16.0) * debugStrength * contribution, pow(nDotH, 16.0) * debugStrength * contribution, pow(nDotH, 16.0) * debugStrength * contribution, 1.0);\n"
		"    if (gTuning.z > 3.5) return float4(glossInput * debugStrength * contribution, glossInput * debugStrength * contribution, glossInput * debugStrength * contribution, 1.0);\n"
		"    if (gTuning.z > 2.5) return float4(specularLight * 4.0, 1.0);\n"
		"    if (gTuning.z > 1.5) return float4(0.0, debugStrength * contribution, 0.0, 1.0);\n"
		"    float3 outLight = diffuseLight + specularLight;\n"
		"    if (gHotspot.x > 0.0) {\n"
		"        float limit = 1.0 / gHotspot.x;\n"
		"        float outLum = luma(outLight);\n"
		"        outLight *= limit / max(limit, outLum);\n"
		"    }\n"
		"    return float4(outLight, 1.0);\n"
		"}\n";

	struct RenderItem {
		NiGeometry* geometry;
		IDirect3DBaseTexture9* diffuse;
		IDirect3DBaseTexture9* normalMap;
		float specularStrength;
		float specularPower;
		float fallbackGloss;
		bool useNormalMap;
	};

	struct StreamState {
		IDirect3DVertexBuffer9* buffer;
		UINT offset;
		UINT stride;
	};

	struct SavedDeviceState {
		IDirect3DVertexShader9* deviceVertexShader;
		IDirect3DVertexShader9* cachedVertexShader;
		IDirect3DPixelShader9* devicePixelShader;
		IDirect3DPixelShader9* cachedPixelShader;
		IDirect3DVertexDeclaration9* deviceVertexDeclaration;
		IDirect3DVertexDeclaration9* cachedVertexDeclaration;
		IDirect3DBaseTexture9* deviceTextures[kSavedTextureCount];
		IDirect3DBaseTexture9* cachedTextures[kSavedTextureCount];
		IDirect3DIndexBuffer9* indexBuffer;
		StreamState streams[kSavedStreamCount];
		DWORD deviceFVF;
		DWORD cachedFVF;
		DWORD deviceRenderStates[kSavedRenderStateCount];
		DWORD cachedRenderStates[kSavedRenderStateCount];
		DWORD deviceSamplerStates[kSavedTextureCount][kSavedSamplerStateCount];
		DWORD cachedSamplerStates[kSavedTextureCount][kSavedSamplerStateCount];
		float vertexConstants[kSavedVertexConstantCount][4];
		float pixelConstants[kSavedPixelConstantCount][4];

		void Save(IDirect3DDevice9* apDevice, NiDX9RenderState* apRenderState) {
			cachedVertexShader = apRenderState->GetVertexShader();
			cachedPixelShader = apRenderState->GetPixelShader();
			cachedVertexDeclaration = apRenderState->GetVertexDeclaration();
			cachedFVF = apRenderState->GetFVF();
			if (cachedVertexShader) cachedVertexShader->AddRef();
			if (cachedPixelShader) cachedPixelShader->AddRef();
			if (cachedVertexDeclaration) cachedVertexDeclaration->AddRef();

			apDevice->GetVertexShader(&deviceVertexShader);
			apDevice->GetPixelShader(&devicePixelShader);
			apDevice->GetVertexDeclaration(&deviceVertexDeclaration);
			apDevice->GetFVF(&deviceFVF);
			for (UInt32 i = 0; i < kSavedTextureCount; ++i) {
				cachedTextures[i] = apRenderState->GetTexture(i);
				if (cachedTextures[i]) cachedTextures[i]->AddRef();
				apDevice->GetTexture(i, &deviceTextures[i]);
			}
			apDevice->GetIndices(&indexBuffer);
			for (UInt32 i = 0; i < kSavedStreamCount; ++i)
				apDevice->GetStreamSource(i, &streams[i].buffer, &streams[i].offset, &streams[i].stride);

			for (UInt32 i = 0; i < kSavedRenderStateCount; ++i) {
				cachedRenderStates[i] = apRenderState->GetRenderState(kSavedRenderStates[i]);
				apDevice->GetRenderState(kSavedRenderStates[i], &deviceRenderStates[i]);
			}

			for (UInt32 i = 0; i < kSavedTextureCount; ++i) {
				for (UInt32 j = 0; j < kSavedSamplerStateCount; ++j) {
					cachedSamplerStates[i][j] = apRenderState->GetSamplerState(i, kSavedSamplerStates[j]);
					apDevice->GetSamplerState(i, kSavedSamplerStates[j], &deviceSamplerStates[i][j]);
				}
			}

			apDevice->GetVertexShaderConstantF(0, &vertexConstants[0][0], kSavedVertexConstantCount);
			apDevice->GetPixelShaderConstantF(0, &pixelConstants[0][0], kSavedPixelConstantCount);
		}

		void Restore(IDirect3DDevice9* apDevice, NiDX9RenderState* apRenderState) {
			apRenderState->SetVertexShader(cachedVertexShader, false);
			apRenderState->SetPixelShader(cachedPixelShader, false);
			if (cachedVertexDeclaration)
				apRenderState->SetVertexDeclaration(cachedVertexDeclaration, false);
			else if (cachedFVF)
				apRenderState->SetFVF(cachedFVF, false);
			for (UInt32 i = 0; i < kSavedTextureCount; ++i)
				apRenderState->SetTexture(i, cachedTextures[i]);

			for (UInt32 i = 0; i < kSavedRenderStateCount; ++i)
				apRenderState->SetRenderState(kSavedRenderStates[i], cachedRenderStates[i], RenderStateArgs);

			for (UInt32 i = 0; i < kSavedTextureCount; ++i) {
				for (UInt32 j = 0; j < kSavedSamplerStateCount; ++j)
					apRenderState->SetSamplerState(i, kSavedSamplerStates[j], cachedSamplerStates[i][j], false);
			}

			apDevice->SetVertexShader(deviceVertexShader);
			apDevice->SetPixelShader(devicePixelShader);
			if (deviceFVF)
				apDevice->SetFVF(deviceFVF);
			else
				apDevice->SetVertexDeclaration(deviceVertexDeclaration);
			for (UInt32 i = 0; i < kSavedTextureCount; ++i)
				apDevice->SetTexture(i, deviceTextures[i]);

			for (UInt32 i = 0; i < kSavedRenderStateCount; ++i)
				apDevice->SetRenderState(kSavedRenderStates[i], deviceRenderStates[i]);

			for (UInt32 i = 0; i < kSavedTextureCount; ++i) {
				for (UInt32 j = 0; j < kSavedSamplerStateCount; ++j)
					apDevice->SetSamplerState(i, kSavedSamplerStates[j], deviceSamplerStates[i][j]);
			}

			apDevice->SetIndices(indexBuffer);
			for (UInt32 i = 0; i < kSavedStreamCount; ++i)
				apDevice->SetStreamSource(i, streams[i].buffer, streams[i].offset, streams[i].stride);
			apDevice->SetVertexShaderConstantF(0, &vertexConstants[0][0], kSavedVertexConstantCount);
			apDevice->SetPixelShaderConstantF(0, &pixelConstants[0][0], kSavedPixelConstantCount);

			if (deviceVertexShader) deviceVertexShader->Release();
			if (cachedVertexShader) cachedVertexShader->Release();
			if (devicePixelShader) devicePixelShader->Release();
			if (cachedPixelShader) cachedPixelShader->Release();
			if (deviceVertexDeclaration) deviceVertexDeclaration->Release();
			if (cachedVertexDeclaration) cachedVertexDeclaration->Release();
			for (UInt32 i = 0; i < kSavedTextureCount; ++i) {
				if (deviceTextures[i]) deviceTextures[i]->Release();
				if (cachedTextures[i]) cachedTextures[i]->Release();
			}
			if (indexBuffer) indexBuffer->Release();
			for (UInt32 i = 0; i < kSavedStreamCount; ++i) {
				if (streams[i].buffer)
					streams[i].buffer->Release();
			}
		}
	};

	static bool g_renderActive = false;
	static std::vector<RenderItem> g_renderQueue;

	struct CaptureCounters {
		UInt32 visited, culled, outOfLight, skinnedOrAlpha, terrainLOD, noGeomData, noTexture, queued;
	};
	static CaptureCounters g_counters{};
	static IDirect3DVertexShader9* g_vertexShader = nullptr;
	static IDirect3DPixelShader9* g_pixelShader = nullptr;
	static IDirect3DVertexShader9* g_normalMapVertexShader = nullptr;
	static IDirect3DPixelShader9* g_normalMapPixelShader = nullptr;
	static bool g_shaderFailed = false;

	static bool IsNiNode(NiAVObject* apObject) {
		return apObject && CdeclCall<bool>(0x43B300, kRTTI_NiNode, apObject);
	}

	static bool IsGeometry(NiAVObject* apObject) {
		return apObject && CdeclCall<bool>(0x43B300, kRTTI_NiGeometry, apObject);
	}

	static bool IsSwitchNode(NiAVObject* apObject) {
		return apObject && CdeclCall<bool>(0x43B300, kRTTI_NiSwitchNode, apObject);
	}

	static NiShadeProperty* GetShadeProperty(NiGeometry* apGeometry) {
		return apGeometry ? apGeometry->propertyState.m_spShadeProperty : nullptr;
	}

	static IDirect3DBaseTexture9* GetTexture(BSShaderPPLightingProperty* apProperty, UInt32 auiIndex) {
		if (!apProperty || auiIndex >= 6 || !apProperty->ppTextures[auiIndex])
			return nullptr;
		NiSourceTexture* texture = *apProperty->ppTextures[auiIndex];
		if (!texture || !texture->rendererData)
			return nullptr;
		return texture->rendererData->dTexture;
	}

	static bool DeclarationHasUsage(IDirect3DVertexDeclaration9* apDeclaration, BYTE aucUsage) {
		if (!apDeclaration)
			return false;

		D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH];
		UINT count = MAXD3DDECLLENGTH;
		if (FAILED(apDeclaration->GetDeclaration(elements, &count)))
			return false;

		for (UINT i = 0; i < count; ++i) {
			if (elements[i].Stream == 0xFF)
				break;
			if (elements[i].Usage == aucUsage && elements[i].UsageIndex == 0)
				return true;
		}
		return false;
	}

	static bool HasTangentBasis(NiGeometry* apGeometry) {
		NiGeometryData* modelData = apGeometry ? apGeometry->geomData : nullptr;
		NiGeometryBufferData* bufferData = modelData ? modelData->m_pkBuffData : nullptr;
		if (!bufferData || !bufferData->VertexDeclaration)
			return false;
		return DeclarationHasUsage(bufferData->VertexDeclaration, D3DDECLUSAGE_TANGENT) &&
			DeclarationHasUsage(bufferData->VertexDeclaration, D3DDECLUSAGE_BINORMAL) &&
			DeclarationHasUsage(bufferData->VertexDeclaration, D3DDECLUSAGE_NORMAL);
	}

	static float ClampFloat(float afValue, float afMin, float afMax) {
		if (afValue < afMin)
			return afMin;
		if (afValue > afMax)
			return afMax;
		return afValue;
	}

	static float GetSpecularScale(NiGeometry* apGeometry) {
		NiMaterialProperty* material = apGeometry ? apGeometry->propertyState.m_spMaterialProperty : nullptr;
		if (!material)
			return 1.0f;

		float scale = material->spec.r;
		if (material->spec.g > scale)
			scale = material->spec.g;
		if (material->spec.b > scale)
			scale = material->spec.b;
		if (scale <= 0.001f)
			return 1.0f;
		return ClampFloat(scale, 0.0f, 4.0f);
	}

	static float GetSpecularPower(NiGeometry* apGeometry) {
		NiMaterialProperty* material = apGeometry ? apGeometry->propertyState.m_spMaterialProperty : nullptr;
		if (!material || material->fShine <= 0.0f)
			return 32.0f;
		return ClampFloat(material->fShine, 4.0f, 128.0f);
	}

	static bool IsWithinLight(NiAVObject* apObject) {
		if (!TheShaderManager || !apObject)
			return false;

		NiBound* bound = apObject->m_kWorldBound;
		const NiPoint3 center = bound ? bound->Center : apObject->m_worldTransform.pos;
		const float boundRadius = bound ? bound->Radius : 0.0f;
		const D3DXVECTOR4& light = TheShaderManager->SpotLightPosition[0];
		const float radius = light.w + boundRadius;
		const float dx = center.x - light.x;
		const float dy = center.y - light.y;
		const float dz = center.z - light.z;
		return (dx * dx + dy * dy + dz * dz) <= radius * radius;
	}

	static bool UsesAlphaBlendOrTest(NiGeometry* apGeometry) {
		NiAlphaProperty* alpha = apGeometry ? apGeometry->propertyState.m_spAlphaProperty : nullptr;
		if (!alpha)
			return false;
		return (alpha->flags & (NiAlphaProperty::ALPHA_BLEND_MASK | NiAlphaProperty::TEST_ENABLE_MASK)) != 0;
	}

	static bool ShouldQueueGeometry(
		NiGeometry* apGeometry,
		BSShaderPPLightingProperty* apProperty,
		IDirect3DBaseTexture9** apDiffuse) {
		if (!g_renderActive || !apGeometry || !apProperty || !apDiffuse)
			return false;
		if ((int)g_renderQueue.size() >= Config::Flashlight.MaterialLightMaxGeometry)
			return false;
		if (apGeometry->skinInstance || UsesAlphaBlendOrTest(apGeometry)) {
			++g_counters.skinnedOrAlpha;
			return false;
		}
		if (apGeometry->m_flags & NiAVObject::APP_CULLED) {
			++g_counters.culled;
			return false;
		}

		const char* name = apGeometry->m_pcName ? apGeometry->m_pcName : "";
		if (strstr(name, "Terrain LOD") == name) {
			++g_counters.terrainLOD;
			return false;
		}

		NiGeometryData* modelData = apGeometry->geomData;
		if (!modelData || !modelData->m_pkBuffData || !modelData->m_pkBuffData->VertCount) {
			++g_counters.noGeomData;
			return false;
		}
		if (!IsWithinLight(apGeometry)) {
			++g_counters.outOfLight;
			return false;
		}

		*apDiffuse = GetTexture(apProperty, 0);
		if (!*apDiffuse) {
			++g_counters.noTexture;
			return false;
		}
		++g_counters.queued;
		return true;
	}

	static void QueueGeometry(
		NiGeometry* apGeometry,
		BSShaderPPLightingProperty* apProperty,
		IDirect3DBaseTexture9* apNormalMap,
		bool abSpecular,
		float afSpecularStrength,
		float afSpecularPower,
		bool abUseNormalMap) {
		IDirect3DBaseTexture9* diffuse = nullptr;
		if (!ShouldQueueGeometry(apGeometry, apProperty, &diffuse))
			return;
		const float specularStrength = abSpecular ? afSpecularStrength : 0.0f;
		g_renderQueue.push_back({ apGeometry, diffuse, abUseNormalMap ? apNormalMap : nullptr, specularStrength, afSpecularPower, 0.5f, abUseNormalMap && apNormalMap });
	}

	void CaptureGeometry(NiGeometry* apGeometry) {
		if (!g_renderActive || !apGeometry)
			return;
		if (!*(void**)apGeometry || !IsGeometry(static_cast<NiAVObject*>(apGeometry)))
			return;

		NiShadeProperty* shade = GetShadeProperty(apGeometry);
		if (!shade)
			return;

		if (shade->m_eShaderType != NiShadeProperty::kProp_PPLighting)
			return;

		auto* property = static_cast<BSShaderPPLightingProperty*>(shade);
		const UInt32 flags0 = property->ulFlags[0];
		const UInt32 flags1 = property->ulFlags[1];
		const bool specular = (flags0 & BSShaderProperty::Specular) != 0;
		const bool normalEligible = (flags1 & BSShaderProperty::Skip_Normal_Maps) == 0;
		const bool modelSpaceNormals = (flags0 & BSShaderProperty::Model_Space_Normals) != 0;
		const float specularStrength = specular ? GetSpecularScale(apGeometry) : 0.0f;
		const float specularPower = GetSpecularPower(apGeometry);
		IDirect3DBaseTexture9* normalMap = GetTexture(property, 1);
		const bool useNormalMap = normalEligible && !modelSpaceNormals && HasTangentBasis(apGeometry) && normalMap;

		QueueGeometry(apGeometry, property, normalMap, specular, specularStrength, specularPower, useNormalMap);
	}

	static void CaptureObject(NiAVObject* apObject) {
		if (!apObject || !*(void**)apObject)
			return;
		++g_counters.visited;
		if (apObject->m_flags & NiAVObject::APP_CULLED) {
			++g_counters.culled;
			return;
		}
		//node bounds here do not enclose their children, so they cannot prune a subtree.
		//the cone test belongs at the geometry, in ShouldQueueGeometry.
		if ((int)g_renderQueue.size() >= Config::Flashlight.MaterialLightMaxGeometry)
			return;

		if (IsGeometry(apObject)) {
			CaptureGeometry(static_cast<NiGeometry*>(apObject));
			return;
		}

		if (!IsNiNode(apObject))
			return;

		auto* node = static_cast<NiNode*>(apObject);
		if (!node->m_children.data || node->m_children.numObjs == 0)
			return;

		if (IsSwitchNode(apObject)) {
			auto* switchNode = static_cast<NiSwitchNode*>(apObject);
			if (switchNode->m_iIndex < 0)
				return;
			UInt32 index = (UInt32)switchNode->m_iIndex;
			if (index < node->m_children.numObjs)
				CaptureObject(node->m_children.data[index]);
			return;
		}

		for (UInt32 i = 0; i < node->m_children.numObjs; ++i)
			CaptureObject(node->m_children.data[i]);
	}

	static double g_captureMS = 0.0;

	void CaptureScene(SceneGraph* apSceneGraph) {
		if (!g_renderActive || !apSceneGraph)
			return;

		if (!Config::Flashlight.MaterialLightLogProjection) {
			CaptureObject(apSceneGraph);
			return;
		}

		LARGE_INTEGER freq, start, end;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&start);
		CaptureObject(apSceneGraph);
		QueryPerformanceCounter(&end);
		g_captureMS = (double)(end.QuadPart - start.QuadPart) * 1000.0 / (double)freq.QuadPart;
	}

	void BeginFrame(bool abActive) {
		g_renderQueue.clear();
		g_counters = CaptureCounters{};
		g_renderActive = abActive && Config::Flashlight.MaterialLightEnabled &&
			Config::Flashlight.MaterialLightRender &&
			Config::Flashlight.MaterialLightIntensity > 0.0f &&
			Config::Flashlight.MaterialLightMaxGeometry > 0;
	}

	static bool CompileShader(const char* apSource, const char* apEntry, const char* apProfile, ID3DXBuffer** apCode) {
		ID3DXBuffer* errors = nullptr;
		HRESULT hr = D3DXCompileShader(apSource, (UINT)strlen(apSource), nullptr, nullptr, apEntry, apProfile, 0, apCode, &errors, nullptr);
		if (FAILED(hr)) {
			const char* text = errors ? (const char*)errors->GetBufferPointer() : nullptr;
			int length = errors ? (int)errors->GetBufferSize() : 0;
			while (length > 0 && (text[length - 1] == '\0' || text[length - 1] == '\n' || text[length - 1] == '\r'))
				--length;
			Log("[matpass] %s shader failed to compile (hr 0x%08X): %.*s", apProfile, hr, length, text ? text : "");
		}
		if (errors)
			errors->Release();
		return SUCCEEDED(hr);
	}

	static bool EnsureShaders(IDirect3DDevice9* apDevice) {
		if (g_vertexShader && g_pixelShader && g_normalMapVertexShader && g_normalMapPixelShader)
			return true;
		if (g_shaderFailed || !apDevice)
			return false;

		ID3DXBuffer* vsCode = nullptr;
		ID3DXBuffer* psCode = nullptr;
		ID3DXBuffer* normalVsCode = nullptr;
		ID3DXBuffer* normalPsCode = nullptr;
		if (!CompileShader(kVertexShaderSource, "main", "vs_3_0", &vsCode) ||
			!CompileShader(kPixelShaderSource, "main", "ps_3_0", &psCode) ||
			!CompileShader(kNormalMapVertexShaderSource, "main", "vs_3_0", &normalVsCode) ||
			!CompileShader(kNormalMapPixelShaderSource, "main", "ps_3_0", &normalPsCode)) {
			g_shaderFailed = true;
			if (vsCode) vsCode->Release();
			if (psCode) psCode->Release();
			if (normalVsCode) normalVsCode->Release();
			if (normalPsCode) normalPsCode->Release();
			return false;
		}

		HRESULT vs = apDevice->CreateVertexShader((const DWORD*)vsCode->GetBufferPointer(), &g_vertexShader);
		HRESULT ps = apDevice->CreatePixelShader((const DWORD*)psCode->GetBufferPointer(), &g_pixelShader);
		HRESULT normalVs = apDevice->CreateVertexShader((const DWORD*)normalVsCode->GetBufferPointer(), &g_normalMapVertexShader);
		HRESULT normalPs = apDevice->CreatePixelShader((const DWORD*)normalPsCode->GetBufferPointer(), &g_normalMapPixelShader);
		vsCode->Release();
		psCode->Release();
		normalVsCode->Release();
		normalPsCode->Release();

		if (FAILED(vs) || FAILED(ps) || FAILED(normalVs) || FAILED(normalPs)) {
			if (g_vertexShader) {
				g_vertexShader->Release();
				g_vertexShader = nullptr;
			}
			if (g_pixelShader) {
				g_pixelShader->Release();
				g_pixelShader = nullptr;
			}
			if (g_normalMapVertexShader) {
				g_normalMapVertexShader->Release();
				g_normalMapVertexShader = nullptr;
			}
			if (g_normalMapPixelShader) {
				g_normalMapPixelShader->Release();
				g_normalMapPixelShader = nullptr;
			}
			g_shaderFailed = true;
			return false;
		}

		return true;
	}

	static void ApplyRenderState() {
		NiDX9Renderer* renderer = TheRenderManager->renderer;
		NiDX9RenderState* renderState = renderer->renderState;
		IDirect3DDevice9* device = renderer->device;
		auto setState = [renderState, device](D3DRENDERSTATETYPE aeState, DWORD auiValue) {
			renderState->SetRenderState(aeState, auiValue, RenderStateArgs);
			device->SetRenderState(aeState, auiValue);
		};
		auto setSampler = [renderState, device](UInt32 auiSampler, D3DSAMPLERSTATETYPE aeState, DWORD auiValue) {
			renderState->SetSamplerState(auiSampler, aeState, auiValue, false);
			device->SetSamplerState(auiSampler, aeState, auiValue);
		};

		renderState->SetVertexShader(g_vertexShader, false);
		renderState->SetPixelShader(g_pixelShader, false);
		device->SetVertexShader(g_vertexShader);
		device->SetPixelShader(g_pixelShader);
		for (UInt32 i = 0; i < kSavedTextureCount; ++i) {
			renderState->SetTexture(i, nullptr);
			device->SetTexture(i, nullptr);
		}

		setState(D3DRS_ZENABLE, D3DZB_TRUE);
		setState(D3DRS_ZWRITEENABLE, D3DZB_FALSE);
		//the projection follows IsReversedDepth, so the comparison has to as well - under
		//reversed Z nearer is larger, and LESSEQUAL passes exactly the occluded fragments
		setState(D3DRS_ZFUNC, TheRenderManager->IsReversedDepth() ? D3DCMP_GREATEREQUAL : D3DCMP_LESSEQUAL);
		setState(D3DRS_ALPHABLENDENABLE, TRUE);
		setState(D3DRS_SRCBLEND, D3DBLEND_ONE);
		setState(D3DRS_DESTBLEND, D3DBLEND_ONE);
		setState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		setState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
		setState(D3DRS_ALPHATESTENABLE, FALSE);
		setState(D3DRS_STENCILENABLE, FALSE);
		setState(D3DRS_COLORWRITEENABLE,
			D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
		setState(D3DRS_FOGENABLE, FALSE);
		setState(D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF);
		setState(D3DRS_SCISSORTESTENABLE, FALSE);
		setState(D3DRS_CLIPPLANEENABLE, 0);

		for (UInt32 i = 0; i < kSavedTextureCount; ++i) {
			setSampler(i, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
			setSampler(i, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
			setSampler(i, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
			setSampler(i, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			setSampler(i, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
		}
	}

	static void SetCullMode(NiGeometry* apGeometry) {
		NiStencilProperty* stencil = apGeometry ? apGeometry->propertyState.m_spStencilProperty : nullptr;
		if (stencil)
			TheRenderManager->renderer->renderState->SetCullMode(stencil->GetDrawMode());
		else
			TheRenderManager->renderer->renderState->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW, RenderStateArgs);
		TheRenderManager->renderer->device->SetRenderState(D3DRS_CULLMODE,
			TheRenderManager->renderer->renderState->GetRenderState(D3DRS_CULLMODE));
	}

	static bool DrawGeometryBuffer(NiGeometry* apGeometry) {
		if (!apGeometry || !apGeometry->geomData)
			return false;
		NiGeometryBufferData* data = apGeometry->geomData->m_pkBuffData;
		if (!data || !data->VertCount)
			return false;
		if (data->StreamCount > kSavedStreamCount || !data->VertexStride)
			return false;

		IDirect3DDevice9* device = TheRenderManager->renderer->device;
		for (UInt32 i = 0; i < data->StreamCount; ++i) {
			if (!data->VBChip || !data->VBChip[i] || !data->VBChip[i]->VB)
				return false;
			device->SetStreamSource(i, data->VBChip[i]->VB, 0, data->VertexStride[i]);
		}

		if (data->IB)
			device->SetIndices(data->IB);
		if (data->FVF) {
			TheRenderManager->renderer->renderState->SetFVF(data->FVF, false);
			device->SetFVF(data->FVF);
		}
		else if (data->VertexDeclaration) {
			TheRenderManager->renderer->renderState->SetVertexDeclaration(data->VertexDeclaration, false);
			device->SetVertexDeclaration(data->VertexDeclaration);
		}
		else
			return false;

		const UInt16 dirtyFlags = apGeometry->geomData->m_usDirtyFlags;
		if (apGeometry->geomData->IsStripsData())
			ThisCall(kRenderTriStripsAlt, NiDX9Renderer::GetSingleton(), apGeometry);
		else
			ThisCall(kRenderTriShapeAlt, NiDX9Renderer::GetSingleton(), apGeometry);
		apGeometry->geomData->m_usDirtyFlags = dirtyFlags;
		return true;
	}

	//the queue is transformed by the frame-start ViewProj while the depth buffer it is tested
	//against belongs to whatever projection the engine last drew with. split the divergence into
	//view vs projection, and report whether there is any depth to test against at all.
	static void LogProjectionDivergence() {
		if (!Config::Flashlight.MaterialLightLogProjection || !WorldSceneGraph || !WorldSceneGraph->camera)
			return;

		static UInt32 throttle = 0;
		if (throttle++ % 300)
			return;

		auto worstOf = [](const D3DXMATRIX& a, const D3DXMATRIX& b, int& aOutIndex) {
			float worst = 0.0f;
			aOutIndex = -1;
			for (int i = 0; i < 16; ++i) {
				float delta = fabsf(((const float*)&a)[i] - ((const float*)&b)[i]);
				if (delta > worst) { worst = delta; aOutIndex = i; }
			}
			return worst;
		};

		const D3DXMATRIX& liveView = TheRenderManager->renderer->viewMatrix;
		const D3DXMATRIX& liveProj = TheRenderManager->renderer->projMatrix;
		int viewIndex = -1, projIndex = -1;
		float viewWorst = worstOf(TheRenderManager->ViewMatrix, liveView, viewIndex);
		float projWorst = worstOf(TheRenderManager->ProjMatrix, liveProj, projIndex);

		IDirect3DSurface9* depth = nullptr;
		TheRenderManager->renderer->device->GetDepthStencilSurface(&depth);
		D3DSURFACE_DESC desc{};
		if (depth) depth->GetDesc(&desc);

		Log("[matpass] walk visited=%u queued=%u in %.3f ms",
			g_counters.visited, g_counters.queued, g_captureMS);

		const NiFrustum& frustum = WorldSceneGraph->camera->Frustum;
		Log("[matpass] near=%.4f far=%.1f queue=%u depth=%s %ux%u",
			frustum.Near, frustum.Far, (unsigned)g_renderQueue.size(),
			depth ? "yes" : "NONE", desc.Width, desc.Height);
		Log("[matpass] view worst=%.6f at [%d] (%.6f vs %.6f)",
			viewWorst, viewIndex,
			viewIndex >= 0 ? ((const float*)&TheRenderManager->ViewMatrix)[viewIndex] : 0.0f,
			viewIndex >= 0 ? ((const float*)&liveView)[viewIndex] : 0.0f);
		Log("[matpass] proj worst=%.6f at [%d] (%.6f vs %.6f)",
			projWorst, projIndex,
			projIndex >= 0 ? ((const float*)&TheRenderManager->ProjMatrix)[projIndex] : 0.0f,
			projIndex >= 0 ? ((const float*)&liveProj)[projIndex] : 0.0f);
		Log("[matpass] proj depth terms ours _33=%.6f _43=%.6f live _33=%.6f _43=%.6f",
			TheRenderManager->ProjMatrix._33, TheRenderManager->ProjMatrix._43,
			liveProj._33, liveProj._43);

		if (depth) depth->Release();
	}

	//latched on the reason pointer, so a steady state logs once rather than every frame
	static void LogPassGate(const char* apReason) {
		static const char* sLastReason = nullptr;
		if (sLastReason == apReason)
			return;
		sLastReason = apReason;
		if (apReason)
			Log("[matpass] not drawing: %s", apReason);
	}

	void RenderWorld() {
		if (!TheRenderManager || !TheShaderManager)
			return;
		if (!g_renderActive) { LogPassGate("pass inactive, flashlight off or disabled in config"); return; }
		if (g_renderQueue.empty()) {
			LogPassGate("nothing queued in the light cone");
			static bool sReported = false;
			if (!sReported && Config::Flashlight.MaterialLightLogProjection) {
				sReported = true;
				const D3DXVECTOR4& light = TheShaderManager->SpotLightPosition[0];
				Log("[matpass] light pos=(%.1f %.1f %.1f) radius=%.1f", light.x, light.y, light.z, light.w);
				Log("[matpass] visited=%u culled=%u outOfLight=%u skinnedOrAlpha=%u terrainLOD=%u noGeomData=%u noTexture=%u queued=%u",
					g_counters.visited, g_counters.culled, g_counters.outOfLight, g_counters.skinnedOrAlpha,
					g_counters.terrainLOD, g_counters.noGeomData, g_counters.noTexture, g_counters.queued);
			}
			return;
		}

		IDirect3DDevice9* device = TheRenderManager->renderer->device;
		if (!EnsureShaders(device)) { LogPassGate("pass shaders unavailable"); return; }
		LogPassGate(nullptr);

		LogProjectionDivergence();

		NiDX9RenderState* renderState = TheRenderManager->renderer->renderState;
		SavedDeviceState saved{};
		saved.Save(device, renderState);
		ApplyRenderState();

		D3DXVECTOR4 lightPos = TheShaderManager->SpotLightPosition[0];
		lightPos.x -= TheRenderManager->CameraPosition.x;
		lightPos.y -= TheRenderManager->CameraPosition.y;
		lightPos.z -= TheRenderManager->CameraPosition.z;

		D3DXVECTOR4 lightDir = TheShaderManager->SpotLightDirection[0];
		D3DXVec3Normalize((D3DXVECTOR3*)&lightDir, (D3DXVECTOR3*)&lightDir);
		lightDir.w = TheShaderManager->SpotLightDirection[0].w;

		D3DXVECTOR4 lightColor = TheShaderManager->SpotLightColor[0];
		lightColor.w = Config::Flashlight.MaterialLightIntensity;
		D3DXVECTOR4 tuning(
			Config::Flashlight.SpotSize,
			Config::Flashlight.NearFade,
			(float)Config::Flashlight.MaterialLightDebugMode,
			0.0f);

		device->SetPixelShaderConstantF(0, (float*)&lightColor, 1);
		device->SetPixelShaderConstantF(1, (float*)&lightDir, 1);
		device->SetPixelShaderConstantF(2, (float*)&tuning, 1);
		device->SetPixelShaderConstantF(3, (float*)&lightPos, 1);
		D3DXVECTOR4 specularTuning(Config::Flashlight.MaterialLightSpecularPower, Config::Flashlight.MaterialLightSpecularBoost, 0.0f, 0.0f);
		device->SetPixelShaderConstantF(5, (float*)&specularTuning, 1);
		D3DXVECTOR4 hotspot(Config::Flashlight.HotspotLimit, 0.0f, 0.0f, 0.0f);
		device->SetPixelShaderConstantF(6, (float*)&hotspot, 1);

		for (const RenderItem& item : g_renderQueue) {
			if (!item.geometry || !item.diffuse)
				continue;
			const bool useNormalMap = item.useNormalMap && item.normalMap;
			const float specularStrength = item.specularStrength * Config::Flashlight.MaterialLightSpecular;

			D3DXMATRIX world;
			TheRenderManager->CreateD3DMatrix(&world, &item.geometry->m_worldTransform);
			D3DXMATRIX worldViewProj = world * TheRenderManager->ViewProjMatrix;
			device->SetVertexShaderConstantF(0, (float*)&worldViewProj, 4);
			device->SetVertexShaderConstantF(4, (float*)&world, 4);

			D3DXVECTOR4 specular(specularStrength, item.specularPower, item.fallbackGloss, Config::Flashlight.MaterialLightNormalStrength);
			device->SetPixelShaderConstantF(4, (float*)&specular, 1);

			TheRenderManager->renderer->renderState->SetVertexShader(useNormalMap ? g_normalMapVertexShader : g_vertexShader, false);
			TheRenderManager->renderer->renderState->SetPixelShader(useNormalMap ? g_normalMapPixelShader : g_pixelShader, false);
			TheRenderManager->renderer->renderState->SetTexture(0, item.diffuse);
			TheRenderManager->renderer->renderState->SetTexture(1, useNormalMap ? item.normalMap : nullptr);
			SetCullMode(item.geometry);
			DrawGeometryBuffer(item.geometry);
		}

		saved.Restore(device, renderState);
	}

	void InitHooks() {}
}
