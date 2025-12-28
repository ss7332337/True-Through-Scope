// Scope Motion Vector Calculation Pixel Shader
// 使用深度重建世界位置，然后投影到上一帧计算正确的 Motion Vector
// 用于修复 TAA 运动模糊问题

cbuffer MVConstants : register(b0)
{
    float4x4 InvViewProj;      // 当前帧逆 ViewProj 矩阵
    float4x4 PrevViewProj;     // 上一帧 ViewProj 矩阵
    float2 ScreenSize;         // 屏幕尺寸
    float2 Padding;
};

Texture2D<float> DepthTexture : register(t0);
SamplerState PointSampler : register(s0);

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 main(PSInput input) : SV_TARGET
{
    // 读取深度值
    float depth = DepthTexture.Sample(PointSampler, input.TexCoord);
    
    // 重建 NDC 坐标 (DX11 depth range [0,1], NDC range [-1,1])
    float2 ndc = input.TexCoord * 2.0 - 1.0;
    ndc.y = -ndc.y; // 翻转 Y (纹理坐标 vs NDC)
    float4 clipPos = float4(ndc, depth, 1.0);
    
    // 使用逆 ViewProj 矩阵重建世界空间位置
    float4 worldPos = mul(InvViewProj, clipPos);
    worldPos /= worldPos.w;
    
    // 投影到上一帧的 clip space
    float4 prevClipPos = mul(PrevViewProj, worldPos);
    float2 prevNDC = prevClipPos.xy / prevClipPos.w;
    
    // 计算屏幕空间 Motion Vector
    // MV = (当前位置 - 上一帧位置) / 2，因为 NDC 范围是 [-1,1]，需要转换为 MV 格式
    float2 velocity = (ndc - prevNDC) * 0.5;
    
    // Fallout 4 的 Motion Vector 格式可能需要调整
    // RT29 存储的是像素级偏移，需要乘以屏幕尺寸
    // velocity *= ScreenSize;
    
    return float4(velocity, 0.0, 0.0);
}
