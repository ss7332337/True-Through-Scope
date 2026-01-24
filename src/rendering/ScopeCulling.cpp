#include "ScopeCulling.h"
#include "RenderUtilities.h"
#include "GlobalTypes.h"
#include <d3d9.h>  // for D3DPERF_BeginEvent / D3DPERF_EndEvent

namespace ThroughScope
{
    // Safety margin for culling (default 0.05 = 5%)
    static float s_CullSafetyMargin = 0.05f;

    ScopedCustomCulling::ScopedCustomCulling(RE::BSCullingProcess* cullingProcess, RE::NiCamera* scopeCamera)
        : m_cullingProcess(cullingProcess)
        , m_originalCustomCullPlanesFlag(false)
        , m_valid(false)
    {
        if (!m_cullingProcess || !scopeCamera) {
            return;
        }

        D3DPERF_BeginEvent(0xFF00AAFF, L"ScopedCustomCulling::Setup");

        // Backup original state
        m_originalCustomCullPlanesFlag = m_cullingProcess->bCustomCullPlanes;
        m_originalPlanes = m_cullingProcess->kCustomCullPlanes;

        // Setup new custom planes
        SetupScopeCullingPlanes(m_cullingProcess, scopeCamera);
        m_valid = true;

        D3DPERF_EndEvent();
    }

    ScopedCustomCulling::~ScopedCustomCulling()
    {
        if (m_cullingProcess && m_valid) {
            D3DPERF_BeginEvent(0xFF00AAFF, L"ScopedCustomCulling::Restore");
            RestoreCullingPlanes(m_cullingProcess, m_originalCustomCullPlanesFlag, m_originalPlanes);
            D3DPERF_EndEvent();
        }
    }

    void SetupScopeCullingPlanes(RE::BSCullingProcess* cullingProcess, RE::NiCamera* scopeCamera)
    {
        if (!cullingProcess || !scopeCamera) {
            return;
        }

        // Calculate frustum planes from scope camera
        // NiFrustumPlanes::Set(NiCamera*) uses RELID: 1361032
        RE::NiFrustumPlanes scopePlanes;
        scopePlanes.Set(scopeCamera);

        // Enable all 6 planes for culling
        scopePlanes.EnableAllPlanes();

        // Set custom planes using engine function
        // NiCullingProcess::SetCustomCullPlanes uses RELID: 778992
        cullingProcess->SetCustomCullPlanes(&scopePlanes);

        // Enable custom planes flag
        cullingProcess->bCustomCullPlanes = true;
    }

    void RestoreCullingPlanes(RE::BSCullingProcess* cullingProcess, 
                               bool originalFlag, 
                               const RE::NiFrustumPlanes& originalPlanes)
    {
        if (!cullingProcess) {
            return;
        }

        // Restore original custom planes flag
        cullingProcess->bCustomCullPlanes = originalFlag;
        
        // Restore original planes if they were in use
        if (originalFlag) {
            cullingProcess->kCustomCullPlanes = originalPlanes;
        }
    }


    // Cached scope frustum planes for the current frame
    static RE::NiFrustumPlanes s_CachedScopePlanes;
    static bool s_CachedScopePlanesValid = false;

    bool TestBoundAgainstFrustum(const RE::NiBound* bound, const RE::NiFrustumPlanes& scopePlanes)
    {
        if (!bound) {
            return true;  // If no bound, assume visible
        }

        // Get sphere center and radius
        const RE::NiPoint3& center = bound->center;
        float radius = bound->fRadius;

        // If radius is 0 or negative, assume visible (point or invalid)
        if (radius <= 0.0f) {
            return true;
        }
        
        int failedPlane = -1;
        float failedDistance = 0;

        // Test against each active plane (matching engine's TestBaseVisibility logic)
        // 引擎使用: distance = dot(normal, center) - constant
        // 如果 distance < -radius, 球体完全在平面外侧 -> 不可见
        for (uint32_t i = 0; i < RE::NiFrustumPlanes::kMax; ++i) {
            if (!scopePlanes.IsPlaneActive(i)) {
                continue;
            }

            const RE::NiPlane& plane = scopePlanes.GetPlane(i);
            
            // 关键修正: 引擎是 **减去** m_fConstant, 不是加
            // 参见 BSCullingProcess::TestBaseVisibility @ 0x1cce790
            float distance = plane.m_kNormal.x * center.x +
                            plane.m_kNormal.y * center.y +
                            plane.m_kNormal.z * center.z -
                            plane.m_fConstant;  // 减去, 不是加!

            // 如果 distance < -radius, 球体完全在平面外侧
            if (distance < -radius) {
                return false;
            }
        }

        // Sphere intersects or is inside the frustum
        return true;
    }

    const RE::NiFrustumPlanes* GetCachedScopeFrustumPlanes()
    {
        if (s_CachedScopePlanesValid) {
            return &s_CachedScopePlanes;
        }
        return nullptr;
    }

    void UpdateCachedScopeFrustumPlanes(RE::NiCamera* scopeCamera)
    {
        if (!scopeCamera) {
            s_CachedScopePlanesValid = false;
            return;
        }

        const RE::NiFrustum& frust = scopeCamera->viewFrustum;
        const RE::NiTransform& world = scopeCamera->world;

        // 获取 ScopeQuad 参数 (UV空间 0-1)
        float centerU = ThroughScope::RenderUtilities::GetScopeQuadCenterU();
        float centerV = ThroughScope::RenderUtilities::GetScopeQuadCenterV();
        float radius = ThroughScope::RenderUtilities::GetScopeQuadRadius();

        // 计算有效视野范围 (UV空间)
        // 使用动态安全余量
        float safetyMargin = radius * s_CullSafetyMargin;
        float effectiveRadius = radius + safetyMargin;

        float uMin = std::max(0.0f, centerU - effectiveRadius);
        float uMax = std::min(1.0f, centerU + effectiveRadius);
        float vMin = std::max(0.0f, centerV - effectiveRadius);
        float vMax = std::min(1.0f, centerV + effectiveRadius);

        // 计算原始视锥体尺寸 (在近平面处)
        float width = frust.right - frust.left;
        float height = frust.top - frust.bottom; // 注意: top > bottom

        // 基于UV插值计算新的视锥体边界
        // U: 0=Left, 1=Right
        float newLeft = frust.left + width * uMin;
        float newRight = frust.left + width * uMax;
        
        // V: 0=Top, 1=Bottom (通常纹理坐标V=0在顶部)
        // 验证假设: View Space Y轴向上(Top), NIF通常如此. UV V向下.
        // newTop对应vMin, newBottom对应vMax
        float newTop = frust.top - height * vMin; // 从顶部向下减少
        float newBottom = frust.top - height * vMax;

        // 从旋转矩阵提取相机轴向（世界空间）
        // Row0 = 前向, Row1 = 上方, Row2 = 右方 (基于之前的调试)
        RE::NiPoint3 forward(world.rotate.entry[0][0], world.rotate.entry[0][1], world.rotate.entry[0][2]);
        RE::NiPoint3 up(world.rotate.entry[1][0], world.rotate.entry[1][1], world.rotate.entry[1][2]);
        RE::NiPoint3 right(world.rotate.entry[2][0], world.rotate.entry[2][1], world.rotate.entry[2][2]);

        // 相机位置
        const RE::NiPoint3& camPos = world.translate;

        // 手动计算 6 个视锥体平面
        // 使用新的 newLeft, newRight, newTop, newBottom

        // Plane 0: 近平面 (normal = forward, pass through camPos + forward * near)
        RE::NiPoint3 nearPoint = camPos + forward * frust.nearPlane;
        s_CachedScopePlanes.m_akCullingPlanes[0].m_kNormal = forward;
        s_CachedScopePlanes.m_akCullingPlanes[0].m_fConstant = forward.x * nearPoint.x + forward.y * nearPoint.y + forward.z * nearPoint.z;

        // Plane 1: 远平面 (normal = -forward, pass through camPos + forward * far)
        RE::NiPoint3 farPoint = camPos + forward * frust.farPlane;
        RE::NiPoint3 negForward(-forward.x, -forward.y, -forward.z);
        s_CachedScopePlanes.m_akCullingPlanes[1].m_kNormal = negForward;
        s_CachedScopePlanes.m_akCullingPlanes[1].m_fConstant = negForward.x * farPoint.x + negForward.y * farPoint.y + negForward.z * farPoint.z;

        // Plane 2: 左平面 - 法线指向右侧（进入视锥体）
        {
            // 左边缘方向: forward*near + right*newLeft
            RE::NiPoint3 leftEdgeDir = forward * frust.nearPlane + right * newLeft;
            // 左平面法线 = leftEdgeDir × up (指向视锥体内部)
            RE::NiPoint3 leftNormal;
            leftNormal.x = leftEdgeDir.y * up.z - leftEdgeDir.z * up.y;
            leftNormal.y = leftEdgeDir.z * up.x - leftEdgeDir.x * up.z;
            leftNormal.z = leftEdgeDir.x * up.y - leftEdgeDir.y * up.x;
            // 归一化
            float len = sqrtf(leftNormal.x * leftNormal.x + leftNormal.y * leftNormal.y + leftNormal.z * leftNormal.z);
            if (len > 0.0001f) {
                leftNormal.x /= len; leftNormal.y /= len; leftNormal.z /= len;
            }
            s_CachedScopePlanes.m_akCullingPlanes[2].m_kNormal = leftNormal;
            s_CachedScopePlanes.m_akCullingPlanes[2].m_fConstant = leftNormal.x * camPos.x + leftNormal.y * camPos.y + leftNormal.z * camPos.z;
        }

        // Plane 3: 右平面 - 法线指向左侧（进入视锥体）
        {
            // 右边缘方向: forward*near + right*newRight
            RE::NiPoint3 rightEdgeDir = forward * frust.nearPlane + right * newRight;
            // 右平面法线 = up × rightEdgeDir
            RE::NiPoint3 rightNormal;
            rightNormal.x = up.y * rightEdgeDir.z - up.z * rightEdgeDir.y;
            rightNormal.y = up.z * rightEdgeDir.x - up.x * rightEdgeDir.z;
            rightNormal.z = up.x * rightEdgeDir.y - up.y * rightEdgeDir.x;
            float len = sqrtf(rightNormal.x * rightNormal.x + rightNormal.y * rightNormal.y + rightNormal.z * rightNormal.z);
            if (len > 0.0001f) {
                rightNormal.x /= len; rightNormal.y /= len; rightNormal.z /= len;
            }
            s_CachedScopePlanes.m_akCullingPlanes[3].m_kNormal = rightNormal;
            s_CachedScopePlanes.m_akCullingPlanes[3].m_fConstant = rightNormal.x * camPos.x + rightNormal.y * camPos.y + rightNormal.z * camPos.z;
        }

        // Plane 4: 上平面 - 法线指向下方（进入视锥体）
        {
            // 上边缘方向: forward*near + up*newTop
            RE::NiPoint3 topEdgeDir = forward * frust.nearPlane + up * newTop;
            // 上平面法线 = topEdgeDir × right (指向视锥体内部，即向下)
            RE::NiPoint3 topNormal;
            topNormal.x = topEdgeDir.y * right.z - topEdgeDir.z * right.y;
            topNormal.y = topEdgeDir.z * right.x - topEdgeDir.x * right.z;
            topNormal.z = topEdgeDir.x * right.y - topEdgeDir.y * right.x;
            float len = sqrtf(topNormal.x * topNormal.x + topNormal.y * topNormal.y + topNormal.z * topNormal.z);
            if (len > 0.0001f) {
                topNormal.x /= len; topNormal.y /= len; topNormal.z /= len;
            }
            s_CachedScopePlanes.m_akCullingPlanes[4].m_kNormal = topNormal;
            s_CachedScopePlanes.m_akCullingPlanes[4].m_fConstant = topNormal.x * camPos.x + topNormal.y * camPos.y + topNormal.z * camPos.z;
        }

        // Plane 5: 下平面 - 法线指向上方（进入视锥体）
        {
            // 下边缘方向: forward*near + up*newBottom
            RE::NiPoint3 bottomEdgeDir = forward * frust.nearPlane + up * newBottom;
            // 下平面法线 = right × bottomEdgeDir (指向视锥体内部，即向上)
            RE::NiPoint3 bottomNormal;
            bottomNormal.x = right.y * bottomEdgeDir.z - right.z * bottomEdgeDir.y;
            bottomNormal.y = right.z * bottomEdgeDir.x - right.x * bottomEdgeDir.z;
            bottomNormal.z = right.x * bottomEdgeDir.y - right.y * bottomEdgeDir.x;
            float len = sqrtf(bottomNormal.x * bottomNormal.x + bottomNormal.y * bottomNormal.y + bottomNormal.z * bottomNormal.z);
            if (len > 0.0001f) {
                bottomNormal.x /= len; bottomNormal.y /= len; bottomNormal.z /= len;
            }
            s_CachedScopePlanes.m_akCullingPlanes[5].m_kNormal = bottomNormal;
            s_CachedScopePlanes.m_akCullingPlanes[5].m_fConstant = bottomNormal.x * camPos.x + bottomNormal.y * camPos.y + bottomNormal.z * camPos.z;
        }

        s_CachedScopePlanes.m_uiActivePlanes = 0x3F;  // All 6 planes active
        s_CachedScopePlanesValid = true;
    }

    void InvalidateCachedScopeFrustumPlanes()
    {
        s_CachedScopePlanesValid = false;
    }

    // ========== Debug Stats Implementation ==========

    // 裁剪统计计数器
    static std::atomic<uint32_t> s_ScopeCullTested{ 0 };
    static std::atomic<uint32_t> s_ScopeCullPassed{ 0 };
    static std::atomic<uint32_t> s_ScopeCullFiltered{ 0 };
    
    // UI显示用的上一帧统计数据
    static uint32_t s_LastFrameTested = 0;
    static uint32_t s_LastFramePassed = 0;
    static uint32_t s_LastFrameFiltered = 0;

    void IncrementCullingTested() { s_ScopeCullTested++; }
    void IncrementCullingPassed() { s_ScopeCullPassed++; }
    void IncrementCullingFiltered() { s_ScopeCullFiltered++; }

    void GetAndResetCullingStats(uint32_t& tested, uint32_t& passed, uint32_t& filtered)
    {
        tested = s_ScopeCullTested.exchange(0);
        passed = s_ScopeCullPassed.exchange(0);
        filtered = s_ScopeCullFiltered.exchange(0);
        
        // 更新上一帧数据供UI显示
        s_LastFrameTested = tested;
        s_LastFramePassed = passed;
        s_LastFrameFiltered = filtered;
    }
    
    void GetLastFrameCullingStats(uint32_t& tested, uint32_t& passed, uint32_t& filtered)
    {
        tested = s_LastFrameTested;
        passed = s_LastFramePassed;
        filtered = s_LastFrameFiltered;
    }

    void SetCullingSafetyMargin(float margin)
    {
        s_CullSafetyMargin = margin;
    }

    float GetCullingSafetyMargin()
    {
        return s_CullSafetyMargin;
    }

    // ========== Shadow Caster Range ==========
    
    static float s_ShadowCasterRange = 5500.0f;

    void SetShadowCasterRange(float range)
    {
        s_ShadowCasterRange = range;
    }

    float GetShadowCasterRange()
    {
        return s_ShadowCasterRange;
    }
}
