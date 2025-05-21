#include "ScopeCamera.h"
#include "Utilities.h"

namespace ThroughScope
{
    // Initialize static members
    RE::NiCamera* ScopeCamera::s_ScopeCamera = nullptr;
    RE::NiCamera* ScopeCamera::s_OriginalCamera = nullptr;
    
    bool ScopeCamera::s_AdjustmentMode = false;
    float ScopeCamera::s_AdjustmentSpeed = DEFAULT_ADJUSTMENT_SPEED;
    float ScopeCamera::s_TargetFOV = DEFAULT_FOV;
    AdjustmentTarget ScopeCamera::s_CurrentAdjustmentTarget = AdjustmentTarget::POSITION;
    int ScopeCamera::s_CurrentAdjustmentAxis = 0;
    
    RE::NiPoint3 ScopeCamera::s_DeltaPos = { 0, 0, 0 };
    RE::NiPoint3 ScopeCamera::s_CachedDeltaPos = { 0, 0, 0 };
    RE::NiMatrix3 ScopeCamera::s_DeltaRot;
    RE::NiMatrix3 ScopeCamera::s_CachedDeltaRot;
    
    bool ScopeCamera::s_OriginalFirstPerson = false;
    bool ScopeCamera::s_OriginalRenderDecals = false;
    bool ScopeCamera::s_IsRenderingForScope = false;

    bool ScopeCamera::Initialize()
    {
        CreateScopeCamera();
        return s_ScopeCamera != nullptr;
    }

    void ScopeCamera::Shutdown()
    {
        if (s_ScopeCamera) {
            if (s_ScopeCamera->DecRefCount() == 0) {
                s_ScopeCamera->DeleteThis();
            }
            s_ScopeCamera = nullptr;
        }
    }

    void ScopeCamera::CreateScopeCamera()
    {
        // Get the player camera
        const auto playerCamera = RE::PlayerCamera::GetSingleton();

        // Create a clone of the player camera for our scope view
        s_ScopeCamera = new RE::NiCamera();
        auto playerCharacter = RE::PlayerCharacter::GetSingleton();
        
        if (playerCharacter && playerCharacter->Get3D()) {
            auto weaponNode = playerCharacter->Get3D()->GetObjectByName("Weapon");
            if (playerCamera->cameraRoot.get()) {
                playerCamera->cameraRoot.get()->AttachChild(s_ScopeCamera, true);
                s_ScopeCamera->viewFrustum = ((RE::NiCamera*)playerCamera->cameraRoot.get())->viewFrustum;
                s_ScopeCamera->port = ((RE::NiCamera*)playerCamera->cameraRoot.get())->port;
				s_TargetFOV = playerCamera->firstPersonFOV - 10; 
                logger::info("Created scope camera successfully");
            } else {
                logger::error("Failed to get camera root node");
            }
        } else {
            logger::error("Player character or 3D not available");
        }
    }

	void ScopeCamera::AdjustPositionFTSNode(float x, float y, float z)
	{
		auto weaponnode = RE::PlayerCharacter::GetSingleton()->Get3D()->GetObjectByName("Weapon")->IsNode();
		auto ftsNode = weaponnode->GetObjectByName("FTSNode");
		if (!ftsNode)
			return;

		ftsNode->local.translate.x += x;
		ftsNode->local.translate.y += y;
		ftsNode->local.translate.z += z;

		if (s_CachedDeltaPos.x != x || s_CachedDeltaPos.y != y || s_CachedDeltaPos.z != z) {
			logger::info("Camera position: [{:.3f}, {:.3f}, {:.3f}]",
				ftsNode->local.translate.x,
				ftsNode->local.translate.y,
				ftsNode->local.translate.z);
			s_CachedDeltaPos = { x, y, z };
		}

		RE::NiUpdateData tempData{};
		tempData.camera = s_ScopeCamera;
		ftsNode->Update(tempData);
	}

    void ScopeCamera::ProcessCameraAdjustment()
    {
        // Handle adjustment mode toggle
        if (GetAsyncKeyState(TOGGLE_ADJUSTMENT_KEY) & 0x1) {
            s_AdjustmentMode = !s_AdjustmentMode;
            logger::info("Camera adjustment mode: {}", s_AdjustmentMode ? "ON" : "OFF");
            return;
        }

        // Skip if not in adjustment mode or scope camera doesn't exist
        if (!s_AdjustmentMode || !s_ScopeCamera || !s_ScopeCamera->parent)
            return;

        // Get keyboard state
        SHORT keyUp = GetAsyncKeyState(VK_UP);
        SHORT keyDown = GetAsyncKeyState(VK_DOWN);
        SHORT keyLeft = GetAsyncKeyState(VK_LEFT);
        SHORT keyRight = GetAsyncKeyState(VK_RIGHT);
        SHORT keyPageUp = GetAsyncKeyState(VK_PRIOR);
        SHORT keyPageDown = GetAsyncKeyState(VK_NEXT);
        SHORT keyIncreaseFOV = GetAsyncKeyState(INCREASE_FOV_KEY);
        SHORT keyDecreaseFOV = GetAsyncKeyState(DECREASE_FOV_KEY);

        // Toggle between position and rotation adjustment
        if (GetAsyncKeyState(TOGGLE_ADJUSTMENT_TARGET_KEY) & 0x1) {
            s_CurrentAdjustmentTarget = (s_CurrentAdjustmentTarget == AdjustmentTarget::POSITION) ? 
                                       AdjustmentTarget::ROTATION : AdjustmentTarget::POSITION;
            logger::info("Adjusting camera {}", 
                (s_CurrentAdjustmentTarget == AdjustmentTarget::POSITION) ? "POSITION" : "ROTATION");
            return;
        }

        // Switch axis
        if (GetAsyncKeyState(CYCLE_ADJUSTMENT_AXIS_KEY) & 0x1) {
            s_CurrentAdjustmentAxis = (s_CurrentAdjustmentAxis + 1) % 3;
            const char* axisNames[] = { "X", "Y", "Z" };
            logger::info("Current axis: {}", axisNames[s_CurrentAdjustmentAxis]);
            return;
        }

        // Adjust speed
        if (GetAsyncKeyState(DECREASE_ADJUSTMENT_SPEED_KEY) & 0x1) {
            s_AdjustmentSpeed /= 2.0f;
            logger::warn("Adjustment speed: {}", s_AdjustmentSpeed);
        }
        if (GetAsyncKeyState(INCREASE_ADJUSTMENT_SPEED_KEY) & 0x1) {
            s_AdjustmentSpeed *= 2.0f;
            logger::warn("Adjustment speed: {}", s_AdjustmentSpeed);
        }

        // Adjust FOV
        if (keyIncreaseFOV & 0x8000)
            s_TargetFOV += 1;
        if (keyDecreaseFOV & 0x8000)
            s_TargetFOV -= 1;

        // Apply adjustments based on target (position or rotation)
        if (s_CurrentAdjustmentTarget == AdjustmentTarget::POSITION) {
            // Reset delta pos
            s_DeltaPos = { 0, 0, 0 };
            
            // X-axis (left/right)
            if (keyRight & 0x8000)
                s_DeltaPos.x += s_AdjustmentSpeed;
            if (keyLeft & 0x8000)
                s_DeltaPos.x -= s_AdjustmentSpeed;

            // Y-axis (up/down)
            if (keyUp & 0x8000)
                s_DeltaPos.y += s_AdjustmentSpeed;
            if (keyDown & 0x8000)
                s_DeltaPos.y -= s_AdjustmentSpeed;

            // Z-axis (page up/down)
            if (keyPageUp & 0x8000)
                s_DeltaPos.z += s_AdjustmentSpeed;
            if (keyPageDown & 0x8000)
                s_DeltaPos.z -= s_AdjustmentSpeed;

            // Apply position changes if any
            if (s_DeltaPos.x != 0.0f || s_DeltaPos.y != 0.0f || s_DeltaPos.z != 0.0f) {
				AdjustPositionFTSNode(s_DeltaPos.x, s_DeltaPos.y, s_DeltaPos.z);
            }
        } else {
            // Adjust rotation on all axes simultaneously
            float xAngle = 0.0f;
            float yAngle = 0.0f;
            float zAngle = 0.0f;

            // X-axis rotation (up/down)
            if (keyPageUp & 0x8000)
                xAngle += s_AdjustmentSpeed * 0.01f;
            if (keyPageDown & 0x8000)
                xAngle -= s_AdjustmentSpeed * 0.01f;

            // Y-axis rotation (left/right)
            if (keyRight & 0x8000)
                yAngle += s_AdjustmentSpeed * 0.01f;
            if (keyLeft & 0x8000)
                yAngle -= s_AdjustmentSpeed * 0.01f;

            // Z-axis rotation
            if (keyUp & 0x8000)
                zAngle += s_AdjustmentSpeed * 0.01f;
            if (keyDown & 0x8000)
                zAngle -= s_AdjustmentSpeed * 0.01f;

            if (xAngle != 0.0f || yAngle != 0.0f || zAngle != 0.0f) {
				AdjustRotationFTSNode(xAngle, yAngle, zAngle);
            }
        }

        // Print current values
        if (GetAsyncKeyState(PRINT_VALUES_KEY) & 0x1) {
            PrintCurrentValues();
        }

        // Reset camera
        if (GetAsyncKeyState(RESET_CAMERA_KEY) & 0x1) {
            ResetCamera();
        }
    }

    void ScopeCamera::AdjustPosition(float x, float y, float z)
    {
        if (!s_ScopeCamera)
            return;
            
        s_ScopeCamera->local.translate.x += x;
        s_ScopeCamera->local.translate.y += y;
        s_ScopeCamera->local.translate.z += z;

        if (s_CachedDeltaPos.x != x || s_CachedDeltaPos.y != y || s_CachedDeltaPos.z != z) {
            logger::info("Camera position: [{:.3f}, {:.3f}, {:.3f}]",
                s_ScopeCamera->local.translate.x,
                s_ScopeCamera->local.translate.y,
                s_ScopeCamera->local.translate.z);
            s_CachedDeltaPos = {x, y, z};
        }

        RE::NiUpdateData tempData{};
        tempData.camera = s_ScopeCamera;
        s_ScopeCamera->Update(tempData);
    }

    void ScopeCamera::AdjustRotation(float x, float y, float z)
    {
        if (!s_ScopeCamera)
            return;
            
        RE::NiMatrix3 rotMat = s_ScopeCamera->local.rotate;
        
        // Create rotation matrices for each axis
        RE::NiMatrix3 xRotMat, yRotMat, zRotMat;
        xRotMat.MakeIdentity();
        yRotMat.MakeIdentity();
        zRotMat.MakeIdentity();
        
        bool hasRotation = false;

        // Apply X rotation if needed
        if (x != 0.0f) {
            xRotMat.FromEulerAnglesXYZ(x, 0.0f, 0.0f);
            hasRotation = true;
        }
        
        // Apply Y rotation if needed
        if (y != 0.0f) {
            yRotMat.FromEulerAnglesXYZ(0.0f, y, 0.0f);
            hasRotation = true;
        }
        
        // Apply Z rotation if needed
        if (z != 0.0f) {
            zRotMat.FromEulerAnglesXYZ(0.0f, 0.0f, z);
            hasRotation = true;
        }
        
        // Apply all rotations in sequence if any changes
        if (hasRotation) {
            // Order matters for rotations, applying X, then Y, then Z
            s_DeltaRot = zRotMat * yRotMat * xRotMat;
            s_ScopeCamera->local.rotate = s_DeltaRot * rotMat;

            // Get Euler angles for display
            float pitch, yaw, roll;
            s_ScopeCamera->local.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);
            logger::info("Camera rotation: [{:.3f}, {:.3f}, {:.3f}]", pitch, yaw, roll);

            RE::NiUpdateData tempData{};
            tempData.camera = s_ScopeCamera;
            s_ScopeCamera->Update(tempData);
        }
    }


	void ScopeCamera::AdjustRotationFTSNode(float x, float y, float z)
	{
		auto weaponnode = RE::PlayerCharacter::GetSingleton()->Get3D()->GetObjectByName("Weapon")->IsNode();
		auto ftsNode = weaponnode->GetObjectByName("FTSNode");
		if (!ftsNode)
			return;

		RE::NiMatrix3 rotMat = ftsNode->local.rotate;

		// Create rotation matrices for each axis
		RE::NiMatrix3 xRotMat, yRotMat, zRotMat;
		xRotMat.MakeIdentity();
		yRotMat.MakeIdentity();
		zRotMat.MakeIdentity();

		bool hasRotation = false;

		// Apply X rotation if needed
		if (x != 0.0f) {
			xRotMat.FromEulerAnglesXYZ(x, 0.0f, 0.0f);
			hasRotation = true;
		}

		// Apply Y rotation if needed
		if (y != 0.0f) {
			yRotMat.FromEulerAnglesXYZ(0.0f, y, 0.0f);
			hasRotation = true;
		}

		// Apply Z rotation if needed
		if (z != 0.0f) {
			zRotMat.FromEulerAnglesXYZ(0.0f, 0.0f, z);
			hasRotation = true;
		}

		// Apply all rotations in sequence if any changes
		if (hasRotation) {
			// Order matters for rotations, applying X, then Y, then Z
			s_DeltaRot = zRotMat * yRotMat * xRotMat;
			ftsNode->local.rotate = s_DeltaRot * rotMat;

			// Get Euler angles for display
			float pitch, yaw, roll;
			ftsNode->local.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);
			logger::info("Camera rotation: [{:.3f}, {:.3f}, {:.3f}]", pitch, yaw, roll);

			RE::NiUpdateData tempData{};
			tempData.camera = s_ScopeCamera;
			ftsNode->Update(tempData);
		}
	}
    void ScopeCamera::PrintCurrentValues()
    {
        if (!s_ScopeCamera)
            return;
            
        float pitch, yaw, roll;
        s_ScopeCamera->local.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);

        logger::info("[Print current values]");
        logger::info("Camera local position: [{:.3f}, {:.3f}, {:.3f}]",
            s_ScopeCamera->local.translate.x,
            s_ScopeCamera->local.translate.y,
            s_ScopeCamera->local.translate.z);
        logger::info("Camera world position: [{:.3f}, {:.3f}, {:.3f}]",
            s_ScopeCamera->world.translate.x,
            s_ScopeCamera->world.translate.y,
            s_ScopeCamera->world.translate.z);
        logger::info("Camera local rotation: [{:.3f}, {:.3f}, {:.3f}]", pitch, yaw, roll);
        s_ScopeCamera->world.rotate.ToEulerAnglesXYZ(pitch, yaw, roll);
        logger::info("Camera world rotation: [{:.3f}, {:.3f}, {:.3f}]", pitch, yaw, roll);
        logger::info("Current FOV: {:.2f}", s_TargetFOV);
        logger::info("[Print End]");
    }

    void ScopeCamera::ResetCamera()
    {
        if (!s_ScopeCamera)
            return;
            
        s_ScopeCamera->local.translate = RE::NiPoint3();
        s_ScopeCamera->local.rotate.MakeIdentity();
        
        RE::NiUpdateData tempData{};
        tempData.camera = s_ScopeCamera;
        s_ScopeCamera->Update(tempData);
        
        logger::info("Camera position/rotation reset");
    }
}
