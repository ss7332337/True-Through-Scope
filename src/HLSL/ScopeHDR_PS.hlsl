// ScopeHDR_PS.hlsl
// 自定义 HDR Tonemapping shader，用于瞄具第二次渲染
// 基于 Fallout 4 原版 ImageSpaceEffectHDR 的 Uncharted 2 Filmic Tonemapping 算法

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// HDR 常量缓冲区 - 必须与 C++ ScopeHDRConstants 结构体完全匹配
cbuffer HDRConstants : register(b0)
{
    // cb0[0]: HDR 曝光参数
    float MaxExposure; // cb0[0].x = 15.0 - 最大曝光值
    float MinExposure; // cb0[0].y = 0.5  - 最小曝光值
    float MiddleGray; // cb0[0].z = 0.18 - 中间灰度值 (曝光key)
    float WhitePoint; // cb0[0].w = 0.02 - 白点参数

    // cb0[1]: 颜色调整参数
    float Saturation; // cb0[1].x = 1.0  - 饱和度
    float _Padding1; // cb0[1].y
    float BlendIntensity; // cb0[1].z = 1.0  - 混合强度
    float Contrast; // cb0[1].w = 1.0  - 对比度

    // cb0[2]: 色调调整
    float4 ColorTint; // cb0[2] = (0,0,0,0) - 色调调整 (RGB + Weight)

    // cb0[3]: UV 缩放和曝光乘数
    float2 BloomUVScale; // cb0[3].xy = (1,1) - Bloom UV 缩放
    float ExposureMultiplier; // cb0[3].z = 1.0 - 自动曝光乘数 (用于调整瞄具场景亮度)
    float _Padding3; // cb0[3].w

    // cb0[4]: 控制参数
    float BloomStrength; // cb0[4].x = 1.0  - Bloom 混合强度
    float FixedExposure; // cb0[4].y = 1.5  - 固定曝光值 (如果 > 0 则使用固定值)
    int SkipHDRTonemapping; // cb0[4].z = 1    - 是否跳过 HDR tonemapping，只应用 Color Grading
    int ApplyColorGrading; // cb0[4].w = 1    - 是否应用 Color Grading/LUT

    // cb0[5]: LUT 混合权重
    float LUTBlendWeight0; // cb0[5].x - LUT 0 权重
    float LUTBlendWeight1; // cb0[5].y - LUT 1 权重
    float LUTBlendWeight2; // cb0[5].z - LUT 2 权重
    float LUTBlendWeight3; // cb0[5].w - LUT 3 权重
};

// 2D 纹理
Texture2D BloomTexture : register(t0); // Bloom 纹理
Texture2D SceneTexture : register(t1); // 场景纹理 (瞄具渲染结果)
Texture2D LuminanceTexture : register(t2); // 亮度/曝光纹理
Texture2D MaskTexture : register(t3); // Mask 纹理

// 3D LUT 纹理 (Color Grading)
Texture3D LUTTexture0 : register(t4);
Texture3D LUTTexture1 : register(t5);
Texture3D LUTTexture2 : register(t6);
Texture3D LUTTexture3 : register(t7);

// 采样器
SamplerState BloomSampler : register(s0);
SamplerState SceneSampler : register(s1);
SamplerState LuminanceSampler : register(s2);
SamplerState MaskSampler : register(s3);
SamplerState LUTSampler : register(s4); // 3D LUT 采样器


// ===========================
// Tunables (no C++ change)
// ===========================

// LUT 输入能量补偿（让色调回到原版偏暖的感觉）
// 建议范围：1.06 ~ 1.12
static const float LUTInputBoost = 1.1f;


// Uncharted 2 Filmic Tonemapping 函数
float3 FilmicTonemap(float3 color, float whitePoint)
{
    float3 x = color * 2.0;

    // 分子: (2*color) * (0.3*color + 0.05) + 0.2*W
    float3 numerator = x * (0.3 * color + 0.05) + 0.2 * whitePoint;

    // 分母: (2*color) * (0.3*color + 0.5) + 0.06
    float3 denominator = x * (0.3 * color + 0.5) + 0.06;

    denominator = max(denominator, 0.0001);

    float3 result = numerator / denominator;

    // 减去偏移: result - W * 3.333
    result = result - whitePoint * 3.333333;

    // 白点归一化
    float whiteScale = (whitePoint * 0.2 + 19.375999) * 0.040856 - whitePoint * 3.333333;
    whiteScale = 1.0 / max(abs(whiteScale), 0.0001);

    return result * whiteScale;
}


// Color Grading / LUT 应用（线性输入）
float3 ApplyColorGradingLUT_Linear(float3 linearColor)
{
    // LUT 坐标映射（原版风格的边缘偏移）
    float3 lutCoord = saturate(linearColor) * 0.9375 + 0.03125;

    float3 lut0 = LUTTexture0.Sample(LUTSampler, lutCoord).rgb;
    float3 lut1 = LUTTexture1.Sample(LUTSampler, lutCoord).rgb;
    float3 lut2 = LUTTexture2.Sample(LUTSampler, lutCoord).rgb;
    float3 lut3 = LUTTexture3.Sample(LUTSampler, lutCoord).rgb;

    float3 result =
        lut0 * LUTBlendWeight0 +
        lut1 * LUTBlendWeight1 +
        lut2 * LUTBlendWeight2 +
        lut3 * LUTBlendWeight3;

    return result;
}

// 仅 Gamma 校正 (当没有 LUT 纹理时使用)
float3 ApplyGammaOnly(float3 color)
{
    return pow(saturate(color), 1.0 / 2.2);
}


float4 main(VSOutput input) : SV_TARGET
{
    float2 uv = input.texCoord;

    // 采样场景颜色
    float4 sceneColorWithAlpha = SceneTexture.Sample(SceneSampler, uv);
    float3 sceneColor = sceneColorWithAlpha.rgb;

    // 跳过 HDR Tonemapping 模式: 只应用 Color Grading
    if (SkipHDRTonemapping)
    {
        if (ApplyColorGrading)
        {
            float totalWeight = LUTBlendWeight0 + LUTBlendWeight1 + LUTBlendWeight2 + LUTBlendWeight3;
            if (totalWeight > 0.001)
            {
                // ✅ 修复：Skip 模式也使用“线性输入 LUT”（不做 Gamma→LUT）
                float3 linearIn = sceneColor;

                // ✅ 色调补偿：LUT 前抬一点中亮度（让暖色回来）
                linearIn = max(linearIn, 0.0) * LUTInputBoost;

                float3 graded = ApplyColorGradingLUT_Linear(linearIn);

                // ✅ 最后统一做一次 gamma（避免重复 gamma 导致偏冷/偏灰）
                graded = ApplyGammaOnly(graded);

                return float4(graded, sceneColorWithAlpha.a);
            }
            else
            {
                // 没有 LUT，只应用 Gamma 校正
                float3 outCol = ApplyGammaOnly(sceneColor);
                return float4(outCol, sceneColorWithAlpha.a);
            }
        }
        else
        {
            // 不做 ColorGrading，也不额外 gamma（保持原样）
            return sceneColorWithAlpha;
        }
    }

    // 完整 HDR Tonemapping 模式
    float2 bloomUV = uv * BloomUVScale;
    float3 bloomColor = BloomTexture.Sample(BloomSampler, bloomUV).rgb;

    // 计算曝光值
    float exposure;
    if (FixedExposure > 0.0)
    {
        exposure = FixedExposure;
    }
    else
    {
        // 自动曝光: 从 luminance 纹理计算
        float luminance = LuminanceTexture.Sample(LuminanceSampler, float2(0.5, 0.5)).r;
        exposure = MiddleGray / max(luminance + 0.001, 0.0001);
        exposure = clamp(exposure, MinExposure, MaxExposure);
        exposure *= ExposureMultiplier;
    }

    // HDR 混合: 场景 + Bloom
    float3 hdrColor = (sceneColor + bloomColor * BloomStrength) * exposure;

    // Filmic Tonemapping
    float3 tonemappedColor = FilmicTonemap(hdrColor, WhitePoint);

    // 饱和度调整
    float lum = dot(tonemappedColor, float3(0.2125, 0.7154, 0.0721));
    float3 saturatedColor = lerp(lum.xxx, tonemappedColor, Saturation);

    // 色调调整
    float3 tintedColor = saturatedColor;
    if (ColorTint.w > 0.0)
    {
        float3 tintResult = lum * ColorTint.rgb;
        tintedColor = lerp(saturatedColor, tintResult, ColorTint.w);
    }

    // 对比度和混合强度
    float3 finalColor = tintedColor * Contrast;
    finalColor = lerp(sceneColor, finalColor, BlendIntensity);

    // Color Grading / LUT
    if (ApplyColorGrading)
    {
        float totalWeight = LUTBlendWeight0 + LUTBlendWeight1 + LUTBlendWeight2 + LUTBlendWeight3;
        if (totalWeight > 0.001)
        {
            // ✅ 色调补偿：LUT 前抬一点中亮度（让暖色回来）
            float3 linearIn = max(finalColor, 0.0) * LUTInputBoost;

            finalColor = ApplyColorGradingLUT_Linear(linearIn);
        }
        else
        {
            // 没有 LUT，则仅 gamma
            finalColor = ApplyGammaOnly(finalColor);
            return float4(finalColor, 1.0);
        }
    }

    // ✅ 最终统一做一次 gamma（只做一次）
    finalColor = ApplyGammaOnly(finalColor);
    return float4(finalColor, 1.0);
}
