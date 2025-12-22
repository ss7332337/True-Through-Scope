#pragma once

#include <Windows.h>
#include "ENB/ENBHelper.h"
#include "rendering/SecondPassRenderer.h"
#include "D3DHooks.h"
#include "ScopeCamera.h"
#include "rendering/RenderStateManager.h"

namespace ThroughScope
{
	/**
	 * ENB 集成管理器
	 * 
	 * 负责与 ENBSeries 进行集成，包括：
	 * 1. 检测 ENB 是否存在
	 * 2. 注册 ENB 回调函数
	 * 3. 在开镜时动态禁用导致鬼影的 ENB 效果 (SSAO, SkyAmbientCalculation)
	 * 4. 在收镜时恢复这些效果
	 */
	class ENBIntegration
	{
	public:
		static ENBIntegration* GetSingleton()
		{
			static ENBIntegration instance;
			return &instance;
		}

		/**
		 * 初始化 ENB 集成
		 * 应在插件加载时调用
		 * @return true 如果 ENB 检测到并成功初始化
		 */
		bool Initialize();

		/**
		 * 关闭 ENB 集成
		 * 应在插件卸载时调用
		 */
		void Shutdown();

		/**
		 * 检查 ENB 是否已检测到
		 */
		bool IsENBDetected() const { return m_enbDetected; }

		/**
		 * 检查 ENB 集成是否已启用
		 */
		bool IsIntegrationEnabled() const { return m_integrationEnabled; }

		/**
		 * 获取 ENB 版本号
		 */
		long GetENBVersion() const { return m_enbVersion; }

		/**
		 * 获取 ENB SDK 版本
		 */
		long GetENBSDKVersion() const { return m_enbSDKVersion; }

		/**
		 * 标记需要在下一个 EndFrame 回调中渲染瞄具
		 */
		void RequestScopeRender() { m_scopeRenderRequested = true; }

		/**
		 * 检查是否使用 ENB 回调渲染
		 * @return 始终返回 false，使用直接渲染方式
		 */
		bool ShouldUseCallbackRendering() const { return false; }

		/**
		 * 设置开镜状态（供外部调用）
		 * 当玩家开镜时调用 SetAiming(true)，收镜时调用 SetAiming(false)
		 * ENB 效果将在下一帧的 BeginFrame 回调中调整
		 */
		void SetAiming(bool isAiming);
		bool IsAiming() const { return m_isAiming; }

		/**
		 * 检查效果切换是否启用
		 */
		bool IsEffectToggleEnabled() const { return m_effectToggleEnabled; }
		void SetEffectToggleEnabled(bool enabled) { m_effectToggleEnabled = enabled; }

	private:
		ENBIntegration() = default;
		~ENBIntegration() = default;
		ENBIntegration(const ENBIntegration&) = delete;
		ENBIntegration& operator=(const ENBIntegration&) = delete;

		// ENB 回调函数（静态，因为 ENB 需要 C 风格回调）
		static void WINAPI ENBCallback(ENBCallbackType calltype);

		// 在回调中禁用导致鬼影的 ENB 效果
		void DisableGhostingEffects();

		// 在回调中恢复 ENB 效果
		void RestoreGhostingEffects();

		// 保存原始 ENB 参数（用于恢复）
		void BackupEffectParameters();

		// ENB API 实例
		ENBHelper::ENBInterface* m_enbAPI = nullptr;

		// 状态标志
		bool m_enbDetected = false;
		bool m_integrationEnabled = false;
		bool m_scopeRenderRequested = false;
		bool m_inCallback = false;

		// 开镜状态
		bool m_isAiming = false;
		bool m_wasAiming = false;  // 上一帧的状态
		bool m_effectsDisabled = false;  // 效果当前是否被禁用
		bool m_effectToggleEnabled = true;  // 是否启用效果切换功能
		bool m_parametersBackedUp = false;  // 是否已备份参数

		// 备份的 ENB 参数
		ENBParameter m_backupSSAO;
		ENBParameter m_backupSkyAmbient;
		bool m_originalSSAOEnabled = true;
		bool m_originalSkyAmbientEnabled = true;

		// ENB 版本信息
		long m_enbVersion = 0;
		long m_enbSDKVersion = 0;
	};
}
