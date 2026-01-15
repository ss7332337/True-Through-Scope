Texture2D scopeTexture : register(t0);
Texture2D reticleTexture : register(t1);


SamplerState scopeSampler : register(s0);

            
// Constants buffer containing screen resolution, camera position and scope position
cbuffer ScopeConstants : register(b0)
{
    float screenWidth;
    float screenHeight;
    int enableNightVision;


    // Viewport dimensions for DLSS/FSR3 upscaling
    float viewportWidth;
    float viewportHeight;
    float3 padding_viewport;

    float3 cameraPosition;
    float padding2; // 16-byte alignment

    float3 scopePosition;
    float padding3; // 16-byte alignment

    float3 lastCameraPosition;
    float padding4; // 16-byte alignment

    float3 lastScopePosition;
    float padding5; // 16-byte alignment

    // 新的视差参数 - 基于真实瞄镜光学原理
    float parallaxStrength;         // 视差偏移强度 (建议 0.02-0.08)
    float parallaxSmoothing;        // 视差时域平滑系数 (0.0-1.0, 越大越平滑)
    float exitPupilRadius;          // 出瞳半径 - 眼睛可视范围 (0.3-0.6)
    float exitPupilSoftness;        // 出瞳边缘柔和度 (0.1-0.3)

    float vignetteStrength;         // 边缘晕影强度 (0.0-1.0)
    float vignetteRadius;           // 晕影起始半径 (0.5-0.9)
    float vignetteSoftness;         // 晕影过渡柔和度 (0.1-0.5)
    float eyeReliefDistance;        // 眼距模拟 - 影响视差灵敏度

    float reticleScale;
    float reticleOffsetX;
    float reticleOffsetY;
    float reticleZoomScale;         // 准星放大倍率（启用时为 1/fovMult，禁用时为 1.0）

    int enableParallax;             // 是否启用视差效果
    float3 _paddingBeforeMatrix;    // 对齐填充，确保 CameraRotation 从 16 字节边界开始

    float4x4 CameraRotation;

    // 夜视效果参数
    float nightVisionIntensity;    // 夜视强度
    float nightVisionNoiseScale;   // 噪点缩放
    float nightVisionNoiseAmount;  // 噪点强度
    float nightVisionGreenTint;    // 绿色色调强度

    // 高级视差参数
    float parallaxFogRadius;           // 边缘渐变半径
    float parallaxMaxTravel;           // 最大移动距离
    float reticleParallaxStrength;     // 准星偏移强度
    float _padding1;                   // 对齐填充

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

// ============================================================================
// 新的科学视差系统 - 基于真实瞄镜光学原理
// ============================================================================

// 安全的向量归一化（避免除零）
float2 safeNormalize(float2 v)
{
    float len = length(v);
    return len > 0.0001 ? v / len : float2(0, 0);
}

// 限制向量长度
float2 clampLength(float2 v, float maxLen)
{
    float len = length(v);
    return len > maxLen ? v * (maxLen / len) : v;
}


float2 calculateEyeOffset(float3 camPos, float3 scopePos, float3 lastCamPos, float3 lastScopePos, float4x4 scopeRotInv)
{

    float3 scopeToEye = camPos - scopePos;
    
    // 计算瞄镜到眼睛的距离（用于归一化）
    float eyeDistance = length(scopeToEye);
    if (eyeDistance < 0.001) {
        return float2(0, 0);
    }
    
    float4 localEyeDir = mul(float4(scopeToEye, 0), scopeRotInv);
    
    // 归一化：将偏移量除以眼距 (Y轴分量近似为距离)
    // 使用 X (Right) 和 Z (Up) 作为横向偏移分量
    float2 eyeOffset;
    eyeOffset.x = localEyeDir.x / eyeDistance; // Horizontal offset
    eyeOffset.y = localEyeDir.z / eyeDistance; // Vertical offset
    
    return eyeOffset;
}

// 视差计算函数 - 计算出瞳边缘的亮度衰减
float getparallax(float d, float2 ds, float radius, float sway, float fogRadius)
{
    // 为了防止除零，radius 应该 > 0
    float safeRadius = max(radius, 0.001);
    
    // 计算暗角因子
    float factor = abs((fogRadius / safeRadius) * d);
    
    // 应用指数衰减
    float vignette = 1.0 - pow(factor, sway);
    
    return clamp(vignette, 0.0, 1.0);
}


float2 aspect_ratio_correction(float2 uv)
{
    float2 centered = uv - 0.5;
    centered.x *= screenWidth / screenHeight;
    return centered;
}

// 边缘晕影效果
float calculateVignette(float2 uv, float strength, float radius, float softness)
{
    // 计算到中心的距离（应用宽高比校正）
    float2 centered = aspect_ratio_correction(uv);
    float dist = length(centered);

    // 平滑的晕影过渡
    float vignette = smoothstep(radius - softness, radius + softness, dist);

    // 返回亮度系数（1.0 = 完全明亮，越小越暗）
    return 1.0 - vignette * strength;
}

struct ParallaxResult
{
    float2 textureOffset;   // 纹理采样偏移 (现在只用于微量修正或置为0)
    float3 brightnessMask;  // 亮度遮罩 (RGB 分离，用于色散)
    float2 reticleOffset;   // 准星偏移
};

ParallaxResult computeParallax(
    float2 uv,
    float2 eyeOffset,
    float strength,
    float eyeRelief,
    float exitPupilRadius,
    float exitPupilSoftness,
    float vignetteStrength,
    float vignetteRadius,
    float vignetteSoftness,
    int enabled)
{
    ParallaxResult result;
    result.textureOffset = float2(0, 0);
    result.brightnessMask = float3(1, 1, 1);
    result.reticleOffset = float2(0, 0);

    if (enabled == 0) {
        // 仅应用普通晕影
        float v = calculateVignette(uv, vignetteStrength, vignetteRadius, vignetteSoftness);
        result.brightnessMask = float3(v, v, v);
        return result;
    }

    // 限制眼睛偏移的最大移动距离
    float2 clampedEyeOffset = clampLength(eyeOffset, parallaxMaxTravel);

    // 1. 动态出瞳 (Dynamic Exit Pupil)
    float sensitivity = 1.0 + eyeRelief; 
    float2 maskCenterOffset = -clampedEyeOffset * strength * sensitivity; // 反向移动
    float2 maskCenter = float2(0.5, 0.5) + maskCenterOffset;
    
    // 计算当前像素到遮罩中心的距离 (宽高比校正)
    float2 correctedUV = aspect_ratio_correction(uv);
    float2 correctedCenter = aspect_ratio_correction(maskCenter);
    float distToCenter = distance(correctedUV, correctedCenter);
    
    // 2. 出瞳边缘遮罩
    float baseRadius = exitPupilRadius;
    
    // 从 exitPupilSoftness 计算边缘过渡指数 (sway)
    float sway = 5.0 * (1.1 - clamp(exitPupilSoftness, 0.1, 1.0));
    
    // 计算出瞳边缘遮罩 (单一值，无色散)
    float mask = getparallax(distToCenter, float2(1,1), baseRadius, sway, parallaxFogRadius);
    
    // 3. 基础晕影叠加
    float staticVignette = calculateVignette(uv, vignetteStrength, vignetteRadius, vignetteSoftness);
    
    result.brightnessMask = float3(mask, mask, mask) * staticVignette;
    
    // 4. 准星视差 (Reticle Parallax)
    // 使用独立的 reticleParallaxStrength 参数
    result.reticleOffset = clampedEyeOffset * reticleParallaxStrength;

    return result;
}

float2 transform_reticle_coords(float2 uv)
{
    float2 centered = uv - 0.5;
    float aspectRatio = screenWidth / screenHeight;
    centered.x *= aspectRatio;
    
    // 最终缩放 = 用户设置的基础缩放 * 放大倍率
    // reticleZoomScale: 启用时为 1/fovMult (越放大越大)，禁用时为 1.0
    float finalScale = max(reticleScale * reticleZoomScale, 0.01);
    centered /= finalScale;
    
    centered.x -= reticleOffsetX * aspectRatio;  // X偏移也需要考虑宽高比
    centered.y += reticleOffsetY;
    // 注意：不需要撤销宽高比校正，因为我们就是要让采样范围不对称
    float2 texCoord = centered + 0.5;
    
    return texCoord;
}

// 生成随机噪点
float random(float2 st) {
    return frac(sin(dot(st.xy, float2(12.9898, 78.233))) * 43758.5453123);
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
    // radiusMask: 0 在中心区域（应用完整畸变），1 在边缘外（无畸变）
    float radiusMask = smoothstep(sphericalDistortionRadius * 0.8, sphericalDistortionRadius, distance);
    distortionFactor = lerp(distortionFactor, 1.0, radiusMask);
    
    // 应用畸变
    uv *= distortionFactor;
    
    // 恢复宽高比校正
    uv.x /= screenWidth / screenHeight;
    
    // 转换回纹理坐标
    return uv + center;
}

// Spherical distortion with chromatic aberration (branchless)
float4 sampleWithSphericalDistortionAndChromatic(Texture2D tex, SamplerState samp, float2 texcoord, float2 textureScale)
{
    float2 center = float2(0.5, 0.5) + sphericalDistortionCenter;
    float2 uv = texcoord - center;
    
    uv.x *= screenWidth / screenHeight;
    
    float distance = length(uv);
    // edgeFade: 1 在中心区域（应用完整畸变），0 在边缘外（无畸变）
    float edgeFade = 1.0 - smoothstep(sphericalDistortionRadius * 0.9, sphericalDistortionRadius, distance);
    
    // Chromatic aberration: different distortion per channel
    // 增强色散差异系数，使效果更明显
    float distortionR = 1.0 + sphericalDistortionStrength * 1.05 * distance * distance;
    float distortionG = 1.0 + sphericalDistortionStrength * distance * distance;
    float distortionB = 1.0 + sphericalDistortionStrength * 0.95 * distance * distance;
    
    // edgeFade=1 时应用完整畸变，edgeFade=0 时无畸变
    distortionR = lerp(1.0, distortionR, edgeFade);
    distortionG = lerp(1.0, distortionG, edgeFade);
    distortionB = lerp(1.0, distortionB, edgeFade);
    
    float2 uvR = uv * distortionR;
    float2 uvG = uv * distortionG;
    float2 uvB = uv * distortionB;
    
    uvR.x /= screenWidth / screenHeight;
    uvG.x /= screenWidth / screenHeight;
    uvB.x /= screenWidth / screenHeight;
    
    uvR += center;
    uvG += center;
    uvB += center;
    
    float2 validR = step(0.0, uvR) * step(uvR, 1.0);
    float2 validG = step(0.0, uvG) * step(uvG, 1.0);
    float2 validB = step(0.0, uvB) * step(uvB, 1.0);
    float borderMask = validR.x * validR.y * validG.x * validG.y * validB.x * validB.y;
    
    uvR = saturate(uvR);
    uvG = saturate(uvG);
    uvB = saturate(uvB);
    
    // Apply texture scale for DLSS/FSR3
    uvR *= textureScale;
    uvG *= textureScale;
    uvB *= textureScale;
    float2 scaledTexcoord = texcoord * textureScale;
    
    float4 distortedSample;
    distortedSample.r = tex.Sample(samp, uvR).r;
    distortedSample.g = tex.Sample(samp, uvG).g;
    distortedSample.b = tex.Sample(samp, uvB).b;
    distortedSample.a = tex.Sample(samp, uvG).a;
    
    float4 originalSample = tex.Sample(samp, scaledTexcoord);
    
    return lerp(originalSample, distortedSample, borderMask);
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float2 texCoord = input.position.xy / float2(viewportWidth, viewportHeight);
    float2 aspectCorrectTex = aspect_ratio_correction(texCoord);

    // 计算眼睛相对于瞄镜的偏移
    float2 eyeOffset = calculateEyeOffset(
        cameraPosition, scopePosition,
        lastCameraPosition, lastScopePosition,
        CameraRotation
    );

    // 计算完整的视差效果
    ParallaxResult parallax = computeParallax(
        texCoord,
        eyeOffset,
        parallaxStrength,
        eyeReliefDistance,
        exitPupilRadius,
        exitPupilSoftness,
        vignetteStrength,
        vignetteRadius,
        vignetteSoftness,
        enableParallax
    );

    // 应用视差偏移到纹理坐标
    float2 parallaxedTexCoord = texCoord + parallax.textureOffset;

    // 确保采样坐标在有效范围内
    parallaxedTexCoord = saturate(parallaxedTexCoord);

    // ========================================================================
    // 球形畸变效果
    // ========================================================================

    float useDistortion = step(0.5, float(enableSphericalDistortion));
    float useChromatic = step(0.5, float(enableChromaticAberration));

    // 在视差偏移后的坐标上应用畸变
    float2 distortedTexCoord = lerp(parallaxedTexCoord, applySphericalDistortion(parallaxedTexCoord), useDistortion);

    float useChromaticSampling = useDistortion * useChromatic;

    // DLSS/FSR3 upscaling: scale UV to valid texture region
    float2 textureScale = float2(viewportWidth / screenWidth, viewportHeight / screenHeight);
    float2 scaledTexCoord = distortedTexCoord * textureScale;

    float4 basicColor = scopeTexture.Sample(scopeSampler, scaledTexCoord);
    float4 chromaticColor = lerp(basicColor,
                                sampleWithSphericalDistortionAndChromatic(scopeTexture, scopeSampler, parallaxedTexCoord, textureScale),
                                useChromaticSampling);

    float4 color = chromaticColor;

    // ========================================================================
    // 准星处理 - 考虑视差偏移
    // ========================================================================

    // 准星坐标 - 直接使用屏幕UV（加上视差偏移）
    // 不需要 aspect_ratio_correction，因为准星纹理本身是方形的
    // FIX: Reverse parallax X offset to compensate for X-flip / 反转视差X偏移以补偿翻转
    float2 reticleInputUV = texCoord + float2(-parallax.reticleOffset.x, parallax.reticleOffset.y);
    float2 reticleTexCoord = transform_reticle_coords(reticleInputUV);
    reticleTexCoord.x = 1.0 - reticleTexCoord.x;
    float4 reticleColor = reticleTexture.Sample(scopeSampler, reticleTexCoord);

    // ========================================================================
    // 特殊视觉效果
    // ========================================================================

    // 计算夜视和热成像效果
    float4 nightVisionColor = applyNightVision(color, texCoord);


    color = lerp(color, nightVisionColor, float(enableNightVision));


    // ========================================================================
    // 最终合成
    // ========================================================================

    // 叠加准星（准星不受亮度遮罩影响）
    float3 finalColor = color.rgb;

    // 应用视差亮度遮罩（出瞳效应 + 晕影）
    // 只影响场景，不影响准星
    finalColor *= parallax.brightnessMask;

    // 叠加准星
    finalColor = lerp(finalColor, reticleColor.rgb, reticleColor.a);

    // 亮度增强（可选，用于补偿整体变暗）
    finalColor *= max(brightnessBoost, 1.0);

    return float4(finalColor, color.a);
}
