#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require
#define PI 3.1415926538

struct Vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
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

const uint Directional = 0x00000001u;
const uint Point = 0x00000002u;
const uint Spot = 0x00000004u;

struct Light {
    vec3 direction;
    vec3 colour;
    float intensity;
    float range;
    float innerAngle;
    float outerAngle;
    uint type;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer MaterialBuffer {
    Material materials[];
};

layout(buffer_reference, std430) readonly buffer LightBuffer {
    Light lights[];
};

layout( push_constant ) uniform constants {
    mat4 renderMatrix;
    VertexBuffer vertexBuffer;
    MaterialBuffer materialBuffer;
    LightBuffer lightBuffer;
    uint materialIndex;
    uint numLights;
} PushConstants;

layout(binding = 0) uniform SceneData {
    mat4 view;
    mat4 projection;
    vec3 cameraPosition;
} sceneData;



