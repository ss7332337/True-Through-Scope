// BloomAccumulate_PS.hlsl
// 复现原版游戏 Bloom 累加 pass (Shader hash a8c2404f)
// 4-tap 加权累加 + INF 值过滤

// 常量缓冲区 - 对应 BloomAccumulateConstants
cbuffer BloomAccumulateConstants : register(b0)
{
    // Bloom 累加偏移和权重 (cb2[8-11])
    float4 BloomOffsetWeight[4];  // xy=UV偏移, z=权重, w=unused

    // 全局参数
    float BloomUVScaleX;
    float BloomUVScaleY;
    float TargetSizeX;  // 1/width
    float TargetSizeY;  // 1/height

    float BloomMultiplier;
    float3 _Padding;
};

// 纹理和采样器
Texture2D BloomTexture0 : register(t0);
Texture2D BloomTexture1 : register(t1);
Texture2D BloomTexture2 : register(t2);
Texture2D BloomTexture3 : register(t3);

SamplerState LinearSampler : register(s0);

// 输入结构
struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// 检查是否为 INF (原版 shader 中的 0x7f800000 检查)
float3 FilterInf(float3 value)
{
    // HLSL 的 isinf() 函数
    return isinf(value) ? float3(0, 0, 0) : value;
}

float4 main(VSOutput input) : SV_TARGET
{
    float3 result = float3(0, 0, 0);
    float2 uvScale = float2(BloomUVScaleX, BloomUVScaleY);
    float2 targetSize = float2(TargetSizeX, TargetSizeY);

    // 4-tap 累加循环
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        // 计算采样 UV
        float2 uv = input.texCoord + BloomOffsetWeight[i].xy * targetSize;
        uv *= uvScale;

        // 从对应的 mip 级别采样 (这里简化为只用第一个纹理)
        float3 sampleColor;
        if (i == 0)
            sampleColor = BloomTexture0.Sample(LinearSampler, uv).rgb;
        else if (i == 1)
            sampleColor = BloomTexture1.Sample(LinearSampler, uv).rgb;
        else if (i == 2)
            sampleColor = BloomTexture2.Sample(LinearSampler, uv).rgb;
        else
            sampleColor = BloomTexture3.Sample(LinearSampler, uv).rgb;

        // INF 过滤 (复现原版 shader 中的检查)
        sampleColor = FilterInf(sampleColor);

        // 加权累加
        result += sampleColor * BloomOffsetWeight[i].z;
    }

    return float4(result, BloomMultiplier);
}
