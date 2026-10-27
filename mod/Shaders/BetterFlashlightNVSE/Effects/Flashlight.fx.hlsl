//NVR's flashlight, restored to its full multi-pass form:
//  NoShadow -> Flashlight -> BoxBlurMin -> BoxBlurAvg -> Combine
//EffectRecord::Render runs every pass and StretchRects each pass output into
//TESR_RenderedBuffer, so the light buffer flows pass->pass exactly as NVR.
//NoShadow seeds RenderedBuffer white (isShadow=1), Flashlight writes the raw
//light, the two box blurs soften the cone, Combine modulates it against scene
//brightness so it self-balances - no hand-tuned scale needed.
//
//runs post-vanilla on currentRTGroup (final tonemapped LDR sRGB feed, needed
//for NVRShadows coexistence). Combine linearizes the source and delinearizes
//on the way out, which is correct on an sRGB buffer.
//
//samplers kept at OURS' contiguous s0-s4 layout. NVR's s3 PointShadowBuffer
//and s5 ShadowSpotlightBuffer0 belong to the Shadows technique, which needs a
//spotlight shadow map we don't render - both techniques here are no-shadow.

//DEBUG: 0 = normal. 2 = show the raw half-res volumetric buffer (diagnostic).
#define FL_DEBUG 0

//runtime tuning from the INI (hot-reloadable). x = spot size scale (lower =
//smaller pool; tightens cone cutoff + pulls the cookie edge in together), y =
//pre-tonemapping intensity (lower = subtler; the game's auto-exposure amplifies
//the HDR light we inject, so this damps the blowout/outer-ring lobes).
float4 TESR_FlashLightTuning;
#define FL_SIZE      TESR_FlashLightTuning.x
#define FL_INTENSITY TESR_FlashLightTuning.y
#define FL_NEARFADE  TESR_FlashLightTuning.z   // fade distance (units): dims the pool point-blank so it doesn't clip to white
#define FL_SOFTEDGE  TESR_FlashLightTuning.w   // 1 = wrap-lambert: lifts dark foliage rims (flatter beam) ; 0 = plain lambert

float4x4 TESR_FlashLightViewProjTransform;
float4   TESR_SpotLightPosition;
float4   TESR_SpotLightColor;
float4   TESR_SpotLightDirection;
float4   TESR_ReciprocalResolution;
float4   TESR_VolumetricControl;   // x = strength (0 = off, also the Combine gate)
float4   TESR_FlashLightHotspot;   // x = hotspot reduction strength; y = cookie/breakup strength
#define FL_COOKIESTRENGTH TESR_FlashLightHotspot.y

sampler2D TESR_SourceBuffer      : register(s0) = sampler_state { ADDRESSU = CLAMP; ADDRESSV = CLAMP; MAGFILTER = LINEAR; MINFILTER = LINEAR; MIPFILTER = LINEAR; };
sampler2D TESR_RenderedBuffer    : register(s1) = sampler_state { ADDRESSU = CLAMP; ADDRESSV = CLAMP; MAGFILTER = LINEAR; MINFILTER = LINEAR; MIPFILTER = LINEAR; };
sampler2D TESR_DepthBuffer       : register(s2) = sampler_state { ADDRESSU = CLAMP; ADDRESSV = CLAMP; MAGFILTER = LINEAR; MINFILTER = ANISOTROPIC; MIPFILTER = LINEAR; };
sampler2D TESR_NormalsBuffer     : register(s3) = sampler_state { ADDRESSU = CLAMP; ADDRESSV = CLAMP; MAGFILTER = LINEAR; MINFILTER = LINEAR; MIPFILTER = LINEAR; };
sampler2D TESR_SpotLightTexture  : register(s4) < string ResourceName = "Effects\Flashlight.png"; > = sampler_state { ADDRESSU = CLAMP; ADDRESSV = CLAMP; MAGFILTER = LINEAR; MINFILTER = LINEAR; MIPFILTER = LINEAR; };
sampler2D TESR_VolumetricBuffer  : register(s5) = sampler_state { ADDRESSU = CLAMP; ADDRESSV = CLAMP; MAGFILTER = LINEAR; MINFILTER = LINEAR; MIPFILTER = NONE; };

#include "Includes/Depth.hlsl"
#include "Includes/Helpers.hlsl"
#include "Includes/Normals.hlsl"


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


//seeds RenderedBuffer with isShadow=1 so the no-shadow technique lights everything
float4 NoShadow(VSOUT IN) : COLOR0 {
    return float4(1, 1, 1, 1);
}

float BeamBreakup(float2 uv) {
    if (FL_COOKIESTRENGTH <= 0.001)
        return 1.0;

    float2 p = uv - 0.5;
    float r = saturate(length(p) * 2.0);

    float warp = sin(uv.x * 17.0 + uv.y * 5.5) * 0.45 + sin(uv.x * -6.5 + uv.y * 19.0) * 0.35;
    float rings = sin(r * 36.0 + warp) * 0.5 + 0.5;
    float mottled = sin(uv.x * 41.0 + sin(uv.y * 13.0) * 2.2) * sin(uv.y * 29.0 + sin(uv.x * 11.0) * 1.7);
    mottled = mottled * 0.5 + 0.5;

    float edge = smoothstep(0.25, 0.95, r);
    float breakup = 1.0 + (rings - 0.5) * (0.10 + 0.14 * edge) + (mottled - 0.5) * 0.12;
    breakup -= edge * (0.04 + 0.04 * mottled);
    return lerp(1.0, saturate(breakup), saturate(FL_COOKIESTRENGTH));
}

float4 Flashlight(VSOUT IN) : COLOR0 {
    float depth = readDepth(IN.UVCoord);
    float3 eyeVector = toWorld(IN.UVCoord);
    float3 eyeDirection = normalize(eyeVector);
    float3 worldPos = TESR_CameraPosition.xyz + eyeVector * depth;
    float3 normal = GetWorldNormal(IN.UVCoord);

    float3 lightpos = TESR_SpotLightPosition.xyz;
    float3 lightDir = TESR_SpotLightDirection.xyz;
    float3 lightToWorld = lightpos - worldPos;
    float3 lightVector = normalize(lightToWorld);

    //soft-edge (wrap-lambert) lifts the dark rims that bad depth-reconstructed normals
    //produce on alpha foliage edges - at the cost of flatter contrast across the beam.
    float ndl = dot(lightVector, normal);
    float diffuse = (FL_SOFTEDGE > 0.5) ? saturate(ndl * 0.5 + 0.5) : max(ndl, 0.0);
    float specular = pows(shades(normalize(eyeDirection + lightVector * -1), normal), 5);

    float radius = TESR_SpotLightPosition.w;
    float Distance = length(lightToWorld) / radius;
    float s = saturate(Distance * Distance);
    float atten = saturate(((1 - s) * (1 - s)) / (1 + 5.0 * s));

    float4 lightSpaceCoord = ScreenCoordToTexCoord(mul(float4(worldPos, 1), TESR_FlashLightViewProjTransform));
    float isShadow = tex2D(TESR_RenderedBuffer, IN.UVCoord);
    //zoom the cookie out around its centre so its dark edge lands at the tighter cone cutoff
    float2 cookieUV = (lightSpaceCoord.xy - 0.5) / max(0.05, FL_SIZE) + 0.5;
    float lightTexture = tex2D(TESR_SpotLightTexture, cookieUV).r * BeamBreakup(cookieUV);

    float3 lightColor = TESR_SpotLightColor.rgb * TESR_SpotLightColor.w;

    float angleCosMax = cos(radians(TESR_SpotLightDirection.w * FL_SIZE));
    float angleCosMin = cos(radians(TESR_SpotLightDirection.w * FL_SIZE * 0.5));
    float cone = pow(invlerps(angleCosMax, angleCosMin, shades(lightDir, lightVector * -1)), 2.0);

    //near-distance fade: ramp the pool down point-blank so a wall a foot away
    //doesn't clip to white. full brightness once past FL_NEARFADE units.
    float nearFade = (FL_NEARFADE > 0.0) ? smoothstep(0.0, FL_NEARFADE, length(lightToWorld)) : 1.0;

    float3 light = (diffuse + specular) * lightColor * cone * atten * lightTexture * isShadow * nearFade;

#if FL_DEBUG == 1
    //show the projected cookie alone, masked to the cone so we see where it lands.
    //if the gobo (bright center + ring + crisp edge) appears on the ground -> cookie
    //works and the gap is brightness. if it's a flat wash -> cookie projection is wrong.
    float cookieDbg = lightTexture * (cone > 0.0 ? 1.0 : 0.0);
    return float4(cookieDbg.xxx, 1);
#endif

    return float4(light, 1);
}


float4 BoxBlurMin(VSOUT IN, uniform sampler2D buffer, uniform float scaleFactor) : COLOR0 {
    float2 io = float2(-1, 1) * 0.5;
    clip((IN.UVCoord <= scaleFactor) - 1);

    float2 maxuv = scaleFactor - 1.5 * TESR_ReciprocalResolution.xy;
    float4 color = tex2D(buffer, IN.UVCoord);
    color = min(color, tex2D(buffer, min(IN.UVCoord + io.xx * TESR_ReciprocalResolution.xy, maxuv)));
    color = min(color, tex2D(buffer, min(IN.UVCoord + io.xy * TESR_ReciprocalResolution.xy, maxuv)));
    color = min(color, tex2D(buffer, min(IN.UVCoord + io.yx * TESR_ReciprocalResolution.xy, maxuv)));
    color = min(color, tex2D(buffer, min(IN.UVCoord + io.yy * TESR_ReciprocalResolution.xy, maxuv)));
    return color;
}


float4 BoxBlurAvg(VSOUT IN, uniform sampler2D buffer, uniform float scaleFactor) : COLOR0 {
    float2 io = float2(-1, 1) * 0.5;
    clip((IN.UVCoord <= scaleFactor) - 1);

    float2 maxuv = scaleFactor - 1.5 * TESR_ReciprocalResolution.xy;
    float4 color = tex2D(buffer, IN.UVCoord);
    color += tex2D(buffer, min(IN.UVCoord + io.xx * TESR_ReciprocalResolution.xy, maxuv));
    color += tex2D(buffer, min(IN.UVCoord + io.xy * TESR_ReciprocalResolution.xy, maxuv));
    color += tex2D(buffer, min(IN.UVCoord + io.yx * TESR_ReciprocalResolution.xy, maxuv));
    color += tex2D(buffer, min(IN.UVCoord + io.yy * TESR_ReciprocalResolution.xy, maxuv));
    return float4(color.rgb /= 5, 1);
}


//depth-aware bilinear upsample of the half-res volumetric buffer: 4 taps weighted
//by scene-depth similarity so the beam doesn't bleed across geometry silhouettes
float3 UpsampleVolumetric(float2 uv) {
    float2 texel = TESR_ReciprocalResolution.xy * 2.0;   // half-res texel in full-res UV
    float centerDepth = readDepth(uv);
    float2 o[4] = { float2(-0.5,-0.5), float2(0.5,-0.5), float2(-0.5,0.5), float2(0.5,0.5) };
    float3 sum = 0; float wsum = 0;
    [unroll]
    for (int i = 0; i < 4; i++) {
        float2 tuv = uv + o[i] * texel;
        float3 v = tex2D(TESR_VolumetricBuffer, tuv).rgb;
        float w = 1.0 / (1e-3 + abs(readDepth(tuv) - centerDepth));
        sum += v * w; wsum += w;
    }
    return sum / max(wsum, 1e-3);
}

float4 Combine(VSOUT IN) : COLOR0 {
#if FL_DEBUG == 1
    //show the projected cookie (blurred) directly so its shape is visible
    return float4(tex2D(TESR_RenderedBuffer, IN.UVCoord).rgb, 1);
#endif
#if FL_DEBUG == 2
    //show the half-res volumetric buffer (amplified). black = march produced nothing;
    //a cone shape = march works and the composite/magnitude is the issue.
    return float4(tex2D(TESR_VolumetricBuffer, IN.UVCoord).rgb * 30.0, 1);
#endif

    //TESR_SourceBuffer here is the game's HDR scene buffer, which is already
    //linear (we run pre-tonemapping). do NOT sRGB-linearize/delinearize it - that
    //double-transform on linear HDR is what produced garbage in earlier attempts.
    //add the light in linear space; vanilla tonemapping rolls it into the frame.
    float4 color = tex2D(TESR_SourceBuffer, IN.UVCoord);
    float4 light = tex2D(TESR_RenderedBuffer, IN.UVCoord);

    float3 addLight = color.rgb * max(0.0, luma(exp(-color.rgb * 3.5)) * light.rgb) * FL_INTENSITY;

    //add the volumetric in-scatter (gated so a stale half-res buffer never bleeds
    //when volumetric is disabled). direct sample - the s5 sampler is bilinear so it
    //upsamples the half-res buffer for free.
    if (TESR_VolumetricControl.x > 0.0) {
        float3 vol = tex2D(TESR_VolumetricBuffer, IN.UVCoord).rgb;
        //the shaft yields where this pixel's SURFACE sits inside the beam - the landing
        //already gets the surface pool, and stacking the air glow on it reads as a
        //hotter cast. geometric (cone x attenuation at the surface), so it holds at any
        //shaft strength. surfaces outside the beam keep the full shaft against them
        float3 surfPos = TESR_CameraPosition.xyz + toWorld(IN.UVCoord) * readDepth(IN.UVCoord);
        float3 surfToL = TESR_SpotLightPosition.xyz - surfPos;
        float  surfDist = length(surfToL);
        float surfCone = pow(invlerps(
            cos(radians(TESR_SpotLightDirection.w * FL_SIZE)),
            cos(radians(TESR_SpotLightDirection.w * FL_SIZE * 0.5)),
            shades(TESR_SpotLightDirection.xyz, -surfToL / max(surfDist, 0.001))), 2.0);
        float sSurf = saturate(sqr(surfDist / TESR_SpotLightPosition.w));
        float surfAtten = saturate(sqr(1.0 - sSurf) / (1.0 + 5.0 * sSurf));
        vol *= 1.0 - 0.85 * saturate(surfCone * surfAtten * 4.0);
        //soft ceiling: rays looking straight down the beam integrate the whole lit
        //column and would pile into a screen-filling bloom - dim shafts pass nearly
        //unchanged, bright columns roll off toward ~0.4
        vol /= 1.0 + luma(vol) * 2.5;
        addLight += vol;
    }

    float reduction = TESR_FlashLightHotspot.x;
    if (reduction > 0.0) {
        float limit = 1.0 / reduction;
        float addLum = luma(addLight);
        addLight *= limit / max(limit, addLum);
    }
    color.rgb += addLight;

    return color;
}


technique {
    pass { VertexShader = compile vs_3_0 FrameVS(); PixelShader = compile ps_3_0 NoShadow(); }
    pass { VertexShader = compile vs_3_0 FrameVS(); PixelShader = compile ps_3_0 Flashlight(); }
    pass { VertexShader = compile vs_3_0 FrameVS(); PixelShader = compile ps_3_0 BoxBlurMin(TESR_RenderedBuffer, 1.0); }
    pass { VertexShader = compile vs_3_0 FrameVS(); PixelShader = compile ps_3_0 BoxBlurAvg(TESR_RenderedBuffer, 1.0); }
    pass { VertexShader = compile vs_3_0 FrameVS(); PixelShader = compile ps_3_0 Combine(); }
}

technique {
    pass { VertexShader = compile vs_3_0 FrameVS(); PixelShader = compile ps_3_0 NoShadow(); }
    pass { VertexShader = compile vs_3_0 FrameVS(); PixelShader = compile ps_3_0 Flashlight(); }
    pass { VertexShader = compile vs_3_0 FrameVS(); PixelShader = compile ps_3_0 BoxBlurMin(TESR_RenderedBuffer, 1.0); }
    pass { VertexShader = compile vs_3_0 FrameVS(); PixelShader = compile ps_3_0 BoxBlurAvg(TESR_RenderedBuffer, 1.0); }
    pass { VertexShader = compile vs_3_0 FrameVS(); PixelShader = compile ps_3_0 Combine(); }
}
