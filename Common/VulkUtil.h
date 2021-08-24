#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include "../../../Common/Vulkan.h"

struct SubmeshGeometry {
	uint32_t indexCount{ 0 };
	uint32_t startIndexLocation{ 0 };
	uint32_t baseVertexLocation{ 0 };
};

struct MeshGeometry {
	//Give it a name so we can look it up by name.
	std::string Name;

	void* vertexBufferCPU{ nullptr };
	void* indexBufferCPU{ nullptr };

	Buffer		vertexBufferGPU;
	Buffer		indexBufferGPU;

	uint32_t vertexByteStride{ 0 };
	uint32_t vertexBufferByteSize{ 0 };
	uint32_t indexBufferByteSize{ 0 };

	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

};

struct Light {
	glm::vec3 Strength = glm::vec3(0.5f);
	float FalloffStart = 1.0f;								//point/spot light only
	glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f);		//directional/spot light only
	float FalloffEnd = 10.0f;								//point/spot light only
	glm::vec3 Position = glm::vec3(0.0f);					//point/spot light only
	float SpotPower = 64.0f;								//spot light only
};

#define MaxLights 16

struct MaterialConstants {
	glm::vec4 DiffuseAlbedo =glm::vec4(1.0f);
	glm::vec3 FresnelR0 = glm::vec3(0.1f);
	float Roughness = 0.25f;
	//used in texture mapping
	glm::mat4 MatTransform = glm::mat4(1.0f);
};


struct Material {
	std::string Name;

	//Index into constant buffer corresponding to this material
	int MatCBIndex{ -1 };

	//Index into heap for diffuse texture?
	int DiffuseSrvHeapIndex = -1;

	//Index into SRV heap for normal texture
	int NormalSrvHeapIndex = -1;

	int NumFramesDirty{ 0 };

	glm::vec4 DiffuseAlbedo = glm::vec4(1.0f);
	glm::vec3 FresnelR0 = glm::vec3(1.0f);
	float Roughness = 0.25f;
	glm::mat4 MatTransform = glm::mat4(1.0f);
};

struct Texture : Image{
	std::string	Name;
	std::string FileName;
};