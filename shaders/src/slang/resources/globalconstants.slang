
implementing resources;

public struct PushConstants {
    public float4x4 renderMatrix;
    public ConstBufferPointer<Vertex> vertices;
    public ConstBufferPointer<Material> materials;
    public ConstBufferPointer<Light> lights;
    public uint materialIndex;
    public uint numLights;
};

public [vk::push_constant] ConstantBuffer<PushConstants> pushConstants;

public struct SceneData {
    public float4x4 view;
    public float4x4 projection;
    public float3 cameraPosition;
};

public layout(binding = 0) ConstantBuffer<SceneData> sceneData;
