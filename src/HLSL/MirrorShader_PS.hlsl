Texture2D MirrorTexture : register(t0);
SamplerState Sampler : register(s0);


struct PS_IN
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

// 像素着色器
float4 main(PS_IN input) : SV_Target
{
    return MirrorTexture.Sample(Sampler, input.Tex);
}
