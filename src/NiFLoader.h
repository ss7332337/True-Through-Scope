#pragma once
#include <vector>
#include <iostream>
#include <string>
#include <memory>



class NIFLoader
{
public:
	static NIFLoader* GetSingleton();

	bool LoadNIFAndAttach(const char* filePath);
	RE::NiNode* LoadNIF(const char* filePath);
	bool LoadNIFLowLevel(const char* filePath);
	RE::NiNode* CreateTTSNodeFromNIF(const char* filePath);


private:
	void ProcessNiNode(RE::NiNode* node);
	void ProcessBSTriShape(RE::BSTriShape* triShape);
	void ProcessBSEffectShaderProperty(RE::BSEffectShaderProperty* property);
	void ProcessNiAlphaProperty(RE::NiAlphaProperty* property);
};

