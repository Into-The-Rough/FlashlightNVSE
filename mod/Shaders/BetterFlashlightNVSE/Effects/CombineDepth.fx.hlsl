float4 TESR_DepthConstants;
float4 TESR_CameraData;

float4x4 TESR_InvProjectionTransform;

sampler2D TESR_DepthBufferWorld : register(s0) = sampler_state { ADDRESSU = CLAMP; ADDRESSV = CLAMP; MAGFILTER = POINT; MINFILTER = POINT; MIPFILTER = NONE; };
sampler2D TESR_DepthBufferViewModel : register(s1) = sampler_state { ADDRESSU = CLAMP; ADDRESSV = CLAMP; MAGFILTER = POINT; MINFILTER = POINT; MIPFILTER = NONE; };

static const float viewModelNearZ = TESR_DepthConstants.x;
static const float invertedDepth = TESR_DepthConstants.z;
static const float useViewModelBuffer = TESR_DepthConstants.w;   //0 in 3rd person: viewmodel buffer is stale, ignore it
static const float nearZ = TESR_CameraData.x;
static const float farZ = TESR_CameraData.y;

struct VSOUT
{
	float4 vertPos : POSITION;
	float2 UVCoord : TEXCOORD0;
};
 
struct VSIN
{
	float4 vertPos : POSITION0;
	float2 UVCoord : TEXCOORD0;
};
 
VSOUT FrameVS(VSIN IN)
{
	VSOUT OUT = (VSOUT)0.0f;
	OUT.vertPos = IN.vertPos;
	OUT.UVCoord = IN.UVCoord;
	return OUT;
}

float IsDepthSet(float depth) {
    return (invertedDepth > 0.5) ? step(1e-6, depth) : step(depth, 0.999999);
}

float IsNearer(float depth, float otherDepth) {
    return (invertedDepth > 0.5) ? step(otherDepth, depth) : step(depth, otherDepth);
}

// Convert depth to view space Z.
float ToVS(float depth, float nearZ, float farZ) {
    if (invertedDepth > 0.5)
        return nearZ * farZ / (nearZ + depth * (farZ - nearZ));

    return nearZ * farZ / (farZ - depth * (farZ - nearZ));
}

// Convert view space Z to depth.
float ToPS(float viewZ, float nearZ, float farZ) {
    if (invertedDepth > 0.5)
        return nearZ * (farZ - viewZ) / (viewZ * (farZ - nearZ));

    return farZ * (viewZ - nearZ) / (viewZ * (farZ - nearZ));
}


float4 CombineDepth(VSOUT IN) : COLOR0 {
	float worldDepth = tex2D(TESR_DepthBufferWorld, IN.UVCoord).x;
	float viewModelDepth = tex2D(TESR_DepthBufferViewModel, IN.UVCoord).x;

	//First-person depth is cleared before the viewmodel draw, so only valid
	//nearer samples should replace the world depth. useViewModelBuffer gates the
	//whole merge off in 3rd person, where the viewmodel buffer is never refreshed.
	//the buffers are written with different near planes, so raw depths are not comparable.
	//decode each with its own, then merge in view space. viewmodel clip is script-settable.
	float worldViewZ = ToVS(worldDepth, nearZ, farZ);
	float viewModelViewZ = ToVS(viewModelDepth, viewModelNearZ, farZ);

	float useViewModel = IsDepthSet(viewModelDepth) * step(viewModelViewZ, worldViewZ) * useViewModelBuffer;
	float combinedViewZ = lerp(worldViewZ, viewModelViewZ, useViewModel);

    return float4(combinedViewZ / farZ, ToPS(combinedViewZ, nearZ, farZ), 1.0, 1.0);
}
 
technique
{
	pass
	{
		VertexShader = compile vs_3_0 FrameVS();
		PixelShader = compile ps_3_0 CombineDepth();
	}
}
