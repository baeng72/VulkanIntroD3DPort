
#pragma once
#include "../../../Common/Vulkan.h"

#include <glm/glm.hpp>

struct ObjectConstants {
	glm::mat4 World = glm::mat4(1.0f);
};

struct PassConstants
{
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
};


struct FrameResource {	
	FrameResource(PassConstants*pc,ObjectConstants*oc);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource();
	
	PassConstants* pPCs{ nullptr };
	ObjectConstants* pOCs{ nullptr };
	
};