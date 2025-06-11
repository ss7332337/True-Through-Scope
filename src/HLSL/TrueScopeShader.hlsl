Texture2D scopeTexture : register(t0);
Texture2D reticleTexture : register(t1);
// Color Grading 3D LUT纹理
Texture3D lutTexture0 : register(t2);
Texture3D lutTexture1 : register(t3);
Texture3D lutTexture2 : register(t4);
Texture3D lutTexture3 : register(t5);

SamplerState scopeSampler : register(s0);
SamplerState lutSampler : register(s1);
            
// Constants buffer containing screen resolution, camera position and scope position
cbuffer ScopeConstants : register(b0)
{
    float screenWidth;
    float screenHeight;
    int enableNightVision;
    int enableThermalVision;


    float3 cameraPosition;
    float padding2; // 16-byte alignment

    float3 scopePosition;
    float padding3; // 16-byte alignment

    float3 lastCameraPosition;
    float padding4; // 16-byte alignment

    float3 lastScopePosition;
    float padding5; // 16-byte alignment

    float parallax_relativeFogRadius;
    float parallax_scopeSwayAmount;
    float parallax_maxTravel;
    float parallax_Radius;
    
    float reticleScale;
    float reticleOffsetX;
    float reticleOffsetY;
    float padding6;

    float4x4 CameraRotation;

    // 夜视效果参数
    float nightVisionIntensity;    // 夜视强度
    float nightVisionNoiseScale;   // 噪点缩放
    float nightVisionNoiseAmount;  // 噪点强度
    float nightVisionGreenTint;    // 绿色色调强度

    // 热成像效果参数
    float thermalIntensity;        // 热成像强度
    float thermalThreshold;        // 热成像阈值
    float thermalContrast;         // 热成像对比度
    float thermalNoiseAmount;      // 热成像噪点强度

    // Color Grading LUT权重
    float4 lutWeights;           // 4个LUT的混合权重
}
            
struct PS_INPUT
{
    float4 position : SV_POSITION;
    //float4 texCoord : TEXCOORD;
    //float4 color0 : COLOR;
    //float4 color0 : COLOR0;
    //float4 fogColor : COLOR1;
};

float2 clampMagnitude(float2 v, float l)
{
    return normalize(v) * min(length(v), l);
}

float getparallax(float d, float2 ds, float dfov)
{
    return clamp(1 - pow(abs(rcp(parallax_Radius * ds.y) * (parallax_relativeFogRadius * d * ds.y)), parallax_scopeSwayAmount), 0, parallax_maxTravel);
}

float2 aspect_ratio_correction(float2 tc)
{
    tc.x -= 0.5f;
    tc.x *= screenWidth * rcp(screenHeight);
    tc.x += 0.5f;
    return tc;
}

float2 transform_reticle_coords(float2 tc)
{
    tc -= float2(0.5, 0.5);
    tc /= reticleScale;
    tc += float2(reticleOffsetX, reticleOffsetY);
    return tc;
}

// 生成随机噪点
float random(float2 st) {
    return frac(sin(dot(st.xy, float2(12.9898, 78.233))) * 43758.5453123);
}

// Color Grading 色彩分级函数
float4 applyColorGrading(float4 color)
{
    float4 r0 = color;
    float3 r1, r2;

    // 对RGB分量取对数
    r0.xyz = log(r0.xyz);
    
    // 直接输出alpha通道
    float4 outputColor;
    outputColor.w = r0.w;
    
    // 应用gamma校正
    r0.xyz = r0.xyz * 0.454545;
    r0.xyz = exp(r0.xyz);
    
    // 调整颜色范围
    r0.xyz = r0.xyz * 0.9375 + 0.03125;
    
    // 从多个3D LUT纹理采样并混合
    r1 = lutTexture1.Sample(lutSampler, r0.xyz) * lutWeights.y;
    r2 = lutTexture0.Sample(lutSampler, r0.xyz) * lutWeights.x;
    r1 += r2;
    
    r2 = lutTexture2.Sample(lutSampler, r0.xyz) * lutWeights.z;
    r1 += r2;
    
    r0.xyz = lutTexture3.Sample(lutSampler, r0.xyz) * lutWeights.w;
    outputColor.xyz = r1 + r0.xyz;
    
    return outputColor;
}

// 夜视效果处理
float4 applyNightVision(float4 color, float2 texcoord)
{
    // 转换为灰度
    float luminance = dot(color.rgb, float3(0.299, 0.587, 0.114));
    
    // 归一化亮度值
    luminance = saturate(luminance);
    
    // 添加噪点
    float2 noiseCoord = texcoord * nightVisionNoiseScale * 100.0;
    float noise = (random(noiseCoord) - 0.5) * nightVisionNoiseAmount;
    
    // 应用绿色色调
    float3 nightVisionColor = float3(0.0, luminance * nightVisionGreenTint, luminance * 0.3);
    
    // 应用强度并添加噪点
    nightVisionColor *= nightVisionIntensity;
    nightVisionColor += noise;
    
    // 确保颜色在合理范围内
    nightVisionColor = saturate(nightVisionColor);
    
    return float4(nightVisionColor, color.a);
}

// 热成像效果处理
float4 applyThermal(float4 color, float2 texcoord)
{
    // 转换为灰度
    float luminance = dot(color.rgb, float3(0.299, 0.587, 0.114));
    
    // 归一化亮度值到 0-1 范围
    luminance = saturate(luminance);
    
    // 使用step函数替代if判断，避免分支
    float isHot = step(thermalThreshold, luminance);
    
    // 热区域计算 (luminance > thermalThreshold)
    float hotT = saturate((luminance - thermalThreshold) / (1.0 - thermalThreshold));
    float3 hotColor = lerp(float3(1.0, 0.0, 0.0), float3(1.0, 1.0, 0.0), hotT);
    
    // 白色混合计算 (t > 0.8)
    float whiteBlend = saturate((hotT - 0.8) / 0.2);
    hotColor = lerp(hotColor, float3(1.0, 1.0, 1.0), whiteBlend);
    
    // 冷区域计算 (luminance <= thermalThreshold)
    float coldT = saturate(luminance / thermalThreshold);
    float3 coldColor = lerp(float3(0.0, 0.0, 0.3), float3(0.0, 0.5, 1.0), coldT);
    
    // 根据isHot选择最终颜色，无分支混合
    float3 thermalColor = lerp(coldColor, hotColor, isHot);
    
    // 添加细微的噪点效果
    float2 noiseCoord = texcoord * 100.0;
    float noise = (random(noiseCoord) - 0.5) * thermalNoiseAmount;
    
    // 应用对比度和强度
    thermalColor = pow(abs(thermalColor), 1.0 / max(thermalContrast, 0.1));
    thermalColor *= thermalIntensity;
    
    // 添加噪点
    thermalColor += noise;
    
    // 确保颜色在合理范围内
    thermalColor = saturate(thermalColor);
    
    return float4(thermalColor, color.a);
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float2 texCoord = input.position.xy / float2(screenWidth, screenHeight);
    float2 aspectCorrectTex = aspect_ratio_correction(texCoord);

    float3 virDir = scopePosition - cameraPosition;
    float3 lastVirDir = lastScopePosition - lastCameraPosition;
    float3 eyeDirectionLerp = virDir - lastVirDir;
    float4 abseyeDirectionLerp = mul(float4((eyeDirectionLerp), 1), CameraRotation);

    if (abseyeDirectionLerp.y < 0 && abseyeDirectionLerp.y >= -0.001)
        abseyeDirectionLerp.y = -0.001;
    else if (abseyeDirectionLerp.y >= 0 && abseyeDirectionLerp.y <= 0.001)
        abseyeDirectionLerp.y = 0.001;

    // Get original texture
    float4 color = scopeTexture.Sample(scopeSampler, texCoord);

    float2 eye_velocity = clampMagnitude(abseyeDirectionLerp.xy, 1.5f);

    float2 parallax_offset = float2(0.5 + eye_velocity.x, 0.5 - eye_velocity.y);
    float distToParallax = distance(aspectCorrectTex, parallax_offset);
    float2 scope_center = float2(0.5, 0.5);
    float distToCenter = distance(aspectCorrectTex, scope_center);

    float parallaxValue = (step(distToCenter, 2) * getparallax(distToParallax, float2(1, 1), 1));
    
    float2 reticleTexCoord = transform_reticle_coords(aspectCorrectTex);
    reticleTexCoord = float2(1.0 - reticleTexCoord.x, reticleTexCoord.y);
    float4 reticleColor = reticleTexture.Sample(scopeSampler, reticleTexCoord);
    
    
    // 应用Color Grading色彩分级（替代原来的亮度提升）
    color = applyColorGrading(color);
    color *= 1.1f;
    
    // 计算夜视和热成像效果（无分支）
    float4 nightVisionColor = applyNightVision(color, texCoord);
    float4 thermalColor = applyThermal(color, texCoord);
    
    // 使用lerp进行无分支混合，enableXXX作为混合因子
    color = lerp(color, nightVisionColor, float(enableNightVision));
    color = lerp(color, thermalColor, float(enableThermalVision));
    
    color = reticleColor * reticleColor.a + color * (1 - reticleColor.a);
    color.rgb *= parallaxValue;
    
    return color;
}
