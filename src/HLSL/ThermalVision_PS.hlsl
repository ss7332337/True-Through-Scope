// 热成像像素着色器 - 实现拟真FLIR风格热成像效果

cbuffer ThermalConstants : register(b0)
{
    float4 temperatureRange;  // x=min, y=max, z=1/(max-min), w=sensitivity(NETD)
    float4 noiseParams;       // x=intensity, y=time, z=seed, w=pattern scale
    float4 edgeParams;        // x=strength, y=threshold, z=sobel scale, w=unused
    float4 gainLevel;         // x=gain, y=level, z=emissivity, w=palette index
    float4x4 noiseMatrix;     // 用于固定模式噪声变换
};

Texture2D<float4> sceneTexture : register(t0);        // 原始场景颜色
Texture2D<float> depthTexture : register(t1);         // 深度缓冲
Texture2D<float4> paletteLUT : register(t2);          // 调色板查找表
Texture2D<float> fixedPatternNoise : register(t3);    // 固定模式噪声
Texture2D<float> temporalNoise : register(t4);        // 时间相关噪声
Texture2D<float4> normalTexture : register(t5);       // 法线贴图（可选）

SamplerState linearSampler : register(s0);
SamplerState pointSampler : register(s1);

// 输入结构
struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// 将RGB转换为亮度（用作温度估算的基础）
float RGBToLuminance(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

// 根据深度和亮度估算温度
float EstimateTemperature(float3 color, float depth, float2 uv)
{
    float luminance = RGBToLuminance(color);

    // 基于亮度的基础温度
    float baseTemp = lerp(temperatureRange.x, temperatureRange.y, luminance);

    // 材质特征检测
    float3 hsv;
    float maxChannel = max(color.r, max(color.g, color.b));
    float minChannel = min(color.r, min(color.g, color.b));
    float delta = maxChannel - minChannel;

    // 色相分析 - 不同颜色暗示不同材质
    float hue = 0;
    if (delta > 0.001)
    {
        if (maxChannel == color.r)
            hue = ((color.g - color.b) / delta);
        else if (maxChannel == color.g)
            hue = 2.0 + ((color.b - color.r) / delta);
        else
            hue = 4.0 + ((color.r - color.g) / delta);
        hue /= 6.0;
    }

    // 饱和度 - 高饱和度可能是热源
    float saturation = (maxChannel > 0) ? (delta / maxChannel) : 0;

    // 根据颜色特征调整温度
    // 红橙色调（火焰、热源）
    if (hue < 0.1 || hue > 0.9)
    {
        baseTemp += saturation * 30.0; // 增加温度
    }
    // 蓝色调（冷物体、水）
    else if (hue > 0.5 && hue < 0.7)
    {
        baseTemp -= saturation * 10.0;
    }

    // 特殊处理：非常亮的像素可能是光源
    if (luminance > 0.9 && saturation < 0.2)
    {
        baseTemp = temperatureRange.y * 0.9; // 接近最高温
    }

    // 深度影响 - 远处物体温度衰减（大气吸收）
    float distanceFactor = 1.0 - saturate(depth * 0.5);
    baseTemp = lerp(20.0, baseTemp, distanceFactor); // 向环境温度收敛

    return baseTemp;
}

// Sobel边缘检测
float SobelEdgeDetection(float2 uv)
{
    float2 texelSize = float2(1.0 / 1920.0, 1.0 / 1080.0); // 假设1080p

    // Sobel算子
    float sobelX[9] = { -1, 0, 1, -2, 0, 2, -1, 0, 1 };
    float sobelY[9] = { -1, -2, -1, 0, 0, 0, 1, 2, 1 };

    float edgeX = 0;
    float edgeY = 0;

    for (int i = -1; i <= 1; i++)
    {
        for (int j = -1; j <= 1; j++)
        {
            float2 offset = float2(i, j) * texelSize * edgeParams.z;
            float3 color = sceneTexture.Sample(linearSampler, uv + offset).rgb;
            float lum = RGBToLuminance(color);

            int index = (j + 1) * 3 + (i + 1);
            edgeX += lum * sobelX[index];
            edgeY += lum * sobelY[index];
        }
    }

    return length(float2(edgeX, edgeY));
}

// 生成热噪声
float GenerateThermalNoise(float2 uv, float temperature)
{
    // 固定模式噪声（FPN）- 传感器缺陷
    float fpn = fixedPatternNoise.Sample(pointSampler, uv * noiseParams.w).r;

    // 时间噪声 - 随时间变化
    float2 noiseUV = uv + float2(noiseParams.y * 0.01, noiseParams.z);
    float temporal = temporalNoise.Sample(pointSampler, noiseUV).r;

    // 1/f噪声（闪烁噪声）
    float flicker = sin(noiseParams.y * 2.3 + uv.x * 100.0) * 0.005;

    // NETD（噪声等效温差）- 温度敏感度
    float netd = temperatureRange.w * (1.0 + 0.5 * temporal);

    // 组合噪声
    float totalNoise = fpn * 0.3 + temporal * 0.5 + flicker;
    totalNoise *= noiseParams.x * (1.0 + netd);

    // 温度相关噪声 - 高温区域噪声较大
    float tempFactor = saturate((temperature - temperatureRange.x) * temperatureRange.z);
    totalNoise *= (1.0 + tempFactor * 0.5);

    return totalNoise;
}

// 应用调色板
float4 ApplyPalette(float normalizedTemp, int paletteIndex)
{
    // 调色板在LUT纹理的不同行
    float2 lutCoord = float2(normalizedTemp, (paletteIndex + 0.5) / 8.0);
    return paletteLUT.Sample(linearSampler, lutCoord);
}

// 模拟热成像传感器响应曲线
float SensorResponse(float temperature)
{
    // S形响应曲线，模拟真实传感器的非线性响应
    float normalized = saturate((temperature - temperatureRange.x) * temperatureRange.z);

    // 应用增益和电平调整
    normalized = saturate((normalized - (0.5 - gainLevel.y)) * gainLevel.x + 0.5);

    // 非线性响应（模拟玻尔兹曼定律）
    normalized = pow(normalized, 1.0 / 2.2);

    return normalized;
}

// 添加热成像特有的光晕效果
float3 AddThermalBloom(float2 uv, float temperature)
{
    if (temperature < temperatureRange.y * 0.8)
        return float3(0, 0, 0);

    float3 bloom = float3(0, 0, 0);
    float2 texelSize = float2(1.0 / 1920.0, 1.0 / 1080.0);

    [unroll]
    for (int i = -2; i <= 2; i++)
    {
        [unroll]
        for (int j = -2; j <= 2; j++)
        {
            float2 offset = float2(i, j) * texelSize * 2.0;
            float3 color = sceneTexture.SampleLevel(linearSampler, uv + offset, 0).rgb;
            float temp = EstimateTemperature(color, 0.5, uv + offset);

            if (temp > temperatureRange.y * 0.8)
            {
                float weight = exp(-length(float2(i, j)) * 0.5);
                bloom += float3(1, 0.8, 0.6) * weight * 0.1;
            }
        }
    }

    return bloom;
}

// 主像素着色器
float4 main(PSInput input) : SV_TARGET
{
    float2 uv = input.texCoord;

    // 采样场景
    float4 sceneColor = sceneTexture.Sample(linearSampler, uv);
    float depth = depthTexture.Sample(pointSampler, uv).r;

    // 估算温度
    float temperature = EstimateTemperature(sceneColor.rgb, depth, uv);

    // 添加热噪声
    float noise = GenerateThermalNoise(uv, temperature);
    temperature += noise * (temperatureRange.y - temperatureRange.x);

    // 应用传感器响应曲线
    float sensorValue = SensorResponse(temperature);

    // 边缘检测增强
    float edge = 0;
    if (edgeParams.x > 0)
    {
        edge = SobelEdgeDetection(uv);
        edge = saturate(edge * edgeParams.x);
    }

    // 应用调色板
    float4 thermalColor = ApplyPalette(sensorValue, (int)gainLevel.w);

    // 添加边缘高亮
    thermalColor.rgb = lerp(thermalColor.rgb, float3(1, 1, 1), edge * 0.3);

    // 添加热光晕（高温物体）
    float3 bloom = AddThermalBloom(uv, temperature);
    thermalColor.rgb += bloom;

    // 模拟传感器饱和
    thermalColor.rgb = saturate(thermalColor.rgb);

    // 添加轻微的暗角效果（镜头特性）
    float2 center = uv - 0.5;
    float vignette = 1.0 - dot(center, center) * 0.3;
    thermalColor.rgb *= vignette;

    // 输出
    return float4(thermalColor.rgb, 1.0);
}

// 简化版本 - 用于性能模式
float4 mainSimple(PSInput input) : SV_TARGET
{
    float2 uv = input.texCoord;
    float4 sceneColor = sceneTexture.Sample(linearSampler, uv);

    // 简单的亮度到温度映射
    float temperature = RGBToLuminance(sceneColor.rgb);

    // 简单噪声
    float noise = temporalNoise.Sample(pointSampler, uv + noiseParams.y * 0.01).r;
    temperature += noise * noiseParams.x * 0.1;

    // 归一化
    float normalized = saturate((temperature - 0.2) * 2.0);

    // 应用调色板
    float4 thermalColor = ApplyPalette(normalized, (int)gainLevel.w);

    return thermalColor;
}
