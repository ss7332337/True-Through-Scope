// LUT Color Grading Pixel Shader
// 复现原版游戏的 LUT pass (Shader hash 7f725af5)
//
// 流程:
// 1. 采样输入纹理
// 2. 应用 gamma 校正 (pow 1/2.2 = 0.454545)
// 3. 缩放到 LUT 范围 (16x16x16 LUT: * 0.9375 + 0.03125)
// 4. 采样并混合 4 个 LUT
// 5. 输出最终颜色

// 常量缓冲区
cbuffer LUTConstants : register(b0)
{
    float4 LUTWeights;  // xyzw = LUT0, LUT1, LUT2, LUT3 的权重
};

// 纹理和采样器
Texture2D<float4> InputTexture : register(t0);      // HDR pass 输出
Texture3D<float4> LUT0 : register(t3);              // LUT 纹理 0
Texture3D<float4> LUT1 : register(t4);              // LUT 纹理 1
Texture3D<float4> LUT2 : register(t5);              // LUT 纹理 2
Texture3D<float4> LUT3 : register(t6);              // LUT 纹理 3

SamplerState LinearSampler : register(s0);
SamplerState LUTSampler0 : register(s3);
SamplerState LUTSampler1 : register(s4);
SamplerState LUTSampler2 : register(s5);
SamplerState LUTSampler3 : register(s6);

// 顶点着色器输出
struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET
{
    // 采样输入纹理
    float4 color = InputTexture.Sample(LinearSampler, input.TexCoord);

    // 保留 alpha
    float alpha = color.a;

    // 应用 gamma 校正: pow(color, 1/2.2) = pow(color, 0.454545)
    // 使用 log/exp 方式实现 (与原版 shader 一致)
    float3 logColor = log(max(color.rgb, 0.00001));  // 避免 log(0)
    logColor *= 0.454545;  // 1/2.2
    float3 gammaColor = exp(logColor);

    // 缩放到 LUT 范围
    // 16x16x16 LUT: 实际范围是 0.5/16 到 15.5/16
    // 公式: color * (15/16) + (0.5/16) = color * 0.9375 + 0.03125
    float3 lutCoord = gammaColor * 0.9375 + 0.03125;

    // 采样所有 LUT 并按权重混合
    float3 result = float3(0, 0, 0);

    // LUT0 (t3)
    if (LUTWeights.x > 0.0)
    {
        result += LUT0.Sample(LUTSampler0, lutCoord).rgb * LUTWeights.x;
    }

    // LUT1 (t4)
    if (LUTWeights.y > 0.0)
    {
        result += LUT1.Sample(LUTSampler1, lutCoord).rgb * LUTWeights.y;
    }

    // LUT2 (t5)
    if (LUTWeights.z > 0.0)
    {
        result += LUT2.Sample(LUTSampler2, lutCoord).rgb * LUTWeights.z;
    }

    // LUT3 (t6)
    if (LUTWeights.w > 0.0)
    {
        result += LUT3.Sample(LUTSampler3, lutCoord).rgb * LUTWeights.w;
    }

    return float4(result, alpha);
}
