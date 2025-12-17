// DOFBlend_PS.hlsl
// 复现原版游戏 DOF 景深混合 pass (Shader hash 681671ef)
// 基于深度纹理混合清晰图像和模糊图像

// 常量缓冲区 - 对应 DOFConstants
cbuffer DOFConstants : register(b0)
{
    // 深度参数 (cb2[0])
    float FocalRange;       // 焦点范围 (近平面模糊因子)
    float BlurFalloff;      // 模糊衰减 (远平面模糊因子)
    float FocalPlane;       // 焦平面距离
    float _Pad0;

    // 控制标志 (cb2[1])
    float DOFStrength;      // DOF 强度 (cb2[1].x)
    int   EnableNearBlur;   // 近平面模糊开关 (cb2[1].y)
    int   EnableFarBlur;    // 远平面模糊开关 (cb2[1].z)
    int   EnableDOF;        // DOF 总开关 (cb2[1].w)

    // 深度重建参数 (cb2[2])
    float DepthNear;        // 近裁剪面
    float DepthOffset;      // 深度偏移 (cb2[2].y)
    float DepthScale;       // 深度缩放 (cb2[2].z)
    float DepthFar;         // 远裁剪面 (cb2[2].w)

    // UV 参数 (cb2[3])
    float BlurUVScaleX;     // 模糊纹理 UV 缩放 X (cb2[3].z)
    float BlurUVScaleY;     // 模糊纹理 UV 缩放 Y (cb2[3].w)
    float2 _Pad1;
};

// 纹理和采样器
Texture2D SharpTexture : register(t0);      // 清晰场景纹理
Texture2D BlurredTexture : register(t1);    // 模糊场景纹理
Texture2D DepthTexture : register(t2);      // 深度纹理

SamplerState LinearSampler : register(s0);  // 场景纹理采样器
SamplerState BlurSampler : register(s1);    // 模糊纹理采样器
SamplerState PointSampler : register(s2);   // 深度纹理采样器

// 输入结构
struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET
{
    float2 uv = input.texCoord;

    // 采样深度 (原版使用反转深度: 1.0 - depth)
    float depth = DepthTexture.Sample(PointSampler, uv).r;
    depth = 1.0 - depth;

    // 线性深度重建 (复现原版公式)
    // linearDepth = DepthScale / (depth + DepthOffset)
    float linearDepth = DepthScale / (depth + DepthOffset);

    // 计算 DOF 因子
    float dofFactor = 0.0;

    // 近平面模糊 (物体比焦平面更近)
    bool isNearBlur = (EnableNearBlur != 0) && (linearDepth < FocalPlane);
    if (isNearBlur)
    {
        float nearDist = FocalPlane - linearDepth;
        dofFactor = saturate(nearDist / FocalRange);
    }

    // 远平面模糊 (物体比焦平面更远)
    bool isFarBlur = (EnableFarBlur != 0) && (linearDepth > FocalPlane);
    if (isFarBlur)
    {
        float farDist = linearDepth - FocalPlane;
        dofFactor = saturate(farDist / BlurFalloff);
    }

    // 应用 DOF 强度
    dofFactor *= DOFStrength;

    // 检查深度有效性 (原版 shader 中有类似检查)
    // 如果深度值非常小 (接近近裁剪面)，可能需要特殊处理
    bool validDepth = (depth > 0.00001);
    dofFactor = validDepth ? dofFactor : 0.0;

    // 采样清晰和模糊图像
    float4 sharp = SharpTexture.Sample(LinearSampler, uv);
    float2 blurUV = uv * float2(BlurUVScaleX, BlurUVScaleY);
    float4 blurred = BlurredTexture.Sample(BlurSampler, blurUV);

    // 混合输出 (复现原版: lerp(sharp, blurred, dofFactor))
    float4 result = lerp(sharp, blurred, dofFactor);

    return result;
}
