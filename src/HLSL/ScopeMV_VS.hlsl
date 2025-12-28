// Scope Motion Vector Calculation Vertex Shader
// 简单的全屏三角形顶点着色器

struct VSInput
{
    uint VertexID : SV_VertexID;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    
    // 使用 VertexID 生成全屏三角形
    // 顶点 0: (-1, -1) -> TexCoord (0, 1)
    // 顶点 1: (-1,  3) -> TexCoord (0, -1)  [会被裁剪]
    // 顶点 2: ( 3, -1) -> TexCoord (2, 1)   [会被裁剪]
    float2 texCoord = float2((input.VertexID << 1) & 2, input.VertexID & 2);
    output.TexCoord = texCoord;
    output.Position = float4(texCoord * 2.0 - 1.0, 0.0, 1.0);
    output.Position.y = -output.Position.y; // 翻转 Y
    
    return output;
}
