#include "ShadowMap.h"

ShadowMap::ShadowMap(VkDevice device_, VkPhysicalDeviceMemoryProperties memoryProperties_, VkQueue queue_, VkCommandBuffer cmd_, uint32_t width_, uint32_t height_) :VulkanObject(device_),
memoryProperties(memoryProperties_), queue(queue_), cmd(cmd_), width(width_), height(height_) {
	viewport = { 0.0f,0.0f,(float)width,(float)height,0.0f,1.0f };
	scissorRect = { {0,0},{width,height} };
	BuildResource();
}

ShadowMap::~ShadowMap() {

}

void ShadowMap::BuildResource() {
	shadowMap = std::make_unique<VulkanTexture>(device, TextureBuilder::begin(device, memoryProperties)
		.setDimensions(width, height)
		.setFormat(format)
		.setImageAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT)
		.setImageUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)		
		.build());
	renderPass = std::make_unique<VulkanRenderPass>(device, RenderPassBuilder::begin(device)
		.setDepthFormat(format)
		.setDepthStoreOp(VK_ATTACHMENT_STORE_OP_STORE)
		.setDepthInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED)
		.setDepthFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		//.setDependency(VK_SUBPASS_EXTERNAL,0,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_DEPENDENCY_BY_REGION_BIT)
		//.setDependency(0,VK_SUBPASS_EXTERNAL,VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_DEPENDENCY_BY_REGION_BIT)
		.build());
	std::vector<VkFramebuffer> framebuffers;
	FramebufferBuilder::begin(device)
		.setDimensions(width, height)
		.setDepthImageView(*shadowMap)
		.setRenderPass(*renderPass)
		.build(framebuffers);
	frameBuffer = std::make_unique<VulkanFramebuffer>(device, framebuffers[0]);
}