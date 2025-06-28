
import resources;

[shader("vertex")]
VSOutput vertexMain(uint vertexID : SV_VertexID) {
    Vertex v = pushConstants.vertices[vertexID];
    VSOutput output;
    output.normal = v.normal;
    output.color = v.color;
    output.uv = float2(v.uv_x, v.uv_y);
    output.fragPosition = float3(v.position);

    float4 position = float4(v.position, 1.0);
    float4x4 viewProjection = sceneData.projection * sceneData.view;
    output.transformedPosition = mul(viewProjection, position);

    return output;
}