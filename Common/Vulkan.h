#pragma once
#include <vector>
#include <cassert>
#include <cmath>

#define NOMINMAX//don't want windows defining min,max
//#include <windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
//#include <algorithm>
#include <vulkan/vulkan.h>
#define __USE__VMA__
#ifdef __USE__VMA__
#include "vk_mem_alloc.h"
#endif

//_UNORM more like GL settings, a bit darker
#define PREFERRED_FORMAT VK_FORMAT_B8G8R8A8_UNORM
#define PREFERRED_IMAGE_FORMAT VK_FORMAT_R8G8B8A8_UNORM
//#define PREFERRED_FORMAT VK_FORMAT_B8G8R8A8_SRGB
//#define PREFERRED_IMAGE_FORMAT VK_FORMAT_R8G8B8A8_SRGB

namespace Vulkan {

	

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


	VkDevice initDevice(VkPhysicalDevice physicalDevice, std::vector<const char*> deviceExtensions, Queues queues, VkPhysicalDeviceFeatures enabledFeatures,uint32_t queueCount=1);
	void cleanupDevice(VkDevice device);

	VkQueue getDeviceQueue(VkDevice device, uint32_t queueFamily,uint32_t queueIndex=0);

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

	VkSwapchainKHR initSwapchain(VkDevice device, VkSurfaceKHR surface, uint32_t width, uint32_t height, VkSurfaceCapabilitiesKHR& surfaceCaps, VkPresentModeKHR& presentMode, VkSurfaceFormatKHR& swapchainFormat, VkExtent2D& swapchainExtent,uint32_t imageCount=UINT32_MAX, VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
	void getSwapchainImages(VkDevice device, VkSwapchainKHR swapchain, std::vector<VkImage>& images);
	void initSwapchainImageViews(VkDevice device, std::vector<VkImage>& swapchainImages, VkFormat& swapchainFormat, std::vector<VkImageView>& swapchainImageViews);
	void cleanupSwapchainImageViews(VkDevice device, std::vector<VkImageView>& imageViews);
	void cleanupSwapchain(VkDevice device, VkSwapchainKHR swapchain);
	uint32_t getSwapchainImageCount(VkSurfaceCapabilitiesKHR& surfaceCaps);

	uint32_t findMemoryType(uint32_t typeFilter, VkPhysicalDeviceMemoryProperties memoryProperties, VkMemoryPropertyFlags properties);
	VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDeviceProperties deviceProperties);

	struct ImageProperties {
		VkFormat format{ PREFERRED_FORMAT };
		VkImageUsageFlags imageUsage{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT };
#ifdef __USE__VMA__
		VmaMemoryUsage usage;
#else
		
		VkMemoryPropertyFlagBits memoryProps{ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
#endif
		VkSampleCountFlagBits samples{ VK_SAMPLE_COUNT_1_BIT };//set to sample count for MSAA
		VkImageAspectFlags aspect{ VK_IMAGE_ASPECT_COLOR_BIT };//change for depth
		VkImageLayout layout{ VK_IMAGE_LAYOUT_UNDEFINED };//images are usually transitions, but set if not
		uint32_t width{ 0 };
		uint32_t height{ 0 };
		uint32_t mipLevels{ 1 };//0 = calculate from width/height
		uint32_t layers{ 1 };//set to 6 for cubemap		
		bool isCubeMap{ false };
	};



	//template <VkFormat format = PREFERRED_FORMAT, VkImageUsageFlagBits imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VkMemoryPropertyFlagBits memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT> struct TImageProperties {
	//	VkFormat _format{ PREFERRED_FORMAT };
	//	VkImageUsageFlagBits _imageUsage{ VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT };
	//	VkMemoryPropertyFlagBits _memoryProps;
	//	VkSampleCountFlagBits _samples;
	//	uint32_t _width{ 0 };
	//	uint32_t _height{ 0 };
	//	uint32_t _mipLevels{ 1 };
	//	uint32_t _layers{ 1 };
	//	TImageProperties(uint32_t width, uint32_t height, uint32_t mipLevels, uint32_t layers) {
	//		_format = format;
	//		_imageUsage = imageUsage;
	//		_memoryProps = memoryProps;
	//		_samples = samples;
	//		_width = width;
	//		_height = height;
	//		_mipLevels = mipLevels;
	//		_layers = layers;
	//	}
	//	TImageProperties() = delete;
	//	const TImageProperties& operator=(const TImageProperties& rhs) = delete;
	//	operator ImageProperties() {
	//		ImageProperties props;
	//		props.format = _format;
	//		props.imageUsage = _imageUsage;
	//		props.memoryProps = _memoryProps;
	//		props.samples = _samples;
	//		props.width = _width;
	//		props.height = _height;
	//		props.mipLevels = _mipLevels;
	//		props.layers = _layers;
	//		return props;
	//	}

	//};




	inline uint32_t getMipLevels(uint32_t width, uint32_t height) {
		uint32_t m = std::max(width, height);
		uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
		return mipLevels;
	}
	inline uint32_t getMipLevels(ImageProperties& image) {
		uint32_t mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(image.width, image.height)))) + 1;
		return mipLevels;
	}

	struct Image {
		VkImage	image{ VK_NULL_HANDLE };
#ifdef __USE__VMA__
		VmaAllocation allocation{ VK_NULL_HANDLE };
		VmaAllocationInfo allocationInfo;
#else
		VkDeviceMemory memory{ VK_NULL_HANDLE };
#endif
		VkImageView imageView{ VK_NULL_HANDLE };

		uint32_t width{ 0 };
		uint32_t height{ 0 };
		uint32_t mipLevels{ 1 };
		uint32_t layerCount{ 1 };
	};

	struct Texture :public Image {
		VkSampler sampler{ VK_NULL_HANDLE };
	};



	void initImage(VkDevice device, /*VkFormatProperties& formatProperties,*/ VkPhysicalDeviceMemoryProperties& memoryProperties, ImageProperties& props, Image& image);
	void initImage(VkDevice device, VkImageCreateInfo& pCreateInfo, Image& image,bool isMapped=false);
	struct SamplerProperties {
		VkFilter filter{ VK_FILTER_LINEAR };
		VkSamplerAddressMode addressMode{ VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE };
	};
	VkSampler initSampler(VkDevice device, SamplerProperties& samplerProps,uint32_t mipLevels=1);
	void initSampler(VkDevice device, SamplerProperties& samplerProps, Texture& image);
	struct TextureProperties : public ImageProperties {
		SamplerProperties samplerProps;
	};
	void initTexture(VkDevice device, /*VkFormatProperties& formatProperties,*/ VkPhysicalDeviceMemoryProperties& memoryProperties, TextureProperties& props, Texture& image);
	void cleanupImage(VkDevice device, Image& image);
	void cleanupSampler(VkDevice device, VkSampler sampler);
	void cleanupTexture(VkDevice device, Texture& texture);

	VkFormat getSupportedDepthFormat(VkPhysicalDevice physicalDevice);
	void generateMipMaps(VkDevice device, VkQueue queue, VkCommandBuffer cmd, Image& image);
	struct Buffer;
	void CopyBufferToImage(VkDevice device, VkQueue queue, VkCommandBuffer cmd, Buffer& src, Image& dst, uint32_t width, uint32_t height, VkDeviceSize offset = 0, uint32_t arrayLayer = 0);
	void CopyBufferToImage(VkDevice device, VkQueue queue, VkCommandBuffer cmd, Buffer& src, Image& dst, std::vector<VkBufferImageCopy>& copyRegions);
	void transitionImage(VkDevice device, VkQueue queue, VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels = 1, uint32_t layerCount = 1,uint32_t layoutIndex=0);
	void transitionImageNoSubmit(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,uint32_t mipLevels = 1, uint32_t layerCount = 1, uint32_t layoutIndex = 0);


	struct RenderPassProperties {
		VkFormat colorFormat{ PREFERRED_FORMAT };
		VkFormat depthFormat{ VK_FORMAT_D24_UNORM_S8_UINT };
		VkFormat resolveFormat{ VK_FORMAT_UNDEFINED };
		VkSampleCountFlagBits sampleCount{ VK_SAMPLE_COUNT_1_BIT };
		VkImageLayout colorInitialLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
		VkImageLayout colorFinalLayout{ VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };
		VkAttachmentLoadOp colorLoadOp{ VK_ATTACHMENT_LOAD_OP_CLEAR };
		VkAttachmentStoreOp colorStoreOp{ VK_ATTACHMENT_STORE_OP_STORE };
		VkAttachmentLoadOp depthLoadOp{ VK_ATTACHMENT_LOAD_OP_CLEAR };
		VkAttachmentStoreOp depthStoreOp{ VK_ATTACHMENT_STORE_OP_DONT_CARE };
		VkImageLayout depthInitialLayout{ VK_IMAGE_LAYOUT_UNDEFINED };
		VkImageLayout depthFinalLayout{ VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
		std::vector<VkSubpassDependency> dependencies;
	};

	VkRenderPass initRenderPass(VkDevice device, RenderPassProperties& props);



	void cleanupRenderPass(VkDevice device, VkRenderPass renderPass);

	struct FramebufferProperties {
		std::vector<VkImageView> colorAttachments;
		VkImageView depthAttachment{ VK_NULL_HANDLE };
		VkImageView resolveAttachment{ VK_NULL_HANDLE };
		uint32_t width{ 0 };
		uint32_t height{ 0 };

	};

	void initFramebuffers(VkDevice device, VkRenderPass renderPass, FramebufferProperties& props, std::vector<VkFramebuffer>& framebuffers);
	void cleanupFramebuffers(VkDevice device, std::vector<VkFramebuffer>& framebuffers);


	struct BufferProperties {
		VkBufferUsageFlags bufferUsage{ VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT };
#ifdef __USE__VMA__
		VmaMemoryUsage usage{ VMA_MEMORY_USAGE_GPU_ONLY };
#else
		VkMemoryPropertyFlags memoryProps{ VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };
#endif
		VkDeviceSize size{ 0 };
	};

	struct Buffer {
		VkBuffer	buffer{ VK_NULL_HANDLE };
#ifdef __USE__VMA__
		VmaAllocation allocation{ VK_NULL_HANDLE };
		VmaAllocationInfo allocationInfo;
#else
		VkDeviceMemory memory{ VK_NULL_HANDLE };
#endif
		VkDeviceSize size{ 0 };

	};

	void initBuffer(VkDevice device, VkPhysicalDeviceMemoryProperties& memoryProperties, BufferProperties& props, Buffer& buffer);
	void cleanupBuffer(VkDevice device, Buffer& buffer);

	void* mapBuffer(VkDevice device, Buffer& buffer);

	void unmapBuffer(VkDevice device, Buffer& buffer);

	void CopyBufferTo(VkDevice device, VkQueue queue, VkCommandBuffer cmd, Buffer& src, Buffer& dst, VkDeviceSize size);
	void CopyBufferTo(VkDevice device, VkQueue queue, VkCommandBuffer cmd, VkFence fence, Buffer& src, Buffer& dst, VkDeviceSize size);


	VkFence initFence(VkDevice device, VkFenceCreateFlags flags = 0);
	void cleanupFence(VkDevice device, VkFence fence);


	VkDescriptorSetLayout initDescriptorSetLayout(VkDevice device, std::vector<VkDescriptorSetLayoutBinding>& descriptorBindings);
	VkDescriptorPool initDescriptorPool(VkDevice deviced, VkDescriptorPoolSize* pPoolSizes, uint32_t poolSizeCount, uint32_t maxSets);
	VkDescriptorPool initDescriptorPool(VkDevice device, std::vector<VkDescriptorPoolSize>& descriptorPoolSizes, uint32_t maxSets);
	VkDescriptorSet initDescriptorSet(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool);
	void initDescriptorSets(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool, VkDescriptorSet* pSets, uint32_t setCount);
	void updateDescriptorSets(VkDevice device, std::vector<VkWriteDescriptorSet> descriptorWrites);
	void cleanupDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool);
	void cleanupDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout);

	VkPipelineLayout initPipelineLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout);
	VkPipelineLayout initPipelineLayout(VkDevice device, std::vector<VkDescriptorSetLayout>& descriptorSetLayouts);
	VkPipelineLayout initPipelineLayout(VkDevice device, std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, std::vector<VkPushConstantRange>& pushConstants);
	void cleanupPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout);

	struct ShaderModule {
		VkShaderModule shaderModule;
		VkShaderStageFlagBits stage;
	};

	VkShaderModule initShaderModule(VkDevice device, const char* filename);
	void cleanupShaderModule(VkDevice device, VkShaderModule shaderModule);

	struct PipelineInfo {
		VkPrimitiveTopology topology= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		VkFrontFace	frontFace = VK_FRONT_FACE_CLOCKWISE;
		VkCullModeFlagBits	cullMode = VK_CULL_MODE_BACK_BIT;
		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
		VkBool32	depthTest = VK_FALSE;
		VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS;
		VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
		VkBool32	stencilTest = VK_FALSE;
		VkStencilOpState stencil;
		VkBool32	blend = VK_FALSE;
		VkBool32	noDraw = VK_FALSE;
		std::vector<VkPipelineColorBlendAttachmentState> attachementStates;//set if using attachment buffers that aren't output framebuffer
		VkBool32 noInputState = VK_FALSE;//use for drawing creating vertices in vertex shader
		//Specialization stuff, leave nullptr if unused
		VkShaderStageFlagBits specializationStage{ VK_SHADER_STAGE_VERTEX_BIT };
		uint32_t specializationSize{ 0 };
		uint8_t* specializationData{ nullptr };
		std::vector<VkSpecializationMapEntry> specializationMap;
	};

	VkPipeline initGraphicsPipeline(VkDevice, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, std::vector<ShaderModule>& shaders, VkVertexInputBindingDescription& bindingDescription, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, PipelineInfo& pipelineInfo);
	VkPipeline initComputePipeline(VkDevice device, VkPipelineLayout pipelineLayout, ShaderModule& shader);
	/*VkPipeline initGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, VkExtent2D extent, std::vector<ShaderModule>& shaders, VkVertexInputBindingDescription& bindingDescription, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, bool depthTest = true, bool stencilTest = false, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkBool32 blendEnable = VK_FALSE, VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL);
	VkPipeline initGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, VkExtent2D extent, std::vector<ShaderModule>& shaders, VkVertexInputBindingDescription& bindingDescription, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, bool depthTest = true, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkBool32 blendEnable = VK_FALSE, VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL);
	VkPipeline initGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, std::vector<ShaderModule>& shaders, VkVertexInputBindingDescription& bindingDescription, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, bool depthTest = true, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkBool32 blendEnable = VK_FALSE, VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL);
	VkPipeline initGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, std::vector<ShaderModule>& shaders, VkVertexInputBindingDescription& bindingDescription, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, bool depthTest = true, bool stencilTest = false, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkBool32 blendEnable = VK_FALSE, VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL);
	VkPipeline initGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, std::vector<ShaderModule>& shaders, VkVertexInputBindingDescription& bindingDescription, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, bool depthTest = true, VkStencilOpState* stencil = nullptr, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkBool32 blendEnable = VK_FALSE, VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL);
	VkPipeline initGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, std::vector<ShaderModule>& shaders, VkVertexInputBindingDescription& bindingDescription, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, bool depthTest = true, VkStencilOpState* stencil = nullptr, bool noDraw = false, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkBool32 blendEnable = VK_FALSE, VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL);
	VkPipeline initGraphicsPipeline(VkDevice device, VkRenderPass renderPass, VkPipelineLayout pipelineLayout, VkRect2D viewRect, std::vector<ShaderModule>& shaders, VkVertexInputBindingDescription& bindingDescription, std::vector<VkVertexInputAttributeDescription>& attributeDescriptions, VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, bool depthTest = true, VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT, VkBool32 blendEnable = VK_FALSE);*/
	void cleanupPipeline(VkDevice device, VkPipeline pipeline);


}