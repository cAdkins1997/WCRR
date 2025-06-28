
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

#define PI 3.1415926538

struct DrawCommand
{
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    uint vertexOffset;
    uint firstInstance;
};

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

struct AABB
{
    vec3 bvMin;
    vec3 bvMax;
};

struct Surface
{
    AABB boundingVolume;
    uint initialIndex;
    uint indexCount;
    uint materialIndex;
};

struct Material {
    vec4 baseColorFactor;
    float metalnessFactor;
    float roughnessFactor;
    float emissiveStrength;

    uint baseColorTexture;
    uint mrTexture;
    uint normalTexture;
    uint occlusionTexture;
    uint emissiveTexture;
};

struct Light {
    vec3 direction;
    vec3 colour;
    float intensity;
    float range;
    float innerAngle;
    float outerAngle;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer MaterialBuffer {
    Material materials[];
};

layout(buffer_reference, scalar) readonly buffer CommandBuffer {
    DrawCommand commands[];
};

layout(buffer_reference, scalar) readonly buffer SurfaceBuffer {
    Surface surfaces[];
};

layout(buffer_reference, std430) readonly buffer LightBuffer {
    Light lights[];
};

layout( push_constant ) uniform constants {
    mat4 renderMatrix;
    VertexBuffer vertexBuffer;
    MaterialBuffer materialBuffer;
    LightBuffer lightBuffer;
    CommandBuffer commandBuffer;
    SurfaceBuffer surfaceBuffer;
    uint materialIndex;
    uint numLights;
} PushConstants;

layout(binding = 0) uniform SceneData {
    vec4 frustum[5];
    mat4 view;
    mat4 projection;
    vec3 cameraPosition;
    uint drawCount;
} sceneData;



