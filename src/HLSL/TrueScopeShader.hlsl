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

    // Viewport dimensions for DLSS/FSR3 upscaling
    float viewportWidth;
    float viewportHeight;
    float2 padding_viewport;

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
    int enableParallax;             // 是否启用视差效果

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

// 计算眼睛相对于瞄镜光轴的偏移
// 返回归一化的2D偏移量，表示眼睛在垂直于瞄镜光轴平面上的位置
// 
// 真实瞄镜视差原理：
// 1. 瞄镜有一个"光轴"（理想观察方向）
// 2. 当眼睛不在光轴正后方时，观察到的远景会相对于准星产生偏移
// 3. 偏移方向：眼睛向左移动 → 远景相对准星向右偏移
//
// 实现方式：
// - 使用 CameraRotation（瞄镜的世界旋转矩阵的转置）将世界空间向量转换到瞄镜局部空间
// - 在瞄镜局部空间中，Z轴是光轴前方，X是右，Y是上
// - 相机相对于瞄镜的X/Y分量就是眼睛偏离光轴的横向偏移
float2 calculateEyeOffset(float3 camPos, float3 scopePos, float3 lastCamPos, float3 lastScopePos, float4x4 scopeRotInv)
{
    // 计算从瞄镜指向相机的向量（世界空间）
    // 这表示眼睛相对于瞄镜的位置
    float3 scopeToEye = camPos - scopePos;
    
    // 计算瞄镜到眼睛的距离（用于归一化）
    float eyeDistance = length(scopeToEye);
    if (eyeDistance < 0.001) {
        return float2(0, 0);
    }
    
    // 将"瞄镜到眼睛"向量转换到瞄镜的局部坐标系
    // scopeRotInv 是瞄镜世界旋转矩阵的转置（逆），用于将世界向量转换为局部向量
    // 在瞄镜局部坐标系中：
    //   - Y轴通常是前方向 (Bethesda/NIF Convention)
    //   - X轴是右方向
    //   - Z轴是上方向
    float4 localEyeDir = mul(float4(scopeToEye, 0), scopeRotInv);
    
    // 归一化：将偏移量除以眼距 (Y轴分量近似为距离)
    // 使用 X (Right) 和 Z (Up) 作为横向偏移分量
    float2 eyeOffset;
    eyeOffset.x = localEyeDir.x / eyeDistance; // Horizontal offset
    eyeOffset.y = localEyeDir.z / eyeDistance; // Vertical offset
    
    return eyeOffset;
}

// FTS 风格的视差计算函数
float getparallax(float d, float2 ds, float radius, float sway, float fogRadius)
{
    // ds.y 在 FTS 中通常是 1.0 (来自 float2(1,1))
    // rcp(radius * ds.y) -> 1/radius
    // fogRadius * d * ds.y -> fogRadius * d
    // abs(...) -> abs((fogRadius/radius) * d)
    // 1 - pow(...) -> 边缘衰减
    // clamp(..., 0, 1) -> 截断
    
    // 为了防止除零，radius 应该 > 0
    float safeRadius = max(radius, 0.001);
    
    // 计算暗角因子
    float factor = abs((fogRadius / safeRadius) * d);
    
    // 应用指数衰减 (sway amount)
    // FTS 中 sway 也是作为指数
    float vignette = 1.0 - pow(factor, sway);
    
    return clamp(vignette, 0.0, 1.0);
}

// 组合所有视差效果 (FTS 移植版 + 增强)

// 宽高比校正函数 (将UV转换为以中心为原点，且修正了长宽比的坐标)
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

    // 1. 动态出瞳 (Dynamic Exit Pupil)
    // ----------------------------------------------------
    // 计算遮罩中心
    // FTS 逻辑: 0.5 + x, 0.5 - y.
    // 我们: x = Cam-Scope (Right+). y = Cam-Scope (Up+).
    // Eye Right -> Mask Left (Negative Offset).
    // Eye Up -> Mask Down (Negative Offset). -> 这样才能露出一部分"下面"的视野?
    // 验证: Look Up -> See "Floor" of tube -> Bright Spot moves Down relative to Near Rim. YES.
    
    float sensitivity = 1.0 + eyeRelief; 
    float2 maskCenterOffset = -eyeOffset * strength * sensitivity; // 反向移动
    float2 maskCenter = float2(0.5, 0.5) + maskCenterOffset;
    
    // 计算当前像素到遮罩中心的距离 (宽高比校正)
    float2 correctedUV = aspect_ratio_correction(uv);
    float2 correctedCenter = aspect_ratio_correction(maskCenter);
    float distToCenter = distance(correctedUV, correctedCenter);
    
    // 2. 出瞳边缘色散 (Chromatic Aberration at Edge)
    // ----------------------------------------------------
    float edgeExponent = 5.0 * (1.1 - clamp(exitPupilSoftness, 0.1, 1.0)); 
    float caShift = 0.015 * strength * 20.0; 
    
    float radiusR = exitPupilRadius * (1.0 - caShift);
    float radiusG = exitPupilRadius;
    float radiusB = exitPupilRadius * (1.0 + caShift);
    
    float maskR = getparallax(distToCenter, float2(1,1), radiusR, edgeExponent, 1.0);
    float maskG = getparallax(distToCenter, float2(1,1), radiusG, edgeExponent, 1.0);
    float maskB = getparallax(distToCenter, float2(1,1), radiusB, edgeExponent, 1.0);
    
    // 3. 基础晕影叠加
    float staticVignette = calculateVignette(uv, vignetteStrength, vignetteRadius, vignetteSoftness);
    
    result.brightnessMask = float3(maskR, maskG, maskB) * staticVignette;
    
    // 4. 准星视差 (Reticle Parallax)
    // 准星与眼睛同向微动，制造悬浮感
    result.reticleOffset = eyeOffset * strength * 0.5;

    return result;
}

// ============================================================================

float2 transform_reticle_coords(float2 uv)
{
    // Input: uv is in [0, 1] range, where (0.5, 0.5) is screen center.
    // Output: Should return texture sample coordinates for the reticle.
    //
    // Goal:
    //   - Offset (0, 0) -> Reticle centered on screen.
    //   - Offset X > 0 -> Reticle moves RIGHT.
    //   - Offset Y > 0 -> Reticle moves UP.
    //   - Scale > 1 -> Larger reticle. Scale < 1 -> Smaller reticle.
    
    // 1. Center UV to [-0.5, 0.5]
    float2 centered = uv - 0.5;
    
    // 2. Aspect ratio correction
    // Screen is typically wider than tall (e.g., 16:9).
    // To maintain reticle proportions, scale X to match Y's physical distance.
    float aspectRatio = screenWidth / screenHeight;
    centered.x *= aspectRatio;
    
    // 3. Apply scale: DIVIDE so larger scale = larger reticle on screen
    // (When we divide by a larger number, the sampling coords are smaller,
    //  meaning we sample a smaller area of texture, making the texture appear bigger)
    float safeScale = max(reticleScale, 0.01); // Prevent division by zero
    centered /= safeScale;
    
    // 4. Apply offset
    // X > 0 -> Reticle moves RIGHT -> subtract from sample X
    // Y > 0 -> Reticle moves UP -> add to sample Y (because UV Y is inverted from screen Y)
    centered.x -= reticleOffsetX;
    centered.y += reticleOffsetY;
    
    // 5. Undo aspect ratio correction before texture sampling
    centered.x /= aspectRatio;
    
    // 6. Return to [0, 1] range for sampling
    float2 texCoord = centered + 0.5;
    
    return texCoord;
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

// Spherical distortion with chromatic aberration (branchless)
float4 sampleWithSphericalDistortionAndChromatic(Texture2D tex, SamplerState samp, float2 texcoord, float2 textureScale)
{
    float2 center = float2(0.5, 0.5) + sphericalDistortionCenter;
    float2 uv = texcoord - center;
    
    uv.x *= screenWidth / screenHeight;
    
    float distance = length(uv);
    float edgeFade = smoothstep(sphericalDistortionRadius, sphericalDistortionRadius * 0.9, distance);
    
    // Chromatic aberration: different distortion per channel
    float distortionR = 1.0 + sphericalDistortionStrength * 1.02 * distance * distance;
    float distortionG = 1.0 + sphericalDistortionStrength * distance * distance;
    float distortionB = 1.0 + sphericalDistortionStrength * 0.98 * distance * distance;
    
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

    // ========================================================================
    // 新的视差系统
    // ========================================================================

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
    float2 reticleInputUV = texCoord + parallax.reticleOffset;
    float2 reticleTexCoord = transform_reticle_coords(reticleInputUV);
    // 注意：不再进行 X 翻转，因为现在 transform_reticle_coords 已正确处理坐标
    float4 reticleColor = reticleTexture.Sample(scopeSampler, reticleTexCoord);

    // ========================================================================
    // 特殊视觉效果
    // ========================================================================

    // 计算夜视和热成像效果
    float4 nightVisionColor = applyNightVision(color, texCoord);
    float4 thermalColor = applyThermal(color, texCoord);

    color = lerp(color, nightVisionColor, float(enableNightVision));
    color = lerp(color, thermalColor, float(enableThermalVision));

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
