//Volumetric flashlight beam - single-scattering view-ray march, half resolution.
//The march covers only the view ray's intersection with the cone volume, so every
//step samples lit air (a full-ray march spends most steps in empty space and the
///steps average buries the few in-beam hits). Clamped to scene depth (v1 occlusion:
//no shadow map, so the shaft can show through walls in the air). Accumulates
//in-scatter with a Henyey-Greenstein phase, height-based density, the same cone +
//cookie + attenuation as the surface light. Output is added to the HDR scene by
//the Flashlight Combine pass before tonemapping, so bloom catches the shaft.

float4 TESR_SpotLightPosition;
float4 TESR_SpotLightColor;
float4 TESR_SpotLightDirection;
float4 TESR_VolumetricControl;   // x=strength, y=anisotropy g, z=height falloff, w=max distance
float4 TESR_VolumetricData;      // xyz=march light pos (pushed off-camera in 1st person for parallax), w=1st-person strength floor
float4 TESR_FlashLightTuning;    // x=spot size scale (match the surface pool)
float4 TESR_ReciprocalResolution;
float4x4 TESR_FlashLightViewProjTransform;

//per-unit-length calibration of the scatter integral
#define VOL_CALIBRATION 0.0008
#define VOL_SURFACE_FADE 256.0
#define FL_SIZE TESR_FlashLightTuning.x

sampler2D TESR_DepthBuffer      : register(s0) = sampler_state { ADDRESSU = CLAMP; ADDRESSV = CLAMP; MAGFILTER = POINT; MINFILTER = POINT; MIPFILTER = NONE; };
sampler2D TESR_SpotLightTexture : register(s1) < string ResourceName = "Effects\Flashlight.png"; > = sampler_state { ADDRESSU = CLAMP; ADDRESSV = CLAMP; MAGFILTER = LINEAR; MINFILTER = LINEAR; MIPFILTER = LINEAR; };

#include "Includes/Depth.hlsl"
#include "Includes/Helpers.hlsl"

#define VOL_STEPS 24

struct VSOUT { float4 vertPos : POSITION; float2 UVCoord : TEXCOORD0; };
struct VSIN  { float4 vertPos : POSITION0; float2 UVCoord : TEXCOORD0; };

VSOUT FrameVS(VSIN IN) {
    VSOUT OUT = (VSOUT)0.0f;
    OUT.vertPos = IN.vertPos;
    OUT.UVCoord = IN.UVCoord;
    return OUT;
}

float4 ScreenCoordToTexCoord(float4 coord) {
    coord.xyz /= coord.w;
    coord.x = coord.x *  0.5f + 0.5f;
    coord.y = coord.y * -0.5f + 0.5f;
    return coord;
}

//Henyey-Greenstein phase (forward-scattering for g>0)
float HenyeyGreenstein(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(max(1e-4, 1.0 + g2 - 2.0 * g * cosTheta), 1.5));
}

//interleaved-gradient noise (Jimenez) - cheap per-pixel dither for the ray start
float ign(float2 px) {
    return frac(52.9829189 * frac(dot(px, float2(0.06711056, 0.00583715))));
}

//[t0,t1] bracket of the view ray inside the cone volume: cone-surface hits on the
//forward nappe within the light range, range-sphere hits inside the cone, plus the
//endpoints when the ray starts or ends inside. co = ray origin relative to the
//apex. a slightly loose bracket is fine - each sample still runs the exact cone and
//attenuation math, the bracket only stops steps being spent on empty air
bool ConeSegment(float3 co, float3 rd, float3 axis, float cosA, float range, float tMax, out float t0, out float t1) {
    float cos2 = cosA * cosA;
    float da   = dot(rd, axis);
    float ca   = dot(co, axis);
    float dco  = dot(rd, co);
    float coco = dot(co, co);

    t0 = tMax; t1 = 0.0;
    float t, h;
    float3 p;

    float A = da * da - cos2;
    float B = 2.0 * (da * ca - dco * cos2);
    float C = ca * ca - coco * cos2;
    float disc = B * B - 4.0 * A * C;
    if (abs(A) > 1e-6 && disc > 0.0) {
        float sq = sqrt(disc);
        t = (-B - sq) / (2.0 * A); h = ca + t * da;
        if (t > 0.0 && t < tMax && h > 0.0 && h < range) { t0 = min(t0, t); t1 = max(t1, t); }
        t = (-B + sq) / (2.0 * A); h = ca + t * da;
        if (t > 0.0 && t < tMax && h > 0.0 && h < range) { t0 = min(t0, t); t1 = max(t1, t); }
    }

    float ds = dco * dco - (coco - range * range);
    if (ds > 0.0) {
        float sq = sqrt(ds);
        t = -dco - sq; p = co + t * rd; h = dot(p, axis);
        if (t > 0.0 && t < tMax && h > 0.0 && h * h > cos2 * dot(p, p)) { t0 = min(t0, t); t1 = max(t1, t); }
        t = -dco + sq; p = co + t * rd; h = dot(p, axis);
        if (t > 0.0 && t < tMax && h > 0.0 && h * h > cos2 * dot(p, p)) { t0 = min(t0, t); t1 = max(t1, t); }
    }

    if (ca > 0.0 && ca < range && ca * ca > cos2 * coco) t0 = 0.0;
    p = co + tMax * rd; h = dot(p, axis);
    if (h > 0.0 && dot(p, p) < range * range && h * h > cos2 * dot(p, p)) t1 = tMax;

    return t1 > t0;
}

float4 Volumetric(VSOUT IN) : COLOR0 {
    float strength = TESR_VolumetricControl.x;
    if (strength <= 0.0) return float4(0, 0, 0, 1);   // disabled - Combine gate also guards this

    float g            = TESR_VolumetricControl.y;
    float heightFall   = TESR_VolumetricControl.z;
    float maxDist      = TESR_VolumetricControl.w;

    float3 rayVec = toWorld(IN.UVCoord);                 // forward-component 1
    float3 dir    = normalize(rayVec);
    float3 camPos = TESR_CameraPosition.xyz;

    float3 lightPos = TESR_VolumetricData.xyz;
    float fpBlend   = saturate(TESR_VolumetricData.w);
    float3 lightDir = TESR_SpotLightDirection.xyz;
    float radius    = TESR_SpotLightPosition.w;
    float angleCosMax = cos(radians(TESR_SpotLightDirection.w * FL_SIZE));
    float angleCosMin = cos(radians(TESR_SpotLightDirection.w * FL_SIZE * 0.5));
    float groundZ   = camPos.z - 128.0;                  // ~player feet; dust settles low

    float sceneDist = length(rayVec) * readDepth(IN.UVCoord);
    float tMax = min(sceneDist, maxDist);

    float tEnter, tExit;
    if (!ConeSegment(camPos - lightPos, dir, lightDir, max(angleCosMax, 0.1), radius, tMax, tEnter, tExit))
        return float4(0, 0, 0, 1);

    float stepLen = (tExit - tEnter) / VOL_STEPS;

    //dithered start offset breaks the step banding (resolved by the bilinear upsample)
    float t = tEnter + ign(IN.UVCoord / TESR_ReciprocalResolution.xy) * stepLen;

    float3 scatter = 0;
    float  transmittance = 1.0;
    [loop]
    for (int i = 0; i < VOL_STEPS; i++) {
        float3 p = camPos + dir * t;
        float3 toL = lightPos - p;
        float  distToL = length(toL);
        float3 Ldir = toL / max(distToL, 0.001);         // sample -> light

        float cone = pow(invlerps(angleCosMax, angleCosMin, shades(lightDir, -Ldir)), 2.0);
        float s = saturate((distToL / radius) * (distToL / radius));
        float atten = saturate(((1 - s) * (1 - s)) / (1 + 5.0 * s));

        float4 lsc = ScreenCoordToTexCoord(mul(float4(p, 1), TESR_FlashLightViewProjTransform));
        float2 cookieUV = (lsc.xy - 0.5) / max(0.05, FL_SIZE) + 0.5;   // match the surface pool size
        //the cookie projects from the TRUE light - in 1st person the march runs from the
        //virtual offset position, so blend the cookie flat to hide the parallax mismatch
        float cookie = lerp(tex2D(TESR_SpotLightTexture, cookieUV).r, 1.0, fpBlend);

        float density = exp(-max(0.0, p.z - groundZ) * heightFall * 0.0005);
        float phase   = HenyeyGreenstein(dot(dir, -Ldir), g);

        float surfaceFade = smoothstep(0.0, VOL_SURFACE_FADE, sceneDist - t);
        float sigma = density * cone * atten * cookie * surfaceFade;
        scatter += transmittance * sigma * phase * stepLen;
        transmittance *= exp(-density * 0.5 * stepLen * 0.001);   // gentle extinction
        t += stepLen;
    }

    //match the surface pool's colour and dimmer
    float3 lightColor = TESR_SpotLightColor.rgb * TESR_SpotLightColor.w;

    //fade with light-camera distance so the in-scatter can't pile onto the surface
    //pool when the source sits near the eye. in 1st person the virtual offset gives
    //the march its parallax and w floors the fade at the configured strength
    float camFade = saturate((length(lightPos - camPos) - 40.0) / 60.0);
    camFade = max(camFade, fpBlend);

    float3 inScatter = scatter * (strength * VOL_CALIBRATION * camFade) * lightColor;
    return float4(inScatter, transmittance);
}

technique {
    pass {
        VertexShader = compile vs_3_0 FrameVS();
        PixelShader  = compile ps_3_0 Volumetric();
    }
}
