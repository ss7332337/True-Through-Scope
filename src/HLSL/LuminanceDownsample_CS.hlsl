// ============================================================
// LuminanceDownsample_CS.hlsl
// Pass 1: 8x8 Downsample with Log-Luminance and Center Weighting
//
// 每个线程组处理 8x8 像素，输出一个 log-luminance 加权和
// ============================================================

cbuffer DownsampleConstants : register(b0)
{
    uint InputSizeX;       // 输入纹理宽度
    uint InputSizeY;       // 输入纹理高度
    uint OutputSizeX;      // 输出纹理宽度
    uint OutputSizeY;      // 输出纹理高度

    float MinLuminance;    // 最小亮度阈值 (避免 log(0))
    float CenterWeightScale; // 中心权重缩放因子
    float2 _Padding;
};

// 输入场景纹理
Texture2D<float4> InputTexture : register(t0);

// 输出: log-luminance 加权和, 权重和
RWTexture2D<float> OutputLogLum : register(u0);
RWTexture2D<float> OutputWeight : register(u1);

// 共享内存: 8x8 = 64 线程
groupshared float SharedLogLum[64];
groupshared float SharedWeight[64];

// BT.709 亮度计算
float GetLuminance(float3 rgb)
{
    return dot(rgb, float3(0.2126f, 0.7152f, 0.0722f));
}

// 计算中心权重 (瞄镜中心权重更高)
float GetCenterWeight(uint2 pixelCoord, uint2 inputSize)
{
    float2 normalizedCoord = (float2(pixelCoord) + 0.5f) / float2(inputSize);
    float2 centerOffset = normalizedCoord - float2(0.5f, 0.5f);

    // 距离中心的归一化距离 (0 = 中心, 1 = 角落)
    float dist = length(centerOffset) * 2.0f;

    // 二次衰减: 中心权重 = 1, 边缘权重趋近于 0
    float weight = 1.0f - saturate(dist * CenterWeightScale);
    weight = weight * weight;

    // 确保最小权重，避免完全忽略边缘
    return max(weight, 0.01f);
}

[numthreads(8, 8, 1)]
void main(
    uint3 groupId : SV_GroupID,
    uint3 threadId : SV_GroupThreadID,
    uint groupIndex : SV_GroupIndex)
{
    // 计算输入像素坐标
    uint2 inputCoord = groupId.xy * 8 + threadId.xy;

    float logLum = 0.0f;
    float weight = 0.0f;

    // 边界检查
    if (inputCoord.x < InputSizeX && inputCoord.y < InputSizeY)
    {
        // 采样输入纹理
        float3 color = InputTexture[inputCoord].rgb;

        // 计算亮度
        float lum = GetLuminance(color);
        lum = max(lum, MinLuminance);  // 避免 log(0)

        // 计算中心权重
        weight = GetCenterWeight(inputCoord, uint2(InputSizeX, InputSizeY));

        // Log 空间亮度 (加权)
        logLum = log(lum) * weight;
    }

    // 存入共享内存
    SharedLogLum[groupIndex] = logLum;
    SharedWeight[groupIndex] = weight;

    GroupMemoryBarrierWithGroupSync();

    // ========== 并行归约 ==========
    // 64 -> 32 -> 16 -> 8 -> 4 -> 2 -> 1

    [unroll]
    for (uint stride = 32; stride > 0; stride >>= 1)
    {
        if (groupIndex < stride)
        {
            SharedLogLum[groupIndex] += SharedLogLum[groupIndex + stride];
            SharedWeight[groupIndex] += SharedWeight[groupIndex + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    // 线程 0 写入结果
    if (groupIndex == 0)
    {
        // 边界检查输出坐标
        if (groupId.x < OutputSizeX && groupId.y < OutputSizeY)
        {
            OutputLogLum[groupId.xy] = SharedLogLum[0];
            OutputWeight[groupId.xy] = SharedWeight[0];
        }
    }
}
