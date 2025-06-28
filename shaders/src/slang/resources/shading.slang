
implementing resources;

public struct Material {
    public float4 baseColorFactor;
    public float metalnessFactor;
    public float roughnessFactor;
    public float emissiveStrength;
    public uint baseColorTexture;
    public uint mrTexture;
    public uint normalTexture;
    public uint occlusionTexture;
    public uint emissiveTexture;
};

public struct Light {
    public float3 direction;
    public float3 colour;
    public float intensity;
    public float range;
    public float innerAngle;
    public float outerAngle;
};

public layout(binding = 1) Sampler2D textures[];
