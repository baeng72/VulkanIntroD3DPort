#pragma once
#include "../../../Common/Vulkan.h"
#include "../../../Common/VulkUtil.h"
#include <glm/glm.hpp>

struct ObjectConstants {
	glm::mat4 World = glm::mat4(1.0f);
	glm::mat4 TexTransform = glm::mat4(1.0f);
};

struct PassConstants {
	glm::mat4 View = glm::mat4(1.0f);
	glm::mat4 InvView = glm::mat4(1.0f);
	glm::mat4 Proj = glm::mat4(1.0f);
	glm::mat4 InvProj = glm::mat4(1.0f);
	glm::mat4 ViewProj = glm::mat4(1.0f);
	glm::mat4 InvViewProj = glm::mat4(1.0f);
	glm::vec3 EyePosW = glm::vec3(0.0f);
	float cbPerObjectPad1 = 0.0f;
	glm::vec2 RenderTargetSize = glm::vec2(0.0f);
	glm::vec2 InvRenderTargetSize = glm::vec2(0.0f);
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
	glm::vec4 AmbientLight = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	Light Lights[MaxLights];
};

struct Vertex {
	glm::vec3 Pos;
	glm::vec3 Normal;
	glm::vec2 TexC;
	static VkVertexInputBindingDescription& getInputBindingDescription() {
		static VkVertexInputBindingDescription bindingDescription = { 0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX };
		return bindingDescription;
	}
	static std::vector<VkVertexInputAttributeDescription>& getInputAttributeDescription() {
		static std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {
			{0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,Pos)},
			{1,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,Normal)},
			{2,0,VK_FORMAT_R32G32_SFLOAT,offsetof(Vertex,TexC)}
		};
		return attributeDescriptions;
	}
};

struct FrameResource {
	FrameResource(PassConstants* pc, ObjectConstants* oc, MaterialConstants* mc,Vertex*pWvs);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();
	PassConstants* pPCs{ nullptr };
	ObjectConstants* pOCs{ nullptr };
	MaterialConstants* pMats{ nullptr };
	Vertex* pWavesVB{ nullptr };
};
