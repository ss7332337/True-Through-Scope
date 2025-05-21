Texture2D scopeTexture : register(t0);
SamplerState scopeSampler : register(s0);
            
// Constants buffer containing screen resolution, camera position and scope position
cbuffer ScopeConstants : register(b0)
{
    float screenWidth;
    float screenHeight;
    float2 padding1; // 16-byte alignment

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

    float4x4 CameraRotation;
}
            
struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 texCoord : TEXCOORD;
    float4 color0 : COLOR;
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

    //if (distToCenter > 2)
    //{
    //    return float4(0, 1, 0, 1); // Red indicates pixels where step() would return 0
    //}
    
    float parallaxValue = (step(distToCenter, 2) * getparallax(distToParallax, float2(1, 1), 1));
    //if (parallaxValue <= 0.01)
    //{
    //    return float4(0, 0, 1, 1); // Green indicates pixels where getparallax() returns near 0
    //}
    
    // Apply final effect
    color.rgb *= parallaxValue;
    return color;
}
