#pragma once

#include <nlohmann/json.hpp>

namespace ThroughScope 
{
    enum class Language 
    {
        English = 0,
        Chinese_Simplified = 1,
        Chinese_Traditional = 2,
        Japanese = 3,
        Korean = 4,
        German = 5,
        French = 6,
        Spanish = 7,
        Russian = 8,
        Italian = 9,
        Portuguese = 10,
        Polish = 11,
        COUNT
    };

    class LocalizationManager 
    {
    public:
        static LocalizationManager* GetSingleton();
        
        // 初始化和清理 
        bool Initialize();
        void Shutdown();
        
        // 语言设置
        void SetLanguage(Language language);
        Language GetCurrentLanguage() const { return m_CurrentLanguage; }
        
        // 翻译功能
        const char* GetText(const char* key) const;
        const char* GetTextFormat(const char* key, ...) const;
        
        // 语言信息
        const char* GetLanguageName(Language language) const;
        const char* GetLanguageCode(Language language) const;
        
        // 重新加载语言文件
        bool ReloadLanguageFiles();
        
        // 检查是否已初始化
        bool IsInitialized() const { return m_Initialized; }

    private:
        LocalizationManager() = default;
        ~LocalizationManager() = default;
        
        // 加载语言文件
        bool LoadLanguageFile(Language language);
        bool LoadLanguageFromJSON(const std::string& filePath, Language language);
        void CreateDefaultEnglishFile(const std::string& filePath);
        nlohmann::json GetDefaultEnglishJSON(); // Helper to retrieve default keys
        
        // 内部变量
        bool m_Initialized = false;
        Language m_CurrentLanguage = Language::English;
        
        // 翻译数据存储
        std::unordered_map<Language, std::unordered_map<std::string, std::string>> m_Translations;
        
        // 格式化缓冲区
        mutable char m_FormatBuffer[1024];
        
        // 语言信息
        struct LanguageInfo 
        {
            const char* name;
            const char* code;
        };
        
        static const LanguageInfo s_LanguageInfo[static_cast<int>(Language::COUNT)];
    };
}

// 便捷宏定义
#define _S(_LITERAL) (const char*)u8##_LITERAL
#define LOCALIZE(key) ThroughScope::LocalizationManager::GetSingleton()->GetText(key)
#define LOCALIZE_FMT(key, ...) ThroughScope::LocalizationManager::GetSingleton()->GetTextFormat(key, __VA_ARGS__)
#define LOC(key) LOCALIZE(key)
#define LOCF(key, ...) LOCALIZE_FMT(key, __VA_ARGS__) 
