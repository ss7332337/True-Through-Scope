#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <string>

namespace ThroughScope
{
    using namespace DirectX;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }
#endif

    // Constant buffer structure for Scope rendering
    struct ScopeConstantBuffer
    {
        float screenWidth;
        float screenHeight;
        int enableNightVision;

        // Viewport dimensions
        float viewportWidth;
        float viewportHeight;
        float padding_viewport[3];

        float cameraPosition[3];
        float padding2;

        float scopePosition[3];
        float padding3;

        float lastCameraPosition[3];
        float padding4;

        float lastScopePosition[3];
        float padding5;

        // Parallax parameters
        float parallaxStrength;
        float parallaxSmoothing;
        float exitPupilRadius;
        float exitPupilSoftness;

        float vignetteStrength;
        float vignetteRadius;
        float vignetteSoftness;
        float eyeReliefDistance;

        float reticleScale;
        float reticleOffsetX;
        float reticleOffsetY;
        float reticleZoomScale;    // 准星放大倍率（启用时为 1/fovMult，禁用时为 1.0）

        int   enableParallax;
        float _paddingBeforeMatrix[3];  // 对齐填充，确保 CameraRotation 从 16 字节边界开始

        XMFLOAT4X4 CameraRotation;

        // Night Vision
        float nightVisionIntensity;
        float nightVisionNoiseScale;
        float nightVisionNoiseAmount;
        float nightVisionGreenTint;

        // 高级视差参数
        float parallaxFogRadius;             // 边缘渐变半径
        float parallaxMaxTravel;             // 最大移动距离
        float reticleParallaxStrength;       // 准星偏移强度
        float _padding1;                     // 对齐填充

        // Distortion
        float sphericalDistortionStrength;
        float sphericalDistortionRadius;
        float sphericalDistortionCenter[2];

        int enableSphericalDistortion;
        int enableChromaticAberration;
        float brightnessBoost;
        float ambientOffset;
    };


    class D3DResourceManager
    {
    public:
        static D3DResourceManager* GetSingleton();

        bool Initialize(ID3D11Device* device);
        void Cleanup();

        // Resource Accessors
        ID3D11PixelShader* GetScopePixelShader() const { return m_scopePixelShader.Get(); }
        ID3D11SamplerState* GetSamplerState() const { return m_samplerState.Get(); }
        ID3D11SamplerState* GetLUTSamplerState() const { return m_lutSamplerState.Get(); }
        ID3D11BlendState* GetBlendState() const { return m_blendState.Get(); }
        ID3D11Buffer* GetConstantBuffer() const { return m_constantBuffer.Get(); }
        
        ID3D11ShaderResourceView* GetScopeTextureView() const { return m_scopeTextureView.Get(); }
        void SetScopeTextureView(ID3D11ShaderResourceView* view) { m_scopeTextureView = view; }

        ID3D11ShaderResourceView* GetReticleSRV() const { return m_reticleSRV.Get(); }

        // Resource Operations
        HRESULT CreateShaderFromFile(const wchar_t* csoFileName, const wchar_t* hlslFileName, LPCSTR entryPoint, LPCSTR shaderModel, ID3DBlob** ppBlobOut);
        bool LoadReticleTexture(ID3D11Device* device, const std::string& path);
        
        // Ensures the staging texture exists and matches the description. Returns true if recreated or valid.
        bool EnsureStagingTexture(ID3D11Device* device, const D3D11_TEXTURE2D_DESC* desc);
        ID3D11Texture2D* GetStagingTexture() const { return m_stagingTexture.Get(); }

        void UpdateConstantBuffer(ID3D11DeviceContext* context, const ScopeConstantBuffer& data);

    private:
        D3DResourceManager() = default;
        ~D3DResourceManager() = default;
        D3DResourceManager(const D3DResourceManager&) = delete;
        D3DResourceManager& operator=(const D3DResourceManager&) = delete;

        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_scopePixelShader;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_lutSamplerState;
        Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendState;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;
        
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_stagingTexture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_scopeTextureView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_reticleTexture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_reticleSRV;
    };
}
