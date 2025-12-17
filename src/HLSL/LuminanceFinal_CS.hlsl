// ============================================================
// LuminanceFinal_CS.hlsl
// Pass 2: Final Reduction to 1x1 with Temporal Smoothing
//
// 归约中间纹理到 1x1，应用 exp() 还原，并进行时域平滑
// ============================================================

cbuffer FinalConstants : register(b0)
{
    uint IntermediateSizeX;  // 中间纹理宽度
    uint IntermediateSizeY;  // 中间纹理高度
    float AdaptationSpeed;   // 时域平滑速度 (e.g., 2.0)
    float DeltaTime;         // 帧时间

    float TotalWeight;       // 总权重 (预留，实际在 shader 中计算)
    float MinAdaptedLum;     // 最小适应亮度
    float MaxAdaptedLum;     // 最大适应亮度
    float _Padding;
};

// 输入纹理
Texture2D<float> IntermediateLogLum : register(t0);  // log-luminance 加权和
Texture2D<float> IntermediateWeight : register(t1);  // 权重和
Texture2D<float> PreviousLuminance : register(t2);   // 上一帧亮度

// 输出: 1x1 亮度纹理
RWTexture2D<float> OutputLuminance : register(u0);

// 共享内存: 16x16 = 256 线程
groupshared float SharedLogLum[256];
groupshared float SharedWeight[256];

[numthreads(16, 16, 1)]
void main(
    uint3 threadId : SV_GroupThreadID,
    uint groupIndex : SV_GroupIndex)
{
    // ========== 阶段 1: 每个线程累加多个中间纹理像素 ==========
    float sumLogLum = 0.0f;
    float sumWeight = 0.0f;

    // 每个线程负责遍历部分中间纹理
    uint2 coord;
    coord.y = threadId.y;

    while (coord.y < IntermediateSizeY)
    {
        coord.x = threadId.x;
        while (coord.x < IntermediateSizeX)
        {
            sumLogLum += IntermediateLogLum[coord];
            sumWeight += IntermediateWeight[coord];
            coord.x += 16;
        }
        coord.y += 16;
    }

    SharedLogLum[groupIndex] = sumLogLum;
    SharedWeight[groupIndex] = sumWeight;

    GroupMemoryBarrierWithGroupSync();

    // ========== 阶段 2: 并行归约 ==========
    // 256 -> 128 -> 64 -> 32 -> 16 -> 8 -> 4 -> 2 -> 1

    [unroll]
    for (uint stride = 128; stride > 0; stride >>= 1)
    {
        if (groupIndex < stride)
        {
            SharedLogLum[groupIndex] += SharedLogLum[groupIndex + stride];
            SharedWeight[groupIndex] += SharedWeight[groupIndex + stride];
        }
        GroupMemoryBarrierWithGroupSync();
    }

    // ========== 阶段 3: 线程 0 计算最终结果 ==========
    if (groupIndex == 0)
    {
        // DEBUG: 临时使用固定亮度值测试流程
        // 设置为 1 表示启用调试模式，0 表示正常计算
        #define DEBUG_FIXED_LUMINANCE 0

        #if DEBUG_FIXED_LUMINANCE
        // 使用固定的中间灰度值 (0.18) 测试
        // 这应该产生 exposure = 0.18 / 0.18 = 1.0 的曝光
        float adaptedLum = 0.18f;
        #else
        float totalLogLum = SharedLogLum[0];
        float totalWeight = SharedWeight[0];

        // 防止除零
        totalWeight = max(totalWeight, 0.001f);

        // 计算加权平均 log-luminance，然后还原
        float avgLogLum = totalLogLum / totalWeight;
        float currentLum = exp(avgLogLum);

        // 获取上一帧亮度
        float prevLum = PreviousLuminance[uint2(0, 0)];

        // 时域平滑 (指数移动平均)
        // adaptedLum = lerp(prevLum, currentLum, 1 - exp(-speed * dt))
        float adaptFactor = 1.0f - exp(-AdaptationSpeed * DeltaTime);
        adaptFactor = saturate(adaptFactor);

        float adaptedLum = lerp(prevLum, currentLum, adaptFactor);

        // 限制范围
        adaptedLum = clamp(adaptedLum, MinAdaptedLum, MaxAdaptedLum);
        #endif

        // 输出到 1x1 纹理
        OutputLuminance[uint2(0, 0)] = adaptedLum;
    }
}
