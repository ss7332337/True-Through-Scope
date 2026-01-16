#pragma once

namespace ThroughScope
{
	/**
	 * @brief STS (See Through Scopes) Mod Compatibility
	 * 
	 * Temporarily hides ScopeAiming node during TTS scope rendering
	 * to prevent double parallax/reticle effects.
	 */
	class STSCompatibility
	{
	public:
		static STSCompatibility* GetSingleton()
		{
			static STSCompatibility instance;
			return &instance;
		}

		bool HasSTSScope(RE::NiNode* weaponNode);
		void HideScopeAimingForRender(RE::NiNode* weaponNode);
		void RestoreScopeAimingAfterRender(RE::NiNode* weaponNode);

		bool IsHidingScopeAiming() const { return m_isHidingScopeAiming; }

	private:
		STSCompatibility() = default;
		~STSCompatibility() = default;
		STSCompatibility(const STSCompatibility&) = delete;
		STSCompatibility& operator=(const STSCompatibility&) = delete;

		RE::NiAVObject* m_cachedScopeAiming = nullptr;
		bool m_isHidingScopeAiming = false;
		bool m_originalScopeAimingCulled = false;
	};
}
