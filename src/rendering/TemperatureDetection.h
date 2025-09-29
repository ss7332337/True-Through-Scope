#pragma once

#include <unordered_map>
#include <vector>
#include "RE/Fallout.hpp"

namespace ThroughScope
{
    // 材质类型枚举
    enum class MaterialType
    {
        Unknown,
        Flesh,      // 人类/生物皮肤
        Metal,      // 金属
        Concrete,   // 混凝土
        Wood,       // 木材
        Glass,      // 玻璃
        Water,      // 水
        Fabric,     // 织物
        Plastic,    // 塑料
        Energy,     // 能量武器/等离子
        Fire,       // 火焰
        Ice,        // 冰
        Electronic  // 电子设备
    };

    // 温度配置文件
    struct ThermalProfile
    {
        float baseTemperature;      // 基础温度（摄氏度）
        float emissivity;           // 发射率 (0-1)
        float heatCapacity;         // 热容量
        float thermalConductivity;  // 热导率
        MaterialType material;      // 材质类型
        bool isHeatSource;          // 是否为热源
        float heatGeneration;       // 热量产生率
    };

    // 动态温度信息
    struct DynamicThermalData
    {
        float currentTemperature;   // 当前温度
        float targetTemperature;    // 目标温度
        float temperatureRate;      // 温度变化率
        float lastUpdateTime;       // 上次更新时间
        bool isActive;             // 是否活跃（如机器运行中）
    };

    class TemperatureDetection
    {
    public:
        static TemperatureDetection* GetSingleton();

        void Initialize();
        void Update(float deltaTime);

        // 获取对象温度
        float GetObjectTemperature(RE::TESObjectREFR* refr);
        float GetActorTemperature(RE::Actor* actor);

        // 根据FormID获取温度
        float GetTemperatureByFormID(uint32_t formID);

        // 设置环境温度
        void SetAmbientTemperature(float temp) { m_ambientTemperature = temp; }
        float GetAmbientTemperature() const { return m_ambientTemperature; }

        // 注册自定义温度配置
        void RegisterThermalProfile(uint32_t formID, const ThermalProfile& profile);

        // 获取材质类型
        MaterialType GetMaterialType(RE::TESForm* form);

    private:
        TemperatureDetection();
        ~TemperatureDetection() = default;

        void InitializeDefaultProfiles();
        void UpdateDynamicTemperatures(float deltaTime);

        // 根据actor状态计算温度
        float CalculateActorTemperature(RE::Actor* actor);

        // 根据对象类型估算温度
        float EstimateObjectTemperature(RE::TESObjectREFR* refr);

        // 检测特殊对象类型
        bool IsFireObject(RE::TESForm* form);
        bool IsElectronicObject(RE::TESForm* form);
        bool IsWeapon(RE::TESForm* form);

        // 获取武器温度（考虑最近射击）
        float GetWeaponTemperature(RE::TESObjectWEAP* weapon, RE::Actor* owner);

        // 温度配置映射
        std::unordered_map<uint32_t, ThermalProfile> m_thermalProfiles;

        // 动态温度数据
        std::unordered_map<uint32_t, DynamicThermalData> m_dynamicData;

        // 材质温度范围
        struct MaterialTempRange
        {
            float minTemp;
            float maxTemp;
            float defaultTemp;
        };
        std::unordered_map<MaterialType, MaterialTempRange> m_materialRanges;

        // 环境参数
        float m_ambientTemperature = 20.0f;  // 环境温度
        float m_sunIntensity = 1.0f;         // 太阳强度
        float m_timeOfDay = 12.0f;           // 当前时间

        // 单例
        static TemperatureDetection* s_instance;
    };

    // 温度工具函数
    namespace ThermalUtils
    {
        // 根据距离衰减温度
        float AttenuateTemperature(float temperature, float distance, float ambientTemp);

        // 根据材质获取发射率
        float GetEmissivity(MaterialType material);

        // 计算热辐射
        float CalculateThermalRadiation(float temperature, float emissivity, float area);

        // 混合多个温度源
        float BlendTemperatures(const std::vector<float>& temperatures, const std::vector<float>& weights);

        // 应用天气影响
        float ApplyWeatherEffects(float temperature, RE::TESWeather* weather);
    }
}