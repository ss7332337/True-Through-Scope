// ============================================================
// ScopeHDR_PS.hlsl
// HDR Tonemapping Pass (Shader hash dd14b583)
//
// 只做 HDR 处理，不包含 LUT Color Grading
// LUT 由单独的 LUT_PS.hlsl 处理
// ============================================================

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// ============================================================
// Constant Buffer (matches original dd14b583 layout)
// ============================================================

cbuffer HDRConstants : register(b0)
{
    // cb2[1]
    float MaxExposure;      // cb2[1].x - 曝光上限
    float MinExposure;      // cb2[1].y - 曝光下限
    float MiddleGray;       // cb2[1].z - 中间灰
    float WhitePoint;       // cb2[1].w - 白点 (Filmic tonemap)

    // cb2[2]
    float Saturation;       // cb2[2].x - 饱和度
    float _Pad0;
    float BlendIntensity;   // cb2[2].z - 混合强度
    float Contrast;         // cb2[2].w - 对比度

    // cb2[3]
    float4 ColorTint;       // rgb + weight

    // cb2[4]
    float2 BloomUVScale;    // cb2[4].zw - Bloom UV 缩放
    float ExposureMultiplier;
    float _Pad1;

    // cb2[5] - 控制标志
    float BloomStrength;
    float FixedExposure;    // > 0 则使用固定曝光
    int SkipHDRTonemapping; // 跳过 HDR 直接输出
    int _Pad2;
};

// ============================================================
// Textures (matches original dd14b583)
// ============================================================

Texture2D BloomTexture : register(t0);      // Bloom 纹理
Texture2D SceneTexture : register(t1);      // 场景纹理
Texture2D LuminanceTexture : register(t2);  // 亮度纹理 (自动曝光)
Texture2D MaskTexture : register(t3);       // 材质遮罩

// ============================================================
// Samplers
// ============================================================

SamplerState BloomSampler : register(s0);
SamplerState SceneSampler : register(s1);
SamplerState LuminanceSampler : register(s2);
SamplerState MaskSampler : register(s3);

// ============================================================
// Filmic Tonemap (Uncharted 2) – exact math match to dd14b583
// ============================================================

float3 FilmicTonemap(float3 c, float W)
{
    // 原版算法 (lines 20-31)
    float3 x = c + c;  // r1 = r0 * 2

    // numerator = x * (0.3 * c + 0.05) + W * 0.2
    float3 numerator = x * (0.3 * c + 0.05) + W * 0.2;

    // denominator = x * (0.3 * c + 0.5) + 0.06
    float3 denominator = x * (0.3 * c + 0.5) + 0.06;
    denominator = max(denominator, 0.0001);

    float3 r = numerator / denominator;
    r -= W * 3.333333;

    // 白点缩放
    float ws = (W * 0.2 + 19.375999) * 0.040856 - W * 3.333333;
    ws = 1.0 / max(abs(ws), 0.0001);

    return r * ws;
}

// ============================================================
// Main - HDR Tonemapping only (no LUT)
// ============================================================

float4 main(VSOutput input) : SV_TARGET
{
    float2 uv = input.texCoord;

    // 采样场景纹理
    float4 scene = SceneTexture.Sample(SceneSampler, uv);

    // ========================================================
    // [ORIGINAL MATCH] Mask early-out (lines 1-10)
    // 如果 mask 指示特殊材质，直接输出场景
    // ========================================================
    float mask = MaskTexture.Sample(MaskSampler, uv).r;
    mask = mask * 255.0 - 4.0;  // mad r0.w, r0.w, 255.0, -4.0

    if (abs(mask) < 0.25)
    {
        return float4(scene.rgb, 1.0);
    }

    // ========================================================
    // Skip HDR path - 直接输出场景
    // ========================================================
    if (SkipHDRTonemapping)
    {
        return scene;
    }

    // ========================================================
    // Auto Exposure (lines 11-17)
    // ========================================================
    float exposure;
    if (FixedExposure > 0.0)
    {
        exposure = FixedExposure;
    }
    else
    {
        // 从亮度纹理采样 (使用 .r 通道，兼容 LuminancePass 的 R32_FLOAT 输出)
        float lum = LuminanceTexture.Sample(LuminanceSampler, float2(0.5, 0.5)).r;
        // exposure = MiddleGray / (lum + 0.001)
        exposure = MiddleGray / max(lum + 0.001, 0.0001);
        // clamp to [MinExposure, MaxExposure]
        exposure = clamp(exposure, MinExposure, MaxExposure);
        exposure *= ExposureMultiplier;
    }

    // ========================================================
    // HDR combine (lines 4-5, 18-19)
    // scene + bloom, then multiply by exposure
    // ========================================================
    float3 bloom = BloomTexture.Sample(BloomSampler, uv * BloomUVScale).rgb;
    float3 hdr = (scene.rgb + bloom * BloomStrength) * exposure;

    // ========================================================
    // Filmic Tonemap (lines 20-31)
    // ========================================================
    float3 tm = FilmicTonemap(hdr, WhitePoint);

    // ========================================================
    // Saturation (lines 32-35)
    // ========================================================
    float luma = dot(tm, float3(0.2125, 0.7154, 0.0721));
    float4 tmWithLuma = float4(tm, 0);  // r1.w = 0
    tmWithLuma = lerp(luma.xxxx, tmWithLuma, Saturation);  // saturate mix

    // ========================================================
    // Tint / Contrast / Blend (lines 36-39)
    // ========================================================
    float4 tintMix = lerp(tmWithLuma, float4(luma.xxx * ColorTint.rgb, 0), ColorTint.w);
    float4 finalCol = tintMix * Contrast;
    finalCol = lerp(float4(scene.rgb, mask), finalCol, BlendIntensity);

    return float4(finalCol.rgb, 1.0);
}
