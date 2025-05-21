#pragma once
#include <vector>
#include <iostream>
#include <string>
#include <memory>



class NIFLoader
{
public:
	NIFLoader() = default;
	~NIFLoader() = default;

	bool LoadNIF(const char* filePath);
	bool LoadNIFLowLevel(const char* filePath);

private:
	void ProcessNiNode(RE::NiNode* node);
	void ProcessBSTriShape(RE::BSTriShape* triShape);
	void ProcessBSEffectShaderProperty(RE::BSEffectShaderProperty* property);
	void ProcessNiAlphaProperty(RE::NiAlphaProperty* property);
};

class BSDynamicTriangleBuilder
{
public:
	BSDynamicTriangleBuilder(RE::NiNode* a_parentNode);
	~BSDynamicTriangleBuilder();
	void SetParentNode(RE::NiNode* a_parentNode);
	void SetDynamicFlags(uint32_t a_flags);
	void AddTriangle(const RE::NiPoint3 (&a_vertices)[3], const RE::NiPoint2 (&a_texCoords)[3], const RE::NiColorA& a_color);
	void AddQuad(const RE::NiPoint3& a_topLeft, const RE::NiPoint3& a_topRight,
		const RE::NiPoint3& a_bottomLeft, const RE::NiPoint3& a_bottomRight,
		const RE::NiPoint2& a_texTopLeft, const RE::NiPoint2& a_texTopRight,
		const RE::NiPoint2& a_texBottomLeft, const RE::NiPoint2& a_texBottomRight,
		const RE::NiColorA& a_color);
	void AddSquare(const RE::NiPoint3& a_center, float a_size, const RE::NiColorA& a_color);
	RE::BSDynamicTriShape* Flush();

private:
	

private:
	static constexpr uint32_t MAX_VERTICES = 0xFFFF;  // 最大顶点数 (uint16_t限制)
	static constexpr uint32_t MAX_INDICES = 0x2FFFD;  // 最大索引数 (匹配原始代码限制)

	RE::NiNode* m_parentNode;  // 父节点
	uint32_t m_dynamicFlags;  // 动态标志

	std::vector<RE::NiPoint3> m_vertices;  // 顶点位置
	std::vector<RE::NiPoint2> m_texCoords;  // 纹理坐标
	std::vector<RE::NiColorA> m_colors;     // 顶点颜色
	std::vector<uint16_t> m_indices;    // 索引数据
};
