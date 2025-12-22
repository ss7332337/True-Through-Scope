#include "ENBIntegration.h"
#include "GlobalTypes.h"
#include "rendering/SecondPassRenderer.h"
#include "rendering/ScopeRenderingManager.h"
#include <d3d9.h>

namespace ThroughScope
{
	// 全局指针用于静态回调访问单例
	static ENBIntegration* g_enbIntegration = nullptr;

	bool ENBIntegration::Initialize()
	{
		logger::info("[ENB Integration] Initializing ENB integration...");

		// 尝试获取 ENB API
		m_enbAPI = ENBHelper::CreateInterface(ENBHelper::SDK_VERSION_1_01);

		if (!m_enbAPI) {
			logger::info("[ENB Integration] ENB not detected or incompatible SDK version");
			m_enbDetected = false;
			m_integrationEnabled = false;
			return false;
		}

		m_enbDetected = true;
		m_enbSDKVersion = m_enbAPI->GetSDKVersion();
		m_enbVersion = m_enbAPI->GetVersion();

		logger::info("[ENB Integration] ENB detected!");
		logger::info("[ENB Integration]   SDK Version: {}", m_enbSDKVersion);
		logger::info("[ENB Integration]   ENB Version: 0.{}", m_enbVersion);

		// 保存单例指针供静态回调使用
		g_enbIntegration = this;

		// 注册回调函数
		m_enbAPI->SetCallbackFunction(ENBCallback);
		m_integrationEnabled = true;

		logger::info("[ENB Integration] Callback registered successfully");
		logger::info("[ENB Integration] Effect toggle enabled: SSAO and SkyAmbient will be disabled when aiming");

		return true;
	}

	void ENBIntegration::Shutdown()
	{
		logger::info("[ENB Integration] Shutting down...");

		// 确保效果已恢复
		if (m_effectsDisabled && m_enbAPI) {
			RestoreGhostingEffects();
		}

		if (m_enbAPI) {
			m_enbAPI->SetCallbackFunction(nullptr);
			delete m_enbAPI;
			m_enbAPI = nullptr;
		}

		m_enbDetected = false;
		m_integrationEnabled = false;
		g_enbIntegration = nullptr;

		logger::info("[ENB Integration] Shutdown complete");
	}

	void ENBIntegration::SetAiming(bool isAiming)
	{
		m_isAiming = isAiming;
	}

	void ENBIntegration::BackupEffectParameters()
	{
		if (!m_enbAPI || m_parametersBackedUp) return;

		// 根据 enbseries.ini 配置，参数在 [EFFECT] 节
		// EnableSSAO 和 EnableSkyAmbientCalculation 都在 [EFFECT] 节
		
		// 备份 SSAO 启用状态
		if (m_enbAPI->GetParameter("enbseries.ini", "EFFECT", "EnableSSAO", &m_backupSSAO)) {
			m_originalSSAOEnabled = (m_backupSSAO.Data[0] != 0);
		}

		// 备份 SkyAmbientCalculation
		if (m_enbAPI->GetParameter("enbseries.ini", "EFFECT", "EnableSkyAmbientCalculation", &m_backupSkyAmbient)) {
			m_originalSkyAmbientEnabled = (m_backupSkyAmbient.Data[0] != 0);
		}

		m_parametersBackedUp = true;
	}

	void ENBIntegration::DisableGhostingEffects()
	{
		if (!m_enbAPI || m_effectsDisabled) return;

		// 创建禁用参数
		ENBParameter disableParam;
		disableParam.Type = ENBParam_BOOL;
		disableParam.Size = sizeof(BOOL);
		BOOL falseVal = FALSE;
		memcpy(disableParam.Data, &falseVal, sizeof(BOOL));

		// 禁用 SSAO - 在 [EFFECT] 节
		bool ssaoDisabled = m_enbAPI->SetParameter("enbseries.ini", "EFFECT", "EnableSSAO", &disableParam);
		
		// 禁用 SkyAmbientCalculation - 在 [EFFECT] 节
		bool skyAmbientDisabled = m_enbAPI->SetParameter("enbseries.ini", "EFFECT", "EnableSkyAmbientCalculation", &disableParam);

		if (ssaoDisabled || skyAmbientDisabled) {
			m_effectsDisabled = true;
		}
	}

	void ENBIntegration::RestoreGhostingEffects()
	{
		if (!m_enbAPI || !m_effectsDisabled) return;

		// 创建启用参数
		ENBParameter enableParam;
		enableParam.Type = ENBParam_BOOL;
		enableParam.Size = sizeof(BOOL);
		BOOL trueVal = TRUE;
		memcpy(enableParam.Data, &trueVal, sizeof(BOOL));

		bool ssaoRestored = false;
		bool skyAmbientRestored = false;

		// 恢复 SSAO（使用原始值）- 在 [EFFECT] 节
		if (m_originalSSAOEnabled) {
			ssaoRestored = m_enbAPI->SetParameter("enbseries.ini", "EFFECT", "EnableSSAO", &enableParam);
		}

		// 恢复 SkyAmbientCalculation（使用原始值）- 在 [EFFECT] 节
		if (m_originalSkyAmbientEnabled) {
			skyAmbientRestored = m_enbAPI->SetParameter("enbseries.ini", "EFFECT", "EnableSkyAmbientCalculation", &enableParam);
		}

		m_effectsDisabled = false;
	}

	void WINAPI ENBIntegration::ENBCallback(ENBCallbackType calltype)
	{
		if (!g_enbIntegration) return;

		switch (calltype) {
		case ENBCallback_BeginFrame:
			// BeginFrame: 新帧开始，在此更新效果状态
			if (g_enbIntegration->m_effectToggleEnabled) {
				bool currentlyAiming = g_enbIntegration->m_isAiming;
				
				if (currentlyAiming && !g_enbIntegration->m_effectsDisabled) {
					// 玩家正在开镜且效果未禁用 -> 禁用效果
					g_enbIntegration->DisableGhostingEffects();
				} else if (!currentlyAiming && g_enbIntegration->m_effectsDisabled) {
					// 玩家不在开镜且效果已禁用 -> 恢复效果
					g_enbIntegration->RestoreGhostingEffects();
				}
				
				g_enbIntegration->m_wasAiming = currentlyAiming;
			}
			break;

		case ENBCallback_EndFrame:
			// EndFrame: 帧结束，不做处理
			break;

		case ENBCallback_PostLoad:
			// 配置加载后备份参数
			g_enbIntegration->BackupEffectParameters();
			break;

		case ENBCallback_PreSave:
			// 保存配置前恢复原始参数，避免我们的修改被保存
			if (g_enbIntegration->m_effectsDisabled) {
				g_enbIntegration->RestoreGhostingEffects();
			}
			break;

		case ENBCallback_OnInit:
			g_enbIntegration->BackupEffectParameters();
			break;

		case ENBCallback_OnExit:
			if (g_enbIntegration->m_effectsDisabled) {
				g_enbIntegration->RestoreGhostingEffects();
			}
			break;

		default:
			break;
		}
	}
}
