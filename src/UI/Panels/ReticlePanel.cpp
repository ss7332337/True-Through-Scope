#include "ReticlePanel.h"
#include "D3DHooks.h"
#include "misc/cpp/imgui_stdlib.h"
#include <d3d11.h>

namespace ThroughScope
{

	ReticlePanel::ReticlePanel(PanelManagerInterface* manager) :
		m_Manager(manager)
	{
		// 设置默认值
		m_CurrentSettings.texturePath = "Test.dds";  // 默认纹理
		m_CurrentSettings.scale = 1.0f;
		m_CurrentSettings.offsetX = 0.5f;
		m_CurrentSettings.offsetY = 0.5f;
	}

	bool ReticlePanel::Initialize()
	{
		RefreshReticleTextures();
		LoadSettingsFromConfig();
		return true;
	}

	bool ReticlePanel::ShouldShow() const
	{
		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
		return weaponInfo.currentConfig != nullptr;
	}

	void ReticlePanel::Render()
	{
		OptimizedScan();

		RenderCurrentReticleInfo();
		ImGui::Spacing();
		RenderTextureSelection();
		ImGui::Spacing();
		RenderReticleAdjustments();
		ImGui::Spacing();

		//if (m_ShowPreview) {
		//	//RenderPreviewSection();
		//	ImGui::Spacing();
		//}

		RenderQuickActions();
	}

	void ReticlePanel::Update()
	{
		// 定期刷新纹理文件列表
		float currentTime = ImGui::GetTime();
		if (currentTime > m_NextScanTime) {
			RefreshReticleTextures();
			m_NextScanTime = currentTime + SCAN_INTERVAL;
		}

		// 实时更新瞄准镜设置到D3DHooks（类似CameraAdjustmentPanel的实时调整）
		if (HasReticleChanges()) {
			ApplyReticleSettingsRealtime();
			UpdatePreviousSettings();
		}
	}

	void ReticlePanel::RenderCurrentReticleInfo()
	{
		RenderSectionHeader(LOC("reticle.current_info"));

		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
		if (!weaponInfo.currentConfig) {
			ImGui::TextColored(m_WarningColor, LOC("reticle.no_config_available"));
			return;
		}

		ImGui::BeginGroup();

		// 显示当前纹理信息
		if (!m_CurrentSettings.texturePath.empty()) {
			ImGui::TextColored(m_SuccessColor, LOC("reticle.current_texture"), m_CurrentSettings.texturePath.c_str());

			// 显示当前设置
			ImGui::Text(LOC("reticle.current_scale"), m_CurrentSettings.scale);
			ImGui::Text(LOC("reticle.current_offset"), m_CurrentSettings.offsetX, m_CurrentSettings.offsetY);

			// 显示文件状态
			std::string fullPath = GetFullTexturePath(m_CurrentSettings.texturePath);
			if (std::filesystem::exists(fullPath)) {
				ImGui::Text(LOC("reticle.status_found"));
				try {
					auto fileSize = std::filesystem::file_size(fullPath);
					ImGui::Text(LOC("reticle.file_size"), fileSize / 1024.0f);
				} catch (...) {
					ImGui::TextColored(m_WarningColor, LOC("reticle.size_unknown"));
				}
			} else {
				ImGui::TextColored(m_ErrorColor, LOC("reticle.status_not_found"));
			}
		} else {
			ImGui::TextColored(m_WarningColor, LOC("reticle.no_texture_selected"));
		}

		// 显示是否有未保存的更改
		if (!isSaved) {
			ImGui::TextColored(m_WarningColor, LOC("status.unsaved_changes"));
		}

		ImGui::EndGroup();
	}

	void ReticlePanel::RenderTextureSelection()
	{
		RenderSectionHeader(LOC("reticle.texture_selection"));

		if (m_AvailableTextures.empty()) {
			ImGui::TextColored(m_WarningColor, LOC("reticle.no_textures_found"));
			if (ImGui::Button(LOC("reticle.scan_textures"))) {
				RefreshReticleTextures();
			}
			return;
		}

		int currentTextureIndex = FindTextureIndex(m_CurrentSettings.texturePath);

		// 搜索过滤器
		ImGui::SetNextItemWidth(-100);
		ImGui::InputTextWithHint("##Search", LOC("reticle.search_placeholder"), &m_SearchFilter);
		ImGui::SameLine();
		if (ImGui::Button(LOC("button.clear"))) {
			m_SearchFilter.clear();
		}

		// 纹理列表
		ImGui::Spacing();
		std::string previewText = currentTextureIndex >= 0 ?
		                              GetTextureDisplayName(m_AvailableTextures[currentTextureIndex], true) :
		                              LOC("reticle.select_texture");

		ImGui::SetNextItemWidth(-100);
		if (ImGui::BeginCombo("##TextureSelect", previewText.c_str())) {
			for (int i = 0; i < m_AvailableTextures.size(); i++) {
				const std::string& fileName = m_AvailableTextures[i];

				// 应用搜索过滤器
				if (!m_SearchFilter.empty()) {
					std::string lowerFileName = fileName;
					std::string lowerFilter = m_SearchFilter;
					std::transform(lowerFileName.begin(), lowerFileName.end(), lowerFileName.begin(), ::tolower);
					std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

					if (lowerFileName.find(lowerFilter) == std::string::npos) {
						continue;
					}
				}

				bool isSelected = (i == currentTextureIndex);
				bool isCurrent = (fileName == m_CurrentSettings.texturePath);

				std::string displayName = GetTextureDisplayName(fileName, isCurrent);

				if (ImGui::Selectable(displayName.c_str(), isSelected)) {
					if (fileName != m_CurrentSettings.texturePath) {
						m_CurrentSettings.texturePath = fileName;
						isSaved = false;
						// 加载预览
						CreateTexturePreview(fileName);

						m_Manager->SetDebugText(fmt::format(fmt::runtime(LOC("reticle.texture_selected")), fileName).c_str());
					}
				}

				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}

				// 悬停预览
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(LOC("tooltip.click_to_select_texture"), fileName.c_str());
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		if (ImGui::Button(LOC("button.refresh"))) {
			RefreshReticleTextures();
		}
	}

	void ReticlePanel::RenderReticleAdjustments()
	{
		RenderSectionHeader(LOC("reticle.adjustments"));

		bool settingsChanged = false;

		// 缩放调整
		ImGui::Text(LOC("reticle.scale"));
		ImGui::SetNextItemWidth(-1);
		if (ImGui::SliderFloat("##Scale", &m_CurrentSettings.scale, MIN_SCALE, MAX_SCALE, "%.4f")) {
			settingsChanged = true;
		}
		RenderHelpTooltip(LOC("tooltip.reticle_scale"));

		ImGui::Spacing();

		// X偏移调整
		ImGui::Text(LOC("reticle.horizontal_offset"));
		ImGui::SetNextItemWidth(-1);
		if (ImGui::SliderFloat("##OffsetX", &m_CurrentSettings.offsetX, MIN_OFFSET, MAX_OFFSET, "%.3f")) {
			settingsChanged = true;
		}
		RenderHelpTooltip(LOC("tooltip.horizontal_offset"));

		ImGui::Spacing();

		// Y偏移调整
		ImGui::Text(LOC("reticle.vertical_offset"));
		ImGui::SetNextItemWidth(-1);
		if (ImGui::SliderFloat("##OffsetY", &m_CurrentSettings.offsetY, MIN_OFFSET, MAX_OFFSET, "%.3f")) {
			settingsChanged = true;
		}
		RenderHelpTooltip(LOC("tooltip.vertical_offset"));

		// 如果设置有变化，标记为未保存
		if (settingsChanged) {
			isSaved = false;
		}

		ImGui::Spacing();

		// 快速重置按钮
		if (ImGui::Button(LOC("reticle.reset_to_center"), ImVec2(-1, 0))) {
			m_CurrentSettings.offsetX = 0.5f;
			m_CurrentSettings.offsetY = 0.5f;
			isSaved = false;
		}
		RenderHelpTooltip(LOC("tooltip.reset_to_center"));
	}

	void ReticlePanel::RenderPreviewSection()
	{
		if (!ImGui::CollapsingHeader(LOC("reticle.texture_preview"), ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		if (m_PreviewTextureID && m_PreviewWidth > 0 && m_PreviewHeight > 0) {
			// 计算预览大小，保持宽高比
			float aspectRatio = (float)m_PreviewWidth / (float)m_PreviewHeight;
			ImVec2 previewSize;

			if (aspectRatio > 1.0f) {
				previewSize.x = PREVIEW_SIZE;
				previewSize.y = PREVIEW_SIZE / aspectRatio;
			} else {
				previewSize.x = PREVIEW_SIZE * aspectRatio;
				previewSize.y = PREVIEW_SIZE;
			}

			// 居中显示预览
			float windowWidth = ImGui::GetContentRegionAvail().x;
			float offsetX = (windowWidth - previewSize.x) * 0.5f;
			if (offsetX > 0) {
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
			}

			ImGui::Image((ImTextureID)m_PreviewTextureID, previewSize);

			// 显示纹理信息
			ImGui::Text(LOC("reticle.dimensions"), m_PreviewWidth, m_PreviewHeight);
			ImGui::Text(LOC("reticle.aspect_ratio"), aspectRatio);
		} else {
			ImGui::TextColored(m_WarningColor, LOC("reticle.no_preview"));
			if (ImGui::Button(LOC("reticle.load_preview"))) {
				CreateTexturePreview(m_CurrentSettings.texturePath);
			}
		}
	}

	void ReticlePanel::RenderQuickActions()
	{
		RenderSectionHeader(LOC("reticle.quick_actions"));

		// 由于实时更新，Apply按钮主要用于加载纹理（如果路径改变了）
		if (ImGui::Button(LOC("reticle.reload_texture"), ImVec2(-1, 0))) {
			if (LoadTexture(m_CurrentSettings.texturePath)) {
				CreateTexturePreview(m_CurrentSettings.texturePath);
				m_Manager->SetDebugText(LOC("reticle.reload_success"));
			} else {
				m_Manager->ShowErrorDialog(LOC("reticle.reload_error_title"), LOC("reticle.reload_error_desc"));
			}
		}
		RenderHelpTooltip(LOC("tooltip.reload_texture"));

		// 保存设置
		if (ImGui::Button(LOC("button.save"), ImVec2(-1, 0))) {
			SaveCurrentSettings();
			isSaved = true;
			m_Manager->SetDebugText(LOC("reticle.settings_saved"));
		}
		RenderHelpTooltip(LOC("tooltip.save_settings"));

		// 重置为默认值
		if (ImGui::Button(LOC("reticle.reset_defaults"), ImVec2(-1, 0))) {
			if (ImGui::GetIO().KeyCtrl) {
				ResetToDefaults();
				isSaved = false;
				m_Manager->SetDebugText(LOC("reticle.reset_success"));
			} else {
				m_Manager->SetDebugText(LOC("reticle.reset_instruction"));
			}
		}
		RenderHelpTooltip(LOC("tooltip.reset_defaults"));

		ImGui::Spacing();

		// 显示实时更新状态
		if (!isSaved) {
			ImGui::TextColored(m_WarningColor, LOC("reticle.realtime_changes"));
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), LOC("reticle.save_instruction"));
		} else {
			ImGui::TextColored(m_SuccessColor, LOC("reticle.all_saved"));
		}

		ImGui::Spacing();

		// 预览控制
		ImGui::Checkbox(LOC("reticle.show_preview"), &m_ShowPreview);
		RenderHelpTooltip(LOC("tooltip.show_preview"));
	}

	bool ReticlePanel::LoadTexture(const std::string& texturePath)
	{
		std::string fullPath = GetFullTexturePath(texturePath);
		auto srv = D3DHooks::LoadAimSRV(fullPath);
		bool success = (srv != nullptr);

		if (success) {
			logger::info("Successfully loaded reticle texture: {}", texturePath);
		} else {
			logger::error("Failed to load reticle texture: {}", texturePath);
		}

		return success;
	}

	bool ReticlePanel::ApplyReticleSettings(const ReticleSettings& settings)
	{
		try {
			// 加载纹理
			if (!LoadTexture(settings.texturePath)) {
				logger::error("Failed to load reticle texture: {}", settings.texturePath);
				return false;
			}

			// 应用设置到D3DHooks
			D3DHooks::UpdateReticleSettings(settings.scale, settings.offsetX, settings.offsetY);

			logger::info("Applied reticle settings - Scale: {:.2f}, Offset: [{:.3f}, {:.3f}]",
				settings.scale, settings.offsetX, settings.offsetY);

			return true;
		} catch (const std::exception& e) {
			logger::error("Error applying reticle settings: {}", e.what());
			return false;
		}
	}

	void ReticlePanel::SaveCurrentSettings()
	{
		try {
			auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
			if (!weaponInfo.currentConfig) {
				logger::warn("No configuration to save reticle settings to");
				return;
			}

			// 创建修改后的配置
			auto modifiedConfig = *weaponInfo.currentConfig;
			modifiedConfig.reticleSettings.customReticlePath = m_CurrentSettings.texturePath;
			modifiedConfig.reticleSettings.scale = m_CurrentSettings.scale;
			modifiedConfig.reticleSettings.offsetX = m_CurrentSettings.offsetX;
			modifiedConfig.reticleSettings.offsetY = m_CurrentSettings.offsetY;

			// 保存配置
			auto dataPersistence = DataPersistence::GetSingleton();
			if (dataPersistence->SaveConfig(modifiedConfig)) {
				dataPersistence->LoadAllConfigs();
				isSaved = true;
				logger::info("Reticle settings saved successfully");
			} else {
				logger::error("Failed to save reticle settings");
			}
		} catch (const std::exception& e) {
			logger::error("Error saving reticle settings: {}", e.what());
		}
	}

	void ReticlePanel::LoadSettingsFromConfig()
	{
		auto weaponInfo = m_Manager->GetCurrentWeaponInfo();
		if (weaponInfo.currentConfig) {
			m_CurrentSettings.texturePath = weaponInfo.currentConfig->reticleSettings.customReticlePath;
			// 从配置加载其他设置
			m_CurrentSettings.scale = weaponInfo.currentConfig->reticleSettings.scale;
			m_CurrentSettings.offsetX = weaponInfo.currentConfig->reticleSettings.offsetX;
			m_CurrentSettings.offsetY = weaponInfo.currentConfig->reticleSettings.offsetY;

			// 创建预览
			CreateTexturePreview(m_CurrentSettings.texturePath);
		}

		// 备份当前设置
		m_BackupSettings = m_CurrentSettings;
		m_PreviousSettings = m_CurrentSettings;  // 初始化前一帧设置
		isSaved = true;
	}

	void ReticlePanel::ResetToDefaults()
	{
		m_CurrentSettings.texturePath = "Test.dds";
		m_CurrentSettings.scale = 1.0f;
		m_CurrentSettings.offsetX = 0.5f;
		m_CurrentSettings.offsetY = 0.5f;

		CreateTexturePreview(m_CurrentSettings.texturePath);
	}

	bool ReticlePanel::CreateTexturePreview(const std::string& texturePath)
	{
		// 释放之前的预览
		ReleaseTexturePreview();

		if (texturePath.empty()) {
			return false;
		}

		try {
			std::string fullPath = GetFullTexturePath(texturePath);
			if (!std::filesystem::exists(fullPath)) {
				logger::warn("Texture file not found: {}", fullPath);
				return false;
			}

			// 使用现有的LoadAimSRV函数加载纹理
			auto srv = D3DHooks::LoadAimSRV(fullPath);
			if (!srv) {
				logger::error("Failed to load texture SRV: {}", texturePath);
				return false;
			}

			// 获取纹理尺寸信息
			ID3D11Resource* resource = nullptr;
			srv->GetResource(&resource);

			if (resource) {
				ID3D11Texture2D* texture = nullptr;
				HRESULT hr = resource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&texture);

				if (SUCCEEDED(hr) && texture) {
					D3D11_TEXTURE2D_DESC desc;
					texture->GetDesc(&desc);

					m_PreviewWidth = static_cast<int>(desc.Width);
					m_PreviewHeight = static_cast<int>(desc.Height);

					texture->Release();
				} else {
					// 如果无法获取尺寸，使用默认值
					m_PreviewWidth = 256;
					m_PreviewHeight = 256;
				}

				resource->Release();
			}

			// 将SRV设置为ImGui纹理ID
			m_PreviewTextureID = static_cast<void*>(srv);

			// 增加引用计数，防止被自动释放
			srv->AddRef();

			logger::info("Texture preview created for: {} ({}x{})", texturePath, m_PreviewWidth, m_PreviewHeight);
			return true;
		} catch (const std::exception& e) {
			logger::error("Error creating texture preview: {}", e.what());
			return false;
		}
	}

	void ReticlePanel::ReleaseTexturePreview()
	{
		if (m_PreviewTextureID) {
			// 释放D3D11 ShaderResourceView
			auto srv = static_cast<ID3D11ShaderResourceView*>(m_PreviewTextureID);
			srv->Release();

			m_PreviewTextureID = nullptr;
			m_PreviewWidth = 0;
			m_PreviewHeight = 0;
		}
	}

	void ReticlePanel::RefreshReticleTextures()
	{
		ScanForTextureFiles();
	}

	void ReticlePanel::ScanForTextureFiles()
	{
		m_AvailableTextures.clear();

		try {
			std::filesystem::path texturePath = std::filesystem::current_path() / "Data" / "Textures" / "TTS" / "Reticle";

			if (!std::filesystem::exists(texturePath)) {
				m_Manager->SetDebugText(LOC("reticle.directory_not_found"));
				return;
			}

			for (const auto& entry : std::filesystem::directory_iterator(texturePath)) {
				if (IsValidTextureFile(entry.path())) {
					std::string fileName = entry.path().filename().string();
					m_AvailableTextures.push_back(fileName);
				}
			}

			// 按文件名排序
			std::sort(m_AvailableTextures.begin(), m_AvailableTextures.end());

			m_TexturesScanned = true;
			m_Manager->SetDebugText(fmt::format(fmt::runtime(LOC("reticle.textures_found")), m_AvailableTextures.size()).c_str());

		} catch (const std::exception& e) {
			m_Manager->SetDebugText(fmt::format(fmt::runtime(LOC("reticle.error_scanning")), e.what()).c_str());
		}
	}

	void ReticlePanel::OptimizedScan()
	{
		float currentTime = ImGui::GetTime();
		if (!m_TexturesScanned || currentTime > m_NextScanTime) {
			ScanForTextureFiles();
			m_NextScanTime = currentTime + SCAN_INTERVAL;
		}
	}

	int ReticlePanel::FindTextureIndex(const std::string& texturePath) const
	{
		auto it = std::find(m_AvailableTextures.begin(), m_AvailableTextures.end(), texturePath);
		return it != m_AvailableTextures.end() ? std::distance(m_AvailableTextures.begin(), it) : -1;
	}

	bool ReticlePanel::IsValidTextureFile(const std::filesystem::path& filePath) const
	{
		std::string extension = filePath.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

		return extension == ".dds" && std::filesystem::file_size(filePath) > 0;
	}

	std::string ReticlePanel::GetTextureDisplayName(const std::string& fileName, bool isCurrent) const
	{
		return isCurrent ? fileName + " " + LOC("reticle.current_suffix") : fileName;
	}

	std::string ReticlePanel::GetFullTexturePath(const std::string& relativePath) const
	{
		return (std::filesystem::current_path() / "Data" / "Textures" / "TTS" / "Reticle" / relativePath).string();
	}

	bool ReticlePanel::AreSettingsEqual(const ReticleSettings& a, const ReticleSettings& b) const
	{
		const float epsilon = 0.001f;
		return a.texturePath == b.texturePath &&
		       std::abs(a.scale - b.scale) < epsilon &&
		       std::abs(a.offsetX - b.offsetX) < epsilon &&
		       std::abs(a.offsetY - b.offsetY) < epsilon;
	}

	bool ReticlePanel::HasReticleChanges() const
	{
		return !AreSettingsEqual(m_CurrentSettings, m_PreviousSettings);
	}

	void ReticlePanel::UpdatePreviousSettings()
	{
		m_PreviousSettings = m_CurrentSettings;
	}

	void ReticlePanel::ApplyReticleSettingsRealtime()
	{
		// 实时应用设置到D3DHooks（类似CameraAdjustmentPanel的ApplyAllAdjustments）
		D3DHooks::UpdateReticleSettings(m_CurrentSettings.scale,
			m_CurrentSettings.offsetX,
			m_CurrentSettings.offsetY);

		// 如果纹理路径发生变化，也要加载新纹理
		if (m_CurrentSettings.texturePath != m_PreviousSettings.texturePath) {
			LoadTexture(m_CurrentSettings.texturePath);
		}
	}
}
