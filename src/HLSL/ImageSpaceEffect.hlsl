struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// 常量缓冲区 - 对应cb2
cbuffer ImageEffectConstants : register(b2)
{
    float4 cb2_0; // cb2[0]
    float4 cb2_1; // cb2[1] - 包含深度和颜色参数
    float4 cb2_2; // cb2[2] - 颜色混合参数
    float4 cb2_3; // cb2[3] - 更多颜色参数
    float4 cb2_4; // cb2[4] - UV缩放参数
};

// 纹理和采样器
Texture2D texture0 : register(t0); // 主纹理
Texture2D texture1 : register(t1); // 颜色纹理
Texture2D texture2 : register(t2); // 深度纹理
Texture2D texture3 : register(t3); // 遮罩纹理

SamplerState sampler0 : register(s0);
SamplerState sampler1 : register(s1);
SamplerState sampler2 : register(s2);
SamplerState sampler3 : register(s3);

float4 main(VSOutput input) : SV_TARGET
{
    float2 uv = input.texCoord;

    // 0-1: 采样颜色纹理和遮罩纹理
    float3 color1 = texture1.Sample(sampler1, uv).xyz;
    float mask = texture3.Sample(sampler3, uv).w;
    
    // 2-3: 处理遮罩值
    mask = mask * 255.0 - 4.0;
    bool maskTest = abs(mask) < 0.25;
    
    // 4-5: 使用缩放UV采样主纹理
    float2 scaledUV = uv * cb2_4.zw;
    float3 mainColor = texture0.Sample(sampler0, scaledUV).xyz;
    
    // 6-10: 如果遮罩测试通过，直接返回颜色
    if (maskTest)
    {
        return float4(color1, 1.0);
    }
    
    // 11-17: 采样深度纹理并进行深度计算
    float depth = texture2.Sample(sampler2, uv).y;
    float depthCalc = cb2_1.z / (depth + 0.001);
    depthCalc = clamp(depthCalc, cb2_1.y, cb2_1.x);
    
    // 18-19: 混合颜色
    float3 blendedColor = (color1 + mainColor) * depthCalc;
    
    // 20-31: 复杂的颜色处理算法
    float3 doubledColor = blendedColor + blendedColor;
    
    // 计算第一个颜色分量
    float3 colorComp1 = blendedColor * 0.3 + 0.05;
    float2 temp = cb2_1.w * float2(0.2, 3.333333);
    colorComp1 = doubledColor * colorComp1 + temp.x;
    
    // 计算第二个颜色分量
    float3 colorComp2 = blendedColor * 0.3 + 0.5;
    colorComp2 = doubledColor * colorComp2 + 0.06;
    
    // 除法操作 - 添加安全保护避免除零和数值不稳定
    float3 finalColor = colorComp1 / colorComp2;
    
    // 进一步的颜色调整
    finalColor = finalColor - cb2_1.w * 3.333333;
    
    // 计算调整因子
    float adjustFactor = cb2_1.w * 0.2 + 19.375999;
    adjustFactor = adjustFactor * 0.040856 - temp.y;
    adjustFactor = 1.0 / adjustFactor;
    
    finalColor *= adjustFactor;
    
    // 32-39: 亮度计算和最终颜色混合
    float luminance = dot(finalColor, float3(0.2125, 0.7154, 0.0721));
    
    float4 colorWithAlpha = float4(finalColor, 0.0);
    float4 lumDiff = colorWithAlpha - luminance;
    
    // 应用颜色调整参数
    float4 adjusted1 = cb2_2.x * lumDiff + luminance;
    float4 adjusted2 = luminance * cb2_3 - adjusted1;
    adjusted1 = cb2_3.w * adjusted2 + adjusted1;
    adjusted1 = cb2_2.w * adjusted1 - depth; // 使用原始深度值
    
    return cb2_2.z * adjusted1 + depth;
}
