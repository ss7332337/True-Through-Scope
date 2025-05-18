#include "RenderUtilities.h"
#include "Utilities.h"

namespace ThroughScope
{
    // Initialize static members
	ID3D11Texture2D* RenderUtilities::s_DirectX11Texture = nullptr;
	ID3D11ShaderResourceView* RenderUtilities::s_DirectX11SRV = nullptr;

    ID3D11Texture2D* RenderUtilities::s_FirstPassColorTexture = nullptr;
    ID3D11Texture2D* RenderUtilities::s_FirstPassDepthTexture = nullptr;
    ID3D11Texture2D* RenderUtilities::s_SecondPassColorTexture = nullptr;
    ID3D11Texture2D* RenderUtilities::s_SecondPassDepthTexture = nullptr;

    ID3D11VertexShader* RenderUtilities::s_ScreenQuadVS = nullptr;
    ID3D11PixelShader* RenderUtilities::s_ScreenQuadPS = nullptr;
    ID3D11InputLayout* RenderUtilities::s_ScreenQuadInputLayout = nullptr;
    ID3D11Buffer* RenderUtilities::s_ScreenQuadVertexBuffer = nullptr;
    ID3D11SamplerState* RenderUtilities::s_ScreenQuadSamplerState = nullptr;

    RE::BSGraphics::Texture* RenderUtilities::s_ScopeBSTexture = nullptr;
    RE::NiTexture* RenderUtilities::s_ScopeNiTexture = nullptr;

    bool RenderUtilities::s_ScreenQuadInitialized = false;
    bool RenderUtilities::s_FirstPassComplete = false;
    bool RenderUtilities::s_SecondPassComplete = false;
	bool RenderUtilities::s_CreatedMaterial = false;
	bool RenderUtilities::s_SetupScopeQuad = false;
	bool RenderUtilities::s_TriggerScopeQuadSetup = false;

	ThroughScope::RenderUtilities::SavedD3DState RenderUtilities::s_SavedState{};

	static bool firstRun = true;
	static RE::NiPointer<RE::NiTexture> dynamicTexture = nullptr;

	// 顶点着色器
	const char* g_VertexShaderCode = R"(
struct VS_INPUT {
    float3 Position : POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    output.Position = float4(input.Position, 1.0f);
    output.TexCoord = input.TexCoord;
    return output;
}
)";

	// 像素着色器
	const char* g_PixelShaderCode = R"(
Texture2D scopeTexture : register(t0);
SamplerState scopeSampler : register(s0);

struct PS_INPUT {
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET {
    // 基本采样
    float4 color = scopeTexture.Sample(scopeSampler, input.TexCoord);
    
    // 可选：添加边缘效果和色调调整
    float vignetteStrength = 0.8;
    float2 center = float2(0.5, 0.5);
    float dist = length(input.TexCoord - center) * 2.0;
    float vignette = 1.0 - smoothstep(0.5, 1.0, dist) * vignetteStrength;
    
    // 应用渐晕效果
    color.rgb *= vignette;
    
    // 确保 alpha 值正确
    color.a = 1.0;
    
    return color;
}
)";

    bool RenderUtilities::Initialize()
    {
        bool result = CreateTemporaryTextures();
        if (!result) {
            logger::error("Failed to create temporary textures");
            return false;
        }

        result = InitializeScreenQuad();
        if (!result) {
            logger::error("Failed to initialize screen quad");
            return false;
        }

        logger::info("RenderUtilities initialized successfully");
        return true;
    }


	//此处创建LightingShader的Quad是为了将Quad的深度提前写在tGodrayDepth里面
	//使用LightingShader会将Quad放在DeferredPrePass里面Draw，但是不进行更复杂的设置，会导致动态画面(RT0)影响到之后的帧
	//使用EffectShader会将Quad放在Forward里面Draw，但是会被Godray影响，导致画面上面出现鬼影（神鬼二象性)
	//虽然不优雅，但我觉得可能还比较高效，画几个顶点就能解决的问题就不要动纹理了
	bool RenderUtilities::SetupWeaponScopeShape()
	{
		if (s_SetupScopeQuad) {
			// Check if the scope node still exists (it might have been removed by a weapon switch)
			auto playerCharacter = RE::PlayerCharacter::GetSingleton();
			if (playerCharacter && playerCharacter->Get3D()) {
				auto weaponNode = playerCharacter->Get3D()->GetObjectByName("Weapon");
				if (weaponNode && weaponNode->IsNode()) {
					auto weaponNiNode = static_cast<RE::NiNode*>(weaponNode);
					auto existingNode = weaponNiNode->GetObjectByName("ScopeNode");
					if (existingNode) {
						// ScopeNode still exists, we don't need to recreate it
						return true;
					} else {
						// ScopeNode is gone, we need to recreate it
						logger::info("ScopeNode no longer exists, will recreate");
						s_SetupScopeQuad = false;
					}
				}
			}
		}

		// 获取玩家角色
		auto playerCharacter = RE::PlayerCharacter::GetSingleton();
		if (!playerCharacter || !playerCharacter->Get3D()) {
			logger::error("Player character or 3D model not available");
			return false;
		}

		// 查找武器节点
		auto weaponNode = playerCharacter->Get3D()->GetObjectByName("Weapon");
		if (!weaponNode) {
			logger::error("Weapon node not found");
			return false;
		}

		// 转换为 NiNode
		auto weaponNiNode = weaponNode->IsNode() ? static_cast<RE::NiNode*>(weaponNode) : nullptr;
		if (!weaponNiNode) {
			logger::error("Weapon node is not a NiNode");
			return false;
		}

		// 检查是否存在 scope node 以避免重复
		RE::NiAVObject* existingNode = weaponNiNode->GetObjectByName("ScopeNode");
		if (existingNode) {
			weaponNiNode->DetachChild(existingNode);
			logger::info("Removed existing ScopeNode");
		}


		try {
			// 创建 scope node
			
			RE::NiNode* scopeShapeNode = new RE::NiNode(0);
			RE::BSFixedString scopeNodeName("ScopeNode");
			scopeShapeNode->name = scopeNodeName;
			scopeShapeNode->local.translate = RE::NiPoint3(0, 0, 0);
			scopeShapeNode->local.rotate.MakeIdentity();
			// 附加到武器节点
			weaponNiNode->AttachChild(scopeShapeNode, false);
			
			
			if (!scopeShapeNode) {
				logger::error("Failed to create scope node");
				return false;
			}

			auto renderer = RE::BSGraphics::Renderer::GetSingleton();
			auto rendererData = RE::BSGraphics::RendererData::GetSingleton();

			// 创建 BSEffectShaderProperty
			RE::BSLightingShaderProperty* lightProperty = RE::BSLightingShaderProperty::CreateObject();
			RE::BSEffectShaderProperty* effectProperty = RE::BSEffectShaderProperty::CreateObject();

			if (!lightProperty || !effectProperty) {
				logger::error("Failed to create effect shader property");
				return false;
			}

			lightProperty->flags.set(RE::BSShaderProperty::EShaderPropertyFlags::kModelSpaceNormals);
			lightProperty->flags.set(RE::BSShaderProperty::EShaderPropertyFlags::kSpecular);
			lightProperty->flags.set(RE::BSShaderProperty::EShaderPropertyFlags::kTwoSided);
			lightProperty->flags.set(RE::BSShaderProperty::EShaderPropertyFlags::kVertexColors);
			lightProperty->flags.set(RE::BSShaderProperty::EShaderPropertyFlags::kMultipleTextures);
			lightProperty->flags.set(RE::BSShaderProperty::EShaderPropertyFlags::kZBufferTest);
			lightProperty->flags.set(RE::BSShaderProperty::EShaderPropertyFlags::kZBufferWrite);
			effectProperty->flags = lightProperty->flags;

			// 创建几何体
			RE::BSGeometryConstructor* geomConstructor = RE::BSGeometryConstructor::Create(scopeShapeNode);

			RE::NiColorA color(1.0f, 0.0f, 0.0f, 1.0f);
			logger::info("Adding quad with explicit vertices");

			RE::NiPoint3 vertices[4];
			float size = 8.0f;
			vertices[0] = RE::NiPoint3(-size, 10.0f + 0.001f, size + 10);   // Left top
			vertices[1] = RE::NiPoint3(-size, 10.0f + 0.001f, -size + 10);  // Left bottom
			vertices[2] = RE::NiPoint3(size, 10.0f + 0.001f, -size + 10);   // Right bottom
			vertices[3] = RE::NiPoint3(size, 10.0f + 0.001f, size + 10);    // Right top

			// Add quad to geometry constructor
			geomConstructor->AddQuad(vertices, &color);
			geomConstructor->Flush();

			// Check if we have a valid child object
			if (scopeShapeNode->children.size() == 0) {
				logger::error("No children created in scope node");
				return false;
			}

			// Name the quad object
			auto scopeQuad = scopeShapeNode->children[0].get();
			if (!scopeQuad) {
				logger::error("Null child in scope node");
				return false;
			}

			RE::BSFixedString scopeQuadName("ScopeQuad");
			scopeQuad->name = scopeQuadName;

			// Check if it's a triShape
			auto triShape = scopeQuad->IsTriShape();
			if (!triShape) {
				logger::error("Child is not a BSTriShape");
				return false;
			}

			auto geometry = static_cast<RE::BSGeometry*>(triShape);
			geometry->SetProperty(lightProperty);


			vertices[0] = RE::NiPoint3(-size, 10.0f, size + 10);   // Left top
			vertices[1] = RE::NiPoint3(-size, 10.0f, -size + 10);  // Left bottom
			vertices[2] = RE::NiPoint3(size, 10.0f, -size + 10);   // Right bottom
			vertices[3] = RE::NiPoint3(size, 10.0f, size + 10);    // Right top

			// Add quad to geometry constructor
			geomConstructor->AddQuad(vertices, &color);
			geomConstructor->Flush();

			// Check if we have a valid child object
			if (scopeShapeNode->children.size() == 1) {
				logger::error("Failed ScopeNode Effect");
				return false;
			}

			for (auto child : scopeShapeNode->children)
			{
				if (child->name.empty())
				{
					RE::BSFixedString scopeQuadEffectName("ScopeQuadEffect");
					child->name = scopeQuadEffectName;
				}
				
			}

			auto scopeQuadEffectObject = scopeShapeNode->GetObjectByName("ScopeQuadEffect");
			if (!scopeQuadEffectObject) {
				logger::error("Null child in scope node");
				return false;
			}

			auto scopeEffectTriShape = scopeQuadEffectObject->IsTriShape();
			if (!scopeEffectTriShape) {
				logger::error("Child is not a BSTriShape");
				return false;
			}

			auto geometryEffect = static_cast<RE::BSGeometry*>(scopeEffectTriShape);
			geometryEffect->SetProperty(effectProperty);

			logger::info("Created scope quad with texture");
			s_CreatedMaterial = true;

			s_SetupScopeQuad = true;
			return true;

		} catch (const std::exception& e) {
			logger::error("Exception in SetupWeaponScopeShape: {}", e.what());
			return false;
		}
	}

	bool RenderUtilities::RemoveWeaponScopeShape()
	{
		// Get player character
		auto playerCharacter = RE::PlayerCharacter::GetSingleton();
		if (!playerCharacter || !playerCharacter->Get3D()) {
			logger::error("Player character or 3D model not available");
			return false;
		}

		// Find weapon node
		auto weaponNode = playerCharacter->Get3D()->GetObjectByName("Weapon");
		if (!weaponNode) {
			logger::error("Weapon node not found");
			return false;
		}

		// Convert to NiNode
		auto weaponNiNode = weaponNode->IsNode() ? static_cast<RE::NiNode*>(weaponNode) : nullptr;
		if (!weaponNiNode) {
			logger::error("Weapon node is not a NiNode");
			return false;
		}

		// Check if the ScopeNode exists
		auto scopeShapeNode = weaponNiNode->GetObjectByName("ScopeNode");
		if (!scopeShapeNode) {
			logger::info("ScopeNode not found, nothing to remove");
			s_SetupScopeQuad = false;
			return true;  // Not an error, just nothing to do
		}

		try {
			// Detach the node from the parent
			weaponNiNode->DetachChild(scopeShapeNode);
			logger::info("Successfully removed ScopeNode from weapon");
			s_SetupScopeQuad = false;
			return true;
		} catch (const std::exception& e) {
			logger::error("Exception in RemoveWeaponScopeShape: {}", e.what());
			return false;
		}
	}

    void RenderUtilities::Shutdown()
    {
        ReleaseTemporaryTextures();

        // Release screen quad resources
        if (s_ScreenQuadVS) {
            s_ScreenQuadVS->Release();
            s_ScreenQuadVS = nullptr;
        }
        if (s_ScreenQuadPS) {
            s_ScreenQuadPS->Release();
            s_ScreenQuadPS = nullptr;
        }
        if (s_ScreenQuadInputLayout) {
            s_ScreenQuadInputLayout->Release();
            s_ScreenQuadInputLayout = nullptr;
        }
        if (s_ScreenQuadVertexBuffer) {
            s_ScreenQuadVertexBuffer->Release();
            s_ScreenQuadVertexBuffer = nullptr;
        }
        if (s_ScreenQuadSamplerState) {
            s_ScreenQuadSamplerState->Release();
            s_ScreenQuadSamplerState = nullptr;
        }

		if (s_DirectX11SRV) {
			s_DirectX11SRV->Release();
			s_DirectX11SRV = nullptr;
		}

		if (s_DirectX11Texture) {
			s_DirectX11Texture->Release();
			s_DirectX11Texture = nullptr;
		}

        s_ScreenQuadInitialized = false;
    }

    bool RenderUtilities::CreateTemporaryTextures()
    {
        // Get D3D11 device from renderer data
        auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
        ID3D11Device* device = (ID3D11Device*)rendererData->device;

        // Get screen dimensions
        unsigned int width = rendererData->renderWindow[0].windowWidth;
        unsigned int height = rendererData->renderWindow[0].windowHeight;

        logger::info("Creating temporary textures with size {}x{}", width, height);

        // Clean up any existing resources
        ReleaseTemporaryTextures();

        // Create color textures for passes
        D3D11_TEXTURE2D_DESC colorDesc;
        ZeroMemory(&colorDesc, sizeof(colorDesc));
        colorDesc.Width = width;
        colorDesc.Height = height;
        colorDesc.MipLevels = 1;
        colorDesc.ArraySize = 1;
        colorDesc.Format = DXGI_FORMAT_R11G11B10_FLOAT;  // Match render target format
        colorDesc.SampleDesc.Count = 1;
        colorDesc.SampleDesc.Quality = 0;
        colorDesc.Usage = D3D11_USAGE_DEFAULT;
        colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        colorDesc.CPUAccessFlags = 0;
        colorDesc.MiscFlags = 0;

        HRESULT hr = device->CreateTexture2D(&colorDesc, nullptr, &s_FirstPassColorTexture);
        if (FAILED(hr)) {
            logger::error("Failed to create first pass color texture. HRESULT: 0x{:X}", hr);
            return false;
        }

        hr = device->CreateTexture2D(&colorDesc, nullptr, &s_SecondPassColorTexture);
        if (FAILED(hr)) {
            logger::error("Failed to create second pass color texture. HRESULT: 0x{:X}", hr);
            return false;
        }

        // Create depth texture
        D3D11_TEXTURE2D_DESC depthDesc;
        ZeroMemory(&depthDesc, sizeof(depthDesc));
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        depthDesc.CPUAccessFlags = 0;
        depthDesc.MiscFlags = 0;

        hr = device->CreateTexture2D(&depthDesc, nullptr, &s_FirstPassDepthTexture);
        if (FAILED(hr)) {
            logger::error("Failed to create first pass depth texture. HRESULT: 0x{:X}", hr);
            return false;
        }

        hr = device->CreateTexture2D(&depthDesc, nullptr, &s_SecondPassDepthTexture);
        if (FAILED(hr)) {
            logger::error("Failed to create Second pass depth texture. HRESULT: 0x{:X}", hr);
            return false;
        }

        logger::info("Temporary textures created successfully");
        return true;
    }

    void RenderUtilities::ReleaseTemporaryTextures()
    {
        if (s_FirstPassColorTexture) {
            s_FirstPassColorTexture->Release();
            s_FirstPassColorTexture = nullptr;
        }
        if (s_FirstPassDepthTexture) {
            s_FirstPassDepthTexture->Release();
            s_FirstPassDepthTexture = nullptr;
        }
        if (s_SecondPassColorTexture) {
            s_SecondPassColorTexture->Release();
            s_SecondPassColorTexture = nullptr;
        }
        if (s_SecondPassDepthTexture) {
            s_SecondPassDepthTexture->Release();
            s_SecondPassDepthTexture = nullptr;
        }
    }

    bool RenderUtilities::InitializeScreenQuad()
    {
        // Get device
        auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
        ID3D11Device* device = (ID3D11Device*)rendererData->device;

        if (!device) {
            logger::error("Failed to get D3D11 device for screen quad");
            return false;
        }

        // Create vertex shader
        const char* vsCode = R"(
            struct VS_INPUT {
                float3 Position : POSITION;
                float2 TexCoord : TEXCOORD0;
            };
            
            struct VS_OUTPUT {
                float4 Position : SV_POSITION;
                float2 TexCoord : TEXCOORD0;
            };
            
            VS_OUTPUT main(VS_INPUT input) {
                VS_OUTPUT output;
                output.Position = float4(input.Position, 1.0f);
                output.TexCoord = input.TexCoord;
                return output;
            }
        )";

        // Create pixel shader
        const char* psCode = R"(
            Texture2D scopeTexture : register(t0);
            SamplerState scopeSampler : register(s0);
        
            struct PS_INPUT {
                float4 Position : SV_POSITION;
                float2 TexCoord : TEXCOORD0;
            };
        
            float4 main(PS_INPUT input) : SV_TARGET {
                // Sample texture and return color
                float4 color = scopeTexture.Sample(scopeSampler, input.TexCoord);
                
                // Ensure alpha value is 1.0 (fully opaque)
                color.a = 1.0;
                
                return color;
            }
        )";

        // Compile shaders
        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* psBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;

        HRESULT hr = D3DCompile(vsCode, strlen(vsCode), "ScreenQuadVS", nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) {
                logger::error("Vertex shader compilation failed: {}", (char*)errorBlob->GetBufferPointer());
                errorBlob->Release();
            }
            return false;
        }

        hr = D3DCompile(psCode, strlen(psCode), "ScreenQuadPS", nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) {
                logger::error("Pixel shader compilation failed: {}", (char*)errorBlob->GetBufferPointer());
                errorBlob->Release();
            }
            if (vsBlob)
                vsBlob->Release();
            return false;
        }

        // Create shader objects
        hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &s_ScreenQuadVS);
        if (FAILED(hr)) {
            logger::error("Failed to create vertex shader. HRESULT: 0x{:X}", hr);
            vsBlob->Release();
            psBlob->Release();
            return false;
        }

        hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &s_ScreenQuadPS);
        if (FAILED(hr)) {
            logger::error("Failed to create pixel shader. HRESULT: 0x{:X}", hr);
            vsBlob->Release();
            psBlob->Release();
            s_ScreenQuadVS->Release();
            return false;
        }

        // Create input layout
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };

        hr = device->CreateInputLayout(layout, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &s_ScreenQuadInputLayout);
        if (FAILED(hr)) {
            logger::error("Failed to create input layout. HRESULT: 0x{:X}", hr);
            vsBlob->Release();
            psBlob->Release();
            s_ScreenQuadVS->Release();
            s_ScreenQuadPS->Release();
            return false;
        }

        // Release shader blobs
        vsBlob->Release();
        psBlob->Release();

        // Create vertex buffer
        ScreenVertex vertices[] = {
            { -1.0f, -1.0f, 0.0f, 0.0f, 1.0f },  // Bottom left
            { -1.0f, 1.0f, 0.0f, 0.0f, 0.0f },   // Top left
            { 1.0f, -1.0f, 0.0f, 1.0f, 1.0f },   // Bottom right
            { 1.0f, 1.0f, 0.0f, 1.0f, 0.0f }     // Top right
        };

        D3D11_BUFFER_DESC bufferDesc = {};
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth = sizeof(vertices);
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = vertices;

        hr = device->CreateBuffer(&bufferDesc, &initData, &s_ScreenQuadVertexBuffer);
        if (FAILED(hr)) {
            logger::error("Failed to create vertex buffer. HRESULT: 0x{:X}", hr);
            s_ScreenQuadVS->Release();
            s_ScreenQuadPS->Release();
            s_ScreenQuadInputLayout->Release();
            return false;
        }

        // Create sampler state
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

        hr = device->CreateSamplerState(&samplerDesc, &s_ScreenQuadSamplerState);
        if (FAILED(hr)) {
            logger::error("Failed to create sampler state. HRESULT: 0x{:X}", hr);
            s_ScreenQuadVS->Release();
            s_ScreenQuadPS->Release();
            s_ScreenQuadInputLayout->Release();
            s_ScreenQuadVertexBuffer->Release();
            return false;
        }

        s_ScreenQuadInitialized = true;
        logger::info("Screen quad initialized successfully");
        return true;
    }

    void RenderUtilities::RenderScreenQuad(ID3D11ShaderResourceView* textureView, float x, float y, float width, float height)
    {
        if (!s_ScreenQuadInitialized || !textureView) {
            logger::error("Screen quad not initialized or texture view is null");
            return;
        }

        auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
        ID3D11DeviceContext* context = (ID3D11DeviceContext*)rendererData->context;
        ID3D11Device* device = (ID3D11Device*)rendererData->device;

        // Begin event profiling
        Utilities::BeginEvent(L"RenderScreenQuad");

        // Save current render states
        // Get current render target
        ID3D11RenderTargetView* currentRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = { nullptr };
        ID3D11DepthStencilView* currentDSV = nullptr;
        context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, currentRTVs, &currentDSV);

        // Save shader states
        ID3D11VertexShader* oldVS = nullptr;
        ID3D11PixelShader* oldPS = nullptr;
        ID3D11GeometryShader* oldGS = nullptr;
        ID3D11HullShader* oldHS = nullptr;
        ID3D11DomainShader* oldDS = nullptr;
        ID3D11ComputeShader* oldCS = nullptr;

        context->VSGetShader(&oldVS, nullptr, nullptr);
        context->PSGetShader(&oldPS, nullptr, nullptr);
        context->GSGetShader(&oldGS, nullptr, nullptr);
        context->HSGetShader(&oldHS, nullptr, nullptr);
        context->DSGetShader(&oldDS, nullptr, nullptr);
        context->CSGetShader(&oldCS, nullptr, nullptr);

        // Save input assembler state
        ID3D11InputLayout* oldInputLayout = nullptr;
        D3D11_PRIMITIVE_TOPOLOGY oldTopology;
        ID3D11Buffer* oldVB[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = { nullptr };
        UINT oldStrides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = { 0 };
        UINT oldOffsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = { 0 };
        ID3D11Buffer* oldIB = nullptr;
        DXGI_FORMAT oldIBFormat = DXGI_FORMAT_UNKNOWN;
        UINT oldIBOffset = 0;

        context->IAGetInputLayout(&oldInputLayout);
        context->IAGetPrimitiveTopology(&oldTopology);
        context->IAGetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, oldVB, oldStrides, oldOffsets);
        context->IAGetIndexBuffer(&oldIB, &oldIBFormat, &oldIBOffset);

        // Save resources and sampler states
        ID3D11ShaderResourceView* oldSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = { nullptr };
        ID3D11SamplerState* oldSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = { nullptr };

        context->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, oldSRVs);
        context->PSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, oldSamplers);

        // Save blend state, depth state, rasterizer state
        ID3D11BlendState* oldBlendState = nullptr;
        FLOAT oldBlendFactor[4] = { 0 };
        UINT oldSampleMask = 0;
        ID3D11DepthStencilState* oldDepthStencilState = nullptr;
        UINT oldStencilRef = 0;
        ID3D11RasterizerState* oldRasterizerState = nullptr;

        context->OMGetBlendState(&oldBlendState, oldBlendFactor, &oldSampleMask);
        context->OMGetDepthStencilState(&oldDepthStencilState, &oldStencilRef);
        context->RSGetState(&oldRasterizerState);

        // Save viewport
        UINT numViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        D3D11_VIEWPORT oldViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        context->RSGetViewports(&numViewports, oldViewports);

        // Set new render states for quad drawing

        // 1. Create and set blend state - enable Alpha blending
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        ID3D11BlendState* blendState = nullptr;
        HRESULT hr = device->CreateBlendState(&blendDesc, &blendState);
        if (SUCCEEDED(hr)) {
            FLOAT blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            context->OMSetBlendState(blendState, blendFactor, 0xffffffff);
        } else {
            logger::error("Failed to create blend state. HRESULT: 0x{:X}", hr);
        }

        // 2. Create and set depth state - disable depth testing
        D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
        depthStencilDesc.DepthEnable = FALSE;
        depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        depthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        depthStencilDesc.StencilEnable = FALSE;

        ID3D11DepthStencilState* depthStencilState = nullptr;
        hr = device->CreateDepthStencilState(&depthStencilDesc, &depthStencilState);
        if (SUCCEEDED(hr)) {
            context->OMSetDepthStencilState(depthStencilState, 0);
        } else {
            logger::error("Failed to create depth stencil state. HRESULT: 0x{:X}", hr);
        }

        // 3. Create and set rasterizer state - disable face culling
        D3D11_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode = D3D11_FILL_SOLID;
        rasterizerDesc.CullMode = D3D11_CULL_NONE;
        rasterizerDesc.FrontCounterClockwise = FALSE;
        rasterizerDesc.DepthBias = 0;
        rasterizerDesc.DepthBiasClamp = 0.0f;
        rasterizerDesc.SlopeScaledDepthBias = 0.0f;
        rasterizerDesc.DepthClipEnable = TRUE;
        rasterizerDesc.ScissorEnable = FALSE;
        rasterizerDesc.MultisampleEnable = FALSE;
        rasterizerDesc.AntialiasedLineEnable = FALSE;

        ID3D11RasterizerState* rasterizerState = nullptr;
        hr = device->CreateRasterizerState(&rasterizerDesc, &rasterizerState);
        if (SUCCEEDED(hr)) {
            context->RSSetState(rasterizerState);
        } else {
            logger::error("Failed to create rasterizer state. HRESULT: 0x{:X}", hr);
        }

        // 4. Set viewport - adjust to specified region
        D3D11_VIEWPORT viewport = {};
        viewport.TopLeftX = x * oldViewports[0].Width;
        viewport.TopLeftY = y * oldViewports[0].Height;
        viewport.Width = width * oldViewports[0].Width;
        viewport.Height = height * oldViewports[0].Height;
        viewport.MinDepth = 0.0f;
        viewport.MaxDepth = 1.0f;

        context->RSSetViewports(1, &viewport);

        // 5. Set input assembler state
        context->IASetInputLayout(s_ScreenQuadInputLayout);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        UINT stride = sizeof(ScreenVertex);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, &s_ScreenQuadVertexBuffer, &stride, &offset);

        // 6. Set shaders and resources
        context->VSSetShader(s_ScreenQuadVS, nullptr, 0);
        context->PSSetShader(s_ScreenQuadPS, nullptr, 0);
        context->GSSetShader(nullptr, nullptr, 0);
        context->HSSetShader(nullptr, nullptr, 0);
        context->DSSetShader(nullptr, nullptr, 0);

        context->PSSetShaderResources(0, 1, &textureView);
        context->PSSetSamplers(0, 1, &s_ScreenQuadSamplerState);

        // Ensure we have a render target
        if (currentRTVs[0]) {
            // 7. Set render target
            context->OMSetRenderTargets(1, &currentRTVs[0], nullptr);

            // 8. Draw quad
            context->Draw(4, 0);
        } else {
            logger::error("No render target available for drawing screen quad");
        }

        // Restore all render states

        // Restore blend state
        if (blendState)
            blendState->Release();
        context->OMSetBlendState(oldBlendState, oldBlendFactor, oldSampleMask);
        if (oldBlendState)
            oldBlendState->Release();

        // Restore depth state
        if (depthStencilState)
            depthStencilState->Release();
        context->OMSetDepthStencilState(oldDepthStencilState, oldStencilRef);
        if (oldDepthStencilState)
            oldDepthStencilState->Release();

        // Restore rasterizer state
        if (rasterizerState)
            rasterizerState->Release();
        context->RSSetState(oldRasterizerState);
        if (oldRasterizerState)
            oldRasterizerState->Release();

        // Restore viewport
        context->RSSetViewports(numViewports, oldViewports);

        // Restore render target
        context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, currentRTVs, currentDSV);

        // Restore shaders
        context->VSSetShader(oldVS, nullptr, 0);
        context->PSSetShader(oldPS, nullptr, 0);
        context->GSSetShader(oldGS, nullptr, 0);
        context->HSSetShader(oldHS, nullptr, 0);
        context->DSSetShader(oldDS, nullptr, 0);
        context->CSSetShader(oldCS, nullptr, 0);

        // Restore input assembler state
        context->IASetInputLayout(oldInputLayout);
        context->IASetPrimitiveTopology(oldTopology);
        context->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT, oldVB, oldStrides, oldOffsets);
        context->IASetIndexBuffer(oldIB, oldIBFormat, oldIBOffset);

        // Restore resources and samplers
        context->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, oldSRVs);
        context->PSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, oldSamplers);

        // Release references
        if (currentDSV)
            currentDSV->Release();
        for (int i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
            if (currentRTVs[i])
                currentRTVs[i]->Release();
        }

        if (oldVS)
            oldVS->Release();
        if (oldPS)
            oldPS->Release();
        if (oldGS)
            oldGS->Release();
        if (oldHS)
            oldHS->Release();
        if (oldDS)
            oldDS->Release();
        if (oldCS)
            oldCS->Release();

        if (oldInputLayout)
            oldInputLayout->Release();
        for (int i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
            if (oldVB[i])
                oldVB[i]->Release();
        }
        if (oldIB)
            oldIB->Release();

        for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++) {
            if (oldSRVs[i])
                oldSRVs[i]->Release();
        }
        for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; i++) {
            if (oldSamplers[i])
                oldSamplers[i]->Release();
        }

        Utilities::EndEvent();
    }
}
