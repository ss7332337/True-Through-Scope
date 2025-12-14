// ScopeHDR_VS.hlsl
// 全屏三角形顶点着色器 - 用于 HDR 后处理

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// 使用 SV_VertexID 生成全屏三角形，无需顶点缓冲区
VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output;
    output.texCoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0, 1);
    return output;
}
