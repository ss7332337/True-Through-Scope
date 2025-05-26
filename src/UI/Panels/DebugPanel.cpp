
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

		if (m_ShowAdvancedInfo) {
			ImGui::Spacing();
			RenderPerformanceInfo();
			ImGui::Spacing();
			RenderAdvancedDebugInfo();
		}

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
		RenderSectionHeader("Scope Camera Information");

		auto scopeCamera = ScopeCamera::GetScopeCamera();
		if (!scopeCamera) {
			ImGui::TextColored(m_WarningColor, "Scope camera not available");
			return;
		}

		char buffer[128];

		ImGui::Columns(2, "CameraColumns", false);
		ImGui::SetColumnWidth(0, 150);

		// 左列 - 标签
		ImGui::Text("Local Position:");
		ImGui::Text("World Position:");
		ImGui::Text("Local Rotation:");
		ImGui::Text("World Rotation:");
		ImGui::Text("Current FOV:");

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
		RenderSectionHeader("TTSNode Information");

		if (!m_DebugInfo.ttsNodeExists) {
			ImGui::TextColored(m_WarningColor, "TTSNode not found");
			return;
		}

		char buffer[128];

		ImGui::Columns(2, "TTSColumns", false);
		ImGui::SetColumnWidth(0, 150);

		// 左列 - 标签
		ImGui::Text("Local Position:");
		ImGui::Text("World Position:");
		ImGui::Text("Local Rotation:");
		ImGui::Text("World Rotation:");
		ImGui::Text("Local Scale:");

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
		RenderSectionHeader("Rendering Status");

		ImGui::Columns(2, "StatusColumns", false);
		ImGui::SetColumnWidth(0, 180);

		// 左列 - 标签
		ImGui::Text("Rendering Enabled:");
		ImGui::Text("Is Forward Stage:");
		ImGui::Text("Rendering For Scope:");
		ImGui::Text("Node Status:");

		ImGui::NextColumn();

		// 右列 - 状态
		ImGui::TextColored(m_DebugInfo.renderingEnabled ? m_SuccessColor : m_ErrorColor,
			"%s", m_DebugInfo.renderingEnabled ? "Yes" : "No");

		ImGui::TextColored(m_DebugInfo.isForwardStage ? m_SuccessColor : m_WarningColor,
			"%s", m_DebugInfo.isForwardStage ? "Yes" : "No");

		ImGui::TextColored(m_DebugInfo.isRenderingForScope ? m_SuccessColor : m_WarningColor,
			"%s", m_DebugInfo.isRenderingForScope ? "Yes" : "No");

		std::string nodeStatus = GetNodeStatusText();
		ImGui::TextColored(m_DebugInfo.ttsNodeExists ? m_SuccessColor : m_WarningColor,
			"%s", nodeStatus.c_str());

		ImGui::Columns(1);
	}

	void DebugPanel::RenderPerformanceInfo()
	{
		if (!ImGui::CollapsingHeader("Performance Information")) {
			return;
		}

		ImGui::Text("Frame Time: %.3f ms", m_DebugInfo.frameTime * 1000.0f);
		ImGui::Text("FPS: %.1f", 1.0f / m_DebugInfo.frameTime);
		ImGui::Text("Frame Count: %d", m_DebugInfo.frameCount);

		// FPS图表
		static float fpsHistory[100] = {};
		static int fpsHistoryIndex = 0;

		float currentFPS = 1.0f / m_DebugInfo.frameTime;
		fpsHistory[fpsHistoryIndex] = currentFPS;
		fpsHistoryIndex = (fpsHistoryIndex + 1) % 100;

		ImGui::PlotLines("FPS", fpsHistory, 100, fpsHistoryIndex, nullptr, 0.0f, 120.0f, ImVec2(0, 80));
	}

	void DebugPanel::RenderAdvancedDebugInfo()
	{
		if (!ImGui::CollapsingHeader("Advanced Debug Information")) {
			return;
		}

		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();

		ImGui::Text("Weapon Information:");
		if (weaponInfo.weapon) {
			ImGui::BulletText("Form ID: %08X", weaponInfo.weaponFormID);
			ImGui::BulletText("Mod Name: %s", weaponInfo.weaponModName.c_str());
			ImGui::BulletText("Has Config: %s", weaponInfo.currentConfig ? "Yes" : "No");

			if (weaponInfo.currentConfig) {
				ImGui::BulletText("Model: %s", weaponInfo.currentConfig->modelName.c_str());
				ImGui::BulletText("Config Source: %s", weaponInfo.configSource.c_str());
			}
		} else {
			ImGui::TextColored(m_WarningColor, "No weapon equipped");
		}

		ImGui::Spacing();
		ImGui::Text("System Information:");
		ImGui::BulletText("ImGui Version: %s", ImGui::GetVersion());
		ImGui::BulletText("Display Size: %.0fx%.0f", ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
		ImGui::BulletText("Framerate: %.1f FPS", ImGui::GetIO().Framerate);
	}

	void DebugPanel::RenderActionButtons()
	{
		RenderSectionHeader("Debug Actions");

		// 第一行按钮
		if (ImGui::Button("Print Node Hierarchy")) {
			PrintNodeHierarchy();
		}
		RenderHelpTooltip("Print the weapon node hierarchy to the log file");

		ImGui::SameLine();
		if (ImGui::Button("Refresh TTSNode")) {
			RefreshTTSNode();
		}
		RenderHelpTooltip("Force refresh TTSNode information");

		ImGui::SameLine();
		if (ImGui::Button("Copy Values")) {
			CopyValuesToClipboard();
		}
		RenderHelpTooltip("Copy current debug values to clipboard");

		// 第二行按钮
		if (ImGui::Button("Force Update Debug Info")) {
			UpdateDebugInfo();
			m_Manager->SetDebugText("Debug information updated");
		}

		ImGui::SameLine();
		if (ImGui::Button("Clear Debug Log")) {
			m_Manager->SetDebugText("");
		}

		// 设置选项
		ImGui::Spacing();
		ImGui::Separator();

		ImGui::Checkbox("Auto Refresh", &m_AutoRefresh);
		RenderHelpTooltip("Automatically refresh debug information");

		ImGui::SameLine();
		ImGui::Checkbox("Show Advanced Info", &m_ShowAdvancedInfo);
		RenderHelpTooltip("Show additional debug information and performance data");

		if (m_AutoRefresh) {
			ImGui::SameLine();
			ImGui::SetNextItemWidth(100);
			ImGui::SliderFloat("##RefreshRate", &m_RefreshInterval, 0.1f, 5.0f, "%.1fs");
			RenderHelpTooltip("Debug information refresh interval");
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
					m_Manager->SetDebugText("Node hierarchy printed to log file");
				} else {
					m_Manager->SetDebugText("Weapon node not found");
				}
			} else {
				m_Manager->SetDebugText("Player character or 3D not available");
			}
		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format("Error printing hierarchy: {}", e.what()).c_str());
		}
	}

	void DebugPanel::RefreshTTSNode()
	{
		UpdateDebugInfo();

		if (m_DebugInfo.ttsNodeExists) {
			m_Manager->SetDebugText("TTSNode found and information updated");
		} else {
			m_Manager->SetDebugText("TTSNode not found!");
		}
	}

	void DebugPanel::CopyValuesToClipboard()
	{
		try {
			std::string clipboardText;

			// 格式化调试信息
			if (m_DebugInfo.ttsNodeExists) {
				clipboardText = fmt::format(
					"TTS Debug Information\n"
					"=====================\n"
					"TTSNode Position: [{:.3f}, {:.3f}, {:.3f}]\n"
					"TTSNode Rotation: [{:.1f}, {:.1f}, {:.1f}] degrees\n"
					"TTSNode Scale: {:.3f}\n\n"
					"Camera Position: [{:.3f}, {:.3f}, {:.3f}]\n"
					"Camera FOV: {:.2f}°\n\n"
					"Rendering Status: {}\n"
					"Forward Stage: {}\n"
					"Rendering For Scope: {}\n",
					m_DebugInfo.ttsLocalPos.x, m_DebugInfo.ttsLocalPos.y, m_DebugInfo.ttsLocalPos.z,
					m_DebugInfo.ttsLocalRot.x, m_DebugInfo.ttsLocalRot.y, m_DebugInfo.ttsLocalRot.z,
					m_DebugInfo.ttsLocalScale,
					m_DebugInfo.cameraLocalPos.x, m_DebugInfo.cameraLocalPos.y, m_DebugInfo.cameraLocalPos.z,
					m_DebugInfo.currentFOV,
					m_DebugInfo.renderingEnabled ? "Enabled" : "Disabled",
					m_DebugInfo.isForwardStage ? "Yes" : "No",
					m_DebugInfo.isRenderingForScope ? "Yes" : "No");
			} else {
				clipboardText =
					"TTS Debug Information\n"
					"=====================\n"
					"TTSNode: Not Found\n"
					"Please load a model first.\n";
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
				m_Manager->SetDebugText("Debug values copied to clipboard!");
			} else {
				m_Manager->SetDebugText("Failed to access clipboard!");
			}
		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format("Error copying to clipboard: {}", e.what()).c_str());
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
