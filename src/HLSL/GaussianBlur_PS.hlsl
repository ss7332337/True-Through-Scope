// GaussianBlur_PS.hlsl
// 复现原版游戏 7-tap 可分离高斯模糊 (Shader hash bfa8841e)
// 用于水平和垂直两次 pass

// 常量缓冲区 - 对应 GaussianBlurConstants
cbuffer GaussianBlurConstants : register(b0)
{
    // 模糊偏移和权重 (cb2[2-8])
    float4 BlurOffsetWeight[7];  // xy=UV偏移, z=权重, w=unused

    float4 _Padding;
};

// 纹理和采样器
Texture2D InputTexture : register(t0);
SamplerState LinearSampler : register(s0);

// 输入结构
struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET
{
    float4 result = float4(0, 0, 0, 0);

    // 7-tap 高斯模糊
    [unroll]
    for (int i = 0; i < 7; i++)
    {
        float2 uv = input.texCoord + BlurOffsetWeight[i].xy;
        float4 sampleColor = InputTexture.Sample(LinearSampler, uv);
        result += sampleColor * BlurOffsetWeight[i].z;
    }

    return result;
}
