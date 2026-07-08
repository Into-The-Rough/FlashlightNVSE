#pragma once

#define ShadersPath "Data\\Shaders\\BetterFlashlightNVSE\\Shaders\\"
#define EffectsPath "Data\\Shaders\\BetterFlashlightNVSE\\Effects\\"
#define IniPath "Data\\config\\BetterFlashlightNVSE.ini"
#define RenderStateArgs 0, 0

#define SamplerStatesMax 12
#define SpotLightsMax 1
#define TrackedLightsMax 1
#define ShadowCubeMapsMax 1

//unused weather strings kept so NVR headers compile
static const char* WeatherColorTypes[TESWeather::kNumColorTypes] = { "SkyUpper", "Fog", "CloudsLower", "Ambient", "Sunlight", "Sun", "Stars", "SkyLower", "Horizon", "CloudsUpper" };
static const char* WeatherTimesOfDay[TESWeather::kNumTimeOfDay] = { "Sunrise", "Day", "Sunset", "Night" };
