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
	RE::NiPoint3 ScopeCamera::s_DeltaScale = { 1.0f, 1.0f, 1.0f };        // Initialize to 1.0 for neutral scale
	RE::NiPoint3 ScopeCamera::s_CachedDeltaScale = { 1.0f, 1.0f, 1.0f };  // Initialize to 1.0 for neutral scale

	float ScopeCamera::minFov = 1;
	float ScopeCamera::maxFov = 100;
    
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
