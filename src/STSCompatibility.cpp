#include "STSCompatibility.h"
#include "Utilities.h"

namespace ThroughScope
{
	bool STSCompatibility::HasSTSScope(RE::NiNode* weaponNode)
	{
		if (!weaponNode) return false;

		auto scopeAiming = weaponNode->GetObjectByName("ScopeAiming");
		if (!scopeAiming) return false;

		static bool hasLogged = false;
		if (!hasLogged) {
			logger::info("[STS] Detected STS scope structure");
			hasLogged = true;
		}

		return true;
	}

	void STSCompatibility::HideScopeAimingForRender(RE::NiNode* weaponNode)
	{
		if (!weaponNode) return;
		if (m_isHidingScopeAiming) return;

		auto scopeAiming = weaponNode->GetObjectByName("ScopeAiming");
		if (!scopeAiming) return;

		m_cachedScopeAiming = scopeAiming;
		m_originalScopeAimingCulled = (scopeAiming->flags.flags & 0x1) != 0;

		scopeAiming->SetAppCulled(true);
		m_isHidingScopeAiming = true;
	}

	void STSCompatibility::RestoreScopeAimingAfterRender(RE::NiNode* weaponNode)
	{
		if (!m_isHidingScopeAiming) return;

		RE::NiAVObject* scopeAiming = m_cachedScopeAiming;
		if (!scopeAiming && weaponNode) {
			scopeAiming = weaponNode->GetObjectByName("ScopeAiming");
		}

		if (scopeAiming) {
			scopeAiming->SetAppCulled(m_originalScopeAimingCulled);
		}

		m_isHidingScopeAiming = false;
		m_cachedScopeAiming = nullptr;
	}
}
