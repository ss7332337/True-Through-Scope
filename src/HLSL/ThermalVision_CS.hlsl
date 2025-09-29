// 热成像计算着色器 - 用于温度分析和自动增益控制
// 注意：这个文件包含多个计算着色器，需要分别编译每个入口点
// 或者使用main作为默认入口点

cbuffer AnalysisConstants : register(b0)
{
    uint2 textureSize;
    uint histogramBins;
    float temperatureScale;
};

Texture2D<float4> inputTexture : register(t0);
RWStructuredBuffer<uint> histogram : register(u0);
RWStructuredBuffer<float> statistics : register(u1); // [0]=min, [1]=max, [2]=avg, [3]=stddev
RWStructuredBuffer<uint> atomicBuffer : register(u2); // 用于原子操作的辅助缓冲区

groupshared uint localHistogram[256];
groupshared uint localMinInt;  // 使用整数类型进行原子操作
groupshared uint localMaxInt;  // 使用整数类型进行原子操作
groupshared float localSum;
groupshared uint localCount;

float RGBToTemperature(float3 color)
{
    // 简化的温度估算
    float luminance = dot(color, float3(0.299, 0.587, 0.114));

    // 红色通道权重更高（热源）
    float redBias = color.r * 0.3;

    // 蓝色通道负相关（冷源）
    float blueBias = -color.b * 0.1;

    return saturate(luminance + redBias + blueBias);
}

[numthreads(16, 16, 1)]
void AnalyzeTemperature(uint3 id : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    // 初始化共享内存
    if (groupIndex == 0)
    {
        localMinInt = 0xFFFFFFFF;  // 初始化为最大uint值
        localMaxInt = 0;            // 初始化为最小uint值
        localSum = 0.0;
        localCount = 0;
    }

    if (groupIndex < 256)
    {
        localHistogram[groupIndex] = 0;
    }

    GroupMemoryBarrierWithGroupSync();

    // 处理像素
    if (id.x < textureSize.x && id.y < textureSize.y)
    {
        float4 color = inputTexture[uint2(id.x, id.y)];
        float temperature = RGBToTemperature(color.rgb);

        // 更新局部统计 - 将float转换为uint进行原子操作
        uint tempAsUint = asuint(temperature);
        InterlockedMin(localMinInt, tempAsUint);
        InterlockedMax(localMaxInt, tempAsUint);

        uint bin = (uint)(temperature * 255.0);
        InterlockedAdd(localHistogram[bin], 1);

        // 累加求平均
        localSum += temperature;
        InterlockedAdd(localCount, 1);
    }

    GroupMemoryBarrierWithGroupSync();

    // 合并结果到全局缓冲
    if (groupIndex == 0)
    {
        // 将uint转换回float并存储
        float localMinFloat = asfloat(localMinInt);
        float localMaxFloat = asfloat(localMaxInt);

        // 使用原子操作更新全局统计
        // 注意：对于RWStructuredBuffer<float>，我们不能直接使用原子操作
        // 简化方案：直接写入（可能需要多pass或使用其他同步机制）
        statistics[0] = min(statistics[0], localMinFloat);
        statistics[1] = max(statistics[1], localMaxFloat);
        statistics[2] += localSum;
    }

    if (groupIndex < 256)
    {
        InterlockedAdd(histogram[groupIndex], localHistogram[groupIndex]);
    }
}

// 自动增益调整计算
[numthreads(1, 1, 1)]
void ComputeAutoGain(uint3 id : SV_DispatchThreadID)
{
    uint totalPixels = textureSize.x * textureSize.y;

    // 找到包含95%像素的温度范围
    uint cumulativeSum = 0;
    uint lowBin = 0;
    uint highBin = 255;

    // 找到低端5%位置
    for (uint i = 0; i < 256; i++)
    {
        cumulativeSum += histogram[i];
        if (cumulativeSum > (uint)(totalPixels * 0.05f))
        {
            lowBin = i;
            break;
        }
    }

    // 找到高端95%位置
    cumulativeSum = 0;
    for (int j = 255; j >= 0; j--)
    {
        cumulativeSum += histogram[j];
        if (cumulativeSum > (uint)(totalPixels * 0.05f))
        {
            highBin = (uint)j;
            break;
        }
    }

    // 更新统计数据
    statistics[0] = lowBin / 255.0;  // 新的最小值
    statistics[1] = highBin / 255.0;  // 新的最大值

    // 计算平均值
    float sum = asfloat(statistics[2]);
    statistics[2] = sum / totalPixels;

    // 计算标准差（简化版）
    float range = statistics[1] - statistics[0];
    statistics[3] = range * 0.25; // 近似标准差
}

// 非均匀性校正（NUC）
RWTexture2D<float4> outputTexture : register(u2);
Texture2D<float> nucCoefficients : register(t1); // 校正系数

[numthreads(8, 8, 1)]
void NonUniformityCorrection(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= textureSize.x || id.y >= textureSize.y)
        return;

    float4 color = inputTexture[id.xy];
    float coefficient = nucCoefficients[id.xy].r;

    // 应用校正
    color.rgb *= coefficient;

    outputTexture[id.xy] = color;
}

// 时间降噪滤波器
Texture2D<float4> previousFrame : register(t2);
RWTexture2D<float4> denoisedOutput : register(u3);

[numthreads(8, 8, 1)]
void TemporalDenoising(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= textureSize.x || id.y >= textureSize.y)
        return;

    float4 current = inputTexture[id.xy];
    float4 previous = previousFrame[id.xy];

    // 运动检测
    float motion = length(current.rgb - previous.rgb);

    // 自适应混合因子
    float blendFactor = lerp(0.8, 0.2, saturate(motion * 10.0));

    // 时间滤波
    float4 filtered = lerp(previous, current, blendFactor);

    denoisedOutput[id.xy] = filtered;
}

// 主入口点 - 默认使用温度分析
[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
    // 调用温度分析函数
    AnalyzeTemperature(id, groupId, groupIndex);
}