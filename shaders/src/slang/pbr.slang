
import resources;
#define PI 3.1415926538

[shader("pixel")]
float4 pixelMain(VSOutput input) : SV_Target {
    float3 color = input.color.rgb;
    float3 normal = normalize(input.normal);
    float2 uv = input.uv;
    float3 fragPosition = input.fragPosition;
    float3 view = normalize(sceneData.cameraPosition - fragPosition);

    Material material = pushConstants.materials[pushConstants.materialIndex];
    Sampler2D baseColorTexture = textures[NonUniformResourceIndex(material.baseColorTexture)];
    float4 baseColour = baseColorTexture.Sample(uv);
    float3 albedo = baseColour.rgb;

    Sampler2D mrTexture = textures[NonUniformResourceIndex(material.mrTexture)];
    float4 mr = mrTexture.Sample(uv);
    float metalness = mr.b;
    float roughness = mr.g;

    float3 F0 = float3(0.04f);
    F0 = lerp(F0, albedo, metalness);

    float3 Lo = float3(0.0f);

    for (uint i = 0; i < pushConstants.numLights; i++) {
        Light currentLight = pushConstants.lights[i];
        if (currentLight.intensity != 0.0f) {
            float3 lightDirection = normalize(currentLight.direction - fragPosition);
            float3 halfway = normalize(view + lightDirection);

            float distance = length(currentLight.direction - fragPosition);

            float attenuation = currentLight.intensity / ((distance * distance) + 0.000001f);
            float3 radiance = currentLight.colour * attenuation;

            float NDF = dTrowbridgeReitzGGX(normal, halfway, roughness);
            float G = GSmith(normal, view, lightDirection, roughness);
            float3 F = fresnelSchlick(max(dot(halfway, view), 0.0f), F0);

            float3 kSpecular = F;
            float3 kDiffuse = float3(1.0f) - kSpecular;

            float normalLightIncidence = max(dot(normal, lightDirection), 0.0f);
            float3 numerator = NDF * G * F;
            float denominator = 4.0f * max(dot(normal, view), 0.0f) * normalLightIncidence + 0.0001f;
            float3 specular = numerator / denominator;

            Lo += ((kDiffuse * albedo / PI + specular) * radiance * normalLightIncidence);
        }
    }

        float3 ambient = 0.0000001f * albedo;
        color = ambient + Lo;

        color = color / (color + float3(1.0f));
        color = pow(color, float3(1.0f / 2.2f));

        return float4(color, 1.0f);
}

float dTrowbridgeReitzGGX(float3 normal, float3 halfway, float roughness) {
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

float GSmith(float3 normal, float3 view, float3 light, float roughness) {
    float normalViewIncidence = max(dot(normal, view), 0.0f);
    float normalLightIncidence = max(dot(normal, light), 0.0f);
    float ggx1 = GShlickGGX(normalViewIncidence, roughness);
    float ggx2 = GShlickGGX(normalLightIncidence, roughness);
    return ggx1 * ggx2;
}

float3 fresnelSchlick(float normalHalfwayIncidence, float3 F0) {
    return F0 + (1.0f - F0) * pow(1.0f - normalHalfwayIncidence, 5.0f);
}