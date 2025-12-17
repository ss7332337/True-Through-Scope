// 全屏三角形顶点着色器
struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// 顶点着色器 - 生成全屏三角形（使用SV_VertexID，无需顶点缓冲区）
VSOutput main(uint vertexID : SV_VertexID)
{
    VSOutput output;
    
    // 生成全屏三角形的顶点
    // 顶点0: (-1, -1)  纹理坐标: (0, 1)
    // 顶点1: (-1,  3)  纹理坐标: (0, -1)  
    // 顶点2: ( 3, -1)  纹理坐标: (2, 1)
    output.texCoord = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0, 1);
    
    return output;
}
