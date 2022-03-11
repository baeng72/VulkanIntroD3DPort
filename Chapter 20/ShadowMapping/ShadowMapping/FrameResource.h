#pragma once
#include "../../../Common/Vulkan.h"
#include "../../../Common/VulkUtil.h"
#include <glm/glm.hpp>

struct ObjectConstants {
	glm::mat4 World = glm::mat4(1.0f);
	glm::mat4 TexTransform = glm::mat4(1.0f);
	uint32_t MaterialIndex;
	uint32_t ObjPad0;
	uint32_t ObjPad1;
	uint32_t ObjPad2;
};


struct PassConstants {
	glm::mat4 View = glm::mat4(1.0f);
	glm::mat4 InvView = glm::mat4(1.0f);
	glm::mat4 Proj = glm::mat4(1.0f);
	glm::mat4 InvProj = glm::mat4(1.0f);
	glm::mat4 ViewProj = glm::mat4(1.0f);
	glm::mat4 InvViewProj = glm::mat4(1.0f);
	glm::mat4 ShadowTransform = glm::mat4(1.0f);
	glm::vec3 EyePosW = glm::vec3(0.0f);
	float cbPerObjectPad1 = 0.0f;
	glm::vec2 RenderTargetSize = glm::vec2(0.0f);
	glm::vec2 InvRenderTargetSize = glm::vec2(0.0f);
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
	glm::vec4 AmbientLight = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	glm::vec4 FogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
	float gFogStart = 5.0f;
	float gFogRange = 150.0f;
	glm::vec2 cbPerObjectPad2;
	Light Lights[MaxLights];
};

struct MaterialData {
	glm::vec4 DiffuseAlbedo = { 1.0f,1.0f,1.0f,1.0f };
	glm::vec3 FresnelR0 = { 0.01f,0.01,0.0f };
	float Roughness = 64.0f;
	//used in texture mapping
	glm::mat4 MatTransform = glm::mat4(1.0f);
	uint32_t DiffuseMapIndex = 0;
	uint32_t NormalMapIndex = 0;
	uint32_t MaterialPad1;
	uint32_t MaterialPad2;
};

struct Vertex {
	glm::vec3 Pos;
	glm::vec3 Normal;
	glm::vec2 TexC;
	glm::vec3 TangentU;
};

struct FrameResource {
	FrameResource(PassConstants* pc, ObjectConstants* id, MaterialData* md);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();
	PassConstants* pPCs{ nullptr };
	ObjectConstants* pOCs{ nullptr };
	MaterialData* pMats{ nullptr };
};
