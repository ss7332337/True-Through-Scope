#include "rendering/TemperatureDetection.h"
#include <algorithm>
#include <cmath>

namespace ThroughScope
{
    TemperatureDetection* TemperatureDetection::s_instance = nullptr;

    TemperatureDetection* TemperatureDetection::GetSingleton()
    {
        if (!s_instance)
        {
            s_instance = new TemperatureDetection();
        }
        return s_instance;
    }

    TemperatureDetection::TemperatureDetection()
    {
        InitializeDefaultProfiles();
    }

    void TemperatureDetection::Initialize()
    {
        InitializeDefaultProfiles();

        // 设置材质温度范围
        m_materialRanges[MaterialType::Flesh] = {34.0f, 40.0f, 36.5f};
        m_materialRanges[MaterialType::Metal] = {15.0f, 80.0f, 20.0f};
        m_materialRanges[MaterialType::Concrete] = {10.0f, 40.0f, 20.0f};
        m_materialRanges[MaterialType::Wood] = {15.0f, 30.0f, 20.0f};
        m_materialRanges[MaterialType::Glass] = {10.0f, 35.0f, 20.0f};
        m_materialRanges[MaterialType::Water] = {0.0f, 30.0f, 15.0f};
        m_materialRanges[MaterialType::Fabric] = {15.0f, 35.0f, 22.0f};
        m_materialRanges[MaterialType::Plastic] = {10.0f, 50.0f, 20.0f};
        m_materialRanges[MaterialType::Energy] = {100.0f, 500.0f, 200.0f};
        m_materialRanges[MaterialType::Fire] = {200.0f, 800.0f, 500.0f};
        m_materialRanges[MaterialType::Ice] = {-20.0f, 0.0f, -5.0f};
        m_materialRanges[MaterialType::Electronic] = {30.0f, 90.0f, 45.0f};
    }

    void TemperatureDetection::InitializeDefaultProfiles()
    {
        // 人类NPC默认配置
        ThermalProfile humanProfile;
        humanProfile.baseTemperature = 36.5f;
        humanProfile.emissivity = 0.98f;
        humanProfile.heatCapacity = 3470.0f;
        humanProfile.thermalConductivity = 0.2f;
        humanProfile.material = MaterialType::Flesh;
        humanProfile.isHeatSource = true;
        humanProfile.heatGeneration = 100.0f; // 100瓦

        // 超级变种人
        ThermalProfile mutantProfile = humanProfile;
        mutantProfile.baseTemperature = 38.0f; // 稍高体温
        mutantProfile.heatGeneration = 150.0f;

        // 尸鬼
        ThermalProfile ghoulProfile = humanProfile;
        ghoulProfile.baseTemperature = 34.0f; // 较低体温
        ghoulProfile.emissivity = 0.85f;
        ghoulProfile.heatGeneration = 80.0f;

        // 合成人
        ThermalProfile synthProfile;
        synthProfile.baseTemperature = 37.0f; // 模拟人体温度
        synthProfile.emissivity = 0.95f;
        synthProfile.material = MaterialType::Flesh;
        synthProfile.isHeatSource = true;
        synthProfile.heatGeneration = 90.0f;

        // 机器人/机械
        ThermalProfile robotProfile;
        robotProfile.baseTemperature = 45.0f;
        robotProfile.emissivity = 0.85f;
        robotProfile.heatCapacity = 500.0f;
        robotProfile.thermalConductivity = 50.0f;
        robotProfile.material = MaterialType::Metal;
        robotProfile.isHeatSource = true;
        robotProfile.heatGeneration = 200.0f;

        // 保安机器人
        ThermalProfile sentryBotProfile = robotProfile;
        sentryBotProfile.baseTemperature = 60.0f;
        sentryBotProfile.heatGeneration = 500.0f;

        // 激光炮塔
        ThermalProfile turretProfile = robotProfile;
        turretProfile.baseTemperature = 50.0f;
        turretProfile.heatGeneration = 300.0f;

        // 动力装甲
        ThermalProfile powerArmorProfile;
        powerArmorProfile.baseTemperature = 40.0f;
        powerArmorProfile.emissivity = 0.8f;
        powerArmorProfile.material = MaterialType::Metal;
        powerArmorProfile.isHeatSource = true;
        powerArmorProfile.heatGeneration = 250.0f;
    }

    void TemperatureDetection::Update(float deltaTime)
    {
        UpdateDynamicTemperatures(deltaTime);
    }

    void TemperatureDetection::UpdateDynamicTemperatures(float deltaTime)
    {
        for (auto& [formID, data] : m_dynamicData)
        {
            if (!data.isActive)
                continue;

            // 温度衰减/恢复
            float diff = data.targetTemperature - data.currentTemperature;
            data.currentTemperature += diff * data.temperatureRate * deltaTime;

            data.lastUpdateTime += deltaTime;
        }
    }

    float TemperatureDetection::GetObjectTemperature(RE::TESObjectREFR* refr)
    {
        if (!refr)
            return m_ambientTemperature;

        uint32_t formID = refr->formID;

        // 检查是否有自定义配置
        auto profileIt = m_thermalProfiles.find(formID);
        if (profileIt != m_thermalProfiles.end())
        {
            return profileIt->second.baseTemperature;
        }

        // 检查动态温度
        auto dynamicIt = m_dynamicData.find(formID);
        if (dynamicIt != m_dynamicData.end())
        {
            return dynamicIt->second.currentTemperature;
        }

        // 如果是Actor
        if (auto actor = refr->As<RE::Actor>())
        {
            return GetActorTemperature(actor);
        }

        // 估算温度
        return EstimateObjectTemperature(refr);
    }

    float TemperatureDetection::GetActorTemperature(RE::Actor* actor)
    {
        if (!actor)
            return m_ambientTemperature;

        return CalculateActorTemperature(actor);
    }

    float TemperatureDetection::CalculateActorTemperature(RE::Actor* actor)
    {
        float baseTemp = 36.5f; // 默认人体温度

        // 检查是否死亡
        if (actor->IsDead(false))
        {
            // 死亡后体温逐渐降至环境温度
            float deathTime = 300.0f; // 假设5分钟
            baseTemp = std::lerp(m_ambientTemperature, baseTemp, std::exp(-deathTime / 600.0f));
            return baseTemp;
        }

        // 检查种族
        if (actor->race)
        {
            std::string raceName = actor->race->GetFullName();

            if (raceName.find("Ghoul") != std::string::npos)
            {
                baseTemp = 34.0f; // 尸鬼体温较低
            }
            else if (raceName.find("Super Mutant") != std::string::npos)
            {
                baseTemp = 38.0f; // 超级变种人体温较高
            }
            else if (raceName.find("Robot") != std::string::npos ||
                     raceName.find("Synth") != std::string::npos)
            {
                baseTemp = 45.0f; // 机器人/合成人温度
            }
        }

        // 战斗状态增加体温
        if (actor->IsInCombat())
        {
            baseTemp += 2.0f;
        }

        return baseTemp;
    }

    float TemperatureDetection::EstimateObjectTemperature(RE::TESObjectREFR* refr)
    {
        if (!refr || !refr->data.objectReference)
            return m_ambientTemperature;

        auto baseForm = refr->data.objectReference;

        // 检测火焰对象
        if (IsFireObject(baseForm))
        {
            return 500.0f + (rand() % 200); // 500-700°C
        }

        // 检测电子设备
        if (IsElectronicObject(baseForm))
        {
            return 35.0f + (rand() % 15); // 35-50°C
        }

        // 检测武器
        if (IsWeapon(baseForm))
        {
            // 这里可以检测最近是否开火
            return m_ambientTemperature + 5.0f;
        }

        // 根据关键词判断
        if (auto keywordForm = baseForm->As<RE::BGSKeywordForm>())
        {
            // 检查关键词来判断材质
            // 这需要具体的关键词ID
        }

        // 默认返回环境温度
        return m_ambientTemperature + ((rand() % 10) - 5) * 0.1f;
    }

    bool TemperatureDetection::IsFireObject(RE::TESForm* form)
    {
		return false;
    }

    bool TemperatureDetection::IsElectronicObject(RE::TESForm* form)
    {
		return false;
    }

    bool TemperatureDetection::IsWeapon(RE::TESForm* form)
    {
        return form && form->As<RE::TESObjectWEAP>() != nullptr;
    }

    MaterialType TemperatureDetection::GetMaterialType(RE::TESForm* form)
    {
        return MaterialType::Unknown;
    }

    float TemperatureDetection::GetWeaponTemperature(RE::TESObjectWEAP* weapon, RE::Actor* owner)
    {
        if (!weapon)
            return m_ambientTemperature;

        float baseTemp = m_ambientTemperature;
        return baseTemp;
    }

    void TemperatureDetection::RegisterThermalProfile(uint32_t formID, const ThermalProfile& profile)
    {
        m_thermalProfiles[formID] = profile;
    }

    float TemperatureDetection::GetTemperatureByFormID(uint32_t formID)
    {
        // 检查配置
        auto profileIt = m_thermalProfiles.find(formID);
        if (profileIt != m_thermalProfiles.end())
        {
            return profileIt->second.baseTemperature;
        }

        // 检查动态数据
        auto dynamicIt = m_dynamicData.find(formID);
        if (dynamicIt != m_dynamicData.end())
        {
            return dynamicIt->second.currentTemperature;
        }

        return m_ambientTemperature;
    }

    // 工具函数实现
    namespace ThermalUtils
    {
        float AttenuateTemperature(float temperature, float distance, float ambientTemp)
        {
            // 基于距离的温度衰减
            float attenuation = 1.0f / (1.0f + distance * 0.01f);
            return std::lerp(ambientTemp, temperature, attenuation);
        }

        float GetEmissivity(MaterialType material)
        {
            switch (material)
            {
            case MaterialType::Flesh: return 0.98f;
            case MaterialType::Metal: return 0.85f;
            case MaterialType::Concrete: return 0.94f;
            case MaterialType::Wood: return 0.90f;
            case MaterialType::Glass: return 0.92f;
            case MaterialType::Water: return 0.96f;
            case MaterialType::Fabric: return 0.77f;
            case MaterialType::Plastic: return 0.91f;
            case MaterialType::Energy: return 1.0f;
            case MaterialType::Fire: return 1.0f;
            case MaterialType::Ice: return 0.97f;
            case MaterialType::Electronic: return 0.85f;
            default: return 0.9f;
            }
        }

        float CalculateThermalRadiation(float temperature, float emissivity, float area)
        {
            // Stefan-Boltzmann定律
            const float sigma = 5.67e-8f; // Stefan-Boltzmann常数
            float tempKelvin = temperature + 273.15f;
            return emissivity * sigma * area * std::pow(tempKelvin, 4);
        }

        float BlendTemperatures(const std::vector<float>& temperatures, const std::vector<float>& weights)
        {
            if (temperatures.empty() || temperatures.size() != weights.size())
                return 20.0f;

            float totalWeight = 0.0f;
            float weightedSum = 0.0f;

            for (size_t i = 0; i < temperatures.size(); ++i)
            {
                weightedSum += temperatures[i] * weights[i];
                totalWeight += weights[i];
            }

            return (totalWeight > 0) ? (weightedSum / totalWeight) : 20.0f;
        }

        float ApplyWeatherEffects(float temperature, RE::TESWeather* weather)
        {
            if (!weather)
                return temperature;

            // 根据天气调整温度
            // 这需要具体的天气系统实现
            return temperature;
        }
    }
}
