#include "vk_descriptors.h"

VkDescriptorPool createPool(VkDevice device, const PoolSizes& poolSizes, int count, VkDescriptorPoolCreateFlags flags) {
	std::vector<VkDescriptorPoolSize> sizes;
	sizes.reserve(poolSizes.sizes.size());
	for (auto sz : poolSizes.sizes) {
		sizes.push_back({ sz.first,uint32_t(sz.second * count) });
	}
	VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.flags = flags;
	poolInfo.maxSets = count;
	poolInfo.poolSizeCount = (uint32_t)sizes.size();
	poolInfo.pPoolSizes = sizes.data();

	VkDescriptorPool descriptorPool;
	vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
	return descriptorPool;
}

void DescriptorAllocator::reset_pools() {
	for (auto p : usedPools) {
		vkResetDescriptorPool(device, p, 0);
	}
	freePools = usedPools;
	usedPools.clear();
	currentPool = VK_NULL_HANDLE;
}

bool DescriptorAllocator::allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout) {
	if (currentPool == VK_NULL_HANDLE) {
		currentPool = grab_pool();
		usedPools.push_back(currentPool);
	}
	VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.pSetLayouts = &layout;
	allocInfo.descriptorPool = currentPool;
	allocInfo.descriptorSetCount = 1;

	VkResult allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);

	bool needRealocated = false;

	switch (allocResult) {
	case VK_SUCCESS:
		return true;
		break;
	case VK_ERROR_FRAGMENTED_POOL:
	case VK_ERROR_OUT_OF_POOL_MEMORY:
		needRealocated = true;
		break;
	default://unrecoverable error
		return false;
	}
	if (needRealocated) {
		//allocate a new pool and retry
		currentPool = grab_pool();
		usedPools.push_back(currentPool);
		allocResult = vkAllocateDescriptorSets(device, &allocInfo, set);
		if (allocResult == VK_SUCCESS) {
			return true;
		}
	}
	return false;
	
}

void DescriptorAllocator::init(VkDevice newDevice) {
	device = newDevice;
}

void DescriptorAllocator::cleanup() {
	for (auto p : freePools) {
		vkDestroyDescriptorPool(device, p, nullptr);
	}
	for (auto p : usedPools) {
		vkDestroyDescriptorPool(device, p, nullptr);
	}
}

VkDescriptorPool DescriptorAllocator::grab_pool() {
	if (freePools.size() > 0) {
		VkDescriptorPool pool = freePools.back();
		freePools.pop_back();
		return pool;
	}
	else {
		return createPool(device, descriptorSizes, 1000, 0);
	}
}

void DescriptorLayoutCache::init(VkDevice newDevice) {
	device = newDevice;
}

VkDescriptorSetLayout DescriptorLayoutCache::create_descriptor_layout(VkDescriptorSetLayoutCreateInfo* info) {
	DescriptorLayoutInfo layoutInfo;
	layoutInfo.bindings.reserve(info->bindingCount);
	bool isSorted = true;
	int32_t lastBinding = -1;
	for (uint32_t i = 0; i < info->bindingCount; i++) {
		layoutInfo.bindings.push_back(info->pBindings[i]);
		//check that the bindings are in strict increasing order
		if (static_cast<int32_t>(info->pBindings[i].binding) > lastBinding) {
			lastBinding = info->pBindings[i].binding;
		}
		else {
			isSorted = false;
		}
	}
	if (!isSorted) {
		std::sort(layoutInfo.bindings.begin(), layoutInfo.bindings.end(), [](VkDescriptorSetLayoutBinding&a, VkDescriptorSetLayoutBinding& b) {return a.binding < b.binding; });
	}
	auto it = layoutCache.find(layoutInfo);
	if (it != layoutCache.end()) {
		return (*it).second;
	}
	else {
		VkDescriptorSetLayout layout;
		vkCreateDescriptorSetLayout(device, info, nullptr, &layout);
		//add to cach
		layoutCache[layoutInfo] = layout;
		return layout;
	}
	return VK_NULL_HANDLE;//shouldn't get here
}

void DescriptorLayoutCache::cleanup() {
	for (auto pair : layoutCache) {
		vkDestroyDescriptorSetLayout(device, pair.second, nullptr);
	}
}

bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other)const {
	if (other.bindings.size() != bindings.size()) {
		return false;//can't be a match
	}
	else {
		//compare each binding. Bindings are sorted, so will match
		for (int i = 0; i < bindings.size(); i++) {
			if (other.bindings[i].binding != bindings[i].binding) {
				return false;
			}
			if (other.bindings[i].descriptorType != bindings[i].descriptorType) {
				return false;
			}
			if (other.bindings[i].descriptorCount != bindings[i].descriptorCount) {
				return false;
			}
			if (other.bindings[i].stageFlags != bindings[i].stageFlags) {
				return false;
			}
		}
	}
	return true;
}

size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash()const {
	using std::size_t;
	using std::hash;

	size_t result = hash<size_t>()(bindings.size());
	for (const VkDescriptorSetLayoutBinding& b : bindings) {
		//pack the binding data int oa sing int64. 
		size_t binding_hash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;
		//shuffle the packed binding data and xor with with the main hash
		result ^= hash<size_t>()(binding_hash);
	}
	return result;
}

DescriptorBuilder DescriptorBuilder::begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator) {
	DescriptorBuilder builder;
	builder.cache = layoutCache;
	builder.alloc = allocator;
	return builder;
}

DescriptorBuilder& DescriptorBuilder::bind_buffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags) {
	VkDescriptorSetLayoutBinding newBinding{};
	newBinding.descriptorCount = 1;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	bindings.push_back(newBinding);

	VkWriteDescriptorSet newWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	newWrite.descriptorCount = 1;
	newWrite.descriptorType = type;
	newWrite.pBufferInfo = bufferInfo;
	newWrite.dstBinding = binding;
	writes.push_back(newWrite);
	return *this;
}

DescriptorBuilder& DescriptorBuilder::bind_image(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags) {
	VkDescriptorSetLayoutBinding newBinding{};
	newBinding.descriptorCount = 1;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	bindings.push_back(newBinding);

	VkWriteDescriptorSet newWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
	newWrite.pNext = nullptr;
	newWrite.descriptorCount = 1;
	newWrite.descriptorType = type;
	newWrite.pImageInfo = imageInfo;
	newWrite.dstBinding = binding;

	writes.push_back(newWrite);
	return *this;
}

bool DescriptorBuilder::build(VkDescriptorSet& set, VkDescriptorSetLayout& layout) {
	//build layout first
	VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	layoutInfo.pNext = nullptr;
	layoutInfo.pBindings = bindings.data();
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layout = cache->create_descriptor_layout(&layoutInfo);

	//allocate descriptor
	bool success = alloc->allocate(&set, layout);
	if (!success)
		return false;
	for (VkWriteDescriptorSet& w : writes) {
		w.dstSet = set;
	}
	vkUpdateDescriptorSets(*alloc, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	return true;
}

bool DescriptorBuilder::build(VkDescriptorSet& set) {
	VkDescriptorSetLayout layout;
	return build(set, layout);
}