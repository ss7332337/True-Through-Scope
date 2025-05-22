#include "NiFLoader.h"

using namespace RE;

bool NIFLoader::LoadNIF(const char* filePath)
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
	weaponnode->AttachChild(rootNode, true);
	rootNode->local.translate = NiPoint3(0, 0, 10);
	rootNode->local.rotate.MakeIdentity();

	NiUpdateData udata;
	weaponnode->Update(udata);
	rootNode->Update(udata);

	// Process the NiNode and its children
	//ProcessNiNode(rootNode);

	return true;
}

void NIFLoader::ProcessNiNode(NiNode* node)
{
	if (!node) {
		return;
	}

	std::cout << "Processing NiNode: " << node->GetName() << std::endl;

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

	std::cout << "Found BSTriShape: " << triShape->GetName() << std::endl;
	std::cout << "  Triangle Count: " << triShape->numTriangles << std::endl;
	std::cout << "  Vertex Count: " << triShape->numVertices << std::endl;

	// Get the vertex and index data
	BSGraphics::VertexDesc vertexDesc = triShape->vertexDesc;
	std::cout << "  Vertex Desc: 0x" << std::hex << vertexDesc.desc << std::dec << std::endl;

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
		std::cout << "  No shader property found" << std::endl;
	}

	// Get the alpha property
	NiAlphaProperty* alphaProperty = (NiAlphaProperty*)triShape->properties[1].get();
	if (alphaProperty) {
		ProcessNiAlphaProperty(alphaProperty);
	} else {
		std::cout << "  No alpha property found" << std::endl;
	}
}

void NIFLoader::ProcessBSEffectShaderProperty(BSEffectShaderProperty* property)
{
	if (!property) {
		return;
	}

	std::cout << "Found BSEffectShaderProperty" << std::endl;

	// Get the effect shader material (if available)
	BSEffectShaderMaterial* material = dynamic_cast<BSEffectShaderMaterial*>(property->material);
	if (material) {
		std::cout << "  Base Color: ["
				  << material->baseColor.r << ", "
				  << material->baseColor.g << ", "
				  << material->baseColor.b << "]" << std::endl;

		// Get texture information
		if (material->spBaseTexture) {
			std::cout << "  Base Texture: " << material->spBaseTexture->GetName() << std::endl;
		}

		std::cout << "  Falloff Start Angle: " << material->falloffStartAngle << std::endl;
		std::cout << "  Falloff Stop Angle: " << material->falloffStopAngle << std::endl;
		std::cout << "  Base Color Scale: " << material->baseColorScale << std::endl;
	}

	// Check any flags set on the property
	std::cout << "  Alpha: " << property->alpha << std::endl;

	// Output some shader property flags
	std::cout << "  Flags: ";
	if (property->flags.any(BSShaderProperty::EShaderPropertyFlags::kAlphaTest)) {
		std::cout << "AlphaTest ";
	}
	if (property->flags.any(BSShaderProperty::EShaderPropertyFlags::kZBufferTest)) {
		std::cout << "ZBufferTest ";
	}
	if (property->flags.any(BSShaderProperty::EShaderPropertyFlags::kGlowMap)) {
		std::cout << "GlowMap ";
	}
	std::cout << std::endl;
}

void NIFLoader::ProcessNiAlphaProperty(NiAlphaProperty* property)
{
	if (!property) {
		return;
	}

	std::cout << "Found NiAlphaProperty" << std::endl;
	std::cout << "  Alpha Test Ref: " << static_cast<int>(property->alphaTestRef) << std::endl;

}
