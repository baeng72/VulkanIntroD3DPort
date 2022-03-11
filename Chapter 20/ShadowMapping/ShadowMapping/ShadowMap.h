#pragma once
#include <algorithm>
#include <memory>
#include "../../../Common/Vulkan.h"
#include "../../../Common/VulkanEx.h"

class ShadowMap : public VulkanObject {
	VkQueue queue{ VK_NULL_HANDLE };
	VkCommandBuffer cmd{ VK_NULL_HANDLE };
	VkPhysicalDeviceMemoryProperties memoryProperties;
	VkViewport viewport;
	VkRect2D scissorRect;
	uint32_t width{ 0 };
	uint32_t height{ 0 };
	VkFormat format = VK_FORMAT_D32_SFLOAT;
	std::unique_ptr<VulkanTexture> shadowMap;
	std::unique_ptr<VulkanFramebuffer> frameBuffer;
	std::unique_ptr<VulkanRenderPass> renderPass;
	void BuildResource();
public:
	ShadowMap(VkDevice device_, VkPhysicalDeviceMemoryProperties memoryProperties_, VkQueue queue_, VkCommandBuffer cmd_, uint32_t width, uint32_t height);
	ShadowMap(const ShadowMap& rhs) = delete;
	~ShadowMap();
	ShadowMap& operator=(const ShadowMap& rhs) = delete;
	VkViewport Viewport()const { return viewport; }
	VkRect2D ScissorRect()const {		return scissorRect;	}
	uint32_t Width()const {	return width;	}
	uint32_t Height()const { return height; }
	VkRenderPass getRenderPass()const { return renderPass->operator VkRenderPass(); }
	VkFramebuffer getFramebuffer()const { return frameBuffer->operator VkFramebuffer(); }
	VkImageView getRenderTargetView()const { return shadowMap->operator VkImageView(); }
	VkSampler getRenderTargetSampler()const { return shadowMap->operator VkSampler(); }
};