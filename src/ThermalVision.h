#pragma once

#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <memory>
#include <vector>

namespace ThroughScope
{
    enum class ThermalPalette
    {
        WhiteHot,    // 白热模式 - 最常用，高温白色低温黑色
        BlackHot,    // 黑热模式 - 白热的反转
        IronBow,     // 铁弓调色板 - 黑蓝紫红橙黄白
        RainbowHC,   // 高对比彩虹
        Arctic,      // 北极调色板 - 冷蓝热金
        Lava,        // 熔岩调色板 - 冷蓝热红
        Isothermal,  // 等温线模式
        Medical      // 医疗模式 - 优化人体温度范围
    };

    // 热成像配置参数
    struct ThermalConfig
    {
        ThermalPalette palette = ThermalPalette::WhiteHot;
        float minTemperature = -20.0f;  // 最低温度（摄氏度）
        float maxTemperature = 100.0f;  // 最高温度（摄氏度）
        float sensitivity = 0.025f;     // NETD(噪声等效温差) in Kelvin
        float emissivity = 0.95f;       // 发射率
        bool enableNoise = true;        // 启用热噪声
        bool enableEdgeDetection = true; // 启用边缘增强
        float edgeStrength = 0.3f;      // 边缘强度
        float noiseIntensity = 0.02f;   // 噪声强度
        float gain = 1.0f;               // 增益
        float level = 0.5f;              // 电平
        bool autoGain = true;            // 自动增益控制
    };

    // 热成像系统主类
    class ThermalVision
    {
    public:
        static ThermalVision* GetSingleton();

        bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context);
        void Shutdown();

        // 应用热成像效果到渲染目标
        void ApplyThermalEffect(ID3D11RenderTargetView* source,
                                ID3D11RenderTargetView* destination,
                                ID3D11ShaderResourceView* depthSRV);

        // 配置方法
        void SetPalette(ThermalPalette palette) { m_config.palette = palette; }
        void SetTemperatureRange(float min, float max);
        void SetNoiseLevel(float intensity) { m_config.noiseIntensity = intensity; }
        void SetEdgeDetection(bool enable) { m_config.enableEdgeDetection = enable; }
        void SetAutoGain(bool enable) { m_config.autoGain = enable; }

        ThermalConfig& GetConfig() { return m_config; }
        const ThermalConfig& GetConfig() const { return m_config; }

        // 温度估算（基于材质和对象类型）
        float EstimateTemperature(uint32_t objectFormID, float ambientTemp = 20.0f);

    private:
        ThermalVision() = default;
        ~ThermalVision() = default;

        bool CreateShaders();
        bool CreateResources();
        bool CreatePaletteLookupTextures();
        void UpdateConstantBuffer();

        // 温度分析
        void AnalyzeSceneTemperatures(ID3D11ShaderResourceView* sceneSRV);
        void UpdateAutoGain();

        // 着色器和资源
        Microsoft::WRL::ComPtr<ID3D11Device> m_device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;

        // 着色器
        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_thermalPixelShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_edgeDetectShader;
        Microsoft::WRL::ComPtr<ID3D11ComputeShader> m_temperatureAnalysisCS;

        // 常量缓冲区
        struct ThermalConstants
        {
            DirectX::XMFLOAT4 temperatureRange;  // x=min, y=max, z=1/(max-min), w=sensitivity
            DirectX::XMFLOAT4 noiseParams;       // x=intensity, y=time, z=seed, w=pattern scale
            DirectX::XMFLOAT4 edgeParams;        // x=strength, y=threshold, z=unused, w=unused
            DirectX::XMFLOAT4 gainLevel;         // x=gain, y=level, z=emissivity, w=palette index
            DirectX::XMFLOAT4X4 noiseMatrix;     // 用于固定模式噪声
        };
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;

        // 调色板查找纹理
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_paletteLUT;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_paletteSRV;

        // 噪声纹理（固定模式噪声和时间相关噪声）
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_fixedPatternNoise;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_fixedPatternNoiseSRV;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_temporalNoise;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_temporalNoiseSRV;

        // 中间渲染目标
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_tempTexture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_tempRTV;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_tempSRV;

        // 采样器
        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_linearSampler;
        Microsoft::WRL::ComPtr<ID3D11SamplerState> m_pointSampler;

        // 配置
        ThermalConfig m_config;

        // 自动增益控制
        float m_currentMinTemp = -20.0f;
        float m_currentMaxTemp = 100.0f;
        float m_frameTime = 0.0f;

        static ThermalVision* s_instance;
    };
}