
implementing resources;

public struct VSOutput {
    public float4 color;
    public float3 normal;
    public float3 fragPosition;
    public float4 transformedPosition : SV_Position;
    public float2 uv;
};

public struct Vertex {
    public float3 position;
    public float uv_x;
    public float3 normal;
    public float uv_y;
    public float4 color;
};
