#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_shading_language_include : require
#include "resources.h"

layout (location = 0) in vec4 inColor;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;
layout (location = 3) in vec3 inFragPosition;

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 1) uniform sampler2D Textures[];

float dTrowbridgeReitzGGX(vec3 normal, vec3 halfway, float roughness) {
    float numerator = roughness * roughness;
    float normalHalfwayIncidence = max(dot(normal, halfway), 0.0f);
    float nHI2 = pow(normalHalfwayIncidence, 2);

    float denomimator = (nHI2 * (numerator - 1.0f) + 1.0f);
    denomimator = PI * denomimator * denomimator;

    return numerator / denomimator;
}

float GShlickGGX(float normalViewIncidence, float roughness) {
    float denominator = normalViewIncidence * (1.0f - roughness) + roughness;
    return normalViewIncidence / denominator;
}

float GSmith(vec3 normal, vec3 view, vec3 light, float roughness) {
    float normalViewIncidence = max(dot(normal, view), 0.0f);
    float normalLightIncidence = max(dot(normal, light), 0.0f);
    float ggx1 = GShlickGGX(normalViewIncidence, roughness);
    float ggx2 = GShlickGGX(normalLightIncidence, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float normalHalfwayIncidence, vec3 F0) {
    return F0 + (1.0f - F0) * pow(1.0f - normalHalfwayIncidence, 5.0f);
}

void main() {
    MaterialBuffer mb = PushConstants.materialBuffer;
    PointLightBuffer lb = PushConstants.pointLightBuffer;
    Material material = mb.materials[PushConstants.materialIndex];
    vec4 baseColourTextureColour = texture(Textures[material.baseColorTexture], inUV);
    vec3 albedo = baseColourTextureColour.rgb;

    vec4 mrTexture = texture(Textures[material.mrTexture], inUV);
    float metalness = mrTexture.b;
    float roughness = mrTexture.g;

    vec4 aoTexture = texture(Textures[material.occlusionTexture], inUV);

    vec3 normal = normalize(inNormal);
    vec3 view = normalize(sceneData.cameraPosition - inFragPosition);

    vec3 F0 = vec3(0.04f);
    F0 = mix(F0, albedo, metalness);

    vec3 Lo = vec3(0.0f);

    for (uint i = 0; i < PushConstants.numPointLights; i++) {
        PointLight currentLight = lb.lights[i];
        vec3 lightDirection = normalize(currentLight.position - inFragPosition);
        vec3 halfway = normalize(view + lightDirection);

        float distance = length(currentLight.position - inFragPosition);
        float attenuation = currentLight.intensity / ((distance * distance) + 0.0001f);
        vec3 radiance = currentLight.color * attenuation;

        float NDF = dTrowbridgeReitzGGX(normal, halfway, roughness);
        float G = GSmith(normal, view, lightDirection, roughness);
        vec3 F = fresnelSchlick(max(dot(halfway, view), 0.0f), F0);

        vec3 kSpecular = F;
        vec3 kDiffuse = vec3(1.0f) - kSpecular;
        kDiffuse *= 1.0f - metalness;

        float normalLightIncidence = max(dot(normal, lightDirection), 0.0f);
        vec3 numerator = NDF * G * F;
        float denominator = 4.0f * max(dot(normal, view), 0.0f) * normalLightIncidence + 0.0001f;
        vec3 specular = numerator / denominator;

        Lo += ((kDiffuse * albedo / PI + specular) * radiance * normalLightIncidence);
    }

    vec3 ambient = vec3(0.0007f) * albedo;
    vec3 color = ambient + Lo;

    color = color / (color + vec3(1.0f));
    color = pow(color, vec3(1.0f / 2.2f));

    outFragColor = vec4(color, 1.0f);
}