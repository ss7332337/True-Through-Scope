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

    // 球形畸变参数
    float sphericalDistortionStrength;  // 球形畸变强度 (0.0 = 无畸变, 正值 = 桶形畸变, 负值 = 枕形畸变)
    float sphericalDistortionRadius;    // 畸变作用半径 (0.0-1.0)
    float2 sphericalDistortionCenter;   // 畸变中心位置 (相对于屏幕中心的偏移)
    
    int enableSphericalDistortion;      // 是否启用球形畸变 (0 = 禁用, 1 = 启用)
    int enableChromaticAberration;      // 是否启用色散效果 (0 = 禁用, 1 = 启用)
    float brightnessBoost;              // 亮度增强系数
    float ambientOffset;                // 环境光补偿
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
    // 改进的视差计算：更稳定的角度依赖性
    float radiusFactor = max(parallax_Radius * ds.y, 0.001); // 避免除零
    float distanceFactor = parallax_relativeFogRadius * d * ds.y;

    // 使用更稳定的指数函数，减少极端值
    float exponent = clamp(parallax_scopeSwayAmount, 0.1, 2.0); // 限制指数范围
    float parallaxValue = 1.0 - pow(abs(distanceFactor / radiusFactor), exponent);

    // 确保输出在合理范围内，避免完全黑暗
    return clamp(parallaxValue, 0.1, max(parallax_maxTravel, 0.3));
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

    // 对 RGB 分量取对数前进行安全钳制，避免 log(0) / 负数导致的 -INF/NaN
    float3 rgb = log(max(abs(r0.xyz), 1e-6));

    // gamma 校正：等价于 pow(color, 0.454545)
    rgb *= 0.454545;
    rgb = exp(rgb);

    // LUT 坐标范围调整并夹紧到 [0,1]
    float3 uvw = saturate(rgb * 0.9375 + 0.03125);

    // 从多个 3D LUT 采样（显式 .rgb，避免隐式截断）
    float3 c0 = lutTexture0.Sample(lutSampler, uvw).rgb;
    float3 c1 = lutTexture1.Sample(lutSampler, uvw).rgb;
    float3 c2 = lutTexture2.Sample(lutSampler, uvw).rgb;
    float3 c3 = lutTexture3.Sample(lutSampler, uvw).rgb;

    // 按权重混合（保持与你原逻辑一致：不做权重归一化）
    float3 outRGB = c0 * lutWeights.x
                  + c1 * lutWeights.y
                  + c2 * lutWeights.z
                  + c3 * lutWeights.w;

    // 透传 alpha
    return float4(outRGB, r0.w);
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

// 球形畸变函数
float2 applySphericalDistortion(float2 texcoord)
{
    // 将纹理坐标转换为以屏幕中心为原点的坐标系
    float2 center = float2(0.5, 0.5) + sphericalDistortionCenter;
    float2 uv = texcoord - center;
    
    // 应用宽高比校正，确保畸变是圆形的而不是椭圆形
    uv.x *= screenWidth / screenHeight;
    
    // 计算到中心的距离
    float distance = length(uv);
    
    // 应用球形畸变
    // 使用二次函数来模拟球形透镜的畸变效果
    float distortionFactor = 1.0 + sphericalDistortionStrength * distance * distance;
    
    // 限制畸变作用的半径范围
    float radiusMask = smoothstep(sphericalDistortionRadius, sphericalDistortionRadius * 0.8, distance);
    distortionFactor = lerp(distortionFactor, 1.0, radiusMask);
    
    // 应用畸变
    uv *= distortionFactor;
    
    // 恢复宽高比校正
    uv.x /= screenWidth / screenHeight;
    
    // 转换回纹理坐标
    return uv + center;
}

// 带有色散效果的球形畸变函数（无分支版本）
float4 sampleWithSphericalDistortionAndChromatic(Texture2D tex, SamplerState samp, float2 texcoord)
{
    // 基础畸变
    float2 center = float2(0.5, 0.5) + sphericalDistortionCenter;
    float2 uv = texcoord - center;
    
    // 应用宽高比校正
    uv.x *= screenWidth / screenHeight;
    
    float distance = length(uv);
    
    // 柔化边界处理，避免硬边界
    float edgeFade = smoothstep(sphericalDistortionRadius, sphericalDistortionRadius * 0.9, distance);
    
    // 不同颜色通道使用不同的畸变强度来模拟色散
    float distortionR = 1.0 + sphericalDistortionStrength * 1.02 * distance * distance;  // 红色通道稍强
    float distortionG = 1.0 + sphericalDistortionStrength * distance * distance;         // 绿色通道标准
    float distortionB = 1.0 + sphericalDistortionStrength * 0.98 * distance * distance;  // 蓝色通道稍弱
    
    // 在边界区域减少畸变强度
    distortionR = lerp(1.0, distortionR, edgeFade);
    distortionG = lerp(1.0, distortionG, edgeFade);
    distortionB = lerp(1.0, distortionB, edgeFade);
    
    // 应用畸变到不同颜色通道
    float2 uvR = uv * distortionR;
    float2 uvG = uv * distortionG;
    float2 uvB = uv * distortionB;
    
    // 恢复宽高比校正
    uvR.x /= screenWidth / screenHeight;
    uvG.x /= screenWidth / screenHeight;
    uvB.x /= screenWidth / screenHeight;
    
    // 转换回纹理坐标
    uvR += center;
    uvG += center;
    uvB += center;
    
    // 计算边界掩码（无分支方式）
    // 检查原始畸变坐标是否在有效范围内
    float2 validR = step(0.0, uvR) * step(uvR, 1.0);
    float2 validG = step(0.0, uvG) * step(uvG, 1.0);
    float2 validB = step(0.0, uvB) * step(uvB, 1.0);
    float borderMask = validR.x * validR.y * validG.x * validG.y * validB.x * validB.y;
    
    // 使用saturate来安全地限制坐标范围
    uvR = saturate(uvR);
    uvG = saturate(uvG);
    uvB = saturate(uvB);
    
    // 分别采样各颜色通道
    float4 distortedSample;
    distortedSample.r = tex.Sample(samp, uvR).r;
    distortedSample.g = tex.Sample(samp, uvG).g;
    distortedSample.b = tex.Sample(samp, uvB).b;
    distortedSample.a = tex.Sample(samp, uvG).a;
    
    // 原始采样作为备用
    float4 originalSample = tex.Sample(samp, texcoord);
    
    // 无分支混合：根据边界掩码选择使用哪个采样结果
    return lerp(originalSample, distortedSample, borderMask);
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float2 texCoord = input.position.xy / float2(screenWidth, screenHeight);

    float2 aspectCorrectTex = aspect_ratio_correction(texCoord);

    // 改进的视角计算：使用标准化的方向向量
    float3 virDir = normalize(scopePosition - cameraPosition);
    float3 lastVirDir = normalize(lastScopePosition - lastCameraPosition);
    float3 eyeDirectionLerp = virDir - lastVirDir;

    // 应用相机旋转变换
    float4 abseyeDirectionLerp = mul(float4(eyeDirectionLerp, 0), CameraRotation); // 使用0作为w分量，因为这是方向向量

    // 更平滑的边界处理，避免突变
    float epsilon = 0.001;
    abseyeDirectionLerp.y = sign(abseyeDirectionLerp.y) * max(abs(abseyeDirectionLerp.y), epsilon);

    // 无分支球形畸变效果应用
    // 使用step函数创建选择掩码
    float useDistortion = step(0.5, float(enableSphericalDistortion));
    float useChromatic = step(0.5, float(enableChromaticAberration));
    
    // 预先计算基础畸变坐标
    float2 distortedTexCoord = lerp(texCoord, applySphericalDistortion(texCoord), useDistortion);
    
    // 根据设置选择采样方法
    float useChromaticSampling = useDistortion * useChromatic;
    
    // 基础采样（原始或基础畸变）
    float4 basicColor = scopeTexture.Sample(scopeSampler, distortedTexCoord);
    
    // 色散采样（仅当需要时）
    float4 chromaticColor = lerp(basicColor, 
                                sampleWithSphericalDistortionAndChromatic(scopeTexture, scopeSampler, texCoord), 
                                useChromaticSampling);
    
    float4 color = chromaticColor;
    //color *= 100;

    float2 eye_velocity = clampMagnitude(abseyeDirectionLerp.xy, 1.5f);

    float2 parallax_offset = float2(0.5 + eye_velocity.x, 0.5 - eye_velocity.y);
    float distToParallax = distance(aspectCorrectTex, parallax_offset);
    float2 scope_center = float2(0.5, 0.5);
    float distToCenter = distance(aspectCorrectTex, scope_center);

    float parallaxValue = (step(distToCenter, 2) * getparallax(distToParallax, float2(1, 1), 1));
    
    float2 reticleTexCoord = transform_reticle_coords(aspectCorrectTex);
    reticleTexCoord = float2(1.0 - reticleTexCoord.x, reticleTexCoord.y);
    float4 reticleColor = reticleTexture.Sample(scopeSampler, reticleTexCoord);
    
    // 计算夜视和热成像效果（无分支）
    float4 nightVisionColor = applyNightVision(color, texCoord);
    float4 thermalColor = applyThermal(color, texCoord);
    
    // 使用lerp进行无分支混合，enableXXX作为混合因子
    color = lerp(color, nightVisionColor, float(enableNightVision));
    color = lerp(color, thermalColor, float(enableThermalVision));
    
    // 叠加准星
    color = reticleColor * reticleColor.a + color * (1 - reticleColor.a);

    // 改进的视差效果：避免完全消除光照
    // 使用更柔和的混合，确保光照反射在所有角度下都可见
    float softParallax = lerp(0.3, 1.0, parallaxValue); // 最小保持30%的光照
    color.rgb *= softParallax;

    return color;
}
