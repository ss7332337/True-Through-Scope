#include "DebugPanel.h"

namespace ThroughScope
{
	DebugPanel::DebugPanel(PanelManagerInterface* manager) :
		m_Manager(manager)
	{
	}

	void DebugPanel::Render()
	{
		if (m_AutoRefresh) {
			float currentTime = ImGui::GetTime();
			if (currentTime - m_LastRefreshTime > m_RefreshInterval) {
				UpdateDebugInfo();
				m_LastRefreshTime = currentTime;
			}
		}

		RenderCameraInformation();
		ImGui::Spacing();
		RenderTTSNodeInformation();
		ImGui::Spacing();
		RenderRenderingStatus();
		ImGui::Spacing();
		RenderAdvancedDebugInfo();

		ImGui::Spacing();
		RenderActionButtons();
	}

	void DebugPanel::Update()
	{
		if (!m_AutoRefresh) {
			UpdateDebugInfo();
		}
	}

	void DebugPanel::RenderCameraInformation()
	{
		RenderSectionHeader(LOC("debug.scope_camera_info"));

		auto scopeCamera = ScopeCamera::GetScopeCamera();
		if (!scopeCamera) {
			ImGui::TextColored(m_WarningColor, LOC("debug.camera_not_available"));
			return;
		}

		char buffer[128];

		ImGui::Columns(2, "CameraColumns", false);
		ImGui::SetColumnWidth(0, 150);

		// Left column - labels
		ImGui::Text(LOC("debug.local_position"));
		ImGui::Text(LOC("debug.world_position"));
		ImGui::Text(LOC("debug.local_rotation"));
		ImGui::Text(LOC("debug.world_rotation"));
		ImGui::Text(LOC("debug.current_fov"));

		ImGui::NextColumn();

		// 右列 - 值
		FormatVector3(m_DebugInfo.cameraLocalPos, buffer, sizeof(buffer));
		ImGui::Text("%s", buffer);

		FormatVector3(m_DebugInfo.cameraWorldPos, buffer, sizeof(buffer));
		ImGui::Text("%s", buffer);

		FormatRotationDegrees(m_DebugInfo.cameraLocalRot, buffer, sizeof(buffer));
		ImGui::Text("%s", buffer);

		FormatRotationDegrees(m_DebugInfo.cameraWorldRot, buffer, sizeof(buffer));
		ImGui::Text("%s", buffer);

		ImGui::Text("%.2f°", m_DebugInfo.currentFOV);

		ImGui::Columns(1);
	}

	void DebugPanel::RenderTTSNodeInformation()
	{
		RenderSectionHeader(LOC("debug.tts_node_info"));

		if (!m_DebugInfo.ttsNodeExists) {
			ImGui::TextColored(m_WarningColor, LOC("debug.tts_node_not_found"));
			return;
		}

		char buffer[128];

		ImGui::Columns(2, "TTSColumns", false);
		ImGui::SetColumnWidth(0, 150);

		// Left column - labels
		ImGui::Text(LOC("debug.local_position"));
		ImGui::Text(LOC("debug.world_position"));
		ImGui::Text(LOC("debug.local_rotation"));
		ImGui::Text(LOC("debug.world_rotation"));
		ImGui::Text(LOC("debug.local_scale"));


		ImGui::NextColumn();

		// 右列 - 值
		FormatVector3(m_DebugInfo.ttsLocalPos, buffer, sizeof(buffer));
		ImGui::Text("%s", buffer);

		FormatVector3(m_DebugInfo.ttsWorldPos, buffer, sizeof(buffer));
		ImGui::Text("%s", buffer);

		FormatRotationDegrees(m_DebugInfo.ttsLocalRot, buffer, sizeof(buffer));
		ImGui::Text("%s", buffer);

		FormatRotationDegrees(m_DebugInfo.ttsWorldRot, buffer, sizeof(buffer));
		ImGui::Text("%s", buffer);

		ImGui::Text("%.3f", m_DebugInfo.ttsLocalScale);

		ImGui::Columns(1);
	}

	void DebugPanel::RenderRenderingStatus()
	{
		RenderSectionHeader(LOC("debug.rendering_status"));

		ImGui::Columns(2, "StatusColumns", false);
		ImGui::SetColumnWidth(0, 180);

		// Left column - labels
		ImGui::Text(LOC("debug.rendering_enabled"));
		ImGui::Text(LOC("debug.is_forward_stage"));
		ImGui::Text(LOC("debug.rendering_for_scope"));
		ImGui::Text(LOC("debug.node_status"));

		ImGui::NextColumn();

		// 右列 - 状态
		ImGui::TextColored(m_DebugInfo.renderingEnabled ? m_SuccessColor : m_ErrorColor,
			"%s", m_DebugInfo.renderingEnabled ? LOC("common.yes") : LOC("common.no"));

		ImGui::TextColored(m_DebugInfo.isForwardStage ? m_SuccessColor : m_WarningColor,
			"%s", m_DebugInfo.isForwardStage ? LOC("common.yes") : LOC("common.no"));

		ImGui::TextColored(m_DebugInfo.isRenderingForScope ? m_SuccessColor : m_WarningColor,
			"%s", m_DebugInfo.isRenderingForScope ? LOC("common.yes") : LOC("common.no"));

		std::string nodeStatus = GetNodeStatusText();
		ImGui::TextColored(m_DebugInfo.ttsNodeExists ? m_SuccessColor : m_WarningColor,
			"%s", nodeStatus.c_str());

		ImGui::Columns(1);
	}

	void DebugPanel::RenderAdvancedDebugInfo()
	{
		if (!ImGui::CollapsingHeader(LOC("debug.advanced_debug_info"))) {
			return;
		}

		// ========== GBuffer Debug Overlay Controls ==========
		ImGui::Text("Render Target Debug Overlay");
		
		// Enable/Disable checkbox
		ImGui::Checkbox("Show Debug Overlay", &SecondPassRenderer::s_ShowMVDebug);
		RenderHelpTooltip("Display a render target texture in the top-right corner of the screen");
		
		if (SecondPassRenderer::s_ShowMVDebug) {
			ImGui::SameLine();
			ImGui::SetNextItemWidth(180);
			
			// Dropdown for all managed RTs (matches RenderTargetMerger)
			const char* rtOptions[] = {
				"RT_09 SSR_BlurredExtra",
				"RT_20 GBuffer_Normal",
				"RT_22 GBuffer_Albedo", 
				"RT_23 GBuffer_Emissive",
				"RT_24 GBuffer_Material",
				"RT_28 SSAO",
				"RT_29 MotionVectors",
				"RT_39 DepthMips",
				"RT_57 Mask",
				"RT_58 DeferredDiffuse",
				"RT_59 DeferredSpecular"
			};
			static int selectedRTIndex = 1;  // Default to GBuffer_Normal
			static const int rtIndices[] = { 9, 20, 22, 23, 24, 28, 29, 39, 57, 58, 59 };
			constexpr int numRTs = sizeof(rtIndices) / sizeof(rtIndices[0]);
			
			// Find current selection based on s_DebugGBufferIndex
			for (int i = 0; i < numRTs; i++) {
				if (rtIndices[i] == SecondPassRenderer::s_DebugGBufferIndex) {
					selectedRTIndex = i;
					break;
				}
			}
			
			if (ImGui::Combo("##RTSelect", &selectedRTIndex, rtOptions, IM_ARRAYSIZE(rtOptions))) {
				SecondPassRenderer::s_DebugGBufferIndex = rtIndices[selectedRTIndex];
			}
			RenderHelpTooltip("Select which render target to display:\n"
				"- SSR_BlurredExtra: Screen-space reflections (half-res)\n"
				"- GBuffer_Normal: Surface normals\n"
				"- GBuffer_Albedo: Base color\n"
				"- GBuffer_Emissive: Self-illumination (amplified)\n"
				"- GBuffer_Material: Material properties\n"
				"- SSAO: Ambient occlusion (half-res)\n"
				"- MotionVectors: Per-pixel motion (amplified)\n"
				"- DepthMips: Hierarchical depth\n"
				"- Mask: Unknown mask buffer\n"
				"- DeferredDiffuse: SSS diffuse lighting\n"
				"- DeferredSpecular: SSS specular lighting");
		}
		
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();

		ImGui::Text(LOC("debug.weapon_information"));
		if (weaponInfo.weapon) {
			ImGui::BulletText(LOC("debug.form_id"), weaponInfo.weaponFormID);
			ImGui::BulletText(LOC("debug.mod_name"), weaponInfo.weaponModName.c_str());
			ImGui::BulletText(LOC("debug.has_config"), weaponInfo.currentConfig ? LOC("common.yes") : LOC("common.no"));

			if (weaponInfo.currentConfig) {
				ImGui::BulletText(LOC("debug.model_name"), weaponInfo.currentConfig->modelName.c_str());
				ImGui::BulletText(LOC("debug.config_source"), weaponInfo.configSource.c_str());
			}
		} else {
			ImGui::TextColored(m_WarningColor, LOC("debug.no_weapon_equipped"));
		}
	}

	void DebugPanel::RenderActionButtons()
	{
		RenderSectionHeader(LOC("debug.actions"));

		// 第一行按钮
		if (ImGui::Button(LOC("debug.print_hierarchy"))) {
			PrintNodeHierarchy();
		}
		RenderHelpTooltip(LOC("tooltip.print_hierarchy"));

		ImGui::SameLine();
		if (ImGui::Button(LOC("debug.refresh_node"))) {
			RefreshTTSNode();
		}
		RenderHelpTooltip(LOC("tooltip.refresh_node"));

		ImGui::SameLine();
		if (ImGui::Button(LOC("debug.copy_values"))) {
			CopyValuesToClipboard();
		}
		RenderHelpTooltip(LOC("tooltip.copy_values"));

		// 第二行按钮
		if (ImGui::Button(LOC("debug.force_update"))) {
			UpdateDebugInfo();
			m_Manager->SetDebugText(LOC("debug.info_updated"));
		}

		ImGui::SameLine();
		if (ImGui::Button(LOC("debug.clear_log"))) {
			m_Manager->SetDebugText("");
		}

		// 设置选项
		ImGui::Spacing();
		ImGui::Separator();

		ImGui::Checkbox(LOC("debug.auto_refresh"), &m_AutoRefresh);
		RenderHelpTooltip(LOC("tooltip.auto_refresh"));

		ImGui::SameLine();
		ImGui::Checkbox(LOC("debug.show_advanced"), &m_ShowAdvancedInfo);
		RenderHelpTooltip(LOC("tooltip.show_advanced_debug"));

		if (m_AutoRefresh) {
			ImGui::SameLine();
			ImGui::SetNextItemWidth(100);
			ImGui::SliderFloat("##RefreshRate", &m_RefreshInterval, 0.1f, 5.0f, "%.1fs");
			RenderHelpTooltip(LOC("tooltip.refresh_interval"));
		}

	}

	void DebugPanel::PrintNodeHierarchy()
	{
		try {
			auto playerCharacter = RE::PlayerCharacter::GetSingleton();
			if (playerCharacter && playerCharacter->Get3D()) {
				auto weaponNode = playerCharacter->Get3D()->GetObjectByName("Weapon");
				if (weaponNode) {
					ThroughScope::Utilities::PrintNodeHierarchy(weaponNode);
					m_Manager->SetDebugText(LOC("debug.hierarchy_printed"));
				} else {
					m_Manager->SetDebugText(LOC("debug.weapon_node_not_found"));
				}
			} else {
				m_Manager->SetDebugText(LOC("debug.player_not_available"));
			}
		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format(fmt::runtime(LOC("debug.error_printing_hierarchy")), e.what()).c_str());
		}
	}

	void DebugPanel::RefreshTTSNode()
	{
		UpdateDebugInfo();

		if (m_DebugInfo.ttsNodeExists) {
			m_Manager->SetDebugText(LOC("debug.tts_found_updated"));
		} else {
			m_Manager->SetDebugText(LOC("debug.tts_not_found_exclaim"));
		}
	}

	void DebugPanel::CopyValuesToClipboard()
	{
		try {
			std::string clipboardText;

			// 格式化调试信息
			if (m_DebugInfo.ttsNodeExists) {
				clipboardText = fmt::format(
					"{}\n"
					"=====================\n"
					"{}: [{:.3f}, {:.3f}, {:.3f}]\n"
					"{}: [{:.1f}, {:.1f}, {:.1f}] {}\n"
					"{}: {:.3f}\n\n"
					"{}: [{:.3f}, {:.3f}, {:.3f}]\n"
					"{}: {:.2f}°\n\n"
					"{}: {}\n"
					"{}: {}\n"
					"{}: {}\n",
					LOC("debug.clipboard_header"),
					LOC("debug.tts_position"), m_DebugInfo.ttsLocalPos.x, m_DebugInfo.ttsLocalPos.y, m_DebugInfo.ttsLocalPos.z,
					LOC("debug.tts_rotation"), m_DebugInfo.ttsLocalRot.x, m_DebugInfo.ttsLocalRot.y, m_DebugInfo.ttsLocalRot.z, LOC("debug.degrees"),
					LOC("debug.tts_scale"), m_DebugInfo.ttsLocalScale,
					LOC("debug.camera_position"), m_DebugInfo.cameraLocalPos.x, m_DebugInfo.cameraLocalPos.y, m_DebugInfo.cameraLocalPos.z,
					LOC("debug.camera_fov"), m_DebugInfo.currentFOV,
					LOC("debug.rendering_status"), m_DebugInfo.renderingEnabled ? LOC("debug.enabled") : LOC("debug.disabled"),
					LOC("debug.forward_stage"), m_DebugInfo.isForwardStage ? LOC("common.yes") : LOC("common.no"),
					LOC("debug.rendering_for_scope"), m_DebugInfo.isRenderingForScope ? LOC("common.yes") : LOC("common.no"));
			} else {
				clipboardText = fmt::format(
					"{}\n"
					"=====================\n"
					"TTSNode: {}\n"
					"{}\n",
					LOC("debug.clipboard_header"),
					LOC("debug.not_found"),
					LOC("debug.load_model_first"));
			}

			// 复制到剪贴板
			if (::OpenClipboard(nullptr)) {
				EmptyClipboard();
				HGLOBAL hClipboardData = GlobalAlloc(GMEM_DDESHARE, clipboardText.length() + 1);
				if (hClipboardData) {
					char* pchData = (char*)GlobalLock(hClipboardData);
					strcpy_s(pchData, clipboardText.length() + 1, clipboardText.c_str());
					GlobalUnlock(hClipboardData);
					SetClipboardData(CF_TEXT, hClipboardData);
				}
				CloseClipboard();
				m_Manager->SetDebugText(LOC("debug.values_copied"));
			} else {
				m_Manager->SetDebugText(LOC("debug.clipboard_failed"));
			}
		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format(fmt::runtime(LOC("debug.error_copying")), e.what()).c_str());
		}
	}


	void DebugPanel::UpdateDebugInfo()
	{
		// 更新相机信息
		auto scopeCamera = ScopeCamera::GetScopeCamera();
		if (scopeCamera) {
			m_DebugInfo.cameraLocalPos = scopeCamera->local.translate;
			m_DebugInfo.cameraWorldPos = scopeCamera->world.translate;

			float pitch, yaw, roll;
			scopeCamera->local.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);
			m_DebugInfo.cameraLocalRot = RE::NiPoint3(pitch, yaw, roll);

			scopeCamera->world.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);
			m_DebugInfo.cameraWorldRot = RE::NiPoint3(pitch, yaw, roll);

			m_DebugInfo.currentFOV = ScopeCamera::GetTargetFOV();
		}

		// 更新TTSNode信息
		auto ttsNode = m_Manager->GetTTSNode();
		if (ttsNode) {
			m_DebugInfo.ttsNodeExists = true;
			m_DebugInfo.ttsLocalPos = ttsNode->local.translate;
			m_DebugInfo.ttsWorldPos = ttsNode->world.translate;
			m_DebugInfo.ttsLocalScale = ttsNode->local.scale;

			float pitch, yaw, roll;
			ttsNode->local.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);
			m_DebugInfo.ttsLocalRot = RE::NiPoint3(pitch, yaw, roll);

			ttsNode->world.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);
			m_DebugInfo.ttsWorldRot = RE::NiPoint3(pitch, yaw, roll);
		} else {
			m_DebugInfo.ttsNodeExists = false;
		}

		// 更新渲染状态
		m_DebugInfo.renderingEnabled = D3DHooks::IsEnableRender();
		m_DebugInfo.isForwardStage = D3DHooks::GetForwardStage();
		m_DebugInfo.isRenderingForScope = ScopeCamera::IsRenderingForScope();

		// 更新性能信息
		ImGuiIO& io = ImGui::GetIO();
		m_DebugInfo.frameTime = io.DeltaTime;
		m_DebugInfo.frameCount = (int)ImGui::GetFrameCount();
	}

	void DebugPanel::FormatVector3(const RE::NiPoint3& vec, char* buffer, size_t bufferSize, const char* format)
	{
		snprintf(buffer, bufferSize, format, vec.x, vec.y, vec.z);
	}

	void DebugPanel::FormatRotationDegrees(const RE::NiPoint3& rot, char* buffer, size_t bufferSize)
	{
		float pitch = rot.x * 57.2957795f;  // 弧度转度数
		float yaw = rot.y * 57.2957795f;
		float roll = rot.z * 57.2957795f;
		snprintf(buffer, bufferSize, "[%.1f, %.1f, %.1f]°", pitch, yaw, roll);
	}

	std::string DebugPanel::GetRenderingStatusText()
	{
		if (m_DebugInfo.renderingEnabled) {
			return m_DebugInfo.isRenderingForScope ? "Active (Scope)" : "Active (Normal)";
		}
		return "Disabled";
	}

	std::string DebugPanel::GetNodeStatusText()
	{
		return m_DebugInfo.ttsNodeExists ? "✓ Found" : "⚠ Missing";
	}

}
