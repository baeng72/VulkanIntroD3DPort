#pragma once
#include "../../../Common/Vulkan.h"
#include <vector>
#include <unordered_map>
#include <algorithm>


struct PoolSizes {
	std::vector<std::pair<VkDescriptorType, float>> sizes = {
		{VK_DESCRIPTOR_TYPE_SAMPLER,0.5f},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,4.0f},
		{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,4.0f},
		{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1.0f},
		{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,1.0f},
		{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,1.0f},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,2.0f},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2.0f},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,1.0f},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,1.0f},
		{VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,0.5f}
	};
};

class DescriptorAllocator {
	VkDevice device{ VK_NULL_HANDLE };
	VkDescriptorPool grab_pool();
	VkDescriptorPool currentPool{ VK_NULL_HANDLE };
	PoolSizes descriptorSizes;
	std::vector<VkDescriptorPool> usedPools;
	std::vector<VkDescriptorPool> freePools;
public:
	void reset_pools();
	bool allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout);
	void init(VkDevice newDevice);
	void cleanup();
	operator VkDevice()const { return device; }
};

class DescriptorLayoutCache {
	struct DescriptorLayoutInfo {
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		bool operator==(const DescriptorLayoutInfo&)const;
		size_t hash()const;
	};
	struct DescriptorLayoutHash {
		std::size_t operator()(const DescriptorLayoutInfo& k)const {
			return k.hash();
		}
	};
	std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash>layoutCache;
	VkDevice device;

public:
	void init(VkDevice newDevice);
	void cleanup();
	VkDescriptorSetLayout create_descriptor_layout(VkDescriptorSetLayoutCreateInfo* info);


};

class DescriptorBuilder {
	DescriptorLayoutCache* cache;
	DescriptorAllocator* alloc;
	std::vector<VkWriteDescriptorSet> writes;
	std::vector<VkDescriptorSetLayoutBinding> bindings;
public:
	static DescriptorBuilder begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator);
	DescriptorBuilder& bind_buffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
	DescriptorBuilder& bind_image(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
	bool build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);
	bool build(VkDescriptorSet& set);
};