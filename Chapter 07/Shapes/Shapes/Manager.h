#pragma once
#include "../../Common/Vulkan.h"
#include <unordered_map>

struct VulkanInfo {
	VkDevice device;
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceMemoryProperties memoryProperties;
	VkFormatProperties formatProperties;
	VkQueue queue;
	VkCommandBuffer commandBuffer;
	operator VkDevice()const { return device; }
	operator VkQueue()const { return queue; }
	operator VkCommandBuffer()const { return commandBuffer; }
};

struct TextureDesc {
	Vulkan::Image	image;
	uint32_t set{ 0 };
	VkShaderStageFlagBits stage;
	VkDescriptorImageInfo imageInfo{};
};

struct Shader {
	Vulkan::ShaderModule vertShader{ VK_NULL_HANDLE,VK_SHADER_STAGE_VERTEX_BIT };
	Vulkan::ShaderModule fragShader{ VK_NULL_HANDLE,VK_SHADER_STAGE_FRAGMENT_BIT };
	Vulkan::ShaderModule geomShader{ VK_NULL_HANDLE,VK_SHADER_STAGE_GEOMETRY_BIT };//probably superfluous
};

struct BufferDesc {
	Vulkan::Buffer buffer;
	uint32_t set{ 0 };
	VkShaderStageFlagBits stage;
	void* ptr{ nullptr };
	VkDescriptorBufferInfo bufferInfo;
};

struct DynamicBufferDesc : public BufferDesc {
	VkDeviceSize objectSize{ 0 };
};

struct DescriptorCreateInfo {
	std::vector<size_t> descriptorIndices;
};


struct DescriptorInfo {
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
	VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
};

struct PipelineInfo {
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkPipeline pipeline{ VK_NULL_HANDLE };
};

struct DrawInfo {
	PFN_vkCmdBindPipeline pvkCmdBindPipeline{ nullptr };
	PFN_vkCmdBindDescriptorSets pvkCmdBindDescriptorSets{ nullptr };
	PFN_vkCmdBindVertexBuffers pvkCmdBindVertexBuffers{ nullptr };
	PFN_vkCmdBindIndexBuffer pvkCmdBindIndexBuffer{ nullptr };
	PFN_vkCmdDrawIndexed pvkCmdDrawIndexed{ nullptr };
};


struct DrawObjectInfo {
	std::vector<size_t> descriptorIndices;
	size_t pipelineIndex;
	VkVertexInputBindingDescription bindingDescription;
	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	size_t vertShaderIndex{ 0 };
	size_t fragShaderIndex{ 0 };
	bool depthEnable{ true };
	bool blendEnable{ false };
	VkFrontFace frontFace{ VK_FRONT_FACE_CLOCKWISE };
	VkCullModeFlagBits cullMode{ VK_CULL_MODE_BACK_BIT };
};

struct DrawObject {
	std::vector<DescriptorInfo> descriptors;
	PipelineInfo pipeline;	

	
};

class Manager {
	VulkanInfo vulkanInfo;
	size_t indexCount{ 0 };
	std::unordered_map<size_t, TextureDesc> DiffuseMaps;
	std::unordered_map<size_t, TextureDesc> SpecularMaps;
	std::unordered_map<size_t, TextureDesc> NormalMaps;
	std::unordered_map<size_t, std::vector<TextureDesc>> TextureArrays;
	std::unordered_map<size_t, std::vector<TextureDesc>> AlphaMaps;
	std::unordered_map<size_t, TextureDesc> LightMaps;
	std::unordered_map<size_t, std::vector<VkDescriptorImageInfo>> ImageScratch;
	std::unordered_map<size_t, std::vector<VkDescriptorImageInfo>> AlphaScratch;
	std::unordered_map<size_t, BufferDesc> UniformBuffers;
	std::unordered_map<size_t, DynamicBufferDesc> DynamicUniformBuffers;
	std::unordered_map<size_t, BufferDesc> InstanceBuffers;
	std::unordered_map<size_t, BufferDesc> StorageBuffers;
	std::unordered_map<size_t, Vulkan::Buffer> VertexBuffers;
	std::unordered_map<size_t, Vulkan::Buffer> IndexBuffers;
	std::unordered_map<size_t, DescriptorInfo> Descriptors;
	std::unordered_map<size_t, PipelineInfo> Pipelines;
	std::unordered_map<size_t, Shader>Shaders;
	std::unordered_map<size_t, DrawObject> DrawObjects;
	std::unordered_map<size_t, uint32_t> IndexCounts;
	std::unordered_map<size_t, uint32_t> InstanceCounts;
	void LoadTexture(TextureDesc& textureDesc, const char* path, VkShaderStageFlagBits shaderStages, bool enabledLod);
public:
	Manager(VulkanInfo& vulkanInfo);
	Manager(const Manager& rhs) = delete;
	Manager& operator=(const Manager& rhs) = delete;
	~Manager();

	size_t LoadDiffuseMap(const char* path, VkShaderStageFlagBits shaderStages,bool enableLod = false,uint32_t set=0);
	size_t LoadSpecularMap(const char* path, VkShaderStageFlagBits shaderStages,bool enableLod = false,uint32_t set=0);
	size_t LoadNormalMap(const char* path, VkShaderStageFlagBits shaderStages, bool enableLod = false,uint32_t set=0);
	void FreeDiffuseMap(size_t index);
	void FreeSpecularMap(size_t index);
	void FreeNormalMap(size_t index);

	size_t LoadAlphaMapArrayFromMemory(uint8_t* pImageData, VkFormat format, uint32_t imageWidth, uint32_t imageHeight, VkShaderStageFlagBits shaderStages, bool enableLod=false,uint32_t set=0);
	void FreeAlphaMapArray(size_t index);

	size_t LoadTextureArray(std::vector<const char*>& paths, VkShaderStageFlagBits shaderStages, bool enabledLod=false,uint32_t set=0);
	void FreeTextureArray(size_t index);

	size_t LoadLightMapFromMemory(uint8_t* pImageData, VkFormat format, uint32_t imageWidth, uint32_t imageHeight, VkShaderStageFlags shaderStages, bool enableLod=false,uint32_t set=0);
	void FreeLightMap(size_t index);

	size_t InitObjectBuffersWithData(size_t oldIndex,void* vertices, VkDeviceSize vertexSize, uint32_t* indices, VkDeviceSize indexSize);
	void FreeObjectBuffers(size_t index);
	size_t InitVertexBuffer(size_t oldIndex,VkDeviceSize vertexSize);
	void UpdateVertexBuffer(size_t index, void* ptrData, VkDeviceSize dataSize);
	void FreeVertexBuffer(size_t index);

	size_t InitIndexBuffer(size_t oldIndex,VkDeviceSize indexSize);	
	void UpdateIndexBuffer(size_t index, uint32_t* ptrData, VkDeviceSize dataSize);
	void FreeIndexBuffer(size_t index);

	uint32_t GetIndexCount(size_t index);

	size_t InitInstanceBuffer(VkDeviceSize instanceSize, uint32_t instanceCount, VkShaderStageFlagBits stage,uint32_t set=0);
	
	size_t InitStorageBuffer(VkDeviceSize storageSize, uint32_t storageCount, VkShaderStageFlagBits stage,uint32_t set=0);
	
	size_t InitUniformBuffer(VkDeviceSize uniformSize, VkShaderStageFlagBits stage,uint32_t set=0);
	size_t InitDynamicUniformBuffer(VkDeviceSize uniformSize, uint32_t uniformCount, VkShaderStageFlagBits stage,uint32_t set=0);

	
	void* GetUniformPtr(size_t index);	
	void* GetDynamicUniformPtr(size_t index);	
	VkDeviceSize GetDynamicUniformObjectSize(size_t index);	
	void* GetInstancePtr(size_t index);	
	void* GetStoragePtr(size_t index);

	uint32_t GetInstanceCount(size_t index);
	
	size_t InitShaders(const char* vertPath, const char* fragPath, const char* geomPath = nullptr);		
	std::vector<Vulkan::ShaderModule> GetShaderModules(size_t index);
	
	std::vector<VkDescriptorSetLayoutBinding> GetLayoutSetBindings(size_t index,uint32_t set=0);	
	std::vector<VkDescriptorSetLayoutBinding> GetLayoutSetBindings(size_t uniformIndex, size_t dynamicUniformIndex, size_t instanceIndex, size_t storageIndex, size_t diffuseIndex,uint32_t set=0);

	
	std::vector<VkDescriptorPoolSize> GetDescriptorPoolSizes(size_t index);	
	std::vector<VkDescriptorPoolSize> GetDescriptorPoolSizes(size_t index, size_t instanceHash, size_t storageHash, size_t diffuseHash);
	
	std::vector<VkWriteDescriptorSet> GetDescriptorWrites(VkDescriptorSet descriptorSet, size_t index);	

	void UpdateDescriptors(size_t index);

	size_t InitDrawObject(DrawObjectInfo& drawObjectInfo, VkRenderPass renderPass, VkSampleCountFlagBits samples);

	void Draw(size_t drawObjIndex, size_t vertexIndex, size_t indexIndex, DrawInfo& drawInfo, VkCommandBuffer cmd);

	operator VkDevice() { return vulkanInfo.device; }
	





	
};