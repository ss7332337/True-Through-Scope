#include "ImGuiManager.h"
#include <Utilities.h>
#include <NiFLoader.h>

namespace ThroughScope
{
    ImGuiManager* ImGuiManager::GetSingleton()
    {
        static ImGuiManager instance;
        return &instance;
    }
    
    bool ImGuiManager::Initialize()
	{
		logger::info("Initializing ImGui...");

		// Get the D3D11 device and context from the game's renderer
		auto rendererData = RE::BSGraphics::RendererData::GetSingleton();
		if (!rendererData || !rendererData->device || !rendererData->context) {
			logger::error("Failed to get D3D11 device or context");
			return false;
		}
		auto swapchain = (IDXGISwapChain*)static_cast<void*>(rendererData->renderWindow->swapChain);

		DXGI_SWAP_CHAIN_DESC sd;
		swapchain->GetDesc(&sd);

		auto hwnd = sd.OutputWindow;
		if (!hwnd) {
			logger::error("Failed to get window handle");
			return false;
		}

		// Initialize ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable keyboard controls
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable docking

		// Set up ImGui style
		ImGui::StyleColorsDark();

		// Customize ImGui style
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 6.0f;
		style.FrameRounding = 3.0f;
		style.PopupRounding = 3.0f;
		style.ScrollbarRounding = 2.0f;
		style.GrabRounding = 3.0f;
		style.TabRounding = 3.0f;
		style.ChildRounding = 3.0f;
		style.FramePadding = ImVec2(6.0f, 4.0f);
		style.ItemSpacing = ImVec2(8.0f, 4.0f);
		style.Alpha = 0.95f;

		// Set accent colors
		style.Colors[ImGuiCol_Header] = m_AccentColor;
		style.Colors[ImGuiCol_HeaderHovered] = ImVec4(m_AccentColor.x + 0.1f, m_AccentColor.y + 0.1f, m_AccentColor.z + 0.1f, m_AccentColor.w);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(m_AccentColor.x + 0.2f, m_AccentColor.y + 0.2f, m_AccentColor.z + 0.2f, m_AccentColor.w);
		style.Colors[ImGuiCol_Button] = m_AccentColor;
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(m_AccentColor.x + 0.1f, m_AccentColor.y + 0.1f, m_AccentColor.z + 0.1f, m_AccentColor.w);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(m_AccentColor.x + 0.2f, m_AccentColor.y + 0.2f, m_AccentColor.z + 0.2f, m_AccentColor.w);

		// Initialize ImGui platform/renderer backends
		if (!ImGui_ImplWin32_Init(hwnd)) {
			logger::error("Failed to initialize ImGui Win32 backend");
			return false;
		}

		if (!ImGui_ImplDX11_Init((ID3D11Device*)rendererData->device, (ID3D11DeviceContext*)rendererData->context)) {
			logger::error("Failed to initialize ImGui DX11 backend");
			ImGui_ImplWin32_Shutdown();
			return false;
		}

		m_EnableRendering = D3DHooks::IsEnableRender();

		m_Initialized = true;
		logger::info("ImGui initialized successfully");
		return true;
	}
    
    void ImGuiManager::Shutdown()
    {
        if (!m_Initialized)
            return;
            
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        
        m_Initialized = false;
        logger::info("ImGui shut down");
    }
    
    void ImGuiManager::Render()
    {
        if (!m_Initialized || !m_MenuOpen)
            return;
            
       // Start ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// Main menu
		RenderMainMenu();

		// Finish ImGui frame
		ImGui::Render();

		// 获取ImGui绘制数据并渲染
		ImDrawData* draw_data = ImGui::GetDrawData();
		if (draw_data && draw_data->Valid && draw_data->CmdListsCount > 0) {
			// 确保在正确的渲染上下文中
			ImGui_ImplDX11_RenderDrawData(draw_data);
		}
    }
    
    void ImGuiManager::Update()
	{
		// Toggle menu on/off when F2 is pressed
		if (GetAsyncKeyState(VK_F2) & 0x1) {
			ToggleMenu();
			auto mc = RE::MenuCursor::GetSingleton();
			REX::W32::ShowCursor(m_MenuOpen);

			auto input = (RE::BSInputDeviceManager::GetSingleton());
			RE::ControlMap::GetSingleton()->ignoreKeyboardMouse = m_MenuOpen;
			logger::info("Menu toggled: {}", m_MenuOpen ? "open" : "closed");
		}

		// Real-time adjustment updates
		if (m_MenuOpen && m_Initialized && m_RealTimeAdjustment) {
			// Check for position changes
			if (m_DeltaPosX != m_PrevDeltaPosX || m_DeltaPosY != m_PrevDeltaPosY || m_DeltaPosZ != m_PrevDeltaPosZ) {
				ApplyPositionAdjustment();
				m_PrevDeltaPosX = m_DeltaPosX;
				m_PrevDeltaPosY = m_DeltaPosY;
				m_PrevDeltaPosZ = m_DeltaPosZ;
			}

			// Check for rotation changes
			if (m_DeltaRot[0] != m_PrevDeltaRot[0] || m_DeltaRot[1] != m_PrevDeltaRot[1] || m_DeltaRot[2] != m_PrevDeltaRot[2]) {
				ApplyRotationAdjustment();
				m_PrevDeltaRot[0] = m_DeltaRot[0];
				m_PrevDeltaRot[1] = m_DeltaRot[1];
				m_PrevDeltaRot[2] = m_DeltaRot[2];
			}

			// Check for scale changes
			if (m_DeltaScale != m_PrevDeltaScale) {
				ApplyScaleAdjustment();
				m_PrevDeltaScale = m_DeltaScale;
			}
		}
	}

	RE::NiAVObject* ImGuiManager::GetTTSNode()
	{
		try {
			auto playerCharacter = RE::PlayerCharacter::GetSingleton();
			if (playerCharacter && playerCharacter->Get3D()) {
				auto weaponNode = playerCharacter->Get3D()->GetObjectByName("Weapon");
				if (weaponNode && weaponNode->IsNode()) {
					auto weaponNiNode = static_cast<RE::NiNode*>(weaponNode);
					return weaponNiNode->GetObjectByName("TTSNode");
				}
			}
		} catch (...) {
			// Ignore exceptions
		}
		return nullptr;
	}

	void ImGuiManager::ApplyPositionAdjustment()
	{
		try {
			auto ttsNode = GetTTSNode();
			if (ttsNode) {
				ttsNode->local.translate.x = m_DeltaPosX;
				ttsNode->local.translate.y = m_DeltaPosY;
				ttsNode->local.translate.z = m_DeltaPosZ;

				RE::NiUpdateData tempData{};
				tempData.camera = ScopeCamera::GetScopeCamera();
				if (tempData.camera) {
					ttsNode->Update(tempData);
				}

				snprintf(m_DebugText, sizeof(m_DebugText),
					"Position: [%.3f, %.3f, %.3f]", m_DeltaPosX, m_DeltaPosY, m_DeltaPosZ);
			} else {
				snprintf(m_DebugText, sizeof(m_DebugText), "TTSNode not found!");
			}
		} catch (const std::exception& e) {
			snprintf(m_DebugText, sizeof(m_DebugText), "Exception in ApplyPosition: %s", e.what());
		} catch (...) {
			snprintf(m_DebugText, sizeof(m_DebugText), "Unknown exception in ApplyPosition!");
		}
	}

	void ImGuiManager::ApplyRotationAdjustment()
	{
		try {
			auto ttsNode = GetTTSNode();
			if (ttsNode) {
				// Convert degrees to radians for rotation
				float pitch = m_DeltaRot[0] * 0.01745329251f;  // PI/180
				float yaw = m_DeltaRot[1] * 0.01745329251f;
				float roll = m_DeltaRot[2] * 0.01745329251f;

				RE::NiMatrix3 rotMat;
				rotMat.MakeIdentity();
				rotMat.FromEulerAnglesXYZ(pitch, yaw, roll);

				ttsNode->local.rotate = rotMat;

				RE::NiUpdateData tempData{};
				tempData.camera = ScopeCamera::GetScopeCamera();
				if (tempData.camera) {
					ttsNode->Update(tempData);
				}

				snprintf(m_DebugText, sizeof(m_DebugText),
					"Rotation: [%.1f, %.1f, %.1f] degrees", m_DeltaRot[0], m_DeltaRot[1], m_DeltaRot[2]);
			} else {
				snprintf(m_DebugText, sizeof(m_DebugText), "TTSNode not found!");
			}
		} catch (const std::exception& e) {
			snprintf(m_DebugText, sizeof(m_DebugText), "Exception in ApplyRotation: %s", e.what());
		} catch (...) {
			snprintf(m_DebugText, sizeof(m_DebugText), "Unknown exception in ApplyRotation!");
		}
	}

	void ImGuiManager::ApplyScaleAdjustment()
	{
		try {
			auto ttsNode = GetTTSNode();
			if (ttsNode) {
				ttsNode->local.scale = m_DeltaScale;  // Uniform scale
				// If you want non-uniform scaling, you'd need to modify the local transform matrix manually
				// For now, we'll use uniform scaling

				RE::NiUpdateData tempData{};
				tempData.camera = ScopeCamera::GetScopeCamera();
				if (tempData.camera) {
					ttsNode->Update(tempData);
				}

				snprintf(m_DebugText, sizeof(m_DebugText),
					"Scale: [%.3f]", m_DeltaScale);
			} else {
				snprintf(m_DebugText, sizeof(m_DebugText), "TTSNode not found!");
			}
		} catch (const std::exception& e) {
			snprintf(m_DebugText, sizeof(m_DebugText), "Exception in ApplyScale: %s", e.what());
		} catch (...) {
			snprintf(m_DebugText, sizeof(m_DebugText), "Unknown exception in ApplyScale!");
		}
	}

	void ImGuiManager::ResetAllAdjustments()
	{
		m_DeltaPosX = 0.0f;
		m_DeltaPosY = 0.0f;
		m_DeltaPosZ = 0.0f;
		m_DeltaRot[0] = 0.0f;
		m_DeltaRot[1] = 0.0f;
		m_DeltaRot[2] = 0.0f;
		m_DeltaScale = 1.0f;

		// Apply the reset values immediately if real-time is enabled
		if (m_RealTimeAdjustment) {
			ApplyPositionAdjustment();
			ApplyRotationAdjustment();
			ApplyScaleAdjustment();
		}

		snprintf(m_DebugText, sizeof(m_DebugText), "All adjustments reset!");
	}
    
    void ImGuiManager::RenderMainMenu()
    {
        const float menuWidth = 400.0f;
        const float menuHeight = 600.0f;
        
        // Center the window on the screen
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        ImGui::SetNextWindowPos(ImVec2(center.x - menuWidth/2, center.y - menuHeight/2), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(menuWidth, menuHeight), ImGuiCond_FirstUseEver);
        
        if (ImGui::Begin("TrueThroughScope Settings", &m_MenuOpen, ImGuiWindowFlags_MenuBar)) {
            // Menu bar
            if (ImGui::BeginMenuBar()) {
                if (ImGui::MenuItem("About")) {
                    // Implement about dialog if needed
                }
                ImGui::EndMenuBar();
            }
            
            // Tabs
            ImGui::BeginTabBar("MainTabs");
            
            if (ImGui::BeginTabItem("Camera Adjustment")) 
			{
				auto ttsNode = GetTTSNode();
				if (ttsNode) {
					m_DeltaPosX = ttsNode->local.translate.x;
					m_DeltaPosY = ttsNode->local.translate.y;
					m_DeltaPosZ = ttsNode->local.translate.z;
					RE::NiPoint3 ttsNodeRot;
					ttsNode->local.rotate.ToEulerAnglesXYZ(ttsNodeRot);
					m_DeltaRot[0] = ttsNodeRot.x;
					m_DeltaRot[1] = ttsNodeRot.y;
					m_DeltaRot[2] = ttsNodeRot.z;
					m_DeltaScale = ttsNode->local.scale;

					RenderCameraAdjustmentPanel();
					ImGui::EndTabItem();
				} else
					ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("Rendering")) {
                RenderRenderingPanel();
                ImGui::EndTabItem();
            }
            
            if (ImGui::BeginTabItem("Debug")) {
                RenderDebugPanel();
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        ImGui::End();
    }
    
    void ImGuiManager::RenderCameraAdjustmentPanel()
	{
		ImGui::TextColored(m_AccentColor, "Scope Camera Position Adjustment");
		ImGui::Separator();

		// Position sliders with real-time feedback
		ImGui::Text("Position Adjustments");
		ImGui::SliderFloat("X Position", &m_DeltaPosX, -50.0f, 50.0f, "%.3f");
		ImGui::SliderFloat("Y Position", &m_DeltaPosY, -50.0f, 50.0f, "%.3f");
		ImGui::SliderFloat("Z Position", &m_DeltaPosZ, -50.0f, 50.0f, "%.3f");

		// Fine adjustment buttons
		ImGui::Text("Fine Position Adjustments:");
		if (ImGui::Button("X-0.1")) {
			m_DeltaPosX -= 0.1f;
		}
		ImGui::SameLine();
		if (ImGui::Button("X+0.1")) {
			m_DeltaPosX += 0.1f;
		}
		ImGui::SameLine();
		if (ImGui::Button("Y-0.1")) {
			m_DeltaPosY -= 0.1f;
		}
		ImGui::SameLine();
		if (ImGui::Button("Y+0.1")) {
			m_DeltaPosY += 0.1f;
		}
		ImGui::SameLine();
		if (ImGui::Button("Z-0.1")) {
			m_DeltaPosZ -= 0.1f;
		}
		ImGui::SameLine();
		if (ImGui::Button("Z+0.1")) {
			m_DeltaPosZ += 0.1f;
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored(m_AccentColor, "Scope Camera Rotation Adjustment");

		// Rotation sliders (using degrees for more intuitive adjustments)
		ImGui::Text("Rotation Adjustments (Degrees)");
		ImGui::SliderFloat("Pitch (X)", &m_DeltaRot[0], -180.0f, 180.0f, "%.1f");
		ImGui::SliderFloat("Yaw (Y)", &m_DeltaRot[1], -180.0f, 180.0f, "%.1f");
		ImGui::SliderFloat("Roll (Z)", &m_DeltaRot[2], -180.0f, 180.0f, "%.1f");

		// Fine rotation adjustment buttons
		ImGui::Text("Fine Rotation Adjustments:");
		if (ImGui::Button("Pitch-1")) {
			m_DeltaRot[0] -= 1.0f;
		}
		ImGui::SameLine();
		if (ImGui::Button("Pitch+1")) {
			m_DeltaRot[0] += 1.0f;
		}
		ImGui::SameLine();
		if (ImGui::Button("Yaw-1")) {
			m_DeltaRot[1] -= 1.0f;
		}
		ImGui::SameLine();
		if (ImGui::Button("Yaw+1")) {
			m_DeltaRot[1] += 1.0f;
		}
		ImGui::SameLine();
		if (ImGui::Button("Roll-1")) {
			m_DeltaRot[2] -= 1.0f;
		}
		ImGui::SameLine();
		if (ImGui::Button("Roll+1")) {
			m_DeltaRot[2] += 1.0f;
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored(m_AccentColor, "Scope Camera Scale Adjustment");

		// Scale sliders
		ImGui::Text("Scale Adjustments");
		ImGui::SliderFloat("Scale", &m_DeltaScale, 0.1f, 10.0f, "%.3f");

		// Fine scale adjustment buttons
		ImGui::Text("Fine Scale Adjustments:");
		if (ImGui::Button("-0.1")) {
			m_DeltaScale -= 0.1f;
		}

		ImGui::SameLine();
		if (ImGui::Button("+0.1")) {
			m_DeltaScale += 0.1f;
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored(m_AccentColor, "Field of View (FOV) Adjustment");

		ImGui::Spacing();
		ImGui::Separator();

		// Reset and action buttons
		if (ImGui::Button("Reset All Adjustments")) {
			ResetAllAdjustments();
		}

		ImGui::SameLine();

		// Display current debug text
		ImGui::Spacing();
		if (strlen(m_DebugText) > 0) {
			ImGui::TextWrapped("%s", m_DebugText);
		}
	}
    
	void ImGuiManager::RenderRenderingPanel()
	{
		ImGui::TextColored(m_AccentColor, "Scope Rendering Options");
		ImGui::Separator();

		// Enable/disable rendering
		if (ImGui::Checkbox("Enable Scope Rendering", &m_EnableRendering)) {
			D3DHooks::SetEnableRender(m_EnableRendering);
			snprintf(m_DebugText, sizeof(m_DebugText),
				"Scope rendering %s", m_EnableRendering ? "enabled" : "disabled");
		}

		ImGui::TextWrapped(
			"When enabled, the mod will render a second camera view for the scope texture. "
			"This creates a realistic scope effect where the image in the scope shows "
			"what you would actually see through a real scope.");

		// Add more rendering options here if needed
		ImGui::Spacing();
		ImGui::Separator();

		// Display current debug text
		if (strlen(m_DebugText) > 0) {
			ImGui::TextWrapped("%s", m_DebugText);
		}
	}
    
	void ImGuiManager::RenderDebugPanel()
	{
		ImGui::TextColored(m_AccentColor, "Debug Information");
		ImGui::Separator();

		// Display current camera values
		auto scopeCamera = ScopeCamera::GetScopeCamera();
		if (scopeCamera) {
			float pitch, yaw, roll;
			scopeCamera->local.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);

			ImGui::Text("Scope Camera Information:");
			ImGui::BulletText("Local Position: [%.3f, %.3f, %.3f]",
				scopeCamera->local.translate.x,
				scopeCamera->local.translate.y,
				scopeCamera->local.translate.z);

			ImGui::BulletText("World Position: [%.3f, %.3f, %.3f]",
				scopeCamera->world.translate.x,
				scopeCamera->world.translate.y,
				scopeCamera->world.translate.z);

			ImGui::BulletText("Local Rotation: [%.3f, %.3f, %.3f]",
				pitch, yaw, roll);

			scopeCamera->world.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);
			ImGui::BulletText("World Rotation: [%.3f, %.3f, %.3f]",
				pitch, yaw, roll);

			ImGui::BulletText("Current FOV: %.2f", ScopeCamera::GetTargetFOV());
		} else {
			ImGui::TextColored(m_WarningColor, "Scope camera not available");
		}

		ImGui::Spacing();
		ImGui::Separator();

		// FTS Node information
		auto ttsNode = GetTTSNode();
		ImGui::Text("TTSNode Information:");
		if (ttsNode) {
			float pitch, yaw, roll;
			ttsNode->local.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);

			ImGui::BulletText("Local Position: [%.3f, %.3f, %.3f]",
				ttsNode->local.translate.x,
				ttsNode->local.translate.y,
				ttsNode->local.translate.z);

			ImGui::BulletText("World Position: [%.3f, %.3f, %.3f]",
				ttsNode->world.translate.x,
				ttsNode->world.translate.y,
				ttsNode->world.translate.z);

			ImGui::BulletText("Local Rotation: [%.3f, %.3f, %.3f]",
				pitch, yaw, roll);

			ttsNode->world.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);
			ImGui::BulletText("World Rotation: [%.3f, %.3f, %.3f]",
				pitch, yaw, roll);

			ImGui::BulletText("Local Scale: %.3f", ttsNode->local.scale);
		} else {
			ImGui::TextColored(m_WarningColor, "TTSNode not found");
		}

		ImGui::Spacing();
		ImGui::Separator();

		// Current adjustment values
		ImGui::Text("Current Adjustment Values:");
		ImGui::BulletText("Position Delta: [%.3f, %.3f, %.3f]", m_DeltaPosX, m_DeltaPosY, m_DeltaPosZ);
		ImGui::BulletText("Rotation Delta: [%.1f, %.1f, %.1f] degrees", m_DeltaRot[0], m_DeltaRot[1], m_DeltaRot[2]);
		ImGui::BulletText("Scale Delta: [%.3f]", m_DeltaScale);

		ImGui::Spacing();
		ImGui::Separator();

		// Status information
		ImGui::Text("Status Information:");
		ImGui::BulletText("Rendering Enabled: %s", D3DHooks::IsEnableRender() ? "Yes" : "No");
		ImGui::BulletText("Is Forward Stage: %s", D3DHooks::GetForwardStage() ? "Yes" : "No");
		ImGui::BulletText("Is Rendering For Scope: %s", ScopeCamera::IsRenderingForScope() ? "Yes" : "No");
		ImGui::BulletText("Real-time Adjustment: %s", m_RealTimeAdjustment ? "Enabled" : "Disabled");

		ImGui::Spacing();
		ImGui::Separator();

		// Action buttons
		if (ImGui::Button("Print Node Hierarchy")) {
			auto weaponnode = RE::PlayerCharacter::GetSingleton()->Get3D()->GetObjectByName("Weapon");
			RE::NiAVObject* existingNode = weaponnode->GetObjectByName("ScopeNode");
			ThroughScope::Utilities::PrintNodeHierarchy(existingNode);
			snprintf(m_DebugText, sizeof(m_DebugText), "Node hierarchy printed to log");
		}

		ImGui::SameLine();

		if (ImGui::Button("Load Template NIF")) {
			// This is from the hkPCUpdateMainThread function that loads the NIF template
			NIFLoader* nifloader = new NIFLoader();
			nifloader->LoadNIF("TTSTemplate_Circle.nif");
			snprintf(m_DebugText, sizeof(m_DebugText), "Template NIF loaded");
		}

		ImGui::SameLine();

		if (ImGui::Button("Refresh TTSNode")) {
			auto node = GetTTSNode();
			if (node) {
				snprintf(m_DebugText, sizeof(m_DebugText), "TTSNode found and refreshed");
			} else {
				snprintf(m_DebugText, sizeof(m_DebugText), "TTSNode not found!");
			}
		}

		// Copy current values to clipboard (useful for saving configurations)
		if (ImGui::Button("Copy Values to Clipboard")) {
			char clipboardText[512];
			snprintf(clipboardText, sizeof(clipboardText),
				"Position: [%.3f, %.3f, %.3f]\n"
				"Rotation: [%.1f, %.1f, %.1f]\n"
				"Scale: [%.3f]\n",
				m_DeltaPosX, m_DeltaPosY, m_DeltaPosZ,
				m_DeltaRot[0], m_DeltaRot[1], m_DeltaRot[2],
				m_DeltaScale);

			if (::OpenClipboard(nullptr)) {
				EmptyClipboard();
				HGLOBAL hClipboardData = GlobalAlloc(GMEM_DDESHARE, strlen(clipboardText) + 1);
				if (hClipboardData) {
					char* pchData = (char*)GlobalLock(hClipboardData);
					strcpy_s(pchData, strlen(clipboardText) + 1, clipboardText);
					GlobalUnlock(hClipboardData);
					SetClipboardData(CF_TEXT, hClipboardData);
				}
				CloseClipboard();
				snprintf(m_DebugText, sizeof(m_DebugText), "Values copied to clipboard!");
			} else {
				snprintf(m_DebugText, sizeof(m_DebugText), "Failed to access clipboard!");
			}
		}

		// Display current debug text
		ImGui::Spacing();
		if (strlen(m_DebugText) > 0) {
			ImGui::TextWrapped("%s", m_DebugText);
		}
	}
    
}
