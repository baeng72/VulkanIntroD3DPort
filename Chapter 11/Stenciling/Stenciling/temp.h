#pragma once
#include <vector>
#include <array>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include "../../../Common/Vulkan.h"

#define DESCRIPTOR_POOL_SIZE 0x100 //allocate this for a block



class DescriptorSetPoolCache {
	VkDevice device{ VK_NULL_HANDLE };
	VkDescriptorPool getPool();
	VkDescriptorPool currentPool{ VK_NULL_HANDLE };
	std::vector<VkDescriptorPool> allocatedPools;
	std::vector<VkDescriptorPool> freePools;
	VkDescriptorPool createPool();
public:
	DescriptorSetPoolCache(VkDevice device_);
	~DescriptorSetPoolCache();
	void resetPools();
	bool allocateDescriptorSet(VkDescriptorSet* pSet, VkDescriptorSetLayout layout);
	bool allocateDescriptorSets(VkDescriptorSet* pSet, VkDescriptorSetLayout*pLayouts, uint32_t count );
	operator VkDevice()const { return device; }
};

class DescriptorSetLayoutCache {
	struct DescriptorSetLayoutInfo {
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		bool operator==(const DescriptorSetLayoutInfo& k)const;
		size_t hash()const;
	};
	struct DescriptorSetLayoutHash {
		size_t operator()(const DescriptorSetLayoutInfo& k)const {
			return k.hash();
		}
	};
	std::unordered_map<DescriptorSetLayoutInfo, VkDescriptorSetLayout, DescriptorSetLayoutHash> layoutCache;
	VkDevice device;
public:
	DescriptorSetLayoutCache(VkDevice device_);
	~DescriptorSetLayoutCache();
	VkDescriptorSetLayout create(std::vector<VkDescriptorSetLayoutBinding>& bindings);
	operator VkDevice()const { return device; }
	bool getBindings(VkDescriptorSetLayout layout,std::vector<VkDescriptorSetLayoutBinding>& bindings) {
		bindings.clear();
		for (auto& pair : layoutCache) {
			if (pair.second == layout) {
				bindings = std::vector<VkDescriptorSetLayoutBinding>(pair.first.bindings);
				return true;
			}
		}
		return false;
	}
};



class DescriptorSetBuilder2 {
	DescriptorSetPoolCache* pPool{ nullptr };
	DescriptorSetLayoutCache* pLayout{ nullptr };
	std::vector<VkDescriptorSetLayoutBinding> bindings;
public:
	static DescriptorSetBuilder2 begin(DescriptorSetPoolCache* pPool_, DescriptorSetLayoutCache* pLayout_);
	DescriptorSetBuilder2& AddBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags);
	bool build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);
	bool build(VkDescriptorSet& set);
	bool build(std::vector<VkDescriptorSet>& sets, VkDescriptorSetLayout& layout,uint32_t count);
	bool build(std::vector<VkDescriptorSet>& sets,uint32_t count);

};

class DescriptorSetUpdater {
	DescriptorSetLayoutCache* pLayout{ nullptr };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	std::vector<VkDescriptorSetLayoutBinding> bindings;
	std::vector<VkWriteDescriptorSet> writes;
	void setBindings();
public:
	static DescriptorSetUpdater begin(DescriptorSetLayoutCache *pLayout_,VkDescriptorSetLayout descriptorSetLayout_,VkDescriptorSet descriptorSet_);
	DescriptorSetUpdater& AddBinding(uint32_t binding, VkDescriptorType type, VkDescriptorBufferInfo* bufferInfo);
	DescriptorSetUpdater& AddBinding(uint32_t binding, VkDescriptorType type, VkDescriptorImageInfo* imageInfo);
	void update();
};

struct UniformBufferInfo {
	VkDeviceSize objectSize{ 0 };
	VkDeviceSize objectCount{ 0 };
	VkDeviceSize repeatCount{ 0 };
	void* ptr{ nullptr };
};
class UniformBufferBuilder {
	
	VkDevice device{ VK_NULL_HANDLE };
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceMemoryProperties memoryProperties;
	VkDescriptorType descriptorType;
	bool isMapped{ false };
	std::vector<UniformBufferInfo> bufferInfo;
	Vulkan::Buffer buffer;
public:
	static UniformBufferBuilder begin(VkDevice device_,VkPhysicalDeviceProperties&deviceProperties_, VkPhysicalDeviceMemoryProperties& memoryProperties_,VkDescriptorType descriptorType_,bool isMapped_);
	UniformBufferBuilder& AddBuffer(VkDeviceSize objectSize_, VkDeviceSize objectCount_, VkDeviceSize repeatCount_);
	void build(Vulkan::Buffer&buffer_,std::vector<UniformBufferInfo>&bufferInfo_);
};

class VulkanBuffer {
	VkDevice device{ VK_NULL_HANDLE };
	Vulkan::Buffer buffer;
public:
	VulkanBuffer() = delete;
	VulkanBuffer(VkDevice device_,Vulkan::Buffer& buffer_);
	~VulkanBuffer();
	operator VkBuffer()const { return buffer.buffer; }
};

class UniformBuffer:public VulkanBuffer{
	std::vector<UniformBufferInfo> bufferInfo;
public:
	UniformBuffer() = delete;
	UniformBuffer(VkDevice device_,Vulkan::Buffer& buffer_, std::vector<UniformBufferInfo>& bufferInfo_);
	operator size_t()const {return bufferInfo.size();}
	UniformBufferInfo operator[](int i)const {return bufferInfo[i];	}
};

class VulkanTexture {
	VkDevice device{ VK_NULL_HANDLE };
	Vulkan::Texture texture;
public:
	VulkanTexture(VkDevice device_, Vulkan::Texture& texture_);
	~VulkanTexture();
	operator VkImageView()const { return texture.imageView; }
	operator VkSampler()const { return texture.sampler; };
};

class VulkanTextureList {
	VkDevice device{ VK_NULL_HANDLE };
	std::vector<Vulkan::Texture> textures;
public:
	VulkanTextureList() = delete;
	VulkanTextureList(VkDevice device_, std::vector<Vulkan::Texture>& textures_);
	~VulkanTextureList();
	operator size_t()const { return textures.size(); }
	const Vulkan::Texture& operator[](int i)const { return textures[i]; }
};
class TextureBuilder {
	VkDevice device{ VK_NULL_HANDLE };
	VkCommandBuffer commandBuffer{ VK_NULL_HANDLE };
	VkQueue queue{ VK_NULL_HANDLE };
	VkPhysicalDeviceMemoryProperties memoryProperties;
	struct TextureInfo {
		uint8_t* pixels;
		uint32_t width;
		uint32_t height;
		VkFormat format;
		bool enableLod;
	};
	std::vector<TextureInfo> textureInfo;
public:
	static TextureBuilder begin(VkDevice device_, VkCommandBuffer commandBuffer_, VkQueue queue_, VkPhysicalDeviceMemoryProperties memoryProperties_);
	TextureBuilder& AddTexture(uint8_t* pixels,uint32_t width, uint32_t height, VkFormat format, bool enableLod = false);
	bool build(std::vector<Vulkan::Texture>& textures);
};

struct ImageInfo {
	uint8_t* pixels{ nullptr };
	uint32_t width;
	uint32_t height;
	VkFormat format;
	~ImageInfo();
};
class ImageLoader {
	std::vector<std::string> imagePaths;
	VkDevice device{ VK_NULL_HANDLE };
	VkCommandBuffer commandBuffer{ VK_NULL_HANDLE };
	VkQueue queue{ VK_NULL_HANDLE };
	VkPhysicalDeviceMemoryProperties memoryProperties;
public:
	static ImageLoader begin();
	ImageLoader& AddImage(const char* imagePath);
	bool build(std::vector<ImageInfo>& imageInfo);
};

class TextureLoader {
	VkDevice device{ VK_NULL_HANDLE };
	VkCommandBuffer commandBuffer{ VK_NULL_HANDLE };
	VkQueue queue{ VK_NULL_HANDLE };
	VkPhysicalDeviceMemoryProperties memoryProperties;
	std::vector<std::string> imagePaths;
	std::vector<bool> enableLods;
	TextureLoader(VkDevice device_, VkCommandBuffer commandBuffer_, VkQueue queue_, VkPhysicalDeviceMemoryProperties memoryProperties_);
	void loadTexture(uint8_t* pixels, uint32_t width, uint32_t height, VkFormat format,bool enableLod, Vulkan::Texture& texture);
public:
	static TextureLoader begin(VkDevice device_, VkCommandBuffer commandBuffer_, VkQueue queue_, VkPhysicalDeviceMemoryProperties memoryProperties_);
	TextureLoader& addTexture(const char* imagePath,bool enableLod=false);
	bool load(std::vector<Vulkan::Texture>& textures);
};

class ShaderProgramLoader {
	VkDevice device{ VK_NULL_HANDLE };
	std::vector<std::string> shaderPaths;
	VkVertexInputBindingDescription vertexInputDescription;
	std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
	std::unordered_map<VkShaderStageFlagBits, VkShaderModule> shaders;
	ShaderProgramLoader(VkDevice device_);
public:
	static ShaderProgramLoader begin(VkDevice device_);
	ShaderProgramLoader& AddShaderPath(const char* shaderPath);
	void load(std::vector<Vulkan::ShaderModule>& shaders_);
	bool load(std::vector<Vulkan::ShaderModule>& shaders_, VkVertexInputBindingDescription& vertexInputDescription_, std::vector<VkVertexInputAttributeDescription>& vertexAttributeDescriptions_);
};

class PipelineLayoutBuilder{
	VkDevice device{ VK_NULL_HANDLE };
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	PipelineLayoutBuilder(VkDevice device_);
public:
	static PipelineLayoutBuilder begin(VkDevice device_);
	PipelineLayoutBuilder& AddDescriptorSetLayout(VkDescriptorSetLayout layout_);
	PipelineLayoutBuilder& AddDescriptorSetLayouts(std::vector<VkDescriptorSetLayout>& layouts_);
	void build(VkPipelineLayout& pipelineLayout);	
};



class PipelineBuilder {
	VkDevice device{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkRenderPass renderPass{ VK_NULL_HANDLE };
	std::vector<Vulkan::ShaderModule>& shaders;
	VkVertexInputBindingDescription vertexInputDescription;
	std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
	VkCullModeFlagBits cullMode{ VK_CULL_MODE_BACK_BIT };
	VkPolygonMode polygonMode{ VK_POLYGON_MODE_FILL };
	VkBool32 depthTest{ VK_FALSE };
	VkBool32 stencilTest{ VK_FALSE };
	VkStencilOpState stencil;
	VkBool32 blend{ VK_FALSE };
	VkFrontFace frontFace{ VK_FRONT_FACE_COUNTER_CLOCKWISE };
	VkBool32 noDraw{ VK_FALSE };
	PipelineBuilder(VkDevice device_, VkPipelineLayout pipelineLayout_,VkRenderPass renderPass_, std::vector<Vulkan::ShaderModule>& shaders_, VkVertexInputBindingDescription& vertexInputDescription_, std::vector<VkVertexInputAttributeDescription>& vertexAttributeDescriptions_);
public:
	static PipelineBuilder begin(VkDevice device_, VkPipelineLayout pipelineLayout_, VkRenderPass renderPass_, std::vector<Vulkan::ShaderModule>& shaders_, VkVertexInputBindingDescription& vertexInputDescription_, std::vector<VkVertexInputAttributeDescription>& vertexAttributeDescriptions_);
	PipelineBuilder& setCullMode(VkCullModeFlagBits cullMode_);
	PipelineBuilder& setPolygonMode(VkPolygonMode polygonMode_);
	PipelineBuilder& setFrontFace(VkFrontFace frontFace_);
	PipelineBuilder& setDepthTest(VkBool32 depthTest_);
	PipelineBuilder& setBlend(VkBool32 blend_);
	PipelineBuilder& setStencilTest(VkBool32 stencilTest_);
	PipelineBuilder& setNoDraw(VkBool32 noDraw_);
	PipelineBuilder& setStencilState(VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp, uint32_t compareMask, uint32_t writeMask, uint32_t reference);
	void build(VkPipeline& pipeline);

};

class VulkanDescriptorList {
	VkDevice device{ VK_NULL_HANDLE };
	std::vector<VkDescriptorSet> descriptorSetList;
public:
	VulkanDescriptorList(VkDevice device_,std::vector<VkDescriptorSet>& descriptorSetList_);
	~VulkanDescriptorList();
	VkDescriptorSet operator[](int i) { return descriptorSetList[i]; }
};

class VulkanPipelineLayout {
	VkDevice device{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
public:
	VulkanPipelineLayout() = delete;
	VulkanPipelineLayout(VkDevice device_,VkPipelineLayout pipelineLayout_);
	~VulkanPipelineLayout();
	operator VkPipelineLayout()const {
		return pipelineLayout;
	}
};

class VulkanPipeline {
	VkDevice device{ VK_NULL_HANDLE };
	VkPipeline pipeline{ VK_NULL_HANDLE };
public:
	VulkanPipeline() = delete;
	VulkanPipeline(VkDevice device_, VkPipeline pipeline_);
	~VulkanPipeline();
	operator VkPipeline()const {
		return pipeline;
	}
};
class DescriptorSetBuilder {
	
	DescriptorSetPoolCache* pPool{ nullptr };
	DescriptorSetLayoutCache* pLayout{ nullptr };	
	std::vector<std::vector<VkWriteDescriptorSet>> writes;
	std::vector<std::vector<VkDescriptorSetLayoutBinding>> bindings;
public:
	static DescriptorSetBuilder begin(DescriptorSetPoolCache*pPool_,DescriptorSetLayoutCache*pLayout_);
	DescriptorSetBuilder& AddBinding(uint32_t set, uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, VkDescriptorBufferInfo* pBufferInfo);
	DescriptorSetBuilder& AddBinding(uint32_t set, uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, VkDescriptorImageInfo* pBufferInfo);
	bool build(std::vector<std::vector<VkDescriptorSet>>&sets,std::vector<std::vector<VkDescriptorSetLayout>>&layouts);
	bool build(std::vector<std::vector<VkDescriptorSet>>& sets);
	
};

struct VulkContext {
	VkDevice device{ VK_NULL_HANDLE };
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceMemoryProperties memoryProperties;
	VkCommandBuffer commandBuffer{ VK_NULL_HANDLE };
	VkQueue queue{ VK_NULL_HANDLE };
	operator VkDevice()const { return device; }
	operator VkCommandBuffer()const { return commandBuffer; }
	operator VkQueue()const { return queue; }
};

struct DescriptorSetInfo {
	VkDescriptorType descriptorType{ VK_DESCRIPTOR_TYPE_MAX_ENUM };
	VkShaderStageFlags stageFlags{ VK_SHADER_STAGE_ALL };
};

template <typename T> class MyArray {
	T* ptr{ nullptr };
	size_t count{ 0 };
public:
	MyArray(size_t count_):count(count_)
	{
		ptr = new T[count];
		
	}
	MyArray() {

	}
	~MyArray() {
		clear();
	}

	T& operator[](int i)const {
		assert(i > -1 && i < count);
		return ptr[i];
	}
	void clear() {
		if (ptr != nullptr) {
			delete[]ptr;
			ptr = nullptr;
		}
		count = 0;
	}
	void set(T* ptr_, size_t count_) {
		clear();
		count = count_;
		ptr = new T[count];
		memcpy(ptr, ptr_, sizeof(T) * count);
	}

};
#if 0
struct TexArray {
	Vulkan::Texture* pTextures{ nullptr };
	size_t count{ 0 };
	TexArray();

	~TexArray();
	void set(Vulkan::Texture* pTextures_, size_t count_);
};
#endif
typedef MyArray<Vulkan::Texture> TexArray;
#if 0
struct ImageInfoArray {
	VkDescriptorImageInfo* pInfo{ nullptr };
	size_t count{ 0 };
	ImageInfoArray();
	~ImageInfoArray();
	void set(VkDescriptorImageInfo* pInfo_, size_t count_);
};
#endif
typedef MyArray<VkDescriptorImageInfo> ImageInfoArray;
typedef MyArray<VkDescriptorBufferInfo> BufferInfoArray;

struct ShaderResourceInfo{
	VkDescriptorType descriptorType;
	VkShaderStageFlags shaderStage;
	union {
		struct {
			Vulkan::Buffer buffer;
			void* ptr;
			VkDeviceSize objectSize;
			VkDeviceSize objectCount;
			VkDeviceSize bufferRepeat;
		}buffer;
		TexArray textures;
	};
	union {
		//VkDescriptorBufferInfo bufferInfo;
		BufferInfoArray bufferInfo;
		ImageInfoArray imageInfo;
	};
	size_t count{ 1 };
	ShaderResourceInfo() { descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM; };
	ShaderResourceInfo(VkDescriptorType descriptorType_, VkShaderStageFlags shaderStage_, Vulkan::Buffer& buffer_, void* ptr_, VkDeviceSize objectSize_, VkDeviceSize objectCount_, VkDeviceSize bufferRepeat_, VkDescriptorBufferInfo* bufferInfo,size_t bufferInfoCount);
	ShaderResourceInfo(VkDescriptorType descriptorType_,VkShaderStageFlags shaderStage_, Vulkan::Texture* pTextures, size_t textureCount_);
	~ShaderResourceInfo() {};
};


class ShaderResourceCache {
	DescriptorSetPoolCache* pPool{ nullptr };
	DescriptorSetLayoutCache* pLayout{ nullptr };
	
	VulkContext vulkContext;
	std::vector<std::vector<VkDescriptorSetLayout>> descriptorLayouts;
	std::vector<std::vector<VkDescriptorSet>> descriptorSets;
	std::vector<std::vector<ShaderResourceInfo>> shaderResources;

public:
	ShaderResourceCache(VulkContext&vulkContext_,DescriptorSetPoolCache* pPool_, DescriptorSetLayoutCache* pLayout_);
	~ShaderResourceCache();
	ShaderResourceCache& AddResource(uint32_t set, uint32_t binding, VkDescriptorType descriptorType,VkShaderStageFlags shaderStage, VkDeviceSize objSize, VkDeviceSize objCount, VkDeviceSize bufferRepeat,bool isMapped=true);
	ShaderResourceCache& AddResource(uint32_t set, uint32_t binding, VkDescriptorType descriptorType,VkShaderStageFlags shaderStage, Vulkan::Texture* pTextures, size_t textureCount);
	void build();
};