#pragma once
#include <algorithm>
#include <memory>
#include "../../../Common/Vulkan.h"
#include "../../../Common/VulkanEx.h"

enum class CubeMapFace : int
{
	PositiveX = 0,
	NegativeX = 1,
	PositiveY = 2,
	NegativeY = 3,
	PositiveZ = 4,
	NegativeZ = 5
};

#define CUBE_DIM 512
#define DEPTH_FORMAT VK_FORMAT_D32_SFLOAT

class CubeRenderTarget : public VulkanObject {
	VkQueue queue{ VK_NULL_HANDLE };
	VkCommandBuffer cmd{ VK_NULL_HANDLE };
	VkPhysicalDeviceMemoryProperties memoryProperties;
	std::unique_ptr<VulkanImage> cubeMapTexture;//this is the cubemap we'll build
	std::unique_ptr<VulkanFramebuffer> frameBuffer;
	std::unique_ptr<VulkanImage> frameTarget;//this is the render target for each pass
	std::unique_ptr<VulkanImage> depthTarget;//this is the render target for each pass
	std::unique_ptr<VulkanRenderPass> renderPass;//offscreen cubemap render pass
	uint32_t width{ 0 };
	uint32_t height{ 0 };
	VkFormat colorFormat{ VK_FORMAT_UNDEFINED };
	void BuildResources();
public:
	CubeRenderTarget(VkDevice device_, VkPhysicalDeviceMemoryProperties memoryProperties_, VkQueue queue_, VkCommandBuffer cmd_,uint32_t width_,uint32_t height_,VkFormat colorFormat_);
	CubeRenderTarget() = delete;
	CubeRenderTarget(const CubeRenderTarget& rhs) = delete;
	~CubeRenderTarget();
	const VkImageView CubeMapImageView()const { return cubeMapTexture->operator VkImageView(); }	
	const VkImage CubeMapImage()const { return cubeMapTexture->operator VkImage(); }
	const VkFramebuffer Framebuffer()const { return *frameBuffer; }
	const VkRenderPass RenderPass()const { return *renderPass; }
	void OnResize(uint32_t width_, uint32_t height_);
	uint32_t getWidth()const { return width; }
	uint32_t getHeight()const { return height; }
	const VkFramebuffer getFrameBuffer()const { return *frameBuffer; }
	const VkRenderPass getRenderPass()const { return *renderPass; }
	const VkImage getRenderTarget()const { return frameTarget->operator VkImage(); }
};