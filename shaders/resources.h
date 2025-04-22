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

struct DirectionalLight {
    vec3 direction;
    vec3 color;
};

struct PointLight {
  vec3 position;
  vec3 color;
  float intensity;
  float quadratic;
};

struct SpotLight {
  vec3 position;
  vec3 direction;
  vec3 color;
  float intensity;
  float quadratic;
  float innerAngle;
  float outerAngle;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer MaterialBuffer {
    Material materials[];
};

layout(buffer_reference, std430) readonly buffer DirectionalLightBuffer {
    DirectionalLight lights[];
};

layout(buffer_reference, std430) readonly buffer PointLightBuffer {
    PointLight lights[];
};

layout(buffer_reference, std430) readonly buffer SpotLightBuffer {
    SpotLight lights[];
};

layout( push_constant ) uniform constants {
    mat4 renderMatrix;
    VertexBuffer vertexBuffer;
    MaterialBuffer materialBuffer;
    DirectionalLightBuffer directionalLightBuffer;
    PointLightBuffer pointLightBuffer;
    SpotLightBuffer spotLightBuffer;
    uint materialIndex;
    uint numDirLights;
    uint numPointLights;
    uint numSpotLights;
} PushConstants;

layout(binding = 0) uniform SceneData {
    mat4 view;
    mat4 projection;
    vec3 cameraPosition;
} sceneData;



