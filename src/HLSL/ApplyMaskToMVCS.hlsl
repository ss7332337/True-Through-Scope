// ApplyMaskToMVCS.hlsl
// Compute shader to apply interpolation mask to motion vectors
// Pixels with mask > 0 will have their MV zeroed, causing Frame Generation
// to copy from source frame rather than interpolate (eliminating ghosting)

// Input textures
Texture2D<float2> MotionVectorInput : register(t0);  // RT_29 Motion Vectors (RG16F)
Texture2D<float> CurrentMask : register(t1);         // Current frame mask (R8_UNORM)
Texture2D<float> PreviousMask : register(t2);        // Previous frame mask (R8_UNORM)

// Output buffer
RWTexture2D<float2> MotionVectorOutput : register(u0);  // Shared MV buffer

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadId.xy;
    
    // Read motion vector from source
    float2 mv = MotionVectorInput[pixelCoord];
    
    // Read masks from both frames
    // Dual-mask approach: use OR of current and previous frame masks
    // This reduces edge artifacts at mask boundaries
    float currentMaskValue = CurrentMask[pixelCoord];
    float previousMaskValue = PreviousMask[pixelCoord];
    
    // If either mask has a value > 0, zero the motion vector
    // This tells Frame Generation to copy this pixel directly instead of interpolating
    float combinedMask = max(currentMaskValue, previousMaskValue);
    
    if (combinedMask > 0.0f)
    {
        // Zero motion vector for masked (scope) pixels
        mv = float2(0.0f, 0.0f);
    }
    
    // Write to shared output buffer
    MotionVectorOutput[pixelCoord] = mv;
}
