#include "temp.h"
#include <stb_image.h>
#include <fstream>
#include "../../../ThirdParty/spirv-reflect/spirv_reflect.h"

DescriptorSetPoolCache::DescriptorSetPoolCache(VkDevice device_) :device(device_) {

}

DescriptorSetPoolCache::~DescriptorSetPoolCache() {
	for (auto& pool: freePools) {
		Vulkan::cleanupDescriptorPool(device, pool);
	}
	for (auto& pool : allocatedPools) {
		Vulkan::cleanupDescriptorPool(device, pool);
	}
}

VkDescriptorPool DescriptorSetPoolCache::getPool() {
	VkDescriptorPool descriptorPool;
	if (freePools.size() > 0) {
		descriptorPool = freePools.back();
		freePools.pop_back();

	}
	else {
		descriptorPool = createPool();
	}
	return descriptorPool;
}

VkDescriptorPool DescriptorSetPoolCache::createPool() {
	//descriptor types currently supported
	std::vector<VkDescriptorPoolSize> sizes = {
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,DESCRIPTOR_POOL_SIZE},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,DESCRIPTOR_POOL_SIZE},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,DESCRIPTOR_POOL_SIZE},
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,DESCRIPTOR_POOL_SIZE}
	};
	VkDescriptorPool descriptorPool = Vulkan::initDescriptorPool(device, sizes, DESCRIPTOR_POOL_SIZE);
	return descriptorPool;
}

void DescriptorSetPoolCache::resetPools() {
	for (auto p : allocatedPools) {
		vkResetDescriptorPool(device, p, 0);
	}
	freePools = allocatedPools;
	allocatedPools.clear();
	currentPool = VK_NULL_HANDLE;
}

bool DescriptorSetPoolCache::allocateDescriptorSets(VkDescriptorSet* pSets,  VkDescriptorSetLayout* pLayouts, uint32_t count) {
	if (currentPool == VK_NULL_HANDLE) {
		currentPool = getPool();
		allocatedPools.push_back(currentPool);
	}
	VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.pSetLayouts = pLayouts;
	allocInfo.descriptorSetCount = count;
	allocInfo.descriptorPool = currentPool;
	VkResult res = vkAllocateDescriptorSets(device, &allocInfo, pSets);

	if (res == VK_SUCCESS) {
		return true;
	}
	else if (res == VK_ERROR_FRAGMENTED_POOL || res == VK_ERROR_OUT_OF_POOL_MEMORY) {
		currentPool = getPool();
		allocatedPools.push_back(currentPool);
		res = vkAllocateDescriptorSets(device, &allocInfo, pSets);
		if (res == VK_SUCCESS)
			return true;
	}
	return false;
}

bool DescriptorSetPoolCache::allocateDescriptorSet(VkDescriptorSet* pSet, VkDescriptorSetLayout layout) {
	if (currentPool == VK_NULL_HANDLE) {
		currentPool = getPool();
		allocatedPools.push_back(currentPool);
	}
	VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.pSetLayouts = &layout;
	allocInfo.descriptorSetCount = 1;
	allocInfo.descriptorPool = currentPool;
	VkResult res = vkAllocateDescriptorSets(device, &allocInfo, pSet);

	if (res == VK_SUCCESS) {
		return true;
	}
	else if (res == VK_ERROR_FRAGMENTED_POOL || res == VK_ERROR_OUT_OF_POOL_MEMORY) {
		currentPool = getPool();
		allocatedPools.push_back(currentPool);
		res = vkAllocateDescriptorSets(device, &allocInfo, pSet);
		if (res == VK_SUCCESS)
			return true;
	}
	return false;
}


DescriptorSetLayoutCache::DescriptorSetLayoutCache(VkDevice device_) :device(device_) {

}

DescriptorSetLayoutCache::~DescriptorSetLayoutCache() {
	for (auto pair : layoutCache) {
		Vulkan::cleanupDescriptorSetLayout(device, pair.second);
	}
}

VkDescriptorSetLayout DescriptorSetLayoutCache::create(std::vector<VkDescriptorSetLayoutBinding>& bindings) {
	DescriptorSetLayoutInfo layoutInfo;
	uint32_t bindingCount = (uint32_t)bindings.size();
	layoutInfo.bindings.reserve(bindingCount);
	bool isSorted = true;
	int32_t lastBinding = -1;
	for (uint32_t i = 0; i < bindingCount; i++) {
		layoutInfo.bindings.push_back(bindings[i]);
		//check that the bindings are in strict increasing over
		if (static_cast<int32_t>(bindings[i].binding) > lastBinding) {
			lastBinding = bindings[i].binding;
		}
		else {
			isSorted = false;
		}
	}

	if (!isSorted) {
		std::sort(layoutInfo.bindings.begin(), layoutInfo.bindings.end(), [](VkDescriptorSetLayoutBinding& a, VkDescriptorSetLayoutBinding& b) {return a.binding < b.binding; });
	}

	VkDescriptorSetLayout descriptorLayout;
	auto it = layoutCache.find(layoutInfo);
	if (it != layoutCache.end()) {
		descriptorLayout =  (*it).second;
	}
	else {
		descriptorLayout = Vulkan::initDescriptorSetLayout(device, bindings);	
		layoutCache[layoutInfo] = descriptorLayout;
	}
	return descriptorLayout;
}

bool DescriptorSetLayoutCache::DescriptorSetLayoutInfo::operator==(const DescriptorSetLayoutInfo& other)const {
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

size_t DescriptorSetLayoutCache::DescriptorSetLayoutInfo::hash()const {
	size_t result = std::hash<size_t>()(bindings.size());
	for (const VkDescriptorSetLayoutBinding& b : bindings) {
		size_t bindingHash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;
		result ^= std::hash<size_t>()(bindingHash);
	}
	return result;
}

DescriptorSetBuilder DescriptorSetBuilder::begin(DescriptorSetPoolCache* pPool_, DescriptorSetLayoutCache* pLayout_)  {
	DescriptorSetBuilder builder;
	builder.pPool = pPool_;
	builder.pLayout = pLayout_;
	return builder;
}


DescriptorSetBuilder& DescriptorSetBuilder::AddBinding(uint32_t set, uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags,VkDescriptorBufferInfo*pBufferInfo) {
	VkDescriptorSetLayoutBinding newBinding{binding,type,1,stageFlags,nullptr};
	if (set >= bindings.size())
		bindings.resize(set + 1);
	bindings[set].push_back(newBinding);
	VkWriteDescriptorSet newWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,VK_NULL_HANDLE,binding,0,1,type,nullptr,pBufferInfo,nullptr };
	if (set >= writes.size())
		writes.resize(set + 1);
	writes[set].push_back(newWrite);
	return *this;
}

DescriptorSetBuilder& DescriptorSetBuilder::AddBinding(uint32_t set, uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags, VkDescriptorImageInfo* pImageInfo) {
	VkDescriptorSetLayoutBinding newBinding{ binding,type,1,stageFlags,nullptr };
	if (set >= bindings.size())
		bindings.resize(set + 1);
	bindings[set].push_back(newBinding);
	VkWriteDescriptorSet newWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,VK_NULL_HANDLE,binding,0,1,type,pImageInfo,nullptr,nullptr };
	if (set >= writes.size())
		writes.resize(set + 1);
	writes[set].push_back(newWrite);
	return *this;
}



bool DescriptorSetBuilder::build(std::vector<std::vector<VkDescriptorSet>>& sets, std::vector<std::vector<VkDescriptorSetLayout>>& layouts) {
	
	assert(bindings.size() == writes.size());
	sets.resize(bindings.size());
	layouts.resize(bindings.size());

	for (size_t i = 0; i < bindings.size(); i++) {
		//build layout first
		auto& setBindings = bindings[i];
		sets[i].resize(setBindings.size());
		layouts[i].resize(setBindings.size());
		if (setBindings.size()>1&&setBindings[0].binding == setBindings[1].binding) {
			//guess it's a texture array, so different descriptors for each
			for (size_t k = 0; k < setBindings.size();++k) {
				std::vector<VkDescriptorSetLayoutBinding> newBindings;
				newBindings.push_back(setBindings[k]);
				VkDescriptorSetLayout layout = pLayout->create(newBindings);
				layouts[i][k]=layout;
				//layouts.push_back(layout);
				VkDescriptorSet set;
				bool success = pPool->allocateDescriptorSet(&set, layout);
				if (success) {
					std::vector<VkWriteDescriptorSet> newWrites;
					newWrites.push_back(writes[i][k]);
					newWrites[0].dstSet = set;
					vkUpdateDescriptorSets(*pPool, static_cast<uint32_t>(newWrites.size()), newWrites.data(), 0, nullptr);
				}
				else
					return false;//bail on error
				sets[i][k]=set;
			}
		}
		else {
			VkDescriptorSetLayout layout = pLayout->create(setBindings);
			layouts[i][0]=layout;

			VkDescriptorSet set;
			bool success = pPool->allocateDescriptorSet(&set, layout);
			if (success) {
				for (VkWriteDescriptorSet& w : writes[i]) {
					w.dstSet = set;
				}
				vkUpdateDescriptorSets(*pPool, static_cast<uint32_t>(writes[i].size()), writes[i].data(), 0, nullptr);
			}
			else
				return false;//bail on error
			sets[i][0]=set;
			
		}
	}
	return true;
}

DescriptorSetBuilder2 DescriptorSetBuilder2::begin(DescriptorSetPoolCache* pPool_, DescriptorSetLayoutCache* pLayout_) {
	DescriptorSetBuilder2 builder;
	builder.pPool = pPool_;
	builder.pLayout = pLayout_;
	return builder;
}

DescriptorSetBuilder2& DescriptorSetBuilder2::AddBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stageFlags) {
	VkDescriptorSetLayoutBinding newBinding{ binding,type,1,stageFlags,nullptr };
	bindings.push_back(newBinding);
	return *this;
}

bool DescriptorSetBuilder2::build(VkDescriptorSet& set, VkDescriptorSetLayout& layout) {
	layout = pLayout->create(bindings);
	return pPool->allocateDescriptorSet(&set, layout);
}

bool DescriptorSetBuilder2::build(VkDescriptorSet& set) {
	VkDescriptorSetLayout layout;
	return build(set, layout);
}

bool DescriptorSetBuilder2::build(std::vector<VkDescriptorSet>& sets, VkDescriptorSetLayout& layout, uint32_t count) {
	layout = pLayout->create(bindings);
	sets.resize(count);
	std::vector<VkDescriptorSetLayout> layouts(count, layout);
	
	return pPool->allocateDescriptorSets(sets.data(), layouts.data(),count);
}

bool DescriptorSetBuilder2::build(std::vector<VkDescriptorSet>& sets, uint32_t count) {
	VkDescriptorSetLayout layout;
	return build(sets, layout, count);	
}

VulkanDescriptorList::VulkanDescriptorList(VkDevice device_, std::vector<VkDescriptorSet>& descriptorSetList_) :device(device_), descriptorSetList(descriptorSetList_) {

}

VulkanDescriptorList::~VulkanDescriptorList() {
	
}



void DescriptorSetUpdater::setBindings() {
	assert(pLayout->getBindings(descriptorSetLayout, bindings));
	writes.resize(bindings.size(), { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET });
	for(size_t i=0;i<bindings.size();++i){
		writes[i].descriptorType = bindings[i].descriptorType;
		writes[i].descriptorCount = bindings[i].descriptorCount;
		writes[i].dstSet = descriptorSet;		
	}
}

DescriptorSetUpdater DescriptorSetUpdater::begin(DescriptorSetLayoutCache* pLayout_,VkDescriptorSetLayout descriptorSetLayout_, VkDescriptorSet descriptorSet_) {
	DescriptorSetUpdater updater;
	updater.pLayout = pLayout_;
	updater.descriptorSetLayout = descriptorSetLayout_;
	updater.descriptorSet = descriptorSet_;
	updater.setBindings();
	return updater;
}

DescriptorSetUpdater& DescriptorSetUpdater::AddBinding(uint32_t binding_, VkDescriptorType descriptorType_, VkDescriptorBufferInfo* bufferInfo_) {
	assert(bindings.size() > binding_);
	assert(bindings[binding_].descriptorType == descriptorType_);
	//writes[binding_].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[binding_].pBufferInfo = bufferInfo_;
	return *this;
}



DescriptorSetUpdater& DescriptorSetUpdater::AddBinding(uint32_t binding_, VkDescriptorType descriptorType_, VkDescriptorImageInfo* imageInfo_) {
	assert(bindings.size() > binding_);
	assert(bindings[binding_].descriptorType == descriptorType_);
	writes[binding_].pImageInfo = imageInfo_;
	return *this;
}

void DescriptorSetUpdater::update() {
	vkUpdateDescriptorSets(*pLayout, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}

UniformBufferBuilder UniformBufferBuilder::begin(VkDevice device_,VkPhysicalDeviceProperties&deviceProperties_, VkPhysicalDeviceMemoryProperties& memoryProperties_, VkDescriptorType descriptorType_,bool isMapped_) {
	UniformBufferBuilder builder;
	builder.device = device_;
	builder.deviceProperties = deviceProperties_;
	builder.memoryProperties = memoryProperties_;
	builder.descriptorType = descriptorType_;
	builder.isMapped = isMapped_;
	return builder;
}

UniformBufferBuilder& UniformBufferBuilder::AddBuffer(VkDeviceSize objectSize_, VkDeviceSize objectCount_, VkDeviceSize repeatCount_) {
	bufferInfo.push_back({ objectSize_,objectCount_,repeatCount_,nullptr });
	return *this;
}

void UniformBufferBuilder::build(Vulkan::Buffer& buffer, std::vector<UniformBufferInfo>& bufferInfo_) {
	Vulkan::BufferProperties props;
#ifdef __USE__VMA__
	props.usage = isMapped ? VMA_MEMORY_USAGE_CPU_ONLY : VMA_MEMORY_USAGE_GPU_ONLY;
#else
	props.memoryProps = bufferInfo.isMapped ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
#endif
	VkDeviceSize alignment = 256;//jsut a guess 
	VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	//VkDescriptorType descriptorType = descriptorType;
	switch (descriptorType) {
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		alignment = deviceProperties.limits.minUniformBufferOffsetAlignment;
		break;

	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
		usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		alignment = deviceProperties.limits.minStorageBufferOffsetAlignment;
		break;
	}
	bufferInfo_.clear();
	VkDeviceSize totalSize = 0;
	for (auto& binfo : bufferInfo) {
		
		
		VkDeviceSize objectSize = (binfo.objectSize + alignment - 1) & ~(alignment - 1);

		VkDeviceSize bufferSize = objectSize * binfo.objectCount;
		UniformBufferInfo newInfo{};
		newInfo.objectSize = objectSize;
		newInfo.objectCount = binfo.objectCount;
		newInfo.repeatCount = binfo.repeatCount;
		bufferInfo_.push_back(newInfo);
		VkDeviceSize bufReptSize = binfo.repeatCount * bufferSize;
		totalSize += bufReptSize;
	}
	props.size = totalSize;
	Vulkan::initBuffer(device, memoryProperties, props, buffer);
	void* ptr = nullptr;
	if (isMapped) {
		ptr = Vulkan::mapBuffer(device, buffer);
		VkDeviceSize offset = 0;
		for (auto& binfo : bufferInfo_) {
			
			binfo.ptr = ((uint8_t*)ptr + offset);
			offset += binfo.objectSize * binfo.objectCount * binfo.repeatCount;
		}
	}
}

VulkanBuffer::VulkanBuffer(VkDevice device_, Vulkan::Buffer& buffer_) :device(device_), buffer(buffer_) {

}

VulkanBuffer::~VulkanBuffer() {
	Vulkan::cleanupBuffer(device, buffer);
}

UniformBuffer::UniformBuffer(VkDevice device_, Vulkan::Buffer& buffer_, std::vector<UniformBufferInfo>& bufferInfo_):VulkanBuffer(device_,buffer_),bufferInfo(bufferInfo_) {

}

TextureBuilder TextureBuilder::begin(VkDevice device_, VkCommandBuffer commandBuffer_, VkQueue queue_, VkPhysicalDeviceMemoryProperties memoryProperties_) {
	TextureBuilder builder;
	builder.device = device_;
	builder.commandBuffer = commandBuffer_;
	builder.queue = queue_;
	builder.memoryProperties = memoryProperties_;
	return builder;
}

TextureBuilder& TextureBuilder::AddTexture(uint8_t* pixels, uint32_t width, uint32_t height, VkFormat format, bool enableLod) {
	textureInfo.push_back({ pixels,width,height,format,enableLod });
	return *this;
}

bool TextureBuilder::build(std::vector<Vulkan::Texture>& textures) {
	textures.resize(textureInfo.size());

	for (size_t i = 0; i < textureInfo.size();++i) {
		auto& info = textureInfo[i];
		if (info.pixels == nullptr)return false;
		Vulkan::Texture texture;
		VkDeviceSize imageSize = (VkDeviceSize)(info.width * info.height * 4);
		Vulkan::TextureProperties props;
		props.format = PREFERRED_IMAGE_FORMAT;
		props.imageUsage = (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
#ifdef __USE__VMA__
		props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
#else
		props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
#endif
		props.width = (uint32_t)info.width;
		props.height = (uint32_t)info.height;
		props.mipLevels = info.enableLod ? 0 : 1;
		props.samplerProps.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		initTexture(device, memoryProperties, props, texture);
		Vulkan::transitionImage(device, queue, commandBuffer, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,texture.mipLevels);


		//we could cache the staging buffer and not allocate/deallocate each time, instead only reallaocate when larger required, but meh
		VkDeviceSize maxSize = imageSize;
		Vulkan::BufferProperties bufProps;
		bufProps.size = maxSize;
		bufProps.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
#ifdef __USE__VMA__
		bufProps.usage = VMA_MEMORY_USAGE_CPU_ONLY;
#else
		bufProps.memoryProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
#endif
		Vulkan::Buffer stagingBuffer;

		initBuffer(device, memoryProperties, bufProps, stagingBuffer);
		void* ptr = mapBuffer(device, stagingBuffer);
		//copy image data using staging buffer
		memcpy(ptr, info.pixels, imageSize);
		
		CopyBufferToImage(device, queue, commandBuffer, stagingBuffer, texture, info.width, info.height);

		//even if lod not enabled, need to transition, so use this code.
		Vulkan::generateMipMaps(device, queue, commandBuffer, texture);
		unmapBuffer(device, stagingBuffer);
		cleanupBuffer(device, stagingBuffer);
		textures[i] = texture;
	}
	return true;
}

ImageLoader ImageLoader::begin() {
	ImageLoader loader;
	return loader;
}

ImageLoader& ImageLoader::AddImage(const char* imagePath) {
	imagePaths.push_back(imagePath);
	return *this;
}

bool ImageLoader::build(std::vector<ImageInfo>& imageInfoList) {
	imageInfoList.resize(imagePaths.size());
	for (size_t i = 0; i < imagePaths.size();++i) {
		auto& imagePath = imagePaths[i];
		int texWidth, texHeight, texChannels;
		stbi_uc* texPixels = stbi_load(imagePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (texPixels == nullptr)
			return false;
		imageInfoList[i] = { texPixels,(uint32_t)texWidth,(uint32_t)texHeight,PREFERRED_FORMAT };

	}
	return true;
}

ImageInfo::~ImageInfo() {
	if(pixels!=nullptr)
		stbi_image_free(pixels);
}

TextureLoader::TextureLoader(VkDevice device_, VkCommandBuffer commandBuffer_, VkQueue queue_, VkPhysicalDeviceMemoryProperties memoryProperties_):device(device_),
	commandBuffer(commandBuffer_),queue(queue_),memoryProperties(memoryProperties_){
	
}

TextureLoader TextureLoader::begin(VkDevice device_, VkCommandBuffer commandBuffer_, VkQueue queue_, VkPhysicalDeviceMemoryProperties memoryProperties_) {
	TextureLoader loader(device_, commandBuffer_, queue_, memoryProperties_);
	return loader;
}

TextureLoader& TextureLoader::addTexture(const char* imagePath,bool enableLod) {
	imagePaths.push_back(imagePath);
	enableLods.push_back(enableLod);
	return *this;
}

void TextureLoader::loadTexture(uint8_t* pixels, uint32_t width, uint32_t height, VkFormat format,bool enableLod, Vulkan::Texture& texture) {
	VkDeviceSize imageSize = (VkDeviceSize)(width * height * 4);
	Vulkan::TextureProperties props;
	props.format = PREFERRED_IMAGE_FORMAT;
	props.imageUsage = (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
#endif
	props.width = (uint32_t)width;
	props.height = (uint32_t)height;
	props.mipLevels = enableLod ? 0 : 1;
	props.samplerProps.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	initTexture(device, memoryProperties, props, texture);
	Vulkan::transitionImage(device, queue, commandBuffer, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.mipLevels);


	//we could cache the staging buffer and not allocate/deallocate each time, instead only reallaocate when larger required, but meh
	VkDeviceSize maxSize = imageSize;
	Vulkan::BufferProperties bufProps;
	bufProps.size = maxSize;
	bufProps.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
#ifdef __USE__VMA__
	bufProps.usage = VMA_MEMORY_USAGE_CPU_ONLY;
#else
	bufProps.memoryProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
#endif
	Vulkan::Buffer stagingBuffer;

	initBuffer(device, memoryProperties, bufProps, stagingBuffer);
	void* ptr = mapBuffer(device, stagingBuffer);
	//copy image data using staging buffer
	memcpy(ptr, pixels, imageSize);

	CopyBufferToImage(device, queue, commandBuffer, stagingBuffer, texture, width, height);

	//even if lod not enabled, need to transition, so use this code.
	Vulkan::generateMipMaps(device, queue, commandBuffer, texture);
	unmapBuffer(device, stagingBuffer);
	cleanupBuffer(device, stagingBuffer);
}

bool TextureLoader::load(std::vector<Vulkan::Texture>& textures) {
	textures.resize(imagePaths.size());
	for (size_t i = 0; i < imagePaths.size(); ++i) {
		auto& imagePath = imagePaths[i];
		int texWidth, texHeight, texChannels;
		stbi_uc* texPixels = stbi_load(imagePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		if (texPixels == nullptr)
			return false;
		Vulkan::Texture texture;
		loadTexture(texPixels, (uint32_t)texWidth, (uint32_t)texHeight, PREFERRED_FORMAT, enableLods[i],texture);
		stbi_image_free(texPixels);
		textures[i] = texture;

	}
	return true;
}


VulkanTextureList::VulkanTextureList(VkDevice device_, std::vector<Vulkan::Texture>& textures_):device(device_) {
	textures = textures_;//best way to copy?
}
VulkanTextureList::~VulkanTextureList() {
	for (auto& texture : textures) {		
		
		Vulkan::cleanupTexture(device,texture);
		
	}
}

ShaderProgramLoader::ShaderProgramLoader(VkDevice device_) :device(device_) {

}

ShaderProgramLoader ShaderProgramLoader::begin(VkDevice device_) {
	ShaderProgramLoader loader(device_);
	return loader;
}

ShaderProgramLoader& ShaderProgramLoader::AddShaderPath(const char* shaderPath) {
	shaderPaths.push_back(shaderPath);
	return *this;
}

void ShaderProgramLoader::load(std::vector<Vulkan::ShaderModule>& shaders_) {
	//shaders_.resize(shaderPaths.size());
	for (size_t i = 0; i < shaderPaths.size(); ++i) {
		auto& shaderPath = shaderPaths[i];
		std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
		assert(file.is_open());


		size_t fileSize = (size_t)file.tellg();
		std::vector<char> shaderData(fileSize);

		file.seekg(0);
		file.read(shaderData.data(), fileSize);

		file.close();
		SpvReflectShaderModule module = {};
		SpvReflectResult result = spvReflectCreateShaderModule(shaderData.size(), shaderData.data(), &module);
		assert(result == SPV_REFLECT_RESULT_SUCCESS);


		VkShaderStageFlagBits stage;
		switch (module.shader_stage) {
		case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
			stage = VK_SHADER_STAGE_VERTEX_BIT;
			break;
		case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
			stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			break;
		case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
			stage = VK_SHADER_STAGE_GEOMETRY_BIT;
			break;
		case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
			stage = VK_SHADER_STAGE_COMPUTE_BIT;
			break;
		default:
			assert(0);
			break;
		}

		if (stage == VK_SHADER_STAGE_VERTEX_BIT) {

			vertexAttributeDescriptions.resize(module.input_variable_count);

			uint32_t offset = 0;
			std::vector<uint32_t> sizes(module.input_variable_count);
			std::vector<VkFormat> formats(module.input_variable_count);
			for (uint32_t i = 0; i < module.input_variable_count; ++i) {
				SpvReflectInterfaceVariable& inputVar = module.input_variables[i];
				uint32_t size = 0;
				VkFormat format;
				switch (inputVar.format) {

				case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
					format = VK_FORMAT_R32G32B32A32_SFLOAT;
					size = sizeof(float) * 4;
					break;
				case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
					format = VK_FORMAT_R32G32B32_SFLOAT;
					size = sizeof(float) * 3;
					break;
				case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
					format = VK_FORMAT_R32G32_SFLOAT;
					size = sizeof(float) * 2;
					break;
				default:
					assert(0);
					break;
				}
				sizes[inputVar.location] = (uint32_t)size;
				formats[inputVar.location] = format;
			}

			for (uint32_t i = 0; i < module.input_variable_count; i++) {

				vertexAttributeDescriptions[i].location = i;
				vertexAttributeDescriptions[i].offset = offset;
				vertexAttributeDescriptions[i].format = formats[i];
				offset += sizes[i];
			}
			vertexInputDescription.binding = 0;
			vertexInputDescription.stride = offset;
			vertexInputDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		}
		spvReflectDestroyShaderModule(&module);

		VkShaderModule shader = VK_NULL_HANDLE;

		VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		createInfo.codeSize = shaderData.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderData.data());
		VkResult res = vkCreateShaderModule(device, &createInfo, nullptr, &shader);
		assert(res == VK_SUCCESS);
		shaders.insert(std::pair<VkShaderStageFlagBits, VkShaderModule>(stage, shader));
		//shaders.push_back(shader);
		shaderData.clear();
	}
	//insert in order VERTEX,GEOMETRY,FRAGMENT
	shaders_.clear();
	if (shaders.find(VK_SHADER_STAGE_VERTEX_BIT) != shaders.end()) {
		Vulkan::ShaderModule vertexShader{ shaders[VK_SHADER_STAGE_VERTEX_BIT],VK_SHADER_STAGE_VERTEX_BIT };
		shaders_.push_back(vertexShader);
	}
	if (shaders.find(VK_SHADER_STAGE_GEOMETRY_BIT) != shaders.end()) {
		Vulkan::ShaderModule geometryShader{ shaders[VK_SHADER_STAGE_GEOMETRY_BIT],VK_SHADER_STAGE_GEOMETRY_BIT };
		shaders_.push_back(geometryShader);
	}
	if (shaders.find(VK_SHADER_STAGE_FRAGMENT_BIT) != shaders.end()) {
		Vulkan::ShaderModule fragmentShader{ shaders[VK_SHADER_STAGE_FRAGMENT_BIT],VK_SHADER_STAGE_FRAGMENT_BIT };
		shaders_.push_back(fragmentShader);
	}
}

bool ShaderProgramLoader::load(std::vector<Vulkan::ShaderModule>& shaders_, VkVertexInputBindingDescription& vertexInputDescription_, std::vector<VkVertexInputAttributeDescription>& vertexAttributeDescriptions_) {
	//shaders_.resize(shaderPaths.size());
	for (size_t i = 0; i < shaderPaths.size(); ++i) {
		auto& shaderPath = shaderPaths[i];
		std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
		assert(file.is_open());


		size_t fileSize = (size_t)file.tellg();
		std::vector<char> shaderData(fileSize);

		file.seekg(0);
		file.read(shaderData.data(), fileSize);

		file.close();
		SpvReflectShaderModule module = {};
		SpvReflectResult result = spvReflectCreateShaderModule(shaderData.size(), shaderData.data(), &module);
		assert(result == SPV_REFLECT_RESULT_SUCCESS);


		VkShaderStageFlagBits stage;
		switch (module.shader_stage) {
		case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
			stage = VK_SHADER_STAGE_VERTEX_BIT;
			break;
		case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
			stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			break;
		case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
			stage = VK_SHADER_STAGE_GEOMETRY_BIT;
			break;
		case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
			stage = VK_SHADER_STAGE_COMPUTE_BIT;
			break;
		default:
			assert(0);
			break;
		}

		if (stage == VK_SHADER_STAGE_VERTEX_BIT) {

			vertexAttributeDescriptions.resize(module.input_variable_count);

			uint32_t offset = 0;
			std::vector<uint32_t> sizes(module.input_variable_count);
			std::vector<VkFormat> formats(module.input_variable_count);
			for (uint32_t i = 0; i < module.input_variable_count; ++i) {
				SpvReflectInterfaceVariable& inputVar = module.input_variables[i];
				uint32_t size = 0;
				VkFormat format;
				switch (inputVar.format) {

				case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
					format = VK_FORMAT_R32G32B32A32_SFLOAT;
					size = sizeof(float) * 4;
					break;
				case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
					format = VK_FORMAT_R32G32B32_SFLOAT;
					size = sizeof(float) * 3;
					break;
				case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
					format = VK_FORMAT_R32G32_SFLOAT;
					size = sizeof(float) * 2;
					break;
				default:
					assert(0);
					break;
				}
				sizes[inputVar.location] = (uint32_t)size;
				formats[inputVar.location] = format;
			}

			for (uint32_t i = 0; i < module.input_variable_count; i++) {

				vertexAttributeDescriptions[i].location = i;
				vertexAttributeDescriptions[i].offset = offset;
				vertexAttributeDescriptions[i].format = formats[i];
				offset += sizes[i];
			}
			vertexInputDescription.binding = 0;
			vertexInputDescription.stride = offset;
			vertexInputDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		}
		spvReflectDestroyShaderModule(&module);

		VkShaderModule shader = VK_NULL_HANDLE;

		VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
		createInfo.codeSize = shaderData.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderData.data());
		VkResult res = vkCreateShaderModule(device, &createInfo, nullptr, &shader);
		assert(res == VK_SUCCESS);
		shaders.insert(std::pair<VkShaderStageFlagBits, VkShaderModule>(stage, shader));
		//shaders.push_back(shader);
		shaderData.clear();
	}
	//insert in order VERTEX,GEOMETRY,FRAGMENT
	shaders_.clear();
	if (shaders.find(VK_SHADER_STAGE_VERTEX_BIT) != shaders.end()) {	
		Vulkan::ShaderModule vertexShader{ shaders[VK_SHADER_STAGE_VERTEX_BIT],VK_SHADER_STAGE_VERTEX_BIT };
		shaders_.push_back(vertexShader);
		vertexInputDescription_ = vertexInputDescription;
		vertexAttributeDescriptions_ = vertexAttributeDescriptions;
	}
	else {
		return false;//no input descriptions usefull for pipeline
	}
	if (shaders.find(VK_SHADER_STAGE_GEOMETRY_BIT) != shaders.end()) {		
		Vulkan::ShaderModule geometryShader{ shaders[VK_SHADER_STAGE_GEOMETRY_BIT],VK_SHADER_STAGE_GEOMETRY_BIT };
		shaders_.push_back(geometryShader);
	}
	if (shaders.find(VK_SHADER_STAGE_FRAGMENT_BIT) != shaders.end()) {		
		Vulkan::ShaderModule fragmentShader{ shaders[VK_SHADER_STAGE_FRAGMENT_BIT],VK_SHADER_STAGE_FRAGMENT_BIT };
		shaders_.push_back(fragmentShader);
	}
	return true;

}

PipelineLayoutBuilder::PipelineLayoutBuilder(VkDevice device_) :device(device_) {

}

PipelineLayoutBuilder PipelineLayoutBuilder::begin(VkDevice device_) {
	PipelineLayoutBuilder builder(device_);
	return builder;
}

PipelineLayoutBuilder& PipelineLayoutBuilder::AddDescriptorSetLayout(VkDescriptorSetLayout layout_) {	
	descriptorSetLayouts.push_back(layout_);	
	return *this;
}

PipelineLayoutBuilder& PipelineLayoutBuilder::AddDescriptorSetLayouts(std::vector<VkDescriptorSetLayout>& layouts_) {
	descriptorSetLayouts.insert(descriptorSetLayouts.end(), layouts_.begin(),layouts_.end());	
	return *this;
}

void PipelineLayoutBuilder::build(VkPipelineLayout& pipelineLayout) {
	pipelineLayout = Vulkan::initPipelineLayout(device, descriptorSetLayouts);
}

VulkanPipelineLayout::VulkanPipelineLayout(VkDevice device_, VkPipelineLayout pipelineLayout_) :device(device_), pipelineLayout(pipelineLayout_) {
}

VulkanPipelineLayout::~VulkanPipelineLayout() {
	Vulkan::cleanupPipelineLayout(device, pipelineLayout);
}



PipelineBuilder::PipelineBuilder(VkDevice device_, VkPipelineLayout pipelineLayout_, VkRenderPass renderPass_, std::vector<Vulkan::ShaderModule>& shaders_, VkVertexInputBindingDescription& vertexInputDescription_, std::vector<VkVertexInputAttributeDescription>& vertexAttributeDescriptions_):
device(device_),pipelineLayout(pipelineLayout_),shaders(shaders_),renderPass(renderPass_), vertexInputDescription(vertexInputDescription_),vertexAttributeDescriptions(vertexAttributeDescriptions_){
	
}


PipelineBuilder PipelineBuilder::begin(VkDevice device_, VkPipelineLayout pipelineLayout_,VkRenderPass renderPass_, std::vector<Vulkan::ShaderModule>& shaders_, VkVertexInputBindingDescription& vertexInputDescription_, std::vector<VkVertexInputAttributeDescription>& vertexAttributeDescriptions_) {
	PipelineBuilder builder(device_, pipelineLayout_, renderPass_, shaders_, vertexInputDescription_, vertexAttributeDescriptions_);
	return builder;
}

PipelineBuilder& PipelineBuilder::setCullMode(VkCullModeFlagBits cullMode_) {
	cullMode = cullMode_;
	return *this;
}

PipelineBuilder& PipelineBuilder::setPolygonMode(VkPolygonMode polygonMode_) {
	polygonMode = polygonMode_;
	return *this;
}

PipelineBuilder& PipelineBuilder::setFrontFace(VkFrontFace frontFace_) {
	frontFace = frontFace_;
	return *this;
}

PipelineBuilder& PipelineBuilder::setBlend(VkBool32 blend_) {
	blend = blend_;
	return *this;
}

PipelineBuilder& PipelineBuilder::setDepthTest(VkBool32 depthTest_) {
	depthTest = depthTest_;
	return *this;
}

PipelineBuilder& PipelineBuilder::setStencilTest(VkBool32 stencilTest_) {
	stencilTest = stencilTest_;
	return *this;
}

PipelineBuilder& PipelineBuilder::setNoDraw(VkBool32 noDraw_) {
	noDraw = noDraw_;
	return *this;
}

PipelineBuilder& PipelineBuilder::setStencilState(VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp, uint32_t compareMask, uint32_t writeMask, uint32_t reference) {
	stencil.failOp = failOp;
	stencil.passOp = passOp;
	stencil.depthFailOp = depthFailOp;
	stencil.compareOp = compareOp;
	stencil.compareMask = compareMask;
	stencil.writeMask = writeMask;
	stencil.reference = reference;
	return *this;
}

void PipelineBuilder::build(VkPipeline& pipeline) {
	Vulkan::PipelineInfo pipelineInfo;
	pipelineInfo.cullMode = cullMode;
	pipelineInfo.depthTest = depthTest;
	pipelineInfo.polygonMode = polygonMode;
	pipelineInfo.blend = blend;
	pipelineInfo.stencilTest = stencilTest;
	pipelineInfo.stencil = stencil;
	pipelineInfo.frontFace = frontFace;
	pipelineInfo.noDraw = noDraw;
	pipeline = Vulkan::initGraphicsPipeline(device, renderPass, pipelineLayout, shaders, vertexInputDescription, vertexAttributeDescriptions, pipelineInfo);

}

VulkanPipeline::VulkanPipeline(VkDevice device_, VkPipeline pipeline_) :device(device_), pipeline(pipeline_) {
}

VulkanPipeline::~VulkanPipeline() {
	Vulkan::cleanupPipeline(device, pipeline);
}


ShaderResourceInfo::ShaderResourceInfo(VkDescriptorType descriptorType_,VkShaderStageFlags shaderStage_, Vulkan::Buffer& buffer_, void* ptr_, VkDeviceSize objectSize_, VkDeviceSize objectCount_, VkDeviceSize bufferRepeat_,VkDescriptorBufferInfo*bufferInfo_,size_t bufferInfoCount) {
	descriptorType = descriptorType_;
	shaderStage = shaderStage_;
	buffer.buffer = buffer_;
	buffer.ptr = ptr_;
	buffer.objectSize = objectSize_;
	buffer.objectCount = objectCount_;
	buffer.bufferRepeat = bufferRepeat_;
//	bufferInfo = bufferInfo_;
	bufferInfo.set(bufferInfo_, bufferInfoCount);
}
ShaderResourceInfo::ShaderResourceInfo(VkDescriptorType descriptorType_,VkShaderStageFlags shaderStage_, Vulkan::Texture* pTextures, size_t textureCount) {
	descriptorType = descriptorType_;
	shaderStage = shaderStage_;
	textures.set(pTextures, textureCount);

}

ShaderResourceCache::ShaderResourceCache(VulkContext&vulkContext, DescriptorSetPoolCache* pPool_, DescriptorSetLayoutCache* pLayout_) :vulkContext(vulkContext), pPool(pPool_), pLayout(pLayout_) {

}

ShaderResourceCache::~ShaderResourceCache() {

}
#if 0
TexArray::TexArray() {
	pTextures = nullptr;
}
TexArray::~TexArray() {
	if (pTextures != nullptr)
		delete[] pTextures;
}

void TexArray::set(Vulkan::Texture* pTex, size_t count_) {
	pTextures = new Vulkan::Texture[count_];
	memcpy(pTextures, pTex, sizeof(Vulkan::Texture) * count_);
	count = count_;

}
#endif
#if 0
ImageInfoArray::ImageInfoArray() {
	pInfo = nullptr;
}
ImageInfoArray::~ImageInfoArray() {
	if (pInfo != nullptr)
		delete[] pInfo;
}

void ImageInfoArray::set(VkDescriptorImageInfo* pInfo_, size_t count_) {
	count = count_;
	pInfo = new VkDescriptorImageInfo[count];
	memcpy(pInfo, pInfo_, sizeof(VkDescriptorImageInfo) * count);
}
#endif
ShaderResourceCache& ShaderResourceCache::AddResource(uint32_t set, uint32_t binding, VkDescriptorType descriptorType,VkShaderStageFlags shaderStage, VkDeviceSize objSize, VkDeviceSize objCount, VkDeviceSize bufferRepeat,bool isMapped) {
	Vulkan::Buffer buffer;
	Vulkan::BufferProperties props;
#ifdef __USE__VMA__
	props.usage = isMapped ? VMA_MEMORY_USAGE_CPU_ONLY : VMA_MEMORY_USAGE_GPU_ONLY;
#else
	props.memoryProps = bufferInfo.isMapped ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
#endif
	VkDeviceSize alignment = 256;//jsut a guess 
	VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;	
	switch (descriptorType) {
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		alignment = vulkContext.deviceProperties.limits.minUniformBufferOffsetAlignment;
		break;

	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
		usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		alignment = vulkContext.deviceProperties.limits.minStorageBufferOffsetAlignment;
		break;
	}
	VkDeviceSize objectSize = (objSize + alignment - 1) & ~(alignment - 1);
	VkDeviceSize bufferSize = objectSize * objCount;
	VkDeviceSize range = bufferSize;
	VkDeviceSize totalSize = bufferRepeat * bufferSize;
	props.size = totalSize;
	Vulkan::initBuffer(vulkContext, vulkContext.memoryProperties, props, buffer);
	void* ptr = nullptr;
	if (isMapped) {
		ptr = Vulkan::mapBuffer(vulkContext, buffer);
	}
	if (set + 1 > shaderResources.size()) {
		shaderResources.resize(set + 1);
	}
	if ((binding + 1) > shaderResources[set].size()) {
		shaderResources[set].resize(binding + 1);
	}
	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = buffer.buffer;
	bufferInfo.offset = 0;
	bufferInfo.range = totalSize;
	shaderResources[set][binding] = ShaderResourceInfo(descriptorType,shaderStage, buffer, ptr, objectSize, objCount, bufferRepeat,&bufferInfo,1);
	/*shaderResources[set][binding].buffer.buffer = buffer;
	shaderResources[set][binding].buffer.objectCount = objCount;
	shaderResources[set][binding].buffer.objectSize = objectSize;
	shaderResources[set][binding].buffer.bufferRepeat = bufferRepeat;
	shaderResources[set][binding].descriptorType = descriptorType;*/
	
	return *this;
}


ShaderResourceCache& ShaderResourceCache::AddResource(uint32_t set, uint32_t binding, VkDescriptorType descriptorType,VkShaderStageFlags shaderStage, Vulkan::Texture* pTextures, size_t textureCount){
	if (set + 1 > shaderResources.size()) {
		shaderResources.resize(set + 1);
	}
	if ((binding + 1) > shaderResources[set].size()) {
		shaderResources[set].resize(binding + 1);
	}
	
	shaderResources[set][binding] = ShaderResourceInfo(descriptorType,shaderStage, pTextures, textureCount);
	std::vector<VkDescriptorImageInfo> imageInfo(textureCount);
	for (size_t i = 0; i < textureCount; i++) {
		imageInfo[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo[i].imageView = pTextures[i].imageView;
		imageInfo[i].sampler = pTextures[i].sampler;
	}
	shaderResources[set][binding].imageInfo.set(imageInfo.data(), textureCount);
	return *this;
}

void ShaderResourceCache::build() {
	DescriptorSetBuilder descriptorBuilder = DescriptorSetBuilder::begin(pPool, pLayout);
	for(uint32_t set=0;set<shaderResources.size();++set){
		
		for (uint32_t binding = 0; binding < shaderResources[set].size(); ++binding) {
			switch (shaderResources[set][binding].descriptorType) {
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
				for(size_t i=0;i<shaderResources[set][binding].count;++i)
					descriptorBuilder.AddBinding(set, binding, shaderResources[set][binding].descriptorType, shaderResources[set][binding].shaderStage,  &shaderResources[set][binding].bufferInfo[i]);
				break;
			case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:								
				for(size_t i=0;i<shaderResources[set][binding].count;++i)
					descriptorBuilder.AddBinding(set, binding, shaderResources[set][binding].descriptorType, shaderResources[set][binding].shaderStage,  &shaderResources[set][binding].imageInfo[i]);				
				break;
			}
			
		}
		
	}
	descriptorBuilder.build(descriptorSets, descriptorLayouts);
}