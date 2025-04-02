#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_shading_language_include : require
#include "resources.h"

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec2 outUV;
layout (location = 2) out vec3 outNormal;
layout (location = 3) out vec3 outFragPosition;

void main() {
    VertexBuffer vb = PushConstants.vertexBuffer;
    Vertex v = vb.vertices[gl_VertexIndex];
    vec4 position = vec4(v.position, 1.0f);

    gl_Position = sceneData.projection * sceneData.view * PushConstants.renderMatrix * position;

    outNormal = v.normal;
    outColor = v.color;
    outUV = vec2(v.uv_x, v.uv_y);
    outFragPosition = vec3(PushConstants.renderMatrix * position);
}