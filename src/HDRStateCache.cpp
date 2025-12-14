#include "PCH.h"
#include "HDRStateCache.h"
#include "D3DHooks.h"

namespace ThroughScope
{
    // 全局 HDR 状态缓存实例
    HDRStateCache g_HDRStateCache;

    ID3D11DeviceContext* GetCurrentD3DContext()
    {
        auto d3dHooks = D3DHooks::GetSington();
        if (d3dHooks) {
            return d3dHooks->GetContext();
        }
        return nullptr;
    }

    void HDRStateCache::Capture(ID3D11DeviceContext* context)
    {
        if (!context) {
            logger::warn("HDRStateCache::Capture: context is null");
            return;
        }

        Clear();

        // 捕获 Pixel Shader
        context->PSGetShader(pixelShader.ReleaseAndGetAddressOf(), nullptr, nullptr);
        
        // 捕获 Vertex Shader  
        context->VSGetShader(vertexShader.ReleaseAndGetAddressOf(), nullptr, nullptr);
        
        // 捕获 PS Constant Buffers
        ID3D11Buffer* cbuffers[MAX_CB] = {};
        context->PSGetConstantBuffers(0, MAX_CB, cbuffers);
        for (UINT i = 0; i < MAX_CB; ++i) {
            psConstantBuffers[i].Attach(cbuffers[i]);
            if (cbuffers[i]) numPSConstantBuffers = i + 1;
        }
        
        // 捕获 VS Constant Buffers
        ID3D11Buffer* vsCbuffers[MAX_CB] = {};
        context->VSGetConstantBuffers(0, MAX_CB, vsCbuffers);
        for (UINT i = 0; i < MAX_CB; ++i) {
            vsConstantBuffers[i].Attach(vsCbuffers[i]);
            if (vsCbuffers[i]) numVSConstantBuffers = i + 1;
        }
        
        // 捕获 PS Shader Resources
        ID3D11ShaderResourceView* srvs[MAX_SRV] = {};
        context->PSGetShaderResources(0, MAX_SRV, srvs);
        for (UINT i = 0; i < MAX_SRV; ++i) {
            psSRVs[i].Attach(srvs[i]);
            if (srvs[i]) numPSSRVs = i + 1;
        }

        // 复制关键纹理内容 (避免后续纹理内容被修改导致的问题)
        // 这是解决开枪时瞄具画面异常的关键
        ID3D11Device* device = nullptr;
        context->GetDevice(&device);
        if (device) {
            // 复制 Bloom 纹理 (t0)
            if (psSRVs[0]) {
                ID3D11Resource* bloomResource = nullptr;
                psSRVs[0]->GetResource(&bloomResource);
                if (bloomResource) {
                    ID3D11Texture2D* bloomTex = nullptr;
                    if (SUCCEEDED(bloomResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&bloomTex))) {
                        D3D11_TEXTURE2D_DESC desc;
                        bloomTex->GetDesc(&desc);

                        // 检查是否需要创建或重建复制纹理
                        bool needCreate = false;
                        if (!bloomTextureCopy) {
                            needCreate = true;
                        } else {
                            D3D11_TEXTURE2D_DESC existingDesc;
                            bloomTextureCopy->GetDesc(&existingDesc);
                            if (existingDesc.Width != desc.Width || existingDesc.Height != desc.Height || existingDesc.Format != desc.Format) {
                                bloomTextureCopy.Reset();
                                bloomSRVCopy.Reset();
                                needCreate = true;
                            }
                        }

                        if (needCreate) {
                            D3D11_TEXTURE2D_DESC copyDesc = desc;
                            copyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                            copyDesc.Usage = D3D11_USAGE_DEFAULT;
                            copyDesc.CPUAccessFlags = 0;
                            copyDesc.MiscFlags = 0;

                            if (SUCCEEDED(device->CreateTexture2D(&copyDesc, nullptr, bloomTextureCopy.GetAddressOf()))) {
                                device->CreateShaderResourceView(bloomTextureCopy.Get(), nullptr, bloomSRVCopy.GetAddressOf());
                            }
                        }

                        // 复制纹理内容
                        if (bloomTextureCopy) {
                            context->CopyResource(bloomTextureCopy.Get(), bloomTex);
                        }

                        bloomTex->Release();
                    }
                    bloomResource->Release();
                }
            }

            // 复制 Luminance 纹理 (t2)
            if (psSRVs[2]) {
                ID3D11Resource* lumResource = nullptr;
                psSRVs[2]->GetResource(&lumResource);
                if (lumResource) {
                    ID3D11Texture2D* lumTex = nullptr;
                    if (SUCCEEDED(lumResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&lumTex))) {
                        D3D11_TEXTURE2D_DESC desc;
                        lumTex->GetDesc(&desc);

                        // 检查是否需要创建或重建复制纹理
                        bool needCreate = false;
                        if (!luminanceTextureCopy) {
                            needCreate = true;
                        } else {
                            D3D11_TEXTURE2D_DESC existingDesc;
                            luminanceTextureCopy->GetDesc(&existingDesc);
                            if (existingDesc.Width != desc.Width || existingDesc.Height != desc.Height || existingDesc.Format != desc.Format) {
                                luminanceTextureCopy.Reset();
                                luminanceSRVCopy.Reset();
                                needCreate = true;
                            }
                        }

                        if (needCreate) {
                            D3D11_TEXTURE2D_DESC copyDesc = desc;
                            copyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                            copyDesc.Usage = D3D11_USAGE_DEFAULT;
                            copyDesc.CPUAccessFlags = 0;
                            copyDesc.MiscFlags = 0;

                            if (SUCCEEDED(device->CreateTexture2D(&copyDesc, nullptr, luminanceTextureCopy.GetAddressOf()))) {
                                device->CreateShaderResourceView(luminanceTextureCopy.Get(), nullptr, luminanceSRVCopy.GetAddressOf());
                            }
                        }

                        // 复制纹理内容
                        if (luminanceTextureCopy) {
                            context->CopyResource(luminanceTextureCopy.Get(), lumTex);
                        }

                        lumTex->Release();
                    }
                    lumResource->Release();
                }
            }

            device->Release();
        }

        // 捕获 PS Samplers
        ID3D11SamplerState* samplers[MAX_SAMPLERS] = {};
        context->PSGetSamplers(0, MAX_SAMPLERS, samplers);
        for (UINT i = 0; i < MAX_SAMPLERS; ++i) {
            psSamplers[i].Attach(samplers[i]);
            if (samplers[i]) numPSSamplers = i + 1;
        }
        
        // 捕获 Render Targets
        ID3D11RenderTargetView* rtvs[MAX_RENDER_TARGETS] = {};
        ID3D11DepthStencilView* dsv = nullptr;
        context->OMGetRenderTargets(MAX_RENDER_TARGETS, rtvs, &dsv);
        for (UINT i = 0; i < MAX_RENDER_TARGETS; ++i) {
            renderTargets[i].Attach(rtvs[i]);
            if (rtvs[i]) numRenderTargets = i + 1;
        }
        depthStencilView.Attach(dsv);
        
        // 捕获 Blend State
        context->OMGetBlendState(blendState.ReleaseAndGetAddressOf(), blendFactor, &sampleMask);
        
        // 捕获 Depth Stencil State
        context->OMGetDepthStencilState(depthStencilState.ReleaseAndGetAddressOf(), &stencilRef);
        
        // 捕获 Rasterizer State
        context->RSGetState(rasterizerState.ReleaseAndGetAddressOf());
        
        // 捕获 Viewport
        UINT numViewports = 1;
        context->RSGetViewports(&numViewports, &viewport);
        hasViewport = (numViewports > 0);
        
        // 捕获 Input Layout
        context->IAGetInputLayout(inputLayout.ReleaseAndGetAddressOf());
        
        // 捕获 Primitive Topology
        context->IAGetPrimitiveTopology(&topology);
        
        // 捕获 Vertex Buffer
        ID3D11Buffer* vb = nullptr;
        context->IAGetVertexBuffers(0, 1, &vb, &vertexStride, &vertexOffset);
        vertexBuffer.Attach(vb);
        
        isValid = true;
        captureCount++;
    }

    void HDRStateCache::Apply(ID3D11DeviceContext* context)
    {
        if (!context) {
            logger::warn("HDRStateCache::Apply: context is null");
            return;
        }
        
        if (!isValid) {
            logger::warn("[HDR-DEBUG] Apply: State is NOT valid, skipping");
            return;
        }
        
        // 应用 Pixel Shader
        context->PSSetShader(pixelShader.Get(), nullptr, 0);
        
        // 应用 Vertex Shader
        context->VSSetShader(vertexShader.Get(), nullptr, 0);
        
        // 应用 PS Constant Buffers
        if (numPSConstantBuffers > 0) {
            ID3D11Buffer* cbuffers[MAX_CB] = {};
            for (UINT i = 0; i < numPSConstantBuffers; ++i) {
                cbuffers[i] = psConstantBuffers[i].Get();
            }
            context->PSSetConstantBuffers(0, numPSConstantBuffers, cbuffers);
        }
        
        // 应用 VS Constant Buffers
        if (numVSConstantBuffers > 0) {
            ID3D11Buffer* cbuffers[MAX_CB] = {};
            for (UINT i = 0; i < numVSConstantBuffers; ++i) {
                cbuffers[i] = vsConstantBuffers[i].Get();
            }
            context->VSSetConstantBuffers(0, numVSConstantBuffers, cbuffers);
        }
        
        // 应用 PS Shader Resources
        if (numPSSRVs > 0) {
            ID3D11ShaderResourceView* srvs[MAX_SRV] = {};
            for (UINT i = 0; i < numPSSRVs; ++i) {
                srvs[i] = psSRVs[i].Get();
            }
            context->PSSetShaderResources(0, numPSSRVs, srvs);
        }
        
        // 应用 PS Samplers
        if (numPSSamplers > 0) {
            ID3D11SamplerState* samplers[MAX_SAMPLERS] = {};
            for (UINT i = 0; i < numPSSamplers; ++i) {
                samplers[i] = psSamplers[i].Get();
            }
            context->PSSetSamplers(0, numPSSamplers, samplers);
        }
        
        // 应用 Render Targets (关键! 之前缺失此步骤导致状态泄漏)
        if (numRenderTargets > 0) {
            ID3D11RenderTargetView* rtvs[MAX_RENDER_TARGETS] = {};
            for (UINT i = 0; i < numRenderTargets; ++i) {
                rtvs[i] = renderTargets[i].Get();
            }
            context->OMSetRenderTargets(numRenderTargets, rtvs, depthStencilView.Get());
        }
        
        // 应用 Blend State
        context->OMSetBlendState(blendState.Get(), blendFactor, sampleMask);
        
        // 应用 Depth Stencil State
        context->OMSetDepthStencilState(depthStencilState.Get(), stencilRef);
        
        // 应用 Rasterizer State
        context->RSSetState(rasterizerState.Get());
        
        // 应用 Viewport
        if (hasViewport) {
            context->RSSetViewports(1, &viewport);
        }
        
        // 应用 Input Layout
        context->IASetInputLayout(inputLayout.Get());
        
        // 应用 Primitive Topology
        context->IASetPrimitiveTopology(topology);
        
        // 应用 Vertex Buffer
        if (vertexBuffer) {
            ID3D11Buffer* vbs[] = { vertexBuffer.Get() };
            context->IASetVertexBuffers(0, 1, vbs, &vertexStride, &vertexOffset);
        }
        
    }

    void HDRStateCache::Clear()
    {
        pixelShader.Reset();
        vertexShader.Reset();
        
        for (UINT i = 0; i < MAX_CB; ++i) {
            psConstantBuffers[i].Reset();
            vsConstantBuffers[i].Reset();
        }
        numPSConstantBuffers = 0;
        numVSConstantBuffers = 0;
        
        for (UINT i = 0; i < MAX_SRV; ++i) {
            psSRVs[i].Reset();
        }
        numPSSRVs = 0;
        
        for (UINT i = 0; i < MAX_SAMPLERS; ++i) {
            psSamplers[i].Reset();
        }
        numPSSamplers = 0;
        
        for (UINT i = 0; i < MAX_RENDER_TARGETS; ++i) {
            renderTargets[i].Reset();
        }
        depthStencilView.Reset();
        numRenderTargets = 0;
        
        blendState.Reset();
        depthStencilState.Reset();
        rasterizerState.Reset();
        inputLayout.Reset();
        vertexBuffer.Reset();
        
        isValid = false;
    }
}
