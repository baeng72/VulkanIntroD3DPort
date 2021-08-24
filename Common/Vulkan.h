#pragma once

#include <vector>
#include <cassert>
#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR

#include <vulkan/vulkan.h>

//_UNORM more like GL settings, a bit darker
#define PREFERRED_FORMAT VK_FORMAT_B8G8R8A8_UNORM
#define PREFERRED_IMAGE_FORMAT VK_FORMAT_R8G8B8A8_UNORM
//#define PREFERRED_FORMAT VK_FORMAT_B8G8R8A8_SRGB
//#define PREFERRED_IMAGE_FORMAT VK_FORMAT_R8G8B8A8_SRGB


VkInstance initInstance(std::vector<const char*>& requiredExtensions, std::vector<const char*>& requiredLayers);
void cleanupInstance(VkInstance instance);


VkSurfaceKHR initSurface(VkInstance instance, HINSTANCE hInstance, HWND hWnd);
void cleanupSurface(VkInstance instance, VkSurfaceKHR surface);

struct Queues {
	uint32_t graphicsQueueFamily;
	uint32_t presentQueueFamily;
	uint32_t computeQueueFamily;
};

VkPhysicalDevice choosePhysicalDevice(VkInstance instance, VkSurfaceKHR surface, Queues& queues);

VkDevice initDevice(VkPhysicalDevice physicalDevice, std::vector<const char*> deviceExtensions, Queues queues, VkPhysicalDeviceFeatures enabledFeatures);
void cleanupDevice(VkDevice device);

VkQueue getDeviceQueue(VkDevice device, uint32_t queueFamily);

VkPresentModeKHR chooseSwapchainPresentMode(std::vector<VkPresentModeKHR>& presentModes);

VkSurfaceFormatKHR chooseSwapchainFormat(std::vector<VkSurfaceFormatKHR>& formats);

VkSurfaceTransformFlagsKHR chooseSwapchainTransform(VkSurfaceCapabilitiesKHR& surfaceCaps);

VkCompositeAlphaFlagBitsKHR chooseSwapchainComposite(VkSurfaceCapabilitiesKHR& surfaceCaps);

VkSemaphore initSemaphore(VkDevice device);
void cleanupSemaphore(VkDevice device, VkSemaphore semaphore);

VkCommandPool initCommandPool(VkDevice device, uint32_t queueFamily);
void initCommandPools(VkDevice device, size_t size, uint32_t queueFamily, std::vector<VkCommandPool>& commandPools);
VkCommandBuffer initCommandBuffer(VkDevice device, VkCommandPool commandPool);
void initCommandBuffers(VkDevice device, std::vector<VkCommandPool>& commandPools, std::vector<VkCommandBuffer>& commandBuffers);
void cleanupCommandBuffers(VkDevice device, std::vector<VkCommandPool>& commandPools, std::vector<VkCommandBuffer>& commandBuffers);
void cleanupCommandBuffer(VkDevice device, VkCommandPool commandPool, VkCommandBuffer commandBuffer);
void cleanupCommandPools(VkDevice device, std::vector<VkCommandPool>& commandPools);
void cleanupCommandPool(VkDevice device, VkCommandPool commandPool);

VkSwapchainKHR initSwapchain(VkDevice device, VkSurfaceKHR surface, uint32_t width, uint32_t height, VkSurfaceCapabilitiesKHR& surfaceCaps, VkPresentModeKHR& presentMode, VkSurfaceFormatKHR& swapchainFormat, VkExtent2D& swapchainExtent,VkSwapchainKHR oldSwapchain=VK_NULL_HANDLE);
void getSwapchainImages(VkDevice device, VkSwapchainKHR swapchain, std::vector<VkImage>& images);
void initSwapchainImageViews(VkDevice device, std::vector<VkImage>& swapchainImages, VkFormat& swapchainFormat, std::vector<VkImageView>& swapchainImageViews);
void cleanupSwapchainImageViews(VkDevice device, std::vector<VkImageView>& imageViews);
void cleanupSwapchain(VkDevice device, VkSwapchainKHR swapchain);
uint32_t getSwapchainImageCount(VkSurfaceCapabilitiesKHR& surfaceCaps);

VkFence initFence(VkDevice device, VkFenceCreateFlags flags = 0);
void cleanupFence(VkDevice device, VkFence fence);


struct Buffer {
	VkBuffer	buffer{ VK_NULL_HANDLE };
	VkDeviceMemory memory{ VK_NULL_HANDLE };
	VkDeviceSize size{ 0 };

};

uint32_t findMemoryType(uint32_t typeFilter, VkPhysicalDeviceMemoryProperties memoryProperties, VkMemoryPropertyFlags properties);

void initBuffer(VkDevice device, VkPhysicalDeviceMemoryProperties& memoryProperties, VkDeviceSize size, VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, Buffer& buffer);
void cleanupBuffer(VkDevice device, Buffer& buffer);

void* mapBuffer(VkDevice device, Buffer& buffer);

void unmapBuffer(VkDevice device, Buffer& buffer);

void CopyBufferTo(VkDevice device, VkQueue queue, VkCommandBuffer cmd, Buffer& src, Buffer& dst, VkDeviceSize size);
void CopyBufferTo(VkDevice device, VkQueue queue, VkCommandBuffer cmd,VkFence fence, Buffer& src, Buffer& dst, VkDeviceSize size);

struct Image {
	VkImage	image{ VK_NULL_HANDLE };
	VkDeviceMemory memory{ VK_NULL_HANDLE };
	VkSampler sampler{ VK_NULL_HANDLE };
	VkImageView imageView{ VK_NULL_HANDLE };
	uint32_t width;
	uint32_t height;
	uint32_t mipLevels;
};

void initImage(VkDevice device, VkFormat format, VkFormatProperties& formatProperties, VkPhysicalDeviceMemoryProperties& memoryProperties, VkMemoryPropertyFlags memoryPropertyFlags, uint32_t width, uint32_t height, Image& image);
void initSampler(VkDevice device, Image& image);
void initMSAAImage(VkDevice device, VkFormat format, VkFormatProperties& formatProperties, VkPhysicalDeviceMemoryProperties& memoryProperties, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, uint32_t width, uint32_t height, VkSampleCountFlagBits numSamples, Image& image);
void initColorAttachmentImage(VkDevice device, VkFormat format, VkFormatProperties& formatProperties, VkPhysicalDeviceMemoryProperties& memoryProperties, VkMemoryPropertyFlags memoryPropertyFlags, uint32_t width, uint32_t height, Image& image);
void initDepthImage(VkDevice device, VkFormat format, VkFormatProperties& formatProperties, VkPhysicalDeviceMemoryProperties& memoryProperties, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, uint32_t width, uint32_t height, Image& image);
void initDepthImage(VkDevice device, VkFormat format, VkFormatProperties& formatProperties, VkPhysicalDeviceMemoryProperties& memoryProperties, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, VkSampleCountFlagBits numSamples, uint32_t width, uint32_t height, Image& image);
void initTextureImage(VkDevice device, VkFormat format, VkFormatProperties& formatProperties, VkPhysicalDeviceMemoryProperties& memoryProperties, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, uint32_t width, uint32_t height, Image& image);
void initTextureImageNoMip(VkDevice device, VkFormat format, VkFormatProperties& formatProperties, VkPhysicalDeviceMemoryProperties& memoryProperties, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags, uint32_t width, uint32_t height, Image& image);
void cleanupImage(VkDevice device, Image& image);
void generateMipMaps(VkDevice device, VkQueue queue, VkCommandBuffer cmd, Image& image);
void CopyBufferToImage(VkDevice device, VkQueue queue, VkCommandBuffer cmd, Buffer& src, Image& dst, uint32_t width, uint32_t height, VkDeviceSize offset = 0, uint32_t arrayLayer = 0);
void transitionImage(VkDevice device, VkQueue queue, VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels = 1, uint32_t layerCount = 1);

VkRenderPass initRenderPass(VkDevice device, VkFormat colorFormat);
VkRenderPass initRenderPass(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
VkRenderPass initMSAARenderPass(VkDevice device, VkFormat colorFormat, VkFormat depthFormat, VkSampleCountFlagBits numSamples);
VkRenderPass initMSAARenderPass(VkDevice device, VkFormat colorFormat, VkSampleCountFlagBits numSamples);
VkRenderPass initOffScreenRenderPass(VkDevice device, VkFormat colorFormat, VkFormat depthFormat);
VkRenderPass initShadowRenderPass(VkDevice device, VkFormat depthFormat);
void initFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& colorAttachments, uint32_t width, uint32_t height, std::vector<VkFramebuffer>& framebuffers);
void initFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& colorAttachments, VkImageView depthImageView, uint32_t width, uint32_t height, std::vector<VkFramebuffer>& framebuffers);
void initFramebuffers(VkDevice device, VkRenderPass renderPass, VkImageView depthImageView, uint32_t width, uint32_t height, std::vector<VkFramebuffer>& framebuffers);
void initFramebuffers(VkDevice device, VkRenderPass renderPass, VkImageView depthImageView, uint32_t width, uint32_t height, uint32_t layers, std::vector<VkFramebuffer>& framebuffers);
void initMSAAFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& colorAttachments, VkImageView depthImageView, VkImageView resolveAttachment, uint32_t width, uint32_t height, std::vector<VkFramebuffer>& framebuffers);
void initMSAAFramebuffers(VkDevice device, VkRenderPass renderPass, std::vector<VkImageView>& colorAttachments, VkImageView resolveAttachment, uint32_t width, uint32_t height, std::vector<VkFramebuffer>& framebuffers);
void cleanupFramebuffers(VkDevice device, std::vector<VkFramebuffer>& framebuffers);
void cleanupRenderPass(VkDevice device, VkRenderPass renderPass);



VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDeviceProperties deviceProperties);


VkDescriptorSetLayout initDescriptorSetLayout(VkDevice device, std::vector<VkDescriptorSetLayoutBinding>& descriptorBindings);
VkDescriptorPool initDescriptorPool(VkDevice device, std::vector<VkDescriptorPoolSize>& descriptorPoolSizes, uint32_t maxSets);
VkDescriptorSet initDescriptorSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool);
void initDescriptorSets(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool,VkDescriptorSet*pSets,uint32_t setCount);
void updateDescriptorSets(VkDevice device, std::vector<VkWriteDescriptorSet> descriptorWrites);
void cleanupDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool);
void cleanupDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout);

VkPipelineLayout initPipelineLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout);
VkPipelineLayout initPipelineLayout(VkDevice device, std::vector<VkDescriptorSetLayout> descriptorSetLayouts);
void cleanupPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout);

struct ShaderModule {
	VkShaderModule shaderModule;
	VkShaderStageFlagBits stage;
};
VkPipeline initGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, VkExtent2D extent, std::vector<ShaderModule>& shaders, VkVertexInputBindingDescription& bindingDescription, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, bool depthTest = true,bool stencilTest=false, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkBool32 blendEnable = VK_FALSE,VkPolygonMode polygonMode=VK_POLYGON_MODE_FILL);
VkPipeline initGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, VkExtent2D extent, std::vector<ShaderModule>& shaders, VkVertexInputBindingDescription& bindingDescription, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, bool depthTest = true, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkBool32 blendEnable = VK_FALSE, VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL);
VkPipeline initGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, std::vector<ShaderModule>& shaders, VkVertexInputBindingDescription& bindingDescription, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, bool depthTest = true, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkBool32 blendEnable = VK_FALSE, VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL);
VkPipeline initGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout,  std::vector<ShaderModule>& shaders, VkVertexInputBindingDescription& bindingDescription, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, bool depthTest = true,bool stencilTest=false, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkBool32 blendEnable = VK_FALSE,VkPolygonMode polygonMode=VK_POLYGON_MODE_FILL);
VkPipeline initGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, std::vector<ShaderModule>& shaders, VkVertexInputBindingDescription& bindingDescription, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, bool depthTest = true, VkStencilOpState* stencil=nullptr, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkBool32 blendEnable = VK_FALSE, VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL);
VkPipeline initGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, std::vector<ShaderModule>& shaders, VkVertexInputBindingDescription& bindingDescription, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, bool depthTest = true, VkStencilOpState* stencil = nullptr,bool noDraw=false, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkBool32 blendEnable = VK_FALSE, VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL);
VkPipeline initGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, VkRect2D viewRect, std::vector<ShaderModule>& shaders, VkVertexInputBindingDescription& bindingDescription, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, bool depthTest = true, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkBool32 blendEnable = VK_FALSE);
void cleanupPipeline(VkDevice device, VkPipeline pipeline);


VkShaderModule initShaderModule(VkDevice device, const char* filename);
void cleanupShaderModule(VkDevice device, VkShaderModule shaderModule);


