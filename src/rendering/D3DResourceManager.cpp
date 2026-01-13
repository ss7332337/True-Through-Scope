#include "D3DResourceManager.h"
#include "Utilities.h"
#include <d3dcompiler.h>
#include <DDSTextureLoader11.h>

#pragma comment(lib, "d3dcompiler.lib")

namespace ThroughScope
{
    D3DResourceManager* D3DResourceManager::GetSingleton()
    {
        static D3DResourceManager instance;
        return &instance;
    }

    bool D3DResourceManager::Initialize(ID3D11Device* device)
    {
        if (!device) return false;

        HRESULT hr;

        // 1. Create Sampler State
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        samplerDesc.BorderColor[0] = 0.0f;
        samplerDesc.BorderColor[1] = 0.0f;
        samplerDesc.BorderColor[2] = 0.0f;
        samplerDesc.BorderColor[3] = 0.0f;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        hr = device->CreateSamplerState(&samplerDesc, m_samplerState.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            logger::error("Failed to create sampler state");
            return false;
        }

        // 2. Create LUT Sampler State (Clamp)
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        
        hr = device->CreateSamplerState(&samplerDesc, m_lutSamplerState.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            logger::error("Failed to create LUT sampler state");
            return false;
        }

        // 3. Create Blend State
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        hr = device->CreateBlendState(&blendDesc, m_blendState.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            logger::error("Failed to create blend state");
            return false;
        }

        // 4. Create Constant Buffer
        D3D11_BUFFER_DESC cbd = {};
        cbd.Usage = D3D11_USAGE_DEFAULT;
        cbd.ByteWidth = sizeof(ScopeConstantBuffer);
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.CPUAccessFlags = 0;
        cbd.MiscFlags = 0;
        cbd.StructureByteStride = 0;

        hr = device->CreateBuffer(&cbd, nullptr, m_constantBuffer.ReleaseAndGetAddressOf());
        if (FAILED(hr)) {
            logger::error("Failed to create constant buffer");
            return false;
        }

        // 5. Load Scope Shader
        ID3DBlob* psBlob = nullptr;
        // Use default path or make it configurable
        hr = CreateShaderFromFile(L"Data\\Shaders\\XiFeiLi\\TrueScopeShader.cso", L"src\\HLSL\\TrueScopeShader.hlsl", "main", "ps_5_0", &psBlob);
        if (SUCCEEDED(hr) && psBlob) {
            hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_scopePixelShader.ReleaseAndGetAddressOf());
            psBlob->Release();
            if (FAILED(hr)) {
                logger::error("Failed to create scope pixel shader");
                return false;
            }
        } else {
             logger::error("Failed to compile/load scope shader");
             return false;
        }

        return true;
    }

    void D3DResourceManager::Cleanup()
    {
        m_scopePixelShader.Reset();
        m_samplerState.Reset();
        m_lutSamplerState.Reset();
        m_blendState.Reset();
        m_constantBuffer.Reset();
        m_scopeTextureView.Reset();
        m_reticleTexture.Reset();
        m_reticleSRV.Reset();
    }

    HRESULT D3DResourceManager::CreateShaderFromFile(const wchar_t* csoFileNameInOut, const wchar_t* hlslFileName, LPCSTR entryPoint, LPCSTR shaderModel, ID3DBlob** ppBlobOut)
    {
        HRESULT hr = S_OK;

        if (csoFileNameInOut && D3DReadFileToBlob(csoFileNameInOut, ppBlobOut) == S_OK) {
            return hr;
        } else {
            DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
            dwShaderFlags |= D3DCOMPILE_DEBUG;
            dwShaderFlags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            ID3DBlob* errorBlob = nullptr;
            hr = D3DCompileFromFile(hlslFileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, shaderModel,
                dwShaderFlags, 0, ppBlobOut, &errorBlob);
            if (FAILED(hr)) {
                if (errorBlob != nullptr) {
                    OutputDebugStringA(reinterpret_cast<const char*>(errorBlob->GetBufferPointer()));
                }
                SAFE_RELEASE(errorBlob);
                return hr;
            }

            if (csoFileNameInOut) {
                return D3DWriteBlobToFile(*ppBlobOut, csoFileNameInOut, FALSE);
            }
        }

        return hr;
    }

    bool D3DResourceManager::LoadReticleTexture(ID3D11Device* device, const std::string& path)
    {
        if (path.empty() || !device) return false;

        const wchar_t* tempPath = Utilities::GetWC(path.c_str());
        std::wstring defaultPath = L"Data/Textures/TTS/Reticle/Empty.dds";

        m_reticleTexture.Reset();
        m_reticleSRV.Reset();

        HRESULT hr = CreateDDSTextureFromFile(
            device,
            tempPath ? tempPath : defaultPath.c_str(),
            nullptr,
            m_reticleSRV.GetAddressOf());

        if (tempPath) ::free((void*)tempPath);

        if (FAILED(hr)) {
            logger::error("Failed to load reticle texture from path: {}", path);
            return false;
        }

        return true;
    }

    bool D3DResourceManager::EnsureStagingTexture(ID3D11Device* device, const D3D11_TEXTURE2D_DESC* desc)
    {
        if (!device || !desc) return false;

        bool needRecreate = false;
        if (!m_stagingTexture) {
            needRecreate = true;
        } else {
            D3D11_TEXTURE2D_DESC existingDesc;
            m_stagingTexture->GetDesc(&existingDesc);
            if (existingDesc.Width != desc->Width || 
                existingDesc.Height != desc->Height ||
                existingDesc.Format != desc->Format) {
                needRecreate = true;
            }
        }

        if (needRecreate) {
            m_stagingTexture.Reset();
            m_scopeTextureView.Reset();

            D3D11_TEXTURE2D_DESC stagingDesc = *desc;
            stagingDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            stagingDesc.MiscFlags = 0;
            stagingDesc.SampleDesc.Count = 1;
            stagingDesc.SampleDesc.Quality = 0;
            stagingDesc.Usage = D3D11_USAGE_DEFAULT;
            stagingDesc.CPUAccessFlags = 0;

            HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, m_stagingTexture.GetAddressOf());
            if (FAILED(hr)) {
                logger::error("Failed to create staging texture");
                return false;
            }

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = stagingDesc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;

            hr = device->CreateShaderResourceView(m_stagingTexture.Get(), &srvDesc, m_scopeTextureView.GetAddressOf());
            if (FAILED(hr)) {
                logger::error("Failed to create staging SRV");
                return false;
            }
            //logger::info("Staging texture created/resized: {}x{}", desc->Width, desc->Height);
        }
        return true;
    }

    void D3DResourceManager::UpdateConstantBuffer(ID3D11DeviceContext* context, const ScopeConstantBuffer& data)
    {
        if (context && m_constantBuffer) {
            context->UpdateSubresource(m_constantBuffer.Get(), 0, nullptr, &data, 0, 0);
        }
    }
}
