// MirrorShader.hlsl
cbuffer MirrorCB : register(b0)
{
    matrix World;
    matrix View;
    matrix Projection;
};

struct PS_IN
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

struct VS_IN
{
    float3 Pos : POSITION;
    float2 Tex : TEXCOORD0;
};

// 顶点着色器
PS_IN main(VS_IN input)
{
    PS_IN output;
    
    float4 worldPos = mul(float4(input.Pos, 1.0f), World);
    float4 viewPos = mul(worldPos, View);
    output.Pos = mul(viewPos, Projection);
    
    output.Tex = input.Tex;
    return output;
}
