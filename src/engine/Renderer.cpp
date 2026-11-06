#include "WeaponRigConfig.hpp"
#include "Renderer.hpp"
#include "ProceduralMesh.hpp"
#include "Texture.hpp"
#include "game/World.hpp"
#include "game/Player.hpp"
#include "game/FrontlineAI.hpp"
#include "game/PunctureSystem.hpp"
#include "Network.hpp"
#include <iostream>

namespace Mercenary {

static const char* s_standardVert = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;
layout(location = 3) in vec4 aColor;

uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat4 uView;
uniform mat4 uLightSpaceMatrix;
uniform float uTime;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vViewNormal;
out vec2 vTexCoords;
out vec4 vColor;
out vec4 vFragPosLightSpace;

void main() {
    vec3 animatedPos = aPos;
    // GPU Foliage wind sway for 3D grass blades (TexCoords.x == 5.0)
    if (aTexCoords.x > 4.5 && aTexCoords.x < 5.5) {
        float heightFactor = clamp(aTexCoords.y, 0.0, 1.0);
        float wind = sin(uTime * 2.8 + aPos.x * 0.45 + aPos.z * 0.35) * 0.085 * heightFactor;
        animatedPos.x += wind;
        animatedPos.z += wind * 0.45;
    }

    vec4 worldPos = uModel * vec4(animatedPos, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vViewNormal = mat3(uView) * vNormal;
    vTexCoords = aTexCoords;
    vColor = aColor;
    vFragPosLightSpace = uLightSpaceMatrix * worldPos;
    gl_Position = uViewProj * worldPos;
}
)";

static const char* s_standardFrag = R"(
#version 450 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vViewNormal;
in vec2 vTexCoords;
in vec4 vColor;
in vec4 vFragPosLightSpace;

uniform vec4 uTint;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform float uAmbient;
uniform float uDiffuse;
uniform float uTime;
uniform vec3 uCameraPos;
uniform mat4 uView;
uniform sampler2D uShadowMap;
uniform int uUseTexture;
uniform int uIsTerrain;
uniform sampler2D uAlbedoMap;
uniform sampler2D uNormalMap;
uniform sampler2D uRoadAlbedo;
uniform sampler2D uRoadNormal;
uniform sampler2D uGrassAlbedo;
uniform sampler2D uGrassNormal;
uniform sampler2D uConcreteAlbedo;
uniform sampler2D uConcreteNormal;
uniform int uIsTrainingMap;

uniform int uPunctureCount;
uniform vec4 uPuncturePosR[64];
uniform vec4 uPunctureRayDir[64];
uniform vec4 uPunctureMatSeed[64];
uniform vec4 uPunctureNorm[64];

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 NormalColor;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = 3.14159265 * denom * denom;
    return num / max(denom, 0.000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// High-fidelity 16-tap Poisson-disk PCF shadow filtering with adaptive normal-offset bias
float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;
    }

    float currentDepth = projCoords.z;
    float cosTheta = max(dot(normal, -lightDir), 0.0);
    // Dynamic normal-offset bias eliminates acne while keeping tight contact
    float bias = max(0.0012 * (1.0 - cosTheta), 0.00035);

    vec2 texelSize = 1.0 / textureSize(uShadowMap, 0);

    const vec2 poissonDisk[16] = vec2[](
        vec2(-0.94201624, -0.39906216),
        vec2( 0.94558609, -0.76890725),
        vec2(-0.09418410, -0.92938870),
        vec2( 0.34495938,  0.29387760),
        vec2(-0.91588581,  0.45771432),
        vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543,  0.27676845),
        vec2( 0.97484398,  0.75648377),
        vec2( 0.44323325, -0.97511554),
        vec2( 0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023),
        vec2( 0.79197514,  0.19090160),
        vec2(-0.24188840,  0.99706507),
        vec2(-0.81409955,  0.91437590),
        vec2( 0.19984126,  0.78641367),
        vec2( 0.14383161, -0.14100790)
    );

    float shadow = 0.0;
    float filterRadius = 2.2;
    for (int i = 0; i < 16; ++i) {
        float pcfDepth = texture(uShadowMap, projCoords.xy + poissonDisk[i] * texelSize * filterRadius).r;
        shadow += (currentDepth - bias > pcfDepth) ? 1.0 : 0.0;
    }
    return shadow / 16.0;
}

void main() {
    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing) N = -N;
    vec4 albedo4 = vColor * uTint;
    float tag = vColor.a;
    float roughness = 0.76;
    float metallic = 0.0;
    bool isSkin = false;

    // 3D Physical Surface Deformation & Through-Hole Asset Carving
    if (uPunctureCount > 0) {
        for (int p = 0; p < uPunctureCount; ++p) {
            vec3 pPos = uPuncturePosR[p].xyz;
            float rInner = uPuncturePosR[p].w;
            vec3 pRayDir = uPunctureRayDir[p].xyz;
            float rOuter = uPunctureRayDir[p].w;
            int surfType = int(uPunctureMatSeed[p].x + 0.5);
            bool isThrough = (uPunctureMatSeed[p].y > 0.5);

            vec3 delta = vWorldPos - pPos;
            float distAlongRay = dot(delta, pRayDir);
            if (abs(distAlongRay) < 0.25) {
                float dPerp = length(delta - distAlongRay * pRayDir);
                if (isThrough && dPerp < rInner) {
                    discard; // Physical through-hole: clean see-through!
                } else if (dPerp < rOuter) {
                    float t = clamp((dPerp - rInner) / max(0.001, (rOuter - rInner)), 0.0, 1.0);
                    vec3 scorchColor = (surfType == 1) ? vec3(0.08, 0.08, 0.09) : ((surfType == 2) ? vec3(0.18, 0.12, 0.07) : vec3(0.12, 0.11, 0.10));
                    albedo4.rgb = mix(scorchColor, albedo4.rgb, smoothstep(0.0, 1.0, t));
                    if (surfType == 1) metallic = mix(0.90, metallic, t);
                }
            }
        }
    }

    if (uIsTerrain == 2) {
        // TDM Industrial Tactical Yard Floor: Solid concrete slabs with asphalt/gravel wear
        vec2 uvConc = vWorldPos.xz * 0.16; // ~6m slab repeat
        vec2 uvMacro = vWorldPos.xz * 0.03;
        vec4 concCol = mix(texture(uConcreteAlbedo, uvConc), texture(uConcreteAlbedo, uvMacro), 0.25);
        vec3 concN   = texture(uConcreteNormal, uvConc).xyz * 2.0 - 1.0;

        vec4 roadCol = texture(uRoadAlbedo, vWorldPos.xz * 0.22);
        vec3 roadN   = texture(uRoadNormal, vWorldPos.xz * 0.22).xyz * 2.0 - 1.0;

        // Subtle expansion joints / slab seams
        vec2 fracUV = fract(uvConc);
        float seam = smoothstep(0.025, 0.0, fracUV.x) + smoothstep(0.975, 1.0, fracUV.x) +
                     smoothstep(0.025, 0.0, fracUV.y) + smoothstep(0.975, 1.0, fracUV.y);

        // Perimeter wear & center tactical markings
        float centerDist = length(vWorldPos.xz);
        float wear = smoothstep(15.0, 55.0, centerDist) * 0.35;

        vec3 yardAlbedo = mix(concCol.rgb * 1.05, roadCol.rgb * 0.85, wear);
        yardAlbedo *= (1.0 - clamp(seam, 0.0, 1.0) * 0.25);

        vec3 blendedMapNorm = mix(concN, roadN, wear);
        blendedMapNorm.xy *= 0.65;

        vec3 upVec = abs(N.y) > 0.92 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
        vec3 T = normalize(cross(upVec, N));
        vec3 B = cross(N, T);
        mat3 TBN = mat3(T, B, N);
        N = normalize(TBN * blendedMapNorm);

        albedo4.rgb = yardAlbedo;
        roughness = 0.68;
        metallic = 0.02;
    } else if (uIsTerrain == 1) {
        // Multi-octave texture sampling for razor-sharp micro-detail and broad landscape realism
        vec2 uvMicro = vWorldPos.xz * 0.35;  // ~2.8m micro-detail (pebbles, soil grains, grass thatch)
        vec2 uvMacro = vWorldPos.xz * 0.035; // ~28m macro landscape variation

        vec4 grassCol = mix(texture(uGrassAlbedo, uvMicro), texture(uGrassAlbedo, uvMacro), 0.28);
        vec3 grassN   = texture(uGrassNormal, uvMicro).xyz * 2.0 - 1.0;

        vec4 roadCol  = mix(texture(uRoadAlbedo, uvMicro), texture(uRoadAlbedo, uvMacro), 0.25);
        vec3 roadN    = texture(uRoadNormal, uvMicro).xyz * 2.0 - 1.0;

        vec4 mudCol   = mix(texture(uAlbedoMap, uvMicro), texture(uAlbedoMap, uvMacro), 0.28);
        vec3 mudN     = texture(uNormalMap, uvMicro).xyz * 2.0 - 1.0;

        if (uIsTrainingMap == 1) {
            vec2 uvConc = vWorldPos.xz * 0.12; // ~8m concrete slab repeat
            vec4 concCol = texture(uConcreteAlbedo, uvConc);
            vec3 concN   = texture(uConcreteNormal, uvConc).xyz * 2.0 - 1.0;

            // 1. Structural Concrete Slabs for Firing Pavilion, Armory, and Killhouse Foundation
            // Firing Pavilion Slab: X in [14, 25], Z in [-22, 5]
            float pavSlab = max(max(vWorldPos.x - 25.0, 14.0 - vWorldPos.x), max(vWorldPos.z - 5.0, -22.0 - vWorldPos.z));
            // FOB Arsenal Slab: X in [-16, 2], Z in [-10, 10]
            float hqSlab = max(max(vWorldPos.x - 2.0, -16.0 - vWorldPos.x), max(vWorldPos.z - 10.0, -10.0 - vWorldPos.z));
            // CQB Killhouse Foundation: X in [-28, 4], Z in [34, 76]
            float cqbSlab = max(max(vWorldPos.x - 4.0, -28.0 - vWorldPos.x), max(vWorldPos.z - 76.0, 34.0 - vWorldPos.z));

            float minPadDist = min(min(pavSlab, hqSlab), cqbSlab);
            float padMask = 1.0 - smoothstep(-0.4, 0.5, minPadDist);

            // 2. Natural Compacted Gravel Access Roads Connecting Military Facilities
            // Spine Road: X around [-2, 2], Z from -20 to 80
            float spineRoad = max(abs(vWorldPos.x) - 2.8, max(vWorldPos.z - 80.0, -22.0 - vWorldPos.z));
            // Range Road to Pavilion: Z around [-8, -4], X from 0 to 18
            float rangeRoad = max(abs(vWorldPos.z - (-6.0)) - 2.2, max(vWorldPos.x - 18.0, -2.0 - vWorldPos.x));
            // Drill yard path: Z around [12, 16], X from -35 to -2
            float drillRoad = max(abs(vWorldPos.z - 14.0) - 2.2, max(vWorldPos.x - (-2.0), -38.0 - vWorldPos.x));

            float minRoadDist = min(min(spineRoad, rangeRoad), drillRoad);
            float roadMask = 1.0 - smoothstep(-0.6, 0.8, minRoadDist);

            vec3 groundAlbedo = grassCol.rgb;
            vec3 blendedMapNorm = grassN;
            roughness = 0.82;

            // Gravel / Compacted Soil Road Blend
            groundAlbedo = mix(groundAlbedo, roadCol.rgb * 0.82, roadMask);
            blendedMapNorm = mix(blendedMapNorm, roadN, roadMask);
            roughness = mix(roughness, 0.74, roadMask);

            // Subtle Concrete Seams
            vec2 fracUV = fract(uvConc);
            float seam = smoothstep(0.02, 0.0, fracUV.x) + smoothstep(0.98, 1.0, fracUV.x) +
                         smoothstep(0.02, 0.0, fracUV.y) + smoothstep(0.98, 1.0, fracUV.y);
            vec3 concFinal = concCol.rgb * (1.0 - clamp(seam, 0.0, 1.0) * 0.20);

            groundAlbedo = mix(groundAlbedo, concFinal, padMask);
            blendedMapNorm = mix(blendedMapNorm, concN, padMask);
            roughness = mix(roughness, 0.60, padMask);

            blendedMapNorm.xy *= 0.75;
            vec3 upVec = abs(N.y) > 0.92 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
            vec3 T = normalize(cross(upVec, N));
            vec3 B = cross(N, T);
            mat3 TBN = mat3(T, B, N);
            N = normalize(TBN * blendedMapNorm);

            albedo4.rgb = groundAlbedo;
        } else {
            // =================================================================
            // CONTINENTAL PROCEDURAL BIOME SPLATMAP (MMO SCALE: 2.6km x 3.8km)
            // =================================================================
            vec2 uvConc = vWorldPos.xz * 0.18;
            vec4 concCol = texture(uConcreteAlbedo, uvConc);
            vec3 concN   = texture(uConcreteNormal, uvConc).xyz * 2.0 - 1.0;

            // 1. River Siversky Meandering Waterway
            float riverCenterZ = sin(vWorldPos.x * 0.0032) * 38.0;
            float riverDist = abs(vWorldPos.z - riverCenterZ);
            float riverMudMask = 1.0 - smoothstep(14.0, 26.0, riverDist);
            float riverTrenchMask = (riverDist >= 26.0 && riverDist < 56.0) ? (1.0 - smoothstep(26.0, 56.0, riverDist)) : 0.0;

            // 2. M-04 Trans-Continental Arterial Highway (Z: -1750 to +1750)
            float hwyCenter = sin(vWorldPos.z * 0.0016) * 4.2;
            float distHwy = abs(vWorldPos.x - hwyCenter);
            float hwyEdge = 6.2;
            float hwyMask = 1.0 - smoothstep(hwyEdge - 0.4, hwyEdge + 0.8, distHwy);
            float yellowStripe = (distHwy < 0.18 && fract(vWorldPos.z * 0.14) < 0.65) ? 1.0 : 0.0;
            float whiteShoulder = (abs(distHwy - (hwyEdge - 0.65)) < 0.14) ? 1.0 : 0.0;

            // 3. City Zorya Metropolitan Grid (X: 140 to 860, Z: -860 to -140)
            bool inCity = (vWorldPos.x >= 140.0 && vWorldPos.x <= 860.0 && vWorldPos.z >= -860.0 && vWorldPos.z <= -140.0);
            float cityStreetMask = 0.0;
            float citySidewalkMask = 0.0;
            float cityPlazaMask = 0.0;
            float streetDist = 999.0;
            if (inCity) {
                float distAve = min(min(abs(vWorldPos.x - 260.0), abs(vWorldPos.x - 390.0)),
                                    min(abs(vWorldPos.x - 520.0), abs(vWorldPos.x - 650.0)));
                float zRel = vWorldPos.z - (-800.0);
                float crossDist = abs(fract(zRel / 68.0 + 0.5) - 0.5) * 68.0;
                streetDist = min(distAve, crossDist);
                cityStreetMask = 1.0 - smoothstep(5.2, 5.6, streetDist);
                citySidewalkMask = smoothstep(5.3, 5.6, streetDist) * (1.0 - smoothstep(8.4, 8.8, streetDist));
                if (vWorldPos.x >= 460.0 && vWorldPos.x <= 580.0 && vWorldPos.z >= -510.0 && vWorldPos.z <= -430.0) {
                    cityPlazaMask = 1.0;
                    cityStreetMask = 0.0;
                    citySidewalkMask = 0.0;
                }
            }

            // 4. Promyshlenny Industrial Hub (X: -860 to -140, Z: -860 to -140)
            bool inIndustry = (vWorldPos.x >= -860.0 && vWorldPos.x <= -140.0 && vWorldPos.z >= -860.0 && vWorldPos.z <= -140.0);
            float railBallastMask = 0.0;
            float industryTarmac = 0.0;
            if (inIndustry) {
                float distRail = min(abs(vWorldPos.x - (-660.0)), abs(vWorldPos.x - (-460.0)));
                railBallastMask = 1.0 - smoothstep(4.2, 6.2, distRail);
                industryTarmac = 0.65;
            }

            // 5. Allied & Crimson Citadel Staging Aprons
            float distFOB_A = max(abs(vWorldPos.x) - 125.0, max(vWorldPos.z - (-1340.0), -1610.0 - vWorldPos.z));
            float distFOB_B = max(abs(vWorldPos.x) - 125.0, max(vWorldPos.z - 1610.0, 1340.0 - vWorldPos.z));
            float fobMask = max(1.0 - smoothstep(-0.5, 1.2, distFOB_A), 1.0 - smoothstep(-0.5, 1.2, distFOB_B));

            // 6. Slope Mountain Rock Face (Cliff Bluffs)
            float slope = 1.0 - N.y;
            float rockMask = smoothstep(0.18, 0.38, slope);

            // ================= COMPOSITION & MULTI-LAYER BLENDING =================
            vec3 groundAlbedo = grassCol.rgb;
            vec3 blendedMapNorm = grassN;
            roughness = 0.82;

            // Mud & River silt
            groundAlbedo = mix(groundAlbedo, mudCol.rgb * 0.85, riverMudMask * 0.95);
            blendedMapNorm = mix(blendedMapNorm, mudN, riverMudMask * 0.95);
            roughness = mix(roughness, 0.38, riverMudMask * 0.85);

            // Frontline trench berms
            groundAlbedo = mix(groundAlbedo, mudCol.rgb * 0.95, riverTrenchMask * 0.60);
            blendedMapNorm = mix(blendedMapNorm, mudN, riverTrenchMask * 0.60);

            // Mountain crags
            groundAlbedo = mix(groundAlbedo, roadCol.rgb * 0.60, rockMask);
            blendedMapNorm = mix(blendedMapNorm, roadN, rockMask);
            roughness = mix(roughness, 0.70, rockMask);

            // M-04 Highway
            vec3 asphaltCol = roadCol.rgb * 0.60;
            asphaltCol = mix(asphaltCol, vec3(0.92, 0.80, 0.16), yellowStripe * 0.85);
            asphaltCol = mix(asphaltCol, vec3(0.88, 0.88, 0.88), whiteShoulder * 0.85);
            groundAlbedo = mix(groundAlbedo, asphaltCol, hwyMask);
            blendedMapNorm = mix(blendedMapNorm, roadN, hwyMask);
            roughness = mix(roughness, 0.64, hwyMask);

            // Industrial Tarmac & Rail Ballast
            if (inIndustry) {
                groundAlbedo = mix(groundAlbedo, roadCol.rgb * 0.72, industryTarmac);
                vec3 ballastCol = mudCol.rgb * 0.45;
                if (fract(vWorldPos.z * 1.35) < 0.32) ballastCol += vec3(0.15, 0.10, 0.06);
                groundAlbedo = mix(groundAlbedo, ballastCol, railBallastMask);
                blendedMapNorm = mix(blendedMapNorm, roadN, max(industryTarmac, railBallastMask));
            }

            // City Zorya Streets & Concrete Sidewalks
            if (inCity) {
                vec2 fracUV = fract(uvConc);
                float seam = smoothstep(0.035, 0.0, fracUV.x) + smoothstep(0.965, 1.0, fracUV.x) +
                             smoothstep(0.035, 0.0, fracUV.y) + smoothstep(0.965, 1.0, fracUV.y);
                vec3 sidewalkCol = mix(vec3(0.68, 0.68, 0.66), concCol.rgb * 1.25, 0.65);
                sidewalkCol *= (1.0 - clamp(seam, 0.0, 1.0) * 0.35); // Concrete sidewalk slabs

                vec3 cityStreetCol = mix(roadCol.rgb * 0.45, vec3(0.18, 0.18, 0.20), 0.50);
                float curbLine = smoothstep(0.25, 0.0, abs(streetDist - 5.5));
                cityStreetCol = mix(cityStreetCol, vec3(0.78, 0.78, 0.75), curbLine * 0.65);

                groundAlbedo = mix(groundAlbedo, cityStreetCol, cityStreetMask);
                groundAlbedo = mix(groundAlbedo, sidewalkCol, citySidewalkMask);
                groundAlbedo = mix(groundAlbedo, sidewalkCol * 1.08, cityPlazaMask);

                blendedMapNorm = mix(blendedMapNorm, roadN, cityStreetMask);
                blendedMapNorm = mix(blendedMapNorm, concN, max(citySidewalkMask, cityPlazaMask));
                roughness = mix(roughness, 0.58, max(cityStreetMask, max(citySidewalkMask, cityPlazaMask)));
            }

            // Citadel Bastion Aprons
            groundAlbedo = mix(groundAlbedo, roadCol.rgb * 0.68, fobMask);
            blendedMapNorm = mix(blendedMapNorm, roadN, fobMask);
            roughness = mix(roughness, 0.60, fobMask);

            blendedMapNorm.xy *= 0.75;
            vec3 upVec = abs(N.y) > 0.92 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
            vec3 T = normalize(cross(upVec, N));
            vec3 B = cross(N, T);
            mat3 TBN = mat3(T, B, N);
            N = normalize(TBN * blendedMapNorm);

            albedo4.rgb = groundAlbedo;
        }
    } else if (uUseTexture == 1) {
        // Tile architectural textures when UVs are in metric space (> 1.05)
        vec2 effUV = (vTexCoords.x > 1.05 || vTexCoords.y > 1.05) ? vTexCoords * 0.28 : vTexCoords;
        vec4 texCol = texture(uAlbedoMap, effUV);
        tag = vColor.a;
        if (tag > 0.935 && tag < 0.965) {
            // IFF Tactical Identification Armband: Vibrant solid saturated team color
            albedo4.rgb = mix(vColor.rgb, vColor.rgb * (texCol.r * 0.20 + 0.88), 0.22);
        } else if (vColor.r < 0.98 || vColor.g < 0.98 || vColor.b < 0.98) {
            albedo4.rgb = vColor.rgb * (texCol.rgb * 1.35);
        } else {
            albedo4.rgb *= texCol.rgb;
        }

        // Consistent orthonormal tangent space (T is horizontal, B is vertical)
        vec3 upVec = abs(N.y) > 0.92 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
        vec3 T = normalize(cross(upVec, N));
        vec3 B = cross(N, T);
        mat3 TBN = mat3(T, B, N);

        vec3 mapNorm = texture(uNormalMap, effUV).xyz * 2.0 - 1.0;
        mapNorm.xy *= 0.55; // Tactile micro-relief
        N = normalize(TBN * mapNorm);

        // Calibrated PBR material classification from vertex alpha tag:
        tag = vColor.a;
        if (tag > 0.95) {
            // Uniform Fabric (Crye MultiCam Ripstop)
            roughness = 0.74;
            metallic = 0.0;
        } else if (tag > 0.85) {
            // Cordura 500D Nylon Tactical Gear & Pouches
            roughness = 0.65;
            metallic = 0.02;
        } else if (tag > 0.75) {
            // Ballistic Polymer / Ops-Core Helmet Shell
            roughness = 0.38;
            metallic = 0.08;
        } else if (tag > 0.65) {
            // Tactical Hardware Metal / Wilcox NVG Shroud
            roughness = 0.22;
            metallic = 0.88;
        } else if (tag > 0.55) {
            // Combat Leather Boots & Tactical Gloves
            roughness = 0.48;
            metallic = 0.0;
        } else if (tag > 0.45) {
            // Human Skin with Subsurface Blood Scatter (SSS)
            roughness = 0.44;
            metallic = 0.0;
            isSkin = true;
        } else if (tag > 0.35) {
            // Smoked Polycarbonate Eyewear Lens
            roughness = 0.06;
            metallic = 0.22;
        } else {
            roughness = 0.60;
            metallic = 0.04;
        }
    } else {
        tag = vColor.a;
        if (tag > 0.10 && tag < 0.15) {
            // River Siversky Water Surface: Fine river chop and surface ripples
            vec2 wPos = vWorldPos.xz;
            float dWdx = (1.6 * cos(wPos.x * 1.6 + wPos.y * 0.8 + uTime * 3.4)
                        - 1.2 * cos(wPos.x * -1.2 + wPos.y * 1.8 + uTime * 2.6)
                        + 2.2 * 0.5 * cos(wPos.x * 2.2 - wPos.y * 1.4 + uTime * 4.2)) * 0.18;
            float dWdz = (0.8 * cos(wPos.x * 1.6 + wPos.y * 0.8 + uTime * 3.4)
                        + 1.8 * cos(wPos.x * -1.2 + wPos.y * 1.8 + uTime * 2.6)
                        - 1.4 * 0.5 * cos(wPos.x * 2.2 - wPos.y * 1.4 + uTime * 4.2)) * 0.18;
            N = normalize(vec3(-dWdx, 1.0, -dWdz));
            roughness = 0.08;
            metallic = 0.02;
            albedo4.rgb = vec3(0.03, 0.08, 0.10);
        } else if (tag > 0.65 && tag <= 0.75) {
            // Gun Metal Anodized Steel
            roughness = 0.24;
            metallic = 0.88;
        } else if (tag > 0.75 && tag <= 0.85) {
            // Tactical Polymer Furniture
            roughness = 0.44;
            metallic = 0.04;
        } else if (tag > 0.35 && tag <= 0.45) {
            // Coated Optical Reflex Glass
            roughness = 0.04;
            metallic = 0.18;
        } else if (tag > 0.45 && tag <= 0.55) {
            // Skin
            roughness = 0.44;
            metallic = 0.0;
            isSkin = true;
        } else {
            float matType = vTexCoords.x;
            isSkin = (matType > 0.75 && matType < 1.25 && vTexCoords.y > 90.0);
            bool isMetal = (matType > 1.75 && matType < 2.25);
            bool isPolymer = (matType > 2.75 && matType < 3.75);
            bool isGlass = (matType > 3.75 && matType < 4.5);
            bool isFoliage = (matType > 4.5 && matType < 5.5);

            if (isSkin) {
                roughness = 0.44;
                metallic = 0.0;
            } else if (isMetal) {
                roughness = 0.28;
                metallic = 0.88;
            } else if (isPolymer) {
                roughness = 0.42;
                metallic = 0.05;
            } else if (isGlass) {
                // River Water / Liquid Surface (dynamic scrolling ripples)
                vec2 waveUV1 = vWorldPos.xz * 0.15 + vec2(uTime * 0.05, uTime * 0.03);
                vec2 waveUV2 = vWorldPos.xz * 0.25 - vec2(uTime * 0.04, uTime * 0.06);
                vec3 waveN1 = texture(uNormalMap, waveUV1).xyz * 2.0 - 1.0;
                vec3 waveN2 = texture(uNormalMap, waveUV2).xyz * 2.0 - 1.0;
                vec3 waveNorm = normalize(waveN1 + waveN2);
                vec3 T = vec3(1.0, 0.0, 0.0);
                vec3 B = vec3(0.0, 0.0, 1.0);
                mat3 TBN = mat3(T, B, vec3(0.0, 1.0, 0.0));
                N = normalize(TBN * (waveNorm * vec3(0.35, 0.35, 1.0)));
                roughness = 0.07;
                metallic = 0.18;
            } else if (isFoliage) {
                roughness = 0.82;
                metallic = 0.0;
            } else {
                // Uniform fabric: if dark polymer belt or straps
                if (albedo4.r < 0.16 && albedo4.g < 0.16 && albedo4.b < 0.16) {
                    roughness = 0.50;
                }
            }
        }
    }

    vec3 albedo = albedo4.rgb;
    vec3 V = normalize(uCameraPos - vWorldPos);
    vec3 L = normalize(-uLightDir);
    vec3 H = normalize(V + L);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    // Hemispheric Sky Dome Fill Light (Foxhole / CoH3 morning golden hour)
    vec3 skyColor = vec3(0.55, 0.64, 0.76); // Atmospheric dawn sky dome
    vec3 groundColor = vec3(0.28, 0.24, 0.18); // Dark damp warzone earth bounce
    float hemi = clamp(N.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 ambient = mix(groundColor, skyColor, hemi) * albedo * uAmbient;

    // Real-Time Directional Shadow
    float shadow = calculateShadow(vFragPosLightSpace, N, uLightDir);

    // Subsurface Scattering (MetaHuman Skin Subdermal Blood Scatter)
    vec3 sss = vec3(0.0);
    if (isSkin) {
        float wrap = 0.38;
        float sssNdotL = max(0.0, (dot(N, L) + wrap) / (1.0 + wrap));
        vec3 bloodScatter = vec3(0.92, 0.32, 0.18) * albedo;
        sss = bloodScatter * uLightColor * sssNdotL * 0.45 * (1.0 - shadow * 0.60);
    }

    // Direct PBR Lighting (Cook-Torrance GGX)
    float NdotL = max(dot(N, L), 0.0);
    vec3 foliageTrans = vec3(0.0);
    if (vTexCoords.x > 4.5 && vTexCoords.x < 5.5) {
        // Soft wrap diffuse for two-sided thin grass blades
        NdotL = max(dot(N, L), 0.0) * 0.65 + 0.35;
        // Subtle natural sub-surface sun scatter through thin blades
        float sunScatter = max(0.0, dot(-V, L));
        foliageTrans = albedo * uLightColor * uDiffuse * (sunScatter * 0.08) * (1.0 - shadow * 0.5);
    }
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    vec3 specular     = numerator / denominator;
    if (vTexCoords.x > 4.5 && vTexCoords.x < 5.5) {
        specular *= 0.05; // Non-specular organic matte foliage
    }

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 direct = (kD * albedo + specular) * uLightColor * uDiffuse * NdotL * (1.0 - shadow * 0.85) + foliageTrans;
    vec3 finalRgb = ambient + direct + sss;

    // Dynamic Sky Reflection & Wave Caustics on River Siversky Water Surface
    if (tag > 0.10 && tag < 0.15) {
        vec3 R = reflect(-V, N);
        float sunSpec = pow(max(dot(R, L), 0.0), 64.0) * 1.8;
        float skyGrad = clamp(R.y * 0.65 + 0.35, 0.0, 1.0);
        vec3 deepWaterCol = vec3(0.03, 0.08, 0.11);
        vec3 skyRefl = mix(vec3(0.08, 0.16, 0.22), vec3(0.24, 0.42, 0.54), skyGrad);
        float waveChop = (sin(vWorldPos.x * 1.6 + vWorldPos.z * 0.8 + uTime * 3.4) +
                          sin(vWorldPos.x * -1.2 + vWorldPos.z * 1.8 + uTime * 2.6)) * 0.5;
        float waveGlint = pow(clamp(waveChop * 0.5 + 0.5, 0.0, 1.0), 4.0) * 0.08;
        float fresnelWater = pow(clamp(1.0 - max(dot(N, V), 0.0), 0.0, 1.0), 3.0);
        vec3 waterSurface = mix(deepWaterCol, skyRefl, clamp(0.30 + fresnelWater * 0.50, 0.0, 0.85));
        waterSurface += vec3(waveGlint) + uLightColor * sunSpec * (1.0 - shadow * 0.8);
        finalRgb = waterSurface;
    }

    // Atmospheric Battlefield Depth Fog (activates only on distant horizon, never in tactical view)
    float dist = length(uCameraPos - vWorldPos);
    float fogFactor = clamp((dist - 280.0) / 1050.0, 0.0, 0.85);
    vec3 fogColor = vec3(0.65, 0.72, 0.82) * 0.65; // Soft morning haze on horizon

    float outAlpha = 1.0;
    if (tag > 0.35 && tag < 0.45) {
        outAlpha = 0.45; // Smoked ballistic goggle lens
    }
    FragColor = vec4(mix(finalRgb, fogColor, fogFactor), outAlpha);
    NormalColor = vec4(normalize(mat3(uView) * N), roughness);
}
)";

static const char* s_lineVert = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;

uniform mat4 uViewProj;
out vec4 vColor;

void main() {
    vColor = aColor;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";

static const char* s_lineFrag = R"(
#version 450 core
in vec4 vColor;
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 NormalColor;

void main() {
    FragColor = vColor;
    NormalColor = vec4(0.0, 0.0, 1.0, 1.0);
}
)";

static const char* s_particleVert = R"(
#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoords;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aType;

uniform mat4 uViewProj;
out vec2 vUV;
out vec4 vColor;
out float vType;

void main() {
    vUV = aTexCoords;
    vColor = aColor;
    vType = aType;
    gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";

static const char* s_particleFrag = R"(
#version 450 core
in vec2 vUV;
in vec4 vColor;
in float vType;

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 NormalColor;

void main() {
    vec2 p = vUV * 2.0 - 1.0;
    float dist = length(p);

    if (vType < 0.5) {
        // Type 0: Volumetric Gunsmoke (soft billowing turbulent cloud)
        if (dist > 1.0) discard;
        float radial = 1.0 - dist;
        float angle = atan(p.y, p.x);
        float noise = sin(angle * 3.0 + dist * 5.0) * 0.12 + cos(angle * 5.0 - dist * 4.0) * 0.08;
        float alpha = clamp(radial * 1.4 + noise, 0.0, 1.0);
        alpha = pow(alpha, 1.6) * vColor.a;
        if (alpha < 0.005) discard;
        vec3 col = vColor.rgb * (0.88 + 0.12 * (1.0 - dist));
        FragColor = vec4(col, alpha);
        NormalColor = vec4(0.0, 1.0, 0.0, 0.8);
    } else if (vType < 1.5) {
        // Type 1: High-Fidelity Tactical Flash-Hider Petal Flame (Compensator Vents & Star Flame)
        if (dist > 1.0) discard;
        float angle = atan(p.y, p.x);

        // 4-prong compensator baffle vents with secondary harmonics
        float star4 = max(0.0, cos(angle * 4.0));
        star4 = pow(star4, 3.5) * 0.58;

        float star2 = max(0.0, cos(angle * 2.0));
        star2 = pow(star2, 4.0) * 0.28;

        // Chaotic combustion edge turbulence
        float flameNoise = sin(angle * 9.0 + dist * 14.0) * 0.09 + cos(angle * 15.0 - dist * 11.0) * 0.06;
        float flameRadius = 0.34 + star4 + star2 + flameNoise;

        if (dist > flameRadius) discard;

        float normDist = dist / max(0.001, flameRadius);
        float core = pow(clamp(1.0 - normDist, 0.0, 1.0), 1.6);

        // Radiance temperature ramp: White-Hot Core -> Lemon Gold -> Combustion Orange -> Crimson Rim
        vec3 whiteCore  = vec3(1.0, 0.98, 0.94) * 3.4;
        vec3 goldFlame  = vec3(1.0, 0.80, 0.22) * 2.4;
        vec3 amberFlame = vec3(0.98, 0.42, 0.05) * 1.8;
        vec3 redFlame   = vec3(0.68, 0.12, 0.02);

        vec3 fireCol;
        if (core > 0.60) {
            fireCol = mix(goldFlame, whiteCore, (core - 0.60) / 0.40);
        } else if (core > 0.25) {
            fireCol = mix(amberFlame, goldFlame, (core - 0.25) / 0.35);
        } else {
            fireCol = mix(redFlame, amberFlame, core / 0.25);
        }

        float alpha = pow(clamp(1.0 - normDist, 0.0, 1.0), 1.1) * vColor.a;
        if (alpha < 0.005) discard;

        FragColor = vec4(fireCol, alpha);
        NormalColor = vec4(0.0, 1.0, 0.0, 0.1);
    } else if (vType < 2.5) {
        // Type 2: Dirt Plume / Arterial Blood Mist (organic droplets)
        if (dist > 1.0) discard;
        float radial = 1.0 - dist;
        float alpha = pow(radial, 1.3) * vColor.a;
        if (alpha < 0.01) discard;
        FragColor = vec4(vColor.rgb, alpha);
        NormalColor = vec4(0.0, 1.0, 0.0, 0.6);
    } else if (vType < 3.5) {
        // Type 3: Dynamic Ground Flash Illumination Disc (horizontal projection)
        if (dist > 1.0) discard;
        float radial = pow(clamp(1.0 - dist, 0.0, 1.0), 2.2);
        float alpha = radial * vColor.a;
        if (alpha < 0.003) discard;
        FragColor = vec4(vColor.rgb * 1.6, alpha);
        NormalColor = vec4(0.0, 1.0, 0.0, 0.5);
    } else if (vType < 4.5) {
        // Type 4: Flying Powder Sparks / Burning Embers
        if (dist > 1.0) discard;
        float spark = pow(clamp(1.0 - dist, 0.0, 1.0), 1.8);
        FragColor = vec4(vColor.rgb * 2.2, spark * vColor.a);
        NormalColor = vec4(0.0, 1.0, 0.0, 0.2);
    } else if (vType < 5.5) {
        // Type 5: Supersonic Directional Flame Lance (Combustion Gas Needle)
        float lat = abs(vUV.x * 2.0 - 1.0); // 0 at center, 1 at edge
        float fwd = vUV.y;                  // 0 at muzzle, 1 at tip
        if (fwd < 0.0 || fwd > 1.0) discard;

        // Tapering flame cone with supersonic shock diamonds
        float coneWidth = (1.0 - fwd * 0.75) * 0.70 + 0.05;
        float shockDiamond = sin(fwd * 22.0) * 0.08 * (1.0 - fwd);
        coneWidth += shockDiamond;

        // Jagged combustion flame edge
        float edgeTurbulence = sin(fwd * 18.0 + lat * 12.0) * 0.08 * (1.0 - fwd * 0.5);
        float flameBoundary = coneWidth + edgeTurbulence;

        if (lat > flameBoundary) discard;

        float normLat = lat / max(0.001, flameBoundary);
        float core = (1.0 - normLat) * (1.0 - fwd * 0.65);
        float corePower = pow(clamp(core, 0.0, 1.0), 1.4);

        vec3 whiteHot    = vec3(1.0, 1.0, 0.96) * 3.6;
        vec3 lemonFlame  = vec3(1.0, 0.85, 0.24) * 2.5;
        vec3 orangeBody  = vec3(0.98, 0.42, 0.06) * 1.8;
        vec3 crimsonEdge = vec3(0.65, 0.10, 0.02);

        vec3 flameCol;
        if (corePower > 0.62) {
            flameCol = mix(lemonFlame, whiteHot, (corePower - 0.62) / 0.38);
        } else if (corePower > 0.28) {
            flameCol = mix(orangeBody, lemonFlame, (corePower - 0.28) / 0.34);
        } else {
            flameCol = mix(crimsonEdge, orangeBody, corePower / 0.28);
        }

        float alpha = corePower * vColor.a;
        if (alpha < 0.005) discard;

        FragColor = vec4(flameCol, alpha);
        NormalColor = vec4(0.0, 1.0, 0.0, 0.1);
    } else if (vType < 6.5) {
        // Type 6: Glowing Phosphorus Supersonic Bullet Tracer Ribbon
        float lat = abs(vUV.x * 2.0 - 1.0); // 0 at centerline, 1 at edge
        float fwd = vUV.y;                  // 0 at tail, 1 at projectile tip
        if (fwd < 0.0 || fwd > 1.0) discard;

        // Core white needle streak + soft glowing phosphorus outer envelope
        float core = pow(clamp(1.0 - lat, 0.0, 1.0), 3.0);
        float envelope = pow(clamp(1.0 - lat, 0.0, 1.0), 1.2);

        // Smooth aerodynamic tail taper & incandescent projectile tip
        float tailFade = smoothstep(0.0, 0.22, fwd);
        float tipGlow = smoothstep(0.85, 1.0, fwd);

        vec3 coreCol = vec3(1.0, 1.0, 0.95) * 3.8;
        vec3 bodyCol = vColor.rgb * 2.2;
        vec3 finalCol = mix(bodyCol, coreCol, core) + coreCol * (tipGlow * 0.8);

        float alpha = (core * 0.75 + envelope * 0.45) * tailFade * vColor.a;
        if (alpha < 0.005) discard;

        FragColor = vec4(finalCol, alpha);
        NormalColor = vec4(0.0, 1.0, 0.0, 0.1);
    } else if (vType < 7.5) {
        // Type 7: High-Detail Visceral Arterial Blood Pool / Splatter Decal
        // Radial organic perimeter with viscous coagulation & specular gloss
        if (dist > 1.0) discard;
        float angle = atan(p.y, p.x);
        float wobble = sin(angle * 7.0) * 0.08 + sin(angle * 13.0 + 1.2) * 0.05 + cos(angle * 3.0) * 0.12;
        float edge = clamp(0.85 + wobble, 0.45, 1.0);
        if (dist > edge) discard;

        // Dark clotted coagulated rim vs thick glossy pool center
        float depth = 1.0 - (dist / edge);
        float poolThickness = pow(clamp(depth, 0.0, 1.0), 0.45);
        vec3 freshBlood = vec3(0.58, 0.03, 0.03);
        vec3 clottedBlood = vec3(0.18, 0.01, 0.01);
        vec3 bloodColor = mix(clottedBlood, freshBlood, poolThickness);

        float alpha = clamp(poolThickness * 1.6, 0.0, 1.0) * vColor.a;
        FragColor = vec4(bloodColor, alpha);
        NormalColor = vec4(0.0, 1.0, 0.0, 0.95);
    } else {
        // Type 8: Tactical 3D Aiming Ray / In-World Barrel Sight Line (Crosshair Replacement)
        float lat = abs(vUV.x * 2.0 - 1.0); // 0 at centerline, 1 at edge
        float fwd = vUV.y;                  // 0 at muzzle, 1 at ray end
        if (fwd < 0.0 || fwd > 1.0) discard;

        // Smooth anti-aliased cross-sectional core with soft edge feathering
        float core = pow(clamp(1.0 - lat, 0.0, 1.0), 2.2);
        // Distance falloff: smooth start near muzzle, fades out cleanly as weapon range ends
        float distFade = clamp(1.0 - pow(fwd, 1.35), 0.0, 1.0);
        float muzzleSoft = smoothstep(0.0, 0.025, fwd);

        float alpha = core * distFade * muzzleSoft * vColor.a;
        if (alpha < 0.003) discard;

        FragColor = vec4(vColor.rgb, alpha);
        NormalColor = vec4(0.0, 1.0, 0.0, 0.1);
    }
}
)";

Renderer::Renderer() {}

Renderer::~Renderer() {
    if (m_lineVao != 0) glDeleteVertexArrays(1, &m_lineVao);
    if (m_lineVbo != 0) glDeleteBuffers(1, &m_lineVbo);
    if (m_particleVao != 0) glDeleteVertexArrays(1, &m_particleVao);
    if (m_particleVbo != 0) glDeleteBuffers(1, &m_particleVbo);
    if (m_concreteFloorTex != 0) { Texture::destroyTexture(m_concreteFloorTex); m_concreteFloorTex = 0; }
    if (m_concreteFloorNorm != 0) { Texture::destroyTexture(m_concreteFloorNorm); m_concreteFloorNorm = 0; }
    if (m_steelWallTex != 0) { Texture::destroyTexture(m_steelWallTex); m_steelWallTex = 0; }
    if (m_steelWallNorm != 0) { Texture::destroyTexture(m_steelWallNorm); m_steelWallNorm = 0; }
    if (m_terrainMudTex != 0) { Texture::destroyTexture(m_terrainMudTex); m_terrainMudTex = 0; }
    if (m_terrainMudNorm != 0) { Texture::destroyTexture(m_terrainMudNorm); m_terrainMudNorm = 0; }
    if (m_sandbagTex != 0) { Texture::destroyTexture(m_sandbagTex); m_sandbagTex = 0; }
    if (m_sandbagNorm != 0) { Texture::destroyTexture(m_sandbagNorm); m_sandbagNorm = 0; }
    if (m_terrainRoadTex != 0) { Texture::destroyTexture(m_terrainRoadTex); m_terrainRoadTex = 0; }
    if (m_terrainRoadNorm != 0) { Texture::destroyTexture(m_terrainRoadNorm); m_terrainRoadNorm = 0; }
    if (m_terrainGrassTex != 0) { Texture::destroyTexture(m_terrainGrassTex); m_terrainGrassTex = 0; }
    if (m_terrainGrassNorm != 0) { Texture::destroyTexture(m_terrainGrassNorm); m_terrainGrassNorm = 0; }
    if (m_hescoTex != 0) { Texture::destroyTexture(m_hescoTex); m_hescoTex = 0; }
    if (m_hescoNorm != 0) { Texture::destroyTexture(m_hescoNorm); m_hescoNorm = 0; }
    if (m_drumTex != 0) { Texture::destroyTexture(m_drumTex); m_drumTex = 0; }
    if (m_drumNorm != 0) { Texture::destroyTexture(m_drumNorm); m_drumNorm = 0; }
    if (m_soldierTex != 0) { Texture::destroyTexture(m_soldierTex); m_soldierTex = 0; }
    if (m_soldierNorm != 0) { Texture::destroyTexture(m_soldierNorm); m_soldierNorm = 0; }
    if (m_buildingTex != 0) { Texture::destroyTexture(m_buildingTex); m_buildingTex = 0; }
    if (m_buildingNorm != 0) { Texture::destroyTexture(m_buildingNorm); m_buildingNorm = 0; }
}

bool Renderer::init() {
    m_standardShader = std::make_unique<Shader>();
    if (!m_standardShader->loadFromSource(s_standardVert, s_standardFrag)) {
        std::cerr << "[Renderer Error] Failed to compile standard shader\n";
        return false;
    }

    m_lineShader = std::make_unique<Shader>();
    if (!m_lineShader->loadFromSource(s_lineVert, s_lineFrag)) {
        std::cerr << "[Renderer Error] Failed to compile line shader\n";
        return false;
    }

    m_particleShader = std::make_unique<Shader>();
    if (!m_particleShader->loadFromSource(s_particleVert, s_particleFrag)) {
        std::cerr << "[Renderer Error] Failed to compile particle shader\n";
        return false;
    }

    m_cubeMesh = Mesh::createCube();
    m_cylinderMesh = Mesh::createCylinder(0.5f, 1.0f, 24);
    // Multi-elevation high-relief procedural warzone terrain & lush 3D grass field
    m_groundMesh = ProceduralMesh::createWarzoneTerrain(2600.0f, 3800.0f, 260, 380);
    m_grassMesh = ProceduralMesh::createGrassField(1400.0f, 95000);
    World::setTrainingMapActive(true);
    m_trainingGroundMesh = ProceduralMesh::createTrainingTerrain(650.0f, 650.0f, 260);
    m_trainingGrassMesh = ProceduralMesh::createTrainingGrassField(320.0f, 45000);
    World::setTrainingMapActive(false);
    m_arenaGroundMesh = ProceduralMesh::createArenaTerrain(160.0f, 160.0f, 64);

    // Initialize Modular Urban, Industrial & Regional Architecture
    m_urbanApartmentMesh = ProceduralMesh::createUrbanApartment();
    m_industrialWarehouseMesh = ProceduralMesh::createIndustrialWarehouse();
    m_townChurchMesh = ProceduralMesh::createTownChurch();
    m_freightBoxcarMesh = ProceduralMesh::createFreightBoxcar();
    m_streetLampMesh = ProceduralMesh::createStreetLamp();
    m_commTowerMesh = ProceduralMesh::createCommunicationTower();
    m_refineryTankMesh = ProceduralMesh::createRefineryTank();
    m_shootingTargetMesh = ProceduralMesh::createShootingTarget();
    m_watchtowerMesh = ProceduralMesh::createWatchtower();
    m_guardBoothMesh = ProceduralMesh::createGuardBooth();
    m_perimeterFenceMesh = ProceduralMesh::createPerimeterFence();
    m_gunRackMesh = ProceduralMesh::createGunRack();
    m_obstacleWallMesh = ProceduralMesh::createObstacleWall();

    // Initialize High-Detail Procedural Meshes
    m_soldierMesh = ProceduralMesh::createDetailedSoldier(true);
    m_rifleMesh = ProceduralMesh::createDetailedRifle();
    m_ak74Mesh = ProceduralMesh::createAK74M();
    m_mosinMesh = ProceduralMesh::createMosinNagant();
    m_m9Mesh = ProceduralMesh::createBerettaM9();
    m_sandbagMesh = ProceduralMesh::createDetailedSandbagWall();
    m_crateMesh = ProceduralMesh::createDetailedCrate();
    m_pineTreeMesh = ProceduralMesh::createPineTree();
    m_dragonToothMesh = ProceduralMesh::createDragonTooth();
    m_barbedWireMesh = ProceduralMesh::createBarbedWire();
    m_tankWreckMesh = ProceduralMesh::createTankWreck();
    m_ruinedWallMesh = ProceduralMesh::createRuinedWall();
    m_bridgeMesh = ProceduralMesh::createRiverBridge();
    m_riverWaterMesh = ProceduralMesh::createRiverWater(2600.0f, 54.0f);

    // Military Outpost & Fortification Assets (Thunder Tier One / CoH3 caliber)
    m_hescoMesh = ProceduralMesh::createHescoBastion();
    m_commandTentMesh = ProceduralMesh::createCommandTent();
    m_fuelDrumMesh = ProceduralMesh::createFuelDrum();
    m_cargoPalletMesh = ProceduralMesh::createCargoPallet();
    m_generatorMesh = ProceduralMesh::createFieldGenerator();
    m_militaryTruckMesh = ProceduralMesh::createMilitaryTruck();
    m_damagedBuildingMesh = ProceduralMesh::createUrbanApartment();

    m_grassClumpMesh = ProceduralMesh::createGrassClump();
    m_bushFoliageMesh = ProceduralMesh::createBushFoliage();
    m_concreteBarrierMesh = ProceduralMesh::createConcreteBarrier();
    m_campfireSmokeMesh = ProceduralMesh::createCampfireSmoke();
    m_signalFlareMesh = ProceduralMesh::createSignalFlare();
    m_hmgMesh = ProceduralMesh::createMountedHMG();

    // Initialize Russian & Ukrainian Infantry across 4 roles and 3 LOD tiers
    for (int r = 0; r < 4; ++r) {
        CombatRole role = static_cast<CombatRole>(r);
        for (int l = 0; l < 3; ++l) {
            m_ruInfantry[r][l] = ProceduralMesh::createRussianInfantry(role, l);
            m_uaInfantry[r][l] = ProceduralMesh::createUkrainianInfantry(role, l);
        }
    }

    // Setup line dynamic buffer
    glGenVertexArrays(1, &m_lineVao);
    glGenBuffers(1, &m_lineVbo);

    glBindVertexArray(m_lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVbo);
    glBufferData(GL_ARRAY_BUFFER, 1024 * sizeof(float) * 7, nullptr, GL_DYNAMIC_DRAW);

    // Pos
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    // Color
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindVertexArray(0);

    // Setup particle dynamic buffer
    glGenVertexArrays(1, &m_particleVao);
    glGenBuffers(1, &m_particleVbo);
    glBindVertexArray(m_particleVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);
    glBufferData(GL_ARRAY_BUFFER, 4096 * sizeof(ParticleVertex), nullptr, GL_DYNAMIC_DRAW);

    // Pos (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, pos));
    // UV (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, uv));
    // Color (location = 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, color));
    // Type (location = 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, type));

    glBindVertexArray(0);

    // Load High-Fidelity PBR Bunker & Terrain Textures (Linear srgb=false for all normal maps!)
    m_concreteFloorTex = Texture::loadTexture2D("assets/textures/concrete_floor_diffuse.tga", true);
    m_concreteFloorNorm = Texture::loadTexture2D("assets/textures/concrete_floor_normal.tga", false);
    m_steelWallTex = Texture::loadTexture2D("assets/textures/steel_wall_diffuse.tga", true);
    m_steelWallNorm = Texture::loadTexture2D("assets/textures/steel_wall_normal.tga", false);
    m_terrainMudTex = Texture::loadTexture2D("assets/textures/terrain_mud_diffuse.tga", true);
    m_terrainMudNorm = Texture::loadTexture2D("assets/textures/terrain_mud_normal.tga", false);
    m_sandbagTex = Texture::loadTexture2D("assets/textures/sandbag_diffuse.tga", true);
    m_sandbagNorm = Texture::loadTexture2D("assets/textures/sandbag_normal.tga", false);

    // Tactical PBR Road, Steppe Grass, Hesco & Fuel Drum Textures
    m_terrainRoadTex = Texture::loadTexture2D("assets/textures/terrain_road_diffuse.tga", true);
    m_terrainRoadNorm = Texture::loadTexture2D("assets/textures/terrain_road_normal.tga", false);
    m_terrainGrassTex = Texture::loadTexture2D("assets/textures/terrain_grass_diffuse.tga", true);
    m_terrainGrassNorm = Texture::loadTexture2D("assets/textures/terrain_grass_normal.tga", false);
    m_hescoTex = Texture::loadTexture2D("assets/textures/hesco_diffuse.tga", true);
    m_hescoNorm = Texture::loadTexture2D("assets/textures/hesco_normal.tga", false);
    m_drumTex = Texture::loadTexture2D("assets/textures/drum_diffuse.tga", true);
    m_drumNorm = Texture::loadTexture2D("assets/textures/drum_normal.tga", false);
    m_soldierTex = Texture::loadTexture2D("assets/textures/soldier_diffuse.tga", true);
    m_soldierNorm = Texture::loadTexture2D("assets/textures/soldier_normal.tga", false);
    m_buildingTex = 0;
    m_buildingNorm = 0;

    return true;
}

void Renderer::beginFrame(const Camera& camera, float aspectRatio, const glm::mat4& lightSpaceMatrix, GLuint shadowDepthTexture) {
    glClearColor(0.38f, 0.48f, 0.62f, 1.0f); // Atmospheric daylight sky
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    m_viewProjMatrix = camera.getViewProjectionMatrix(aspectRatio);
    m_cameraPos = camera.getPosition();
    m_viewMatrix = camera.getViewMatrix();
    m_camRight = glm::vec3(m_viewMatrix[0][0], m_viewMatrix[1][0], m_viewMatrix[2][0]);
    m_camUp    = glm::vec3(m_viewMatrix[0][1], m_viewMatrix[1][1], m_viewMatrix[2][1]);

    m_standardShader->bind();
    m_standardShader->setMat4("uViewProj", m_viewProjMatrix);
    m_standardShader->setMat4("uView", camera.getViewMatrix());
    m_standardShader->setMat4("uLightSpaceMatrix", lightSpaceMatrix);
    m_standardShader->setVec3("uLightDir", m_light.direction);
    m_standardShader->setVec3("uLightColor", m_light.color);
    m_standardShader->setFloat("uAmbient", m_light.ambientIntensity);
    m_standardShader->setFloat("uDiffuse", m_light.diffuseIntensity);
    m_standardShader->setVec3("uCameraPos", m_cameraPos);
    m_standardShader->setFloat("uTime", (float)glfwGetTime());
    PunctureSystem::get().uploadToShader(*m_standardShader, m_cameraPos);

    // Bind real-time shadow map to texture unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowDepthTexture);
    m_standardShader->setInt("uShadowMap", 1);

    m_lineShader->bind();
    m_lineShader->setMat4("uViewProj", m_viewProjMatrix);
}

void Renderer::endFrame() {
    flushParticles();
}

void Renderer::drawMesh(const Mesh& mesh, const glm::mat4& modelMatrix, const glm::vec4& tint) {
    m_standardShader->bind();
    m_standardShader->setMat4("uModel", modelMatrix);
    m_standardShader->setVec4("uTint", tint);
    m_standardShader->setInt("uUseTexture", 0);
    mesh.draw();
}

void Renderer::drawTexturedMesh(const Mesh& mesh, const glm::mat4& modelMatrix, GLuint albedoTexture, GLuint normalTexture, const glm::vec4& tint) {
    m_standardShader->bind();
    m_standardShader->setMat4("uModel", modelMatrix);
    m_standardShader->setVec4("uTint", tint);
    m_standardShader->setInt("uUseTexture", albedoTexture != 0 ? 1 : 0);
    if (albedoTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, albedoTexture);
        m_standardShader->setInt("uAlbedoMap", 0);
    }
    if (normalTexture != 0) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, normalTexture);
        m_standardShader->setInt("uNormalMap", 2);
    }
    mesh.draw();
    if (albedoTexture != 0) {
        glBindTexture(GL_TEXTURE_2D, 0);
        m_standardShader->setInt("uUseTexture", 0);
    }
}

void Renderer::drawCube(const glm::vec3& position, const glm::vec3& size, const glm::vec4& color, float yaw) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) {
        model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    model = glm::scale(model, size);
    drawMesh(*m_cubeMesh, model, color);
}

void Renderer::drawTexturedCube(const glm::vec3& position, const glm::vec3& size, GLuint albedoTex, GLuint normalTex, const glm::vec4& tint, float yaw) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) {
        model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    model = glm::scale(model, size);
    drawTexturedMesh(*m_cubeMesh, model, albedoTex, normalTex, tint);
}

void Renderer::drawCylinder(const glm::vec3& position, float radius, float height, const glm::vec4& color) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::scale(model, glm::vec3(radius * 2.0f, height, radius * 2.0f));
    drawMesh(*m_cylinderMesh, model, color);
}

void Renderer::drawGround(float size, bool isTrainingBase) {
    (void)size;
    m_standardShader->bind();
    glm::mat4 model = glm::mat4(1.0f);
    m_standardShader->setMat4("uModel", model);
    m_standardShader->setVec4("uTint", glm::vec4(1.0f));
    m_standardShader->setInt("uUseTexture", 1);
    m_standardShader->setInt("uIsTerrain", 1);
    m_standardShader->setInt("uIsTrainingMap", isTrainingBase ? 1 : 0);

    // Mud texture (Layer 0: churned earth & river silt)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_terrainMudTex);
    m_standardShader->setInt("uAlbedoMap", 0);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_terrainMudNorm);
    m_standardShader->setInt("uNormalMap", 2);

    // Road texture (Layer 1: gravel road & tire tracks)
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_terrainRoadTex != 0 ? m_terrainRoadTex : m_terrainMudTex);
    m_standardShader->setInt("uRoadAlbedo", 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_terrainRoadNorm != 0 ? m_terrainRoadNorm : m_terrainMudNorm);
    m_standardShader->setInt("uRoadNormal", 4);

    // Grass texture (Layer 2: wild steppe grass & undergrowth)
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, m_terrainGrassTex != 0 ? m_terrainGrassTex : m_terrainMudTex);
    m_standardShader->setInt("uGrassAlbedo", 5);

    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, m_terrainGrassNorm != 0 ? m_terrainGrassNorm : m_terrainMudNorm);
    m_standardShader->setInt("uGrassNormal", 6);

    if (isTrainingBase) {
        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, m_concreteFloorTex != 0 ? m_concreteFloorTex : m_terrainRoadTex);
        m_standardShader->setInt("uConcreteAlbedo", 7);

        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, m_concreteFloorNorm != 0 ? m_concreteFloorNorm : m_terrainRoadNorm);
        m_standardShader->setInt("uConcreteNormal", 8);

        if (m_trainingGroundMesh) {
            m_trainingGroundMesh->draw();
        } else {
            m_groundMesh->draw();
        }
    } else {
        m_groundMesh->draw();
    }

    m_standardShader->setInt("uIsTerrain", 0);
    m_standardShader->setInt("uUseTexture", 0);
    m_standardShader->setInt("uIsTrainingMap", 0);
    glActiveTexture(GL_TEXTURE0);
}

void Renderer::drawArenaGround(float size) {
    (void)size;
    if (!m_arenaGroundMesh) {
        m_arenaGroundMesh = ProceduralMesh::createArenaTerrain(160.0f, 160.0f, 64);
    }
    m_standardShader->bind();
    glm::mat4 model = glm::mat4(1.0f);
    m_standardShader->setMat4("uModel", model);
    m_standardShader->setVec4("uTint", glm::vec4(1.0f));
    m_standardShader->setInt("uUseTexture", 1);
    m_standardShader->setInt("uIsTerrain", 2);
    m_standardShader->setInt("uIsTrainingMap", 0);

    // Concrete Floor texture (Layer 0 & Layer 7)
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_concreteFloorTex != 0 ? m_concreteFloorTex : m_terrainRoadTex);
    m_standardShader->setInt("uAlbedoMap", 0);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_concreteFloorNorm != 0 ? m_concreteFloorNorm : m_terrainRoadNorm);
    m_standardShader->setInt("uNormalMap", 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, m_terrainRoadTex != 0 ? m_terrainRoadTex : m_concreteFloorTex);
    m_standardShader->setInt("uRoadAlbedo", 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, m_terrainRoadNorm != 0 ? m_terrainRoadNorm : m_concreteFloorNorm);
    m_standardShader->setInt("uRoadNormal", 4);

    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, m_concreteFloorTex != 0 ? m_concreteFloorTex : m_terrainRoadTex);
    m_standardShader->setInt("uConcreteAlbedo", 7);

    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, m_concreteFloorNorm != 0 ? m_concreteFloorNorm : m_terrainRoadNorm);
    m_standardShader->setInt("uConcreteNormal", 8);

    if (m_arenaGroundMesh) {
        m_arenaGroundMesh->draw();
    }

    m_standardShader->setInt("uIsTerrain", 0);
    m_standardShader->setInt("uUseTexture", 0);
    m_standardShader->setInt("uIsTrainingMap", 0);
    glActiveTexture(GL_TEXTURE0);
}

void Renderer::drawGrassField(bool isTrainingBase) {
    auto& mesh = (isTrainingBase && m_trainingGrassMesh) ? m_trainingGrassMesh : m_grassMesh;
    if (mesh) {
        glm::mat4 model = glm::mat4(1.0f);
        m_standardShader->bind();
        m_standardShader->setInt("uUseTexture", 0);
        m_standardShader->setInt("uIsTerrain", 0);
        m_standardShader->setInt("uIsTrainingMap", 0);
        drawMesh(*mesh, model, glm::vec4(1.0f));
    }
}

void Renderer::drawSoldier(const glm::vec3& position, float yaw, float pitch, const glm::vec4& camoTint) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    if (std::abs(pitch) > 0.001f) {
        model = glm::rotate(model, pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    }
    if (m_soldierTex != 0) {
        drawTexturedMesh(*m_soldierMesh, model, m_soldierTex, m_soldierNorm, camoTint);
    } else {
        drawMesh(*m_soldierMesh, model, camoTint);
    }
}

void Renderer::drawInfantry(Faction faction, CombatRole role, const glm::vec3& position, float yaw, float pitch, int lod) {
    int r = std::clamp(static_cast<int>(role), 0, 3);
    int l = std::clamp(lod, 0, 2);

    std::shared_ptr<Mesh> targetMesh = (faction == Faction::FactionB) ? m_ruInfantry[r][l] : m_uaInfantry[r][l];
    if (!targetMesh) targetMesh = m_soldierMesh;
    if (!targetMesh) return;

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) {
        model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    if (std::abs(pitch) > 0.001f) {
        model = glm::rotate(model, pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    }

    if (m_soldierTex != 0) {
        drawTexturedMesh(*targetMesh, model, m_soldierTex, m_soldierNorm);
    } else {
        drawMesh(*targetMesh, model);
    }
}

void Renderer::updateCustomPlayerMesh(const CharacterCustomization& custom) {
    m_customPlayerMesh = ProceduralMesh::createCustomSoldier(custom);
    m_cachedCustomization = custom;
    m_customMeshDirty = false;
}

void Renderer::drawCustomPlayer(const CharacterCustomization& custom, const glm::vec3& position, float yaw, float pitch) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) {
        model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    if (std::abs(pitch) > 0.001f) {
        model = glm::rotate(model, pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    }

    if (m_customMeshDirty || !m_customPlayerMesh || m_cachedCustomization != custom) {
        updateCustomPlayerMesh(custom);
    }

    if (m_customPlayerMesh) {
        if (m_soldierTex != 0) {
            drawTexturedMesh(*m_customPlayerMesh, model, m_soldierTex, m_soldierNorm);
        } else {
            drawMesh(*m_customPlayerMesh, model);
        }
    }
}

void Renderer::drawRifle(const glm::vec3& position, float yaw, float pitch) {
    drawWeapon("M4A1", position, yaw, pitch);
}

void Renderer::drawWeapon(const std::string& weaponName, const glm::vec3& position, float yaw, float pitch) {
    Mesh* meshToDraw = m_rifleMesh.get();
    if (weaponName.find("AK") != std::string::npos || weaponName.find("74") != std::string::npos || weaponName.find("Kalash") != std::string::npos) {
        if (m_ak74Mesh) meshToDraw = m_ak74Mesh.get();
    } else if (weaponName.find("Mosin") != std::string::npos || weaponName.find("Nagant") != std::string::npos || weaponName.find("Sniper") != std::string::npos) {
        if (m_mosinMesh) meshToDraw = m_mosinMesh.get();
    } else if (weaponName.find("M9") != std::string::npos || weaponName.find("Beretta") != std::string::npos || weaponName.find("Pistol") != std::string::npos) {
        if (m_m9Mesh) meshToDraw = m_m9Mesh.get();
    }

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    if (std::abs(pitch) > 0.001f) {
        model = glm::rotate(model, pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    }
    drawMesh(*meshToDraw, model, glm::vec4(1.0f));
}

void Renderer::drawWeapon(const std::string& weaponName, const glm::mat4& model) {
    Mesh* meshToDraw = m_rifleMesh.get();
    if (weaponName.find("AK") != std::string::npos || weaponName.find("74") != std::string::npos || weaponName.find("Kalash") != std::string::npos) {
        if (m_ak74Mesh) meshToDraw = m_ak74Mesh.get();
    } else if (weaponName.find("Mosin") != std::string::npos || weaponName.find("Nagant") != std::string::npos || weaponName.find("Sniper") != std::string::npos) {
        if (m_mosinMesh) meshToDraw = m_mosinMesh.get();
    } else if (weaponName.find("M9") != std::string::npos || weaponName.find("Beretta") != std::string::npos || weaponName.find("Pistol") != std::string::npos) {
        if (m_m9Mesh) meshToDraw = m_m9Mesh.get();
    }
    drawMesh(*meshToDraw, model, glm::vec4(1.0f));
}

void Renderer::drawWeaponShadow(const std::string& weaponName, Shader* shadowShader, const glm::mat4& model) {
    if (!shadowShader) return;
    Mesh* meshToDraw = m_rifleMesh.get();
    if (weaponName.find("AK") != std::string::npos || weaponName.find("74") != std::string::npos || weaponName.find("Kalash") != std::string::npos) {
        if (m_ak74Mesh) meshToDraw = m_ak74Mesh.get();
    } else if (weaponName.find("Mosin") != std::string::npos || weaponName.find("Nagant") != std::string::npos || weaponName.find("Sniper") != std::string::npos) {
        if (m_mosinMesh) meshToDraw = m_mosinMesh.get();
    } else if (weaponName.find("M9") != std::string::npos || weaponName.find("Beretta") != std::string::npos || weaponName.find("Pistol") != std::string::npos) {
        if (m_m9Mesh) meshToDraw = m_m9Mesh.get();
    }
    shadowShader->setMat4("uModel", model);
    meshToDraw->draw();
}


void Renderer::drawDroppedWeapon(const std::string& weaponName, const glm::vec3& position, float yaw) {
    Mesh* meshToDraw = m_rifleMesh.get();
    if (weaponName.find("AK") != std::string::npos || weaponName.find("74") != std::string::npos || weaponName.find("Kalash") != std::string::npos) {
        if (m_ak74Mesh) meshToDraw = m_ak74Mesh.get();
    } else if (weaponName.find("Mosin") != std::string::npos || weaponName.find("Nagant") != std::string::npos || weaponName.find("Sniper") != std::string::npos) {
        if (m_mosinMesh) meshToDraw = m_mosinMesh.get();
    } else if (weaponName.find("M9") != std::string::npos || weaponName.find("Beretta") != std::string::npos || weaponName.find("Pistol") != std::string::npos) {
        if (m_m9Mesh) meshToDraw = m_m9Mesh.get();
    }

    const auto& gripCfg = WeaponRigManager::get().getConfig(weaponName);
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position + glm::vec3(0.0f, gripCfg.groundClearance, 0.0f));
    model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    drawMesh(*meshToDraw, model, glm::vec4(1.0f));
}

void Renderer::drawDroppedWeaponShadow(const std::string& weaponName, Shader* shadowShader, const glm::vec3& position, float yaw) {
    if (!shadowShader) return;
    const auto& gripCfg = WeaponRigManager::get().getConfig(weaponName);
    if (!shadowShader) return;
    Mesh* meshToDraw = m_rifleMesh.get();
    if (weaponName.find("AK") != std::string::npos || weaponName.find("74") != std::string::npos || weaponName.find("Kalash") != std::string::npos) {
        if (m_ak74Mesh) meshToDraw = m_ak74Mesh.get();
    } else if (weaponName.find("Mosin") != std::string::npos || weaponName.find("Nagant") != std::string::npos || weaponName.find("Sniper") != std::string::npos) {
        if (m_mosinMesh) meshToDraw = m_mosinMesh.get();
    } else if (weaponName.find("M9") != std::string::npos || weaponName.find("Beretta") != std::string::npos || weaponName.find("Pistol") != std::string::npos) {
        if (m_m9Mesh) meshToDraw = m_m9Mesh.get();
    }

    glm::mat4 model = glm::translate(glm::mat4(1.0f), position + glm::vec3(0.0f, gripCfg.groundClearance, 0.0f));
    model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    shadowShader->setMat4("uModel", model);
    meshToDraw->draw();
}

void Renderer::drawSandbagWall(const glm::vec3& position, float yaw) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) {
        model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    if (m_sandbagTex != 0) {
        drawTexturedMesh(*m_sandbagMesh, model, m_sandbagTex, m_sandbagNorm, glm::vec4(1.0f));
    } else {
        drawMesh(*m_sandbagMesh, model, glm::vec4(1.0f));
    }
}

void Renderer::drawMilitaryCrate(const glm::vec3& position, float yaw, bool looted) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) {
        model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    }
    glm::vec4 tint = looted ? glm::vec4(0.45f, 0.45f, 0.45f, 1.0f) : glm::vec4(1.0f);
    if (m_steelWallTex != 0) {
        drawTexturedMesh(*m_crateMesh, model, m_steelWallTex, m_steelWallNorm, tint);
    } else {
        drawMesh(*m_crateMesh, model, tint);
    }
}

void Renderer::drawPineTree(const glm::vec3& position, float yaw) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));
    drawMesh(*m_pineTreeMesh, model);
}

void Renderer::drawDragonTooth(const glm::vec3& position, float yaw) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));
    if (m_concreteFloorTex != 0) {
        drawTexturedMesh(*m_dragonToothMesh, model, m_concreteFloorTex, m_concreteFloorNorm);
    } else {
        drawMesh(*m_dragonToothMesh, model);
    }
}

void Renderer::drawBarbedWire(const glm::vec3& position, float yaw) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));
    drawMesh(*m_barbedWireMesh, model);
}

void Renderer::drawTankWreck(const glm::vec3& position, float yaw) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));
    if (m_steelWallTex != 0) {
        drawTexturedMesh(*m_tankWreckMesh, model, m_steelWallTex, m_steelWallNorm);
    } else {
        drawMesh(*m_tankWreckMesh, model);
    }
}

void Renderer::drawRuinedWall(const glm::vec3& position, float yaw) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));
    if (m_buildingTex != 0) {
        drawTexturedMesh(*m_ruinedWallMesh, model, m_buildingTex, m_buildingNorm);
    } else if (m_concreteFloorTex != 0) {
        drawTexturedMesh(*m_ruinedWallMesh, model, m_concreteFloorTex, m_concreteFloorNorm);
    } else {
        drawMesh(*m_ruinedWallMesh, model);
    }
}

void Renderer::drawBridge(const glm::vec3& position, float yaw) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));
    if (m_concreteFloorTex != 0) {
        drawTexturedMesh(*m_bridgeMesh, model, m_concreteFloorTex, m_concreteFloorNorm);
    } else {
        drawMesh(*m_bridgeMesh, model);
    }
}

void Renderer::drawRiverWater(const glm::vec3& position) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (m_terrainMudNorm != 0) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, m_terrainMudNorm);
        m_standardShader->setInt("uNormalMap", 2);
    }
    drawMesh(*m_riverWaterMesh, model);
}

void Renderer::drawHescoBastion(const glm::vec3& position, float yaw) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));
    if (m_hescoTex != 0) {
        drawTexturedMesh(*m_hescoMesh, model, m_hescoTex, m_hescoNorm);
    } else {
        drawMesh(*m_hescoMesh, model);
    }
}

void Renderer::drawCommandTent(const glm::vec3& position, float yaw) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));
    if (m_hescoTex != 0) {
        drawTexturedMesh(*m_commandTentMesh, model, m_hescoTex, m_hescoNorm);
    } else {
        drawMesh(*m_commandTentMesh, model);
    }
}

void Renderer::drawFuelDrum(const glm::vec3& position, float yaw) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));
    if (m_drumTex != 0) {
        drawTexturedMesh(*m_fuelDrumMesh, model, m_drumTex, m_drumNorm);
    } else {
        drawMesh(*m_fuelDrumMesh, model);
    }
}

void Renderer::drawCargoPallet(const glm::vec3& position, float yaw) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));
    if (m_buildingTex != 0) {
        drawTexturedMesh(*m_cargoPalletMesh, model, m_buildingTex, m_buildingNorm);
    } else {
        drawMesh(*m_cargoPalletMesh, model);
    }
}

void Renderer::drawFieldGenerator(const glm::vec3& position, float yaw) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));
    if (m_steelWallTex != 0) {
        drawTexturedMesh(*m_generatorMesh, model, m_steelWallTex, m_steelWallNorm);
    } else {
        drawMesh(*m_generatorMesh, model);
    }
}

void Renderer::drawMilitaryTruck(const glm::vec3& position, float yaw) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));
    if (m_steelWallTex != 0) {
        drawTexturedMesh(*m_militaryTruckMesh, model, m_steelWallTex, m_steelWallNorm);
    } else {
        drawMesh(*m_militaryTruckMesh, model);
    }
}

void Renderer::drawMountedHMG(const glm::vec3& position, float yaw, float pitch) {
    if (!m_hmgMesh) return;
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    model = glm::rotate(model, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    if (std::abs(pitch) > 0.001f) {
        model = glm::rotate(model, pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    }
    drawMesh(*m_hmgMesh, model);
}

void Renderer::drawDamagedBuilding(const glm::vec3& position, float yaw, const glm::vec3& size) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (std::abs(yaw) > 0.001f) model = glm::rotate(model, yaw, glm::vec3(0, 1, 0));
    if (m_damagedBuildingMesh) {
        drawMesh(*m_damagedBuildingMesh, model);
    }
}


void Renderer::drawSeveredProp(const Mesh& mesh, const glm::vec3& position, const glm::quat& rotation, const glm::vec4& color) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(rotation);
    drawMesh(mesh, model, color);
}

void Renderer::drawObstacle(const WorldObstacle& obs) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), obs.position);
    if (std::abs(obs.yaw) > 0.0001f)   model = glm::rotate(model, obs.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    if (std::abs(obs.pitch) > 0.0001f) model = glm::rotate(model, obs.pitch, glm::vec3(1.0f, 0.0f, 0.0f));
    if (std::abs(obs.roll) > 0.0001f)  model = glm::rotate(model, obs.roll, glm::vec3(0.0f, 0.0f, 1.0f));

    if (obs.isSeveredStump && obs.customMesh) {
        drawMesh(*obs.customMesh, model, obs.color);
        return;
    }

    switch (obs.type) {
    case ObstacleType::Sandbag:
        if (m_sandbagTex != 0) drawTexturedMesh(*m_sandbagMesh, model, m_sandbagTex, m_sandbagNorm, glm::vec4(1.0f));
        else drawMesh(*m_sandbagMesh, model, glm::vec4(1.0f));
        break;
    case ObstacleType::PineTree:
        drawMesh(*m_pineTreeMesh, model);
        break;
    case ObstacleType::DragonTooth:
        if (m_concreteFloorTex != 0) drawTexturedMesh(*m_dragonToothMesh, model, m_concreteFloorTex, m_concreteFloorNorm);
        else drawMesh(*m_dragonToothMesh, model);
        break;
    case ObstacleType::BarbedWire:
        drawMesh(*m_barbedWireMesh, model);
        break;
    case ObstacleType::TankWreck:
        if (m_steelWallTex != 0) drawTexturedMesh(*m_tankWreckMesh, model, m_steelWallTex, m_steelWallNorm);
        else drawMesh(*m_tankWreckMesh, model);
        break;
    case ObstacleType::RuinedWall:
        if (m_buildingTex != 0) drawTexturedMesh(*m_ruinedWallMesh, model, m_buildingTex, m_buildingNorm);
        else if (m_concreteFloorTex != 0) drawTexturedMesh(*m_ruinedWallMesh, model, m_concreteFloorTex, m_concreteFloorNorm);
        else drawMesh(*m_ruinedWallMesh, model);
        break;
    case ObstacleType::RiverBridge:
        if (m_concreteFloorTex != 0) drawTexturedMesh(*m_bridgeMesh, model, m_concreteFloorTex, m_concreteFloorNorm);
        else drawMesh(*m_bridgeMesh, model);
        break;
    case ObstacleType::HescoBastion:
        if (m_hescoTex != 0) drawTexturedMesh(*m_hescoMesh, model, m_hescoTex, m_hescoNorm);
        else drawMesh(*m_hescoMesh, model);
        break;
    case ObstacleType::CommandTent:
        if (m_hescoTex != 0) drawTexturedMesh(*m_commandTentMesh, model, m_hescoTex, m_hescoNorm);
        else drawMesh(*m_commandTentMesh, model);
        break;
    case ObstacleType::FuelDrum:
        if (m_drumTex != 0) drawTexturedMesh(*m_fuelDrumMesh, model, m_drumTex, m_drumNorm);
        else drawMesh(*m_fuelDrumMesh, model);
        break;
    case ObstacleType::CargoPallet:
        if (m_buildingTex != 0) drawTexturedMesh(*m_cargoPalletMesh, model, m_buildingTex, m_buildingNorm);
        else drawMesh(*m_cargoPalletMesh, model);
        break;
    case ObstacleType::FieldGenerator:
        if (m_steelWallTex != 0) drawTexturedMesh(*m_generatorMesh, model, m_steelWallTex, m_steelWallNorm);
        else drawMesh(*m_generatorMesh, model);
        break;
    case ObstacleType::MilitaryTruck:
        if (m_steelWallTex != 0) drawTexturedMesh(*m_militaryTruckMesh, model, m_steelWallTex, m_steelWallNorm);
        else drawMesh(*m_militaryTruckMesh, model);
        break;
    case ObstacleType::DamagedBuilding:
        if (m_damagedBuildingMesh) {
            drawMesh(*m_damagedBuildingMesh, model);
        }
        break;
    case ObstacleType::GrassClump:
        if (m_grassClumpMesh) drawMesh(*m_grassClumpMesh, model);
        break;
    case ObstacleType::BushFoliage:
        if (m_bushFoliageMesh) drawMesh(*m_bushFoliageMesh, model);
        break;
    case ObstacleType::ConcreteBarrier:
        if (m_concreteBarrierMesh) {
            if (m_concreteFloorTex != 0) drawTexturedMesh(*m_concreteBarrierMesh, model, m_concreteFloorTex, m_concreteFloorNorm);
            else drawMesh(*m_concreteBarrierMesh, model);
        }
        break;
    case ObstacleType::CampfireSmoke:
        if (m_campfireSmokeMesh) drawMesh(*m_campfireSmokeMesh, model);
        break;
    case ObstacleType::SignalFlare:
        if (m_signalFlareMesh) drawMesh(*m_signalFlareMesh, model);
        break;
    case ObstacleType::MountedHMG:
        if (m_hmgMesh) drawMesh(*m_hmgMesh, model);
        break;
    case ObstacleType::UrbanApartment:
        if (m_urbanApartmentMesh) {
            if (m_buildingTex != 0) drawTexturedMesh(*m_urbanApartmentMesh, model, m_buildingTex, m_buildingNorm);
            else drawMesh(*m_urbanApartmentMesh, model);
        }
        break;
    case ObstacleType::IndustrialWarehouse:
        if (m_industrialWarehouseMesh) {
            if (m_steelWallTex != 0) drawTexturedMesh(*m_industrialWarehouseMesh, model, m_steelWallTex, m_steelWallNorm);
            else drawMesh(*m_industrialWarehouseMesh, model);
        }
        break;
    case ObstacleType::TownChurch:
        if (m_townChurchMesh) {
            if (m_concreteFloorTex != 0) drawTexturedMesh(*m_townChurchMesh, model, m_concreteFloorTex, m_concreteFloorNorm);
            else drawMesh(*m_townChurchMesh, model);
        }
        break;
    case ObstacleType::FreightBoxcar:
        if (m_freightBoxcarMesh) {
            if (m_steelWallTex != 0) drawTexturedMesh(*m_freightBoxcarMesh, model, m_steelWallTex, m_steelWallNorm);
            else drawMesh(*m_freightBoxcarMesh, model);
        }
        break;
    case ObstacleType::StreetLamp:
        if (m_streetLampMesh) drawMesh(*m_streetLampMesh, model);
        break;
    case ObstacleType::CommunicationTower:
        if (m_commTowerMesh) drawMesh(*m_commTowerMesh, model);
        break;
    case ObstacleType::RefineryTank:
        if (m_refineryTankMesh) {
            if (m_steelWallTex != 0) drawTexturedMesh(*m_refineryTankMesh, model, m_steelWallTex, m_steelWallNorm);
            else drawMesh(*m_refineryTankMesh, model);
        }
        break;
    case ObstacleType::GuardBooth:
        if (m_guardBoothMesh) drawMesh(*m_guardBoothMesh, model);
        break;
    case ObstacleType::Watchtower:
        if (m_watchtowerMesh) drawMesh(*m_watchtowerMesh, model);
        break;
    case ObstacleType::PerimeterFence:
        if (m_perimeterFenceMesh) drawMesh(*m_perimeterFenceMesh, model);
        break;
    case ObstacleType::ObstacleWall:
        if (m_obstacleWallMesh) drawMesh(*m_obstacleWallMesh, model);
        break;
    case ObstacleType::GunRack:
        if (m_gunRackMesh) drawMesh(*m_gunRackMesh, model);
        break;
    case ObstacleType::ShootingTarget:
        if (m_shootingTargetMesh) drawMesh(*m_shootingTargetMesh, model);
        break;
    case ObstacleType::WoodenCrate:
        if (m_crateMesh) {
            drawMesh(*m_crateMesh, model, obs.color);
        } else {
            drawCube(obs.position + glm::vec3(0.0f, obs.size.y * 0.5f, 0.0f), obs.size, obs.color, obs.yaw);
        }
        break;
    default:
        drawCube(obs.position + glm::vec3(0.0f, obs.size.y * 0.5f, 0.0f), obs.size, obs.color, obs.yaw);
        break;
    }
}

void Renderer::renderShadowPass(Shader* shadowShader, const World& world, const Player& player, const FrontlineWarfare* frontline) {
    if (!shadowShader) return;
    shadowShader->bind();
    PunctureSystem::get().uploadToShader(*shadowShader, player.getPosition());

    // 1. Shadow for Obstacles & Battlefield Environment
    for (const auto& obs : world.getObstacles()) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), obs.position);
        if (std::abs(obs.yaw) > 0.0001f)   model = glm::rotate(model, obs.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
        if (std::abs(obs.pitch) > 0.0001f) model = glm::rotate(model, obs.pitch, glm::vec3(1.0f, 0.0f, 0.0f));
        if (std::abs(obs.roll) > 0.0001f)  model = glm::rotate(model, obs.roll, glm::vec3(0.0f, 0.0f, 1.0f));

        if (obs.isSeveredStump && obs.customMesh) {
            shadowShader->setMat4("uModel", model);
            obs.customMesh->draw();
            continue;
        }

        switch (obs.type) {
        case ObstacleType::Sandbag:
            shadowShader->setMat4("uModel", model);
            m_sandbagMesh->draw();
            break;
        case ObstacleType::PineTree:
            shadowShader->setMat4("uModel", model);
            m_pineTreeMesh->draw();
            break;
        case ObstacleType::DragonTooth:
            shadowShader->setMat4("uModel", model);
            m_dragonToothMesh->draw();
            break;
        case ObstacleType::BarbedWire:
            shadowShader->setMat4("uModel", model);
            m_barbedWireMesh->draw();
            break;
        case ObstacleType::TankWreck:
            shadowShader->setMat4("uModel", model);
            m_tankWreckMesh->draw();
            break;
        case ObstacleType::RuinedWall:
            shadowShader->setMat4("uModel", model);
            m_ruinedWallMesh->draw();
            break;
        case ObstacleType::RiverBridge:
            shadowShader->setMat4("uModel", model);
            m_bridgeMesh->draw();
            break;
        case ObstacleType::HescoBastion:
            shadowShader->setMat4("uModel", model);
            m_hescoMesh->draw();
            break;
        case ObstacleType::CommandTent:
            shadowShader->setMat4("uModel", model);
            m_commandTentMesh->draw();
            break;
        case ObstacleType::FuelDrum:
            shadowShader->setMat4("uModel", model);
            m_fuelDrumMesh->draw();
            break;
        case ObstacleType::CargoPallet:
            shadowShader->setMat4("uModel", model);
            m_cargoPalletMesh->draw();
            break;
        case ObstacleType::FieldGenerator:
            shadowShader->setMat4("uModel", model);
            m_generatorMesh->draw();
            break;
        case ObstacleType::MilitaryTruck:
            shadowShader->setMat4("uModel", model);
            m_militaryTruckMesh->draw();
            break;
        case ObstacleType::DamagedBuilding:
            if (m_damagedBuildingMesh) {
                shadowShader->setMat4("uModel", model);
                m_damagedBuildingMesh->draw();
            }
            break;
        case ObstacleType::GrassClump:
            if (m_grassClumpMesh) { shadowShader->setMat4("uModel", model); m_grassClumpMesh->draw(); }
            break;
        case ObstacleType::BushFoliage:
            if (m_bushFoliageMesh) { shadowShader->setMat4("uModel", model); m_bushFoliageMesh->draw(); }
            break;
        case ObstacleType::ConcreteBarrier:
            if (m_concreteBarrierMesh) { shadowShader->setMat4("uModel", model); m_concreteBarrierMesh->draw(); }
            break;
        case ObstacleType::CampfireSmoke:
            if (m_campfireSmokeMesh) { shadowShader->setMat4("uModel", model); m_campfireSmokeMesh->draw(); }
            break;
        case ObstacleType::SignalFlare:
            if (m_signalFlareMesh) { shadowShader->setMat4("uModel", model); m_signalFlareMesh->draw(); }
            break;
        case ObstacleType::MountedHMG:
            if (m_hmgMesh) { shadowShader->setMat4("uModel", model); m_hmgMesh->draw(); }
            break;
        case ObstacleType::UrbanApartment:
            if (m_urbanApartmentMesh) { shadowShader->setMat4("uModel", model); m_urbanApartmentMesh->draw(); }
            break;
        case ObstacleType::IndustrialWarehouse:
            if (m_industrialWarehouseMesh) { shadowShader->setMat4("uModel", model); m_industrialWarehouseMesh->draw(); }
            break;
        case ObstacleType::TownChurch:
            if (m_townChurchMesh) { shadowShader->setMat4("uModel", model); m_townChurchMesh->draw(); }
            break;
        case ObstacleType::FreightBoxcar:
            if (m_freightBoxcarMesh) { shadowShader->setMat4("uModel", model); m_freightBoxcarMesh->draw(); }
            break;
        case ObstacleType::StreetLamp:
            if (m_streetLampMesh) { shadowShader->setMat4("uModel", model); m_streetLampMesh->draw(); }
            break;
        case ObstacleType::CommunicationTower:
            if (m_commTowerMesh) { shadowShader->setMat4("uModel", model); m_commTowerMesh->draw(); }
            break;
        case ObstacleType::RefineryTank:
            if (m_refineryTankMesh) { shadowShader->setMat4("uModel", model); m_refineryTankMesh->draw(); }
            break;
        case ObstacleType::GuardBooth:
            if (m_guardBoothMesh) { shadowShader->setMat4("uModel", model); m_guardBoothMesh->draw(); }
            break;
        case ObstacleType::Watchtower:
            if (m_watchtowerMesh) { shadowShader->setMat4("uModel", model); m_watchtowerMesh->draw(); }
            break;
        case ObstacleType::PerimeterFence:
            if (m_perimeterFenceMesh) { shadowShader->setMat4("uModel", model); m_perimeterFenceMesh->draw(); }
            break;
        case ObstacleType::ObstacleWall:
            if (m_obstacleWallMesh) { shadowShader->setMat4("uModel", model); m_obstacleWallMesh->draw(); }
            break;
        case ObstacleType::GunRack:
            if (m_gunRackMesh) { shadowShader->setMat4("uModel", model); m_gunRackMesh->draw(); }
            break;
        case ObstacleType::ShootingTarget:
            if (m_shootingTargetMesh) { shadowShader->setMat4("uModel", model); m_shootingTargetMesh->draw(); }
            break;
        default:
            model = glm::translate(model, glm::vec3(0.0f, obs.size.y * 0.5f, 0.0f));
            model = glm::scale(model, obs.size);
            shadowShader->setMat4("uModel", model);
            m_cubeMesh->draw();
            break;
        }
    }

    // 1b. Shadow for Dynamic Jolt Severed Props
    for (const auto& prop : world.getSeveredProps()) {
        if (prop.mesh) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), prop.position) * glm::mat4_cast(prop.rotation);
            shadowShader->setMat4("uModel", model);
            prop.mesh->draw();
        }
    }

    // 2. Shadow for Crates & Dropped Ground Items
    for (const auto& crate : world.getCrates()) {
        if (crate.isGroundItem) {
            if (crate.looted || crate.contents.empty()) continue;
            if (!crate.contents.empty() && crate.contents[0].category == ItemCategory::Weapon) {
                drawDroppedWeaponShadow(crate.contents[0].name, shadowShader, crate.position, crate.yaw);
            } else {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), crate.position + glm::vec3(0.0f, 0.08f, 0.0f));
                model = glm::rotate(model, crate.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::scale(model, glm::vec3(0.24f, 0.14f, 0.16f));
                shadowShader->setMat4("uModel", model);
                m_cubeMesh->draw();
            }
        } else {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), crate.position);
            shadowShader->setMat4("uModel", model);
            m_crateMesh->draw();
        }
    }

    // 3. Shadow for Target Dummies
    for (const auto& target : world.getTargets()) {
        if (!target.isAlive) continue;
        glm::mat4 model = glm::translate(glm::mat4(1.0f), target.position + glm::vec3(0, 0.5f, 0));
        model = glm::scale(model, glm::vec3(0.6f, 0.9f, 0.15f));
        shadowShader->setMat4("uModel", model);
        m_cubeMesh->draw();
    }

    // 4. Shadow for Player Soldier (uses animated procedural rig when available)
    if (player.getRig().getMesh()) {
        shadowShader->setMat4("uModel", glm::mat4(1.0f));
        player.getRig().getMesh()->draw();
    } else {
        glm::mat4 playerModel = glm::translate(glm::mat4(1.0f), player.getPosition());
        playerModel = glm::rotate(playerModel, player.getYaw(), glm::vec3(0.0f, 1.0f, 0.0f));
        shadowShader->setMat4("uModel", playerModel);
        m_soldierMesh->draw();
    }



    // 5. Dynamic Real-Time Shadows for All Active Frontline AI Bots
    if (frontline) {
        for (const auto& bot : frontline->getBots()) {
            if (bot.state == BotState::Dead) continue;
            glm::mat4 botModel = glm::translate(glm::mat4(1.0f), bot.position);
            botModel = glm::rotate(botModel, bot.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
            shadowShader->setMat4("uModel", botModel);
            int r = std::clamp(static_cast<int>(bot.role), 0, 3);
            std::shared_ptr<Mesh> botMesh = (bot.faction == Faction::FactionB) ? m_ruInfantry[r][0] : m_uaInfantry[r][0];
            if (botMesh) botMesh->draw();
            else m_soldierMesh->draw();
        }
    }

    // 6. Dynamic Real-Time Shadows for Connected Remote Multiplayer Operatives
    for (const auto& [id, remote] : NetworkEngine::get().getRemotePlayers()) {
        if (!remote.active || remote.currentState.health <= 0.0f) continue;
        if (remote.rig && remote.rig->getMesh()) {
            shadowShader->setMat4("uModel", glm::mat4(1.0f));
            remote.rig->getMesh()->draw();
        } else {
            glm::mat4 rModel = glm::translate(glm::mat4(1.0f), remote.currentState.position);
            rModel = glm::rotate(rModel, remote.currentState.yaw, glm::vec3(0.0f, 1.0f, 0.0f));
            shadowShader->setMat4("uModel", rModel);
            m_soldierMesh->draw();
        }
    }
}

void Renderer::drawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color) {
    m_lineShader->bind();

    float lineData[14] = {
        start.x, start.y, start.z, color.r, color.g, color.b, color.a,
        end.x,   end.y,   end.z,   color.r, color.g, color.b, color.a
    };

    glBindVertexArray(m_lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(lineData), lineData);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.5f);

    glDrawArrays(GL_LINES, 0, 2);

    glDisable(GL_BLEND);
    glLineWidth(1.0f);
    glBindVertexArray(0);
}

void Renderer::drawContactShadow(const glm::vec3& centerPos, float radiusX, float radiusZ, float opacity) {
    const int segments = 24;
    const float step = 2.0f * 3.14159265f / segments;

    std::vector<float> shadowData;
    glm::vec3 center(centerPos.x, 0.02f, centerPos.z);
    glm::vec4 centerCol(0.04f, 0.05f, 0.04f, opacity);
    glm::vec4 edgeCol(0.04f, 0.05f, 0.04f, 0.0f);

    // Triangle fan for soft feathered shadow
    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;

        float x1 = center.x + std::cos(a1) * radiusX;
        float z1 = center.z + std::sin(a1) * radiusZ;
        float x2 = center.x + std::cos(a2) * radiusX;
        float z2 = center.z + std::sin(a2) * radiusZ;

        // Center
        shadowData.insert(shadowData.end(), {center.x, center.y, center.z, centerCol.r, centerCol.g, centerCol.b, centerCol.a});
        // Edge 1
        shadowData.insert(shadowData.end(), {x1, center.y, z1, edgeCol.r, edgeCol.g, edgeCol.b, edgeCol.a});
        // Edge 2
        shadowData.insert(shadowData.end(), {x2, center.y, z2, edgeCol.r, edgeCol.g, edgeCol.b, edgeCol.a});
    }

    m_lineShader->bind();
    glBindVertexArray(m_lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVbo);
    glBufferData(GL_ARRAY_BUFFER, shadowData.size() * sizeof(float), shadowData.data(), GL_DYNAMIC_DRAW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); // Don't write to depth to prevent shadow z-fighting

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(shadowData.size() / 7));

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glBindVertexArray(0);
}

void Renderer::drawAimReticle(const glm::vec3& worldPos, float bloomRadius, const glm::vec4& color) {
    const int segments = 24;
    const float step = 2.0f * 3.14159265f / segments;

    std::vector<float> circleData;
    for (int i = 0; i < segments; ++i) {
        float a1 = i * step;
        float a2 = (i + 1) * step;

        float x1 = worldPos.x + std::cos(a1) * bloomRadius;
        float z1 = worldPos.z + std::sin(a1) * bloomRadius;
        float x2 = worldPos.x + std::cos(a2) * bloomRadius;
        float z2 = worldPos.z + std::sin(a2) * bloomRadius;

        circleData.insert(circleData.end(), {x1, worldPos.y + 0.05f, z1, color.r, color.g, color.b, color.a});
        circleData.insert(circleData.end(), {x2, worldPos.y + 0.05f, z2, color.r, color.g, color.b, color.a});
    }

    // Cross tick marks
    float tick = bloomRadius * 0.4f;
    // Top tick
    circleData.insert(circleData.end(), {worldPos.x, worldPos.y + 0.05f, worldPos.z + bloomRadius, color.r, color.g, color.b, color.a});
    circleData.insert(circleData.end(), {worldPos.x, worldPos.y + 0.05f, worldPos.z + bloomRadius + tick, color.r, color.g, color.b, color.a});
    // Bottom tick
    circleData.insert(circleData.end(), {worldPos.x, worldPos.y + 0.05f, worldPos.z - bloomRadius, color.r, color.g, color.b, color.a});
    circleData.insert(circleData.end(), {worldPos.x, worldPos.y + 0.05f, worldPos.z - bloomRadius - tick, color.r, color.g, color.b, color.a});
    // Left tick
    circleData.insert(circleData.end(), {worldPos.x - bloomRadius, worldPos.y + 0.05f, worldPos.z, color.r, color.g, color.b, color.a});
    circleData.insert(circleData.end(), {worldPos.x - bloomRadius - tick, worldPos.y + 0.05f, worldPos.z, color.r, color.g, color.b, color.a});
    // Right tick
    circleData.insert(circleData.end(), {worldPos.x + bloomRadius, worldPos.y + 0.05f, worldPos.z, color.r, color.g, color.b, color.a});
    circleData.insert(circleData.end(), {worldPos.x + bloomRadius + tick, worldPos.y + 0.05f, worldPos.z, color.r, color.g, color.b, color.a});

    // Center precision pip (subtle crosshair center point)
    float pip = std::min(0.08f, bloomRadius * 0.25f);
    circleData.insert(circleData.end(), {worldPos.x - pip, worldPos.y + 0.05f, worldPos.z, color.r, color.g, color.b, color.a * 0.9f});
    circleData.insert(circleData.end(), {worldPos.x + pip, worldPos.y + 0.05f, worldPos.z, color.r, color.g, color.b, color.a * 0.9f});
    circleData.insert(circleData.end(), {worldPos.x, worldPos.y + 0.05f, worldPos.z - pip, color.r, color.g, color.b, color.a * 0.9f});
    circleData.insert(circleData.end(), {worldPos.x, worldPos.y + 0.05f, worldPos.z + pip, color.r, color.g, color.b, color.a * 0.9f});

    m_lineShader->bind();
    glBindVertexArray(m_lineVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_lineVbo);
    glBufferData(GL_ARRAY_BUFFER, circleData.size() * sizeof(float), circleData.data(), GL_DYNAMIC_DRAW);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(2.2f);
    glDisable(GL_DEPTH_TEST);

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(circleData.size() / 7));

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
    glBindVertexArray(0);
}

void Renderer::drawBunkerHideout(const glm::vec3& centerPos) {
    // 1. High-definition PBR scuffed industrial concrete floor with expansion joints
    if (m_concreteFloorTex != 0) {
        drawTexturedCube(centerPos + glm::vec3(0.0f, -0.05f, 0.0f), glm::vec3(18.0f, 0.1f, 18.0f), m_concreteFloorTex, m_concreteFloorNorm, glm::vec4(1.0f));
    } else {
        drawCube(centerPos + glm::vec3(0.0f, -0.05f, 0.0f), glm::vec3(18.0f, 0.1f, 18.0f), glm::vec4(0.18f, 0.20f, 0.22f, 1.0f));
    }

    // 2. Turntable Inspection Platform: Yellow/black hazard boundary lines on the floor
    const glm::vec4 hazardYellow(0.88f, 0.72f, 0.14f, 1.0f);
    drawCube(centerPos + glm::vec3(0.0f, 0.005f, 0.0f), glm::vec3(3.0f, 0.01f, 3.0f), glm::vec4(0.16f, 0.18f, 0.20f, 1.0f));
    // Hazard boundary tape surrounding the operator
    drawCube(centerPos + glm::vec3(0.0f, 0.015f, -1.48f), glm::vec3(3.0f, 0.01f, 0.08f), hazardYellow);
    drawCube(centerPos + glm::vec3(0.0f, 0.015f, 1.48f), glm::vec3(3.0f, 0.01f, 0.08f), hazardYellow);
    drawCube(centerPos + glm::vec3(-1.48f, 0.015f, 0.0f), glm::vec3(0.08f, 0.01f, 3.0f), hazardYellow);
    drawCube(centerPos + glm::vec3(1.48f, 0.015f, 0.0f), glm::vec3(0.08f, 0.01f, 3.0f), hazardYellow);
    // Steel drain grate recessed in the concrete
    drawCube(centerPos + glm::vec3(-2.2f, 0.005f, 1.2f), glm::vec3(0.9f, 0.01f, 0.9f), glm::vec4(0.10f, 0.11f, 0.13f, 1.0f));

    // 3. High-definition corrugated steel bunker walls
    if (m_steelWallTex != 0) {
        drawTexturedCube(centerPos + glm::vec3(0.0f, 2.5f, -5.5f), glm::vec3(18.0f, 5.2f, 0.8f), m_steelWallTex, m_steelWallNorm, glm::vec4(0.92f, 0.92f, 0.94f, 1.0f));
        drawTexturedCube(centerPos + glm::vec3(-7.5f, 2.5f, 0.0f), glm::vec3(0.8f, 5.2f, 18.0f), m_steelWallTex, m_steelWallNorm, glm::vec4(0.88f, 0.88f, 0.90f, 1.0f));
        drawTexturedCube(centerPos + glm::vec3(7.5f, 2.5f, 0.0f), glm::vec3(0.8f, 5.2f, 18.0f), m_steelWallTex, m_steelWallNorm, glm::vec4(0.88f, 0.88f, 0.90f, 1.0f));
    } else {
        drawCube(centerPos + glm::vec3(0.0f, 2.5f, -5.5f), glm::vec3(18.0f, 5.2f, 0.8f), glm::vec4(0.24f, 0.26f, 0.29f, 1.0f));
        drawCube(centerPos + glm::vec3(-7.5f, 2.5f, 0.0f), glm::vec3(0.8f, 5.2f, 18.0f), glm::vec4(0.22f, 0.24f, 0.26f, 1.0f));
        drawCube(centerPos + glm::vec3(7.5f, 2.5f, 0.0f), glm::vec3(0.8f, 5.2f, 18.0f), glm::vec4(0.22f, 0.24f, 0.26f, 1.0f));
    }

    // 4. Overhead structural dark steel I-beams & Industrial Ceiling
    const glm::vec4 darkSteel(0.14f, 0.15f, 0.18f, 1.0f);
    drawCube(centerPos + glm::vec3(0.0f, 4.8f, -2.0f), glm::vec3(16.0f, 0.35f, 0.6f), darkSteel);
    drawCube(centerPos + glm::vec3(0.0f, 4.8f, 2.0f), glm::vec3(16.0f, 0.35f, 0.6f), darkSteel);
    drawCube(centerPos + glm::vec3(0.0f, 5.1f, 0.0f), glm::vec3(18.0f, 0.2f, 18.0f), glm::vec4(0.10f, 0.11f, 0.12f, 1.0f));

    // 5. Overhead industrial tungsten cage lamps mounted on chains
    auto drawHangingLamp = [&](const glm::vec3& pos) {
        drawCylinder(pos + glm::vec3(0.0f, 1.0f, 0.0f), 0.012f, 2.0f, darkSteel);
        drawCube(pos + glm::vec3(0.0f, -0.05f, 0.0f), glm::vec3(0.34f, 0.18f, 0.34f), darkSteel);
        drawCube(pos + glm::vec3(0.0f, -0.12f, 0.0f), glm::vec3(0.22f, 0.14f, 0.22f), glm::vec4(1.0f, 0.92f, 0.70f, 1.0f));
    };
    drawHangingLamp(centerPos + glm::vec3(-2.2f, 3.4f, 0.8f));
    drawHangingLamp(centerPos + glm::vec3(2.2f, 3.4f, 0.8f));

    // 6. Tactical Weapon Pegboard & Gun Wall Rack (Directly behind operative on back wall)
    const glm::vec4 pegboardCol(0.14f, 0.15f, 0.18f, 1.0f);
    const glm::vec4 metalStrut(0.28f, 0.30f, 0.34f, 1.0f);
    drawCube(centerPos + glm::vec3(0.0f, 1.95f, -5.0f), glm::vec3(4.5f, 2.4f, 0.06f), pegboardCol);
    drawCube(centerPos + glm::vec3(-1.8f, 1.95f, -4.95f), glm::vec3(0.12f, 2.4f, 0.08f), metalStrut);
    drawCube(centerPos + glm::vec3(-0.6f, 1.95f, -4.95f), glm::vec3(0.12f, 2.4f, 0.08f), metalStrut);
    drawCube(centerPos + glm::vec3(0.6f, 1.95f, -4.95f), glm::vec3(0.12f, 2.4f, 0.08f), metalStrut);
    drawCube(centerPos + glm::vec3(1.8f, 1.95f, -4.95f), glm::vec3(0.12f, 2.4f, 0.08f), metalStrut);
    // Overhead LED light bar over the pegboard
    drawCube(centerPos + glm::vec3(0.0f, 3.20f, -4.85f), glm::vec3(4.6f, 0.08f, 0.25f), darkSteel);
    drawCube(centerPos + glm::vec3(0.0f, 3.16f, -4.85f), glm::vec3(4.2f, 0.03f, 0.18f), glm::vec4(1.0f, 0.94f, 0.85f, 1.0f));

    // Staged Firearms mounted on wall hooks (M4A1, AK-74M, Mosin-Nagant)
    drawWeapon("M4A1", centerPos + glm::vec3(-0.10f, 2.65f, -4.88f), 1.5708f, 0.0f);
    drawWeapon("AK-74M", centerPos + glm::vec3(0.10f, 2.05f, -4.88f), 1.5708f, 0.0f);
    drawWeapon("Mosin", centerPos + glm::vec3(-0.05f, 1.45f, -4.88f), 1.5708f, 0.0f);

    // 7. Tactical Gunsmith Workbench on Left
    const glm::vec4 woodWorktop(0.28f, 0.20f, 0.15f, 1.0f);
    const glm::vec4 benchSteel(0.18f, 0.20f, 0.22f, 1.0f);
    drawCube(centerPos + glm::vec3(-4.2f, 0.45f, -3.2f), glm::vec3(2.6f, 0.90f, 1.4f), benchSteel);
    drawCube(centerPos + glm::vec3(-4.2f, 0.92f, -3.2f), glm::vec3(2.8f, 0.08f, 1.5f), woodWorktop);
    // Bench vice on edge
    drawCube(centerPos + glm::vec3(-3.0f, 1.05f, -2.7f), glm::vec3(0.25f, 0.22f, 0.35f), glm::vec4(0.12f, 0.13f, 0.15f, 1.0f));
    // Field military radio receiver with antenna & glowing LED panel
    drawCube(centerPos + glm::vec3(-4.6f, 1.15f, -3.4f), glm::vec3(0.70f, 0.40f, 0.45f), glm::vec4(0.22f, 0.28f, 0.20f, 1.0f));
    drawCylinder(centerPos + glm::vec3(-4.8f, 1.70f, -3.5f), 0.010f, 0.85f, darkSteel);
    drawCube(centerPos + glm::vec3(-4.4f, 1.15f, -3.15f), glm::vec3(0.25f, 0.14f, 0.06f), glm::vec4(0.15f, 0.95f, 0.45f, 1.0f));
    // Ammo boxes and tools on workbench
    drawCube(centerPos + glm::vec3(-3.8f, 1.02f, -3.0f), glm::vec3(0.42f, 0.16f, 0.28f), glm::vec4(0.35f, 0.45f, 0.25f, 1.0f));
    drawCube(centerPos + glm::vec3(-3.4f, 0.98f, -3.3f), glm::vec3(0.35f, 0.12f, 0.22f), glm::vec4(0.65f, 0.50f, 0.20f, 1.0f));

    // 8. Stacked Munitions Crates & Pelican Hardcases on Right
    drawMilitaryCrate(centerPos + glm::vec3(3.8f, 0.0f, -3.4f), 0.15f, false);
    drawMilitaryCrate(centerPos + glm::vec3(3.8f, 0.75f, -3.4f), -0.10f, false);
    drawCube(centerPos + glm::vec3(4.2f, 0.22f, -1.8f), glm::vec3(1.10f, 0.45f, 0.70f), glm::vec4(0.16f, 0.18f, 0.15f, 1.0f));
    drawCube(centerPos + glm::vec3(4.2f, 0.48f, -1.8f), glm::vec3(0.95f, 0.12f, 0.55f), glm::vec4(0.75f, 0.45f, 0.15f, 1.0f));

    // 9. Sandbag Wall section on right side
    drawSandbagWall(centerPos + glm::vec3(4.6f, 0.0f, -0.2f), 1.5708f);
}

void Renderer::drawParticleBillboard(const glm::vec3& position, float size, const glm::vec4& color, float rotation, int particleType) {
    glm::vec3 right = m_camRight;
    glm::vec3 up = m_camUp;
    if (std::abs(rotation) > 0.001f) {
        float c = std::cos(rotation);
        float s = std::sin(rotation);
        glm::vec3 r2 = right * c + up * s;
        glm::vec3 u2 = -right * s + up * c;
        right = r2;
        up = u2;
    }
    float hs = size * 0.5f;
    glm::vec3 p0 = position - right * hs - up * hs;
    glm::vec3 p1 = position + right * hs - up * hs;
    glm::vec3 p2 = position + right * hs + up * hs;
    glm::vec3 p3 = position - right * hs + up * hs;

    float pType = static_cast<float>(particleType);
    ParticleVertex v0{p0, glm::vec2(0.0f, 0.0f), color, pType};
    ParticleVertex v1{p1, glm::vec2(1.0f, 0.0f), color, pType};
    ParticleVertex v2{p2, glm::vec2(1.0f, 1.0f), color, pType};
    ParticleVertex v3{p3, glm::vec2(0.0f, 1.0f), color, pType};

    if (particleType == 1 || particleType == 4) {
        m_additiveParticles.push_back(v0);
        m_additiveParticles.push_back(v1);
        m_additiveParticles.push_back(v2);
        m_additiveParticles.push_back(v0);
        m_additiveParticles.push_back(v2);
        m_additiveParticles.push_back(v3);
    } else {
        m_alphaParticles.push_back(v0);
        m_alphaParticles.push_back(v1);
        m_alphaParticles.push_back(v2);
        m_alphaParticles.push_back(v0);
        m_alphaParticles.push_back(v2);
        m_alphaParticles.push_back(v3);
    }
}

void Renderer::drawTracerBeam(const glm::vec3& start, const glm::vec3& end, float width, const glm::vec4& color) {
    glm::vec3 fwd = end - start;
    float len = glm::length(fwd);
    if (len < 0.001f) return;
    fwd /= len;

    glm::vec3 mid = 0.5f * (start + end);
    glm::vec3 camDir = glm::normalize(m_cameraPos - mid);
    glm::vec3 quadRight = glm::cross(camDir, fwd);
    if (glm::length(quadRight) < 0.001f) {
        quadRight = m_camRight;
    } else {
        quadRight = glm::normalize(quadRight);
    }

    float hw = width * 0.5f;
    glm::vec3 p0 = start - quadRight * hw;
    glm::vec3 p1 = start + quadRight * hw;
    glm::vec3 p2 = end   + quadRight * hw;
    glm::vec3 p3 = end   - quadRight * hw;

    float pType = 6.0f; // Tracer Ribbon
    ParticleVertex v0{p0, glm::vec2(0.0f, 0.0f), color, pType};
    ParticleVertex v1{p1, glm::vec2(1.0f, 0.0f), color, pType};
    ParticleVertex v2{p2, glm::vec2(1.0f, 1.0f), color, pType};
    ParticleVertex v3{p3, glm::vec2(0.0f, 1.0f), color, pType};

    m_additiveParticles.push_back(v0);
    m_additiveParticles.push_back(v1);
    m_additiveParticles.push_back(v2);
    m_additiveParticles.push_back(v0);
    m_additiveParticles.push_back(v2);
    m_additiveParticles.push_back(v3);
}

void Renderer::drawAimingRay(const glm::vec3& start, const glm::vec3& end, const glm::vec3& hitNormal, bool hitSurface, float maxRange, float spread, float heat) {
    glm::vec3 fwd = end - start;
    float len = glm::length(fwd);
    if (len < 0.15f) return;
    fwd /= len;

    glm::vec3 mid = 0.5f * (start + end);
    glm::vec3 camDir = glm::normalize(m_cameraPos - mid);
    glm::vec3 quadRight = glm::cross(camDir, fwd);
    if (glm::length(quadRight) < 0.001f) {
        quadRight = m_camRight;
    } else {
        quadRight = glm::normalize(quadRight);
    }
    glm::vec3 quadUp = glm::normalize(glm::cross(fwd, quadRight));

    // High-frequency mechanical vibration / recoil jitter on rapid spam
    glm::vec3 jitterVec(0.0f);
    if (heat > 0.04f) {
        float jMag = heat * (0.006f + spread * 0.02f);
        float jFreq = static_cast<float>(glfwGetTime()) * 65.0f;
        jitterVec = (quadRight * std::sin(jFreq) + quadUp * std::cos(jFreq * 1.37f)) * jMag;
    }

    // Sleek tactical ray thickness with expansion on recoil/spread
    float width = 0.016f + spread * 0.035f + heat * 0.012f;
    float hw = width * 0.5f;

    // Fade out ray as rapid-fire spam and bloom increase (down to 0.16 alpha)
    float baseAlpha = std::clamp(0.65f - heat * 0.45f - spread * 0.8f, 0.16f, 0.72f);
    glm::vec4 beamColor(0.72f, 0.74f, 0.78f, baseAlpha);

    glm::vec3 p0 = start - quadRight * hw;
    glm::vec3 p1 = start + quadRight * hw;
    glm::vec3 p2 = end   + quadRight * hw + jitterVec;
    glm::vec3 p3 = end   - quadRight * hw + jitterVec;

    float pType = 8.0f; // Tactical Aiming Ray

    ParticleVertex v0{p0, glm::vec2(0.0f, 0.0f), beamColor, pType};
    ParticleVertex v1{p1, glm::vec2(1.0f, 0.0f), beamColor, pType};
    ParticleVertex v2{p2, glm::vec2(1.0f, 1.0f), beamColor, pType};
    ParticleVertex v3{p3, glm::vec2(0.0f, 1.0f), beamColor, pType};

    m_alphaParticles.push_back(v0);
    m_alphaParticles.push_back(v1);
    m_alphaParticles.push_back(v2);
    m_alphaParticles.push_back(v0);
    m_alphaParticles.push_back(v2);
    m_alphaParticles.push_back(v3);

    // Thermal Incandescent Muzzle Crown: warm amber/orange glow at the barrel tip during rapid spam
    if (heat > 0.12f) {
        float crownLen = std::min(len * 0.35f, 0.85f);
        float crownHW = hw * 1.6f;
        glm::vec3 cEnd = start + fwd * crownLen;
        glm::vec4 glowColor(1.0f, 0.55f, 0.18f, std::min(0.55f, heat * 0.65f));

        ParticleVertex g0{start - quadRight * crownHW, glm::vec2(0.0f, 0.0f), glowColor, pType};
        ParticleVertex g1{start + quadRight * crownHW, glm::vec2(1.0f, 0.0f), glowColor, pType};
        ParticleVertex g2{cEnd  + quadRight * crownHW * 0.5f, glm::vec2(1.0f, 1.0f), glm::vec4(glowColor.r, glowColor.g, glowColor.b, 0.0f), pType};
        ParticleVertex g3{cEnd  - quadRight * crownHW * 0.5f, glm::vec2(0.0f, 1.0f), glm::vec4(glowColor.r, glowColor.g, glowColor.b, 0.0f), pType};

        m_alphaParticles.push_back(g0);
        m_alphaParticles.push_back(g1);
        m_alphaParticles.push_back(g2);
        m_alphaParticles.push_back(g0);
        m_alphaParticles.push_back(g2);
        m_alphaParticles.push_back(g3);
    }

    // If ray hits an obstacle, cover, or target, render a subtle tactical 3D impact pip on the surface
    if (hitSurface) {
        glm::vec3 norm = (glm::length(hitNormal) > 0.1f) ? glm::normalize(hitNormal) : glm::vec3(0, 1, 0);
        glm::vec3 pipCenter = end + norm * 0.008f + jitterVec;
        float pipAlpha = std::clamp(0.85f - heat * 0.40f, 0.25f, 0.85f);
        drawAimReticle(pipCenter, 0.038f + spread * 0.030f + heat * 0.015f, glm::vec4(0.85f, 0.88f, 0.92f, pipAlpha));
    }
}

void Renderer::drawDirectionalFlame(const glm::vec3& position, const glm::vec3& forwardDir, float length, float width, const glm::vec4& color, float seed) {
    glm::vec3 fwd = glm::normalize(forwardDir);
    glm::vec3 camDir = glm::normalize(m_cameraPos - position);
    glm::vec3 quadRight = glm::cross(camDir, fwd);
    if (glm::length(quadRight) < 0.001f) {
        quadRight = m_camRight;
    } else {
        quadRight = glm::normalize(quadRight);
    }

    float hw = width * 0.5f;
    glm::vec3 p0 = position - quadRight * hw;
    glm::vec3 p1 = position + quadRight * hw;
    glm::vec3 p2 = position + quadRight * hw + fwd * length;
    glm::vec3 p3 = position - quadRight * hw + fwd * length;

    float pType = 5.0f; // Directional supersonic flame lance
    ParticleVertex v0{p0, glm::vec2(0.0f, 0.0f), color, pType};
    ParticleVertex v1{p1, glm::vec2(1.0f, 0.0f), color, pType};
    ParticleVertex v2{p2, glm::vec2(1.0f, 1.0f), color, pType};
    ParticleVertex v3{p3, glm::vec2(0.0f, 1.0f), color, pType};

    m_additiveParticles.push_back(v0);
    m_additiveParticles.push_back(v1);
    m_additiveParticles.push_back(v2);
    m_additiveParticles.push_back(v0);
    m_additiveParticles.push_back(v2);
    m_additiveParticles.push_back(v3);
}

void Renderer::drawGroundDecal(const glm::vec3& position, float radius, const glm::vec4& color) {
    glm::vec3 p0 = position + glm::vec3(-radius, 0.02f, -radius);
    glm::vec3 p1 = position + glm::vec3( radius, 0.02f, -radius);
    glm::vec3 p2 = position + glm::vec3( radius, 0.02f,  radius);
    glm::vec3 p3 = position + glm::vec3(-radius, 0.02f,  radius);

    float pType = 3.0f;
    ParticleVertex v0{p0, glm::vec2(0.0f, 0.0f), color, pType};
    ParticleVertex v1{p1, glm::vec2(1.0f, 0.0f), color, pType};
    ParticleVertex v2{p2, glm::vec2(1.0f, 1.0f), color, pType};
    ParticleVertex v3{p3, glm::vec2(0.0f, 1.0f), color, pType};

    m_alphaParticles.push_back(v0);
    m_alphaParticles.push_back(v1);
    m_alphaParticles.push_back(v2);
    m_alphaParticles.push_back(v0);
    m_alphaParticles.push_back(v2);
    m_alphaParticles.push_back(v3);
}

void Renderer::drawBloodDecal(const glm::vec3& position, float radius, const glm::vec4& color) {
    glm::vec3 p0 = position + glm::vec3(-radius, 0.022f, -radius);
    glm::vec3 p1 = position + glm::vec3( radius, 0.022f, -radius);
    glm::vec3 p2 = position + glm::vec3( radius, 0.022f,  radius);
    glm::vec3 p3 = position + glm::vec3(-radius, 0.022f,  radius);

    float pType = 7.0f; // High-Detail Arterial Blood Pool
    ParticleVertex v0{p0, glm::vec2(0.0f, 0.0f), color, pType};
    ParticleVertex v1{p1, glm::vec2(1.0f, 0.0f), color, pType};
    ParticleVertex v2{p2, glm::vec2(1.0f, 1.0f), color, pType};
    ParticleVertex v3{p3, glm::vec2(0.0f, 1.0f), color, pType};

    m_alphaParticles.push_back(v0);
    m_alphaParticles.push_back(v1);
    m_alphaParticles.push_back(v2);
    m_alphaParticles.push_back(v0);
    m_alphaParticles.push_back(v2);
    m_alphaParticles.push_back(v3);
}

void Renderer::drawShellCasing(const glm::vec3& position, const glm::vec3& rotation, float radius, float length, bool atRest, bool isHeavy) {
    glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
    if (atRest) {
        model = glm::rotate(model, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, 1.57079f, glm::vec3(1.0f, 0.0f, 0.0f));
    } else {
        model = glm::rotate(model, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    }
    model = glm::scale(model, glm::vec3(radius * 2.0f, length, radius * 2.0f));
    glm::vec4 brassColor = isHeavy ? glm::vec4(0.92f, 0.76f, 0.28f, 0.70f) : glm::vec4(0.96f, 0.82f, 0.34f, 0.70f);
    drawMesh(*m_cylinderMesh, model, brassColor);
}

void Renderer::flushParticles() {
    if (m_alphaParticles.empty() && m_additiveParticles.empty()) return;

    m_particleShader->bind();
    m_particleShader->setMat4("uViewProj", m_viewProjMatrix);

    glBindVertexArray(m_particleVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_particleVbo);

    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    if (!m_alphaParticles.empty()) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBufferData(GL_ARRAY_BUFFER, m_alphaParticles.size() * sizeof(ParticleVertex), m_alphaParticles.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_alphaParticles.size()));
        m_alphaParticles.clear();
    }

    if (!m_additiveParticles.empty()) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glBufferData(GL_ARRAY_BUFFER, m_additiveParticles.size() * sizeof(ParticleVertex), m_additiveParticles.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_additiveParticles.size()));
        m_additiveParticles.clear();
    }

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(0);
}

void Renderer::rebuildTerrainMesh(bool isTraining) {
    if (isTraining) {
        m_trainingGroundMesh = ProceduralMesh::createTrainingTerrain(650.0f, 650.0f, 260);
    } else {
        m_groundMesh = ProceduralMesh::createWarzoneTerrain(2600.0f, 3800.0f, 260, 380);
    }
}

} // namespace Mercenary
