#include "NiFLoader.h"

using namespace RE;

NIFLoader* NIFLoader::GetSingleton()
{
	static NIFLoader instance;
	return &instance;
}

NiNode* NIFLoader::LoadNIF(const char* filePath)
{
	// Create a BSStream object to load the NIF file
	BSStream* stream = BSStream::Create();

	// Load the NIF file using BSStream::Load
	if (!stream->Load(filePath)) {
		logger::error("Failed to load NIF file: {}", filePath);
		return nullptr;
	}

	NiObject* rootObject = stream->topObjects[0].get();
	if (!rootObject) {
		logger::error("Failed to get root object from NIF file");
		return nullptr;
	}

	// Cast the root object to NiNode
	NiNode* rootNode = rootObject->IsNode();
	if (!rootNode) {
		logger::error("Root object is not a NiNode");
		return nullptr;
	}

	return rootNode;
}

bool NIFLoader::LoadNIFAndAttach(const char* filePath)
{
	// Create a BSStream object to load the NIF file
	BSStream* stream = BSStream::Create();

	// Load the NIF file using BSStream::Load
	if (!stream->Load(filePath)) {
		logger::error("Failed to load NIF file: {}", filePath);
		return false;
	}

	NiObject* rootObject = stream->topObjects[0].get();
	if (!rootObject) {
		logger::error("Failed to get root object from NIF file");
		return false;
	}

	// Cast the root object to NiNode
	NiNode* rootNode = rootObject->IsNode();
	if (!rootNode) {
		logger::error("Root object is not a NiNode");
		return false;
	}

	auto weaponnode = PlayerCharacter::GetSingleton()->Get3D(true)->GetObjectByName("Weapon")->IsNode();
	weaponnode->AttachChild(rootNode, false);
	rootNode->local.translate = NiPoint3(0, 0, 10);
	rootNode->local.rotate.MakeIdentity();

	NiUpdateData udata;
	weaponnode->Update(udata);
	rootNode->Update(udata);

	return true;
}

void NIFLoader::ProcessNiNode(NiNode* node)
{
	if (!node) {
		return;
	}



	// Iterate through the children of the NiNode
	for (uint16_t i = 0; i < node->children.size(); ++i) {
		NiPointer<NiAVObject> childObject = node->children[i];
		if (childObject) {
			// Check if the child is a BSTriShape
			BSTriShape* triShape = childObject->IsTriShape();
			if (triShape) {
				ProcessBSTriShape(triShape);
			} else {
				// If it's another NiNode, process recursively
				NiNode* childNode = childObject->IsNode();
				if (childNode) {
					ProcessNiNode(childNode);
				}
			}
		}
	}
}

 void NIFLoader::ProcessBSTriShape(BSTriShape* triShape)
{
	if (!triShape) {
		return;
	}



	// Get the vertex and index data
	BSGraphics::VertexDesc vertexDesc = triShape->vertexDesc;


	// Get the shader property
	NiShadeProperty* shadeProperty = triShape->QShaderProperty();
	if (shadeProperty) {
		// Try to cast to BSEffectShaderProperty
		BSEffectShaderProperty* effectShader = nullptr;
		if (shadeProperty->RTTI.id() == RTTI::BSEffectShaderProperty.id()) {
			effectShader = static_cast<BSEffectShaderProperty*>(shadeProperty);
			ProcessBSEffectShaderProperty(effectShader);
		}
	} else {

	}

	// Get the alpha property
	NiAlphaProperty* alphaProperty = (NiAlphaProperty*)triShape->properties[1].get();
	if (alphaProperty) {
		ProcessNiAlphaProperty(alphaProperty);
	} else {

	}
}

void NIFLoader::ProcessBSEffectShaderProperty(BSEffectShaderProperty* property)
{
	if (!property) {
		return;
	}


}

void NIFLoader::ProcessNiAlphaProperty(NiAlphaProperty* property)
{
	if (!property) {
		return;
	}



}
