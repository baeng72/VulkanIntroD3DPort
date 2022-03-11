#include "CubeRenderTarget.h"
CubeRenderTarget::CubeRenderTarget(VkDevice device_, VkPhysicalDeviceMemoryProperties memoryProperties_, VkQueue queue_, VkCommandBuffer cmd_, uint32_t width_, uint32_t height_, VkFormat colorFormat_):VulkanObject(device_),
		memoryProperties(memoryProperties),queue(queue_),cmd(cmd_),colorFormat(colorFormat_),
	width(width_),height(height_)
{
	BuildResources();
}

CubeRenderTarget::~CubeRenderTarget() {

}

void CubeRenderTarget::BuildResources() {
	//build render pass (might need to tweak parameters)
	frameTarget = std::make_unique<VulkanImage>(device, ImageBuilder::begin(device, memoryProperties)
		.setDimensions(width, height)
		.setFormat(colorFormat)
		.setImageAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT)
		.setImageUsage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		.setImageLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.build());
	Vulkan::transitionImage(device, queue, cmd, frameTarget->operator VkImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	depthTarget = std::make_unique<VulkanImage>(device, ImageBuilder::begin(device, memoryProperties)
		.setDimensions(width, height)
		.setFormat(DEPTH_FORMAT)
		.setImageAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT)
		.setImageUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		.setImageLayout(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL)
		.build());
	renderPass = std::make_unique<VulkanRenderPass>(device, RenderPassBuilder::begin(device)
		.setColorFormat(colorFormat)
		.setDepthFormat(DEPTH_FORMAT)
		.setColorInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.setColorFinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		.build());
	std::vector<VkFramebuffer> framebuffers;
	FramebufferBuilder::begin(device)
		.setDimensions(width, height)
		.setColorImageView(*frameTarget)
		.setDepthImageView(*depthTarget)
		.setRenderPass(*renderPass)
		.build(framebuffers);
	frameBuffer = std::make_unique<VulkanFramebuffer>(device, framebuffers[0]);
	//build target cube map
	Vulkan::ImageProperties props;
	props.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	props.format = colorFormat;
	props.height = height;
	props.width = width;
	props.layers = 6;
	props.isCubeMap = true;
	props.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	props.mipLevels = 1;
	props.samples = VK_SAMPLE_COUNT_1_BIT;
	props.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	Vulkan::Image image;
	Vulkan::initImage(device, memoryProperties, props, image);
	cubeMapTexture = std::make_unique<VulkanImage>(device, image);
	Vulkan::transitionImage(device, queue, cmd, cubeMapTexture->operator VkImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,6);


}

void CubeRenderTarget::OnResize(uint32_t width_, uint32_t height_) {
	if (width != width_ || height != height_) {
		width = width_;
		height = height_;
		BuildResources();
	}
}