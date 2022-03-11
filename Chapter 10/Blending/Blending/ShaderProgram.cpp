#include "ShaderProgram.h"
#include "../../../ThirdParty/spirv-reflect/spirv_reflect.h"

TextureArray::TextureArray() {
	pTextures = nullptr;
}
TextureArray::~TextureArray() {
	if (pTextures != nullptr)
		delete[] pTextures;
}

void TextureArray::set(Vulkan::Texture* pTex, size_t count_) {
	pTextures = new Vulkan::Texture[count_];
	memcpy(pTextures, pTex, sizeof(Vulkan::Texture) * count_);
	count = count_;

}

ShaderResource::ShaderResource(VkDescriptorType descriptorType_, VkShaderStageFlags shaderStage_, size_t size_, size_t count_, bool isMapped_) {
	descriptorType = descriptorType_;
	shaderStage = shaderStage_;
	type = prtBuffer;
	buffer.size = size_;
	buffer.count = count_;
	buffer.isMapped = isMapped_;
}

ShaderResource::ShaderResource(VkDescriptorType descriptorType_, VkShaderStageFlags shaderStage_, size_t width_, size_t height_, size_t pixelSize_, uint8_t* pixels_, bool enableLod_) {
	descriptorType = descriptorType_;
	shaderStage = shaderStage_;
	type = prtImageData;//raw image data that needs to be converted to texture
	imageData.width = width_;
	imageData.height = height_;
	imageData.pixelSize = pixelSize_;
	imageData.pixels = pixels_;
	imageData.enableLod = enableLod_;
}

ShaderResource::ShaderResource(VkDescriptorType descriptorType_, VkShaderStageFlags shaderStage_, Vulkan::Texture* pTextures_, size_t count_) {
	descriptorType = descriptorType_;
	shaderStage = shaderStage_;
	type = prtTexture;
	textures.set(pTextures_, count_);
}

ShaderResInternal::ShaderResInternal(VkDescriptorType descriptorType_, Vulkan::Buffer& buffer_, void* ptr, VkDeviceSize objectSize, VkDeviceSize passSize, VkDeviceSize totalSize) {
	descriptorType = descriptorType_;
	buffer.buffer = buffer_;
	buffer.ptr = ptr;
	buffer.objectSize = objectSize;
	buffer.passSize = passSize;
	buffer.totalSize = totalSize;
}

void ShaderResources::AddDescriptorSet(std::vector<std::vector<DescriptorInfo>>& descriptorSetInfos, uint32_t passCount) {
	std::unordered_map<VkDescriptorType, uint32_t> descriptorTypeMap;
	for (auto& descriptorSetInfo : descriptorSetInfos) {
		for (auto& descriptorInfo : descriptorSetInfo) {
			if (descriptorTypeMap.find(descriptorInfo.descriptorType) == descriptorTypeMap.end()) {
				descriptorTypeMap.insert(std::pair<VkDescriptorType, uint32_t>(descriptorInfo.descriptorType, 1));
			}
			else {
				descriptorTypeMap[descriptorInfo.descriptorType]++;
			}
		}
	}
	std::vector<VkDescriptorPoolSize> poolSizes;
	uint32_t maxSet = 0;
	for (auto& pair : descriptorTypeMap) {
		poolSizes.push_back({ pair.first,pair.second * passCount });
		maxSet = std::max(pair.second, maxSet);
	}
	maxSet = std::max(maxSet, (uint32_t)descriptorSetInfos.size());
	descriptorPool = Vulkan::initDescriptorPool(vulkanContext, poolSizes, passCount * maxSet);

	for (auto& descriptorSetInfo : descriptorSetInfos) {
		std::vector<VkDescriptorSetLayoutBinding> descriptorBindings;
		uint32_t binding = 0;

		for (auto& descriptorInfo : descriptorSetInfo) {
			descriptorBindings.push_back({ binding,descriptorInfo.descriptorType,1,descriptorInfo.shaderStage });
		}
		VkDescriptorSetLayout descriptorSetLayout = Vulkan::initDescriptorSetLayout(vulkanContext, descriptorBindings);
		descriptorSetLayouts.push_back(descriptorSetLayout);
		std::vector<VkDescriptorSet> descriptorSet;
		for (uint32_t i = 0; i < passCount; ++i) {
			descriptorSet.push_back(Vulkan::initDescriptorSet(vulkanContext, descriptorSetLayout, descriptorPool));
		}
		descriptorSets.push_back(descriptorSet);
	}
}

void ShaderResources::AddBuffer(ShaderResource& res, uint32_t passCount, ShaderResInternal& pipeRes) {
	Vulkan::Buffer buffer;
	Vulkan::BufferProperties props;
#ifdef __USE__VMA__
	props.usage = res.buffer.isMapped ? VMA_MEMORY_USAGE_CPU_ONLY : VMA_MEMORY_USAGE_GPU_ONLY;
#else
	props.memoryProps = bufferInfo.isMapped ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
#endif
	VkDeviceSize alignment = 256;//jsut a guess 
	VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	VkDescriptorType descriptorType = res.descriptorType;
	switch (descriptorType) {
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
	case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
		usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		alignment = vulkanContext.deviceProperties.limits.minUniformBufferOffsetAlignment;
		break;

	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
	case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
		usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		alignment = vulkanContext.deviceProperties.limits.minStorageBufferOffsetAlignment;
		break;
	}
	VkDeviceSize objectSize = (res.buffer.size + alignment - 1) & ~(alignment - 1);
	VkDeviceSize bufferSize = objectSize * res.buffer.count;
	VkDeviceSize range = bufferSize;
	VkDeviceSize totalSize = passCount * bufferSize;
	props.size = totalSize;
	Vulkan::initBuffer(vulkanContext, vulkanContext.memoryProperties, props, buffer);
	void* ptr = nullptr;
	if (res.buffer.isMapped) {
		ptr = Vulkan::mapBuffer(vulkanContext, buffer);
	}
	pipeRes = { descriptorType,buffer,ptr,objectSize,range,totalSize };
}

void ShaderResources::AddTexture(ShaderResource& res, ShaderResInternal& pipeRes) {
	//texture should already be initialized at this point
	VkDescriptorType descriptorType = res.descriptorType;

	pipeRes = { descriptorType,res.textures };
}

void ShaderResources::AddImageData(ShaderResource& res, ShaderResInternal& pipeRes) {
	VkDescriptorType descriptorType = res.descriptorType;
	assert(res.imageData.pixels != nullptr);
	assert((int)(res.imageData.width * res.imageData.height) > 0);
	assert(res.imageData.pixelSize > 0);
	VkDeviceSize imageSize = res.imageData.width * res.imageData.height * res.imageData.pixelSize;
	Vulkan::TextureProperties props;
	Vulkan::Texture texture;
	props.format = PREFERRED_IMAGE_FORMAT;
	props.imageUsage = (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
#endif
	props.width = (uint32_t)res.imageData.width;
	props.height = (uint32_t)res.imageData.height;
	props.mipLevels = res.imageData.enableLod ? 0 : 1;
	props.samplerProps.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	initTexture(vulkanContext, vulkanContext.memoryProperties, props, texture);
	Vulkan::transitionImage(vulkanContext, vulkanContext, vulkanContext, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.mipLevels);

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

	initBuffer(vulkanContext, vulkanContext.memoryProperties, bufProps, stagingBuffer);
	void* ptr = mapBuffer(vulkanContext, stagingBuffer);
	//copy image data using staging buffer
	memcpy(ptr, res.imageData.pixels, imageSize);
	CopyBufferToImage(vulkanContext, vulkanContext, vulkanContext, stagingBuffer, texture, (uint32_t)res.imageData.width, (uint32_t)res.imageData.height);

	//even if lod not enabled, need to transition, so use this code.
	Vulkan::generateMipMaps(vulkanContext, vulkanContext, vulkanContext, texture);

	Vulkan::unmapBuffer(vulkanContext, stagingBuffer);
	Vulkan::cleanupBuffer(vulkanContext, stagingBuffer);
	TextureArray textures;
	textures.set(&texture, 1);
	pipeRes = { descriptorType,textures };
}


bool ShaderResources::isBufferType(VkDescriptorType descriptorType) {
	return descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
		|| descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
		|| descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
		|| descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;

}

void ShaderResources::UpdateDescriptors(uint32_t passCount) {
	for (size_t j = 0; j < passCount; ++j) {
		std::vector<VkWriteDescriptorSet> descriptorWrites(writeInfos.size());
		for (uint32_t i = 0; i < writeInfos.size(); ++i) {
			if (isBufferType(writeInfos[i][j].descriptorType))
				descriptorWrites[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,descriptorSets[i][j],0,0,1,writeInfos[i][j].descriptorType,nullptr,&writeInfos[i][j].buffer,nullptr };
			else
				descriptorWrites[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,descriptorSets[i][j],0,0,1,writeInfos[i][j].descriptorType,&writeInfos[i][j].image,nullptr,nullptr };
		}
		Vulkan::updateDescriptorSets(vulkanContext, descriptorWrites);
	}
	/*for (uint32_t i = 0; i < passCount; ++i) {
		std::vector<VkWriteDescriptorSet> descriptorWrites(writeInfos.size());
		for (size_t j = 0; j < writeInfos.size();++j) {
			if (isBufferType(writeInfos[j][i].descriptorType))
				descriptorWrites[j] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,descriptorSets[j][i],0,0,1,writeInfos[j][i].descriptorType,nullptr,&writeInfos[j][i].buffer,nullptr };
			else
				descriptorWrites[j] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,descriptorSets[j][i],0,0,1,writeInfos[j][i].descriptorType,&writeInfos[j][i].image,nullptr,nullptr };
		}
		Vulkan::updateDescriptorSets(device, descriptorWrites);
	}*/
}
void ShaderResources::AddShaderResources(std::vector<std::vector<ShaderResource>>& pipelineResData, uint32_t passCount) {
	std::vector<std::vector<DescriptorInfo>> descriptorSetInfos;
	for (auto& pipelineResource : pipelineResData) {
		std::vector<DescriptorInfo> descriptorSetInfo;
		//std::vector<PipelineRes> pipelineResList;
		for (auto& res : pipelineResource) {
			descriptorSetInfo.push_back(res);
			ShaderResInternal pipeRes;
			switch (res.type) {
			case prtBuffer:
				AddBuffer(res, passCount, pipeRes);
				break;
			case prtTexture:
				AddTexture(res, pipeRes);
				break;
			case prtImageData:
				AddImageData(res, pipeRes);
				break;

			}
			pipelineResources.push_back(pipeRes);
		}
		//pipelineResources.push_back(pipelineResList);
		descriptorSetInfos.push_back(descriptorSetInfo);

	}

	AddDescriptorSet(descriptorSetInfos, passCount);

	for (auto& pipelineResource : pipelineResources) {
		std::vector<DescriptorWriteInfo> descriptorWrites(passCount);


		for (uint32_t i = 0; i < passCount; ++i) {
			descriptorWrites[i].descriptorType = pipelineResource.descriptorType;
			if (isBufferType(pipelineResource.descriptorType)) {

				descriptorWrites[i].buffer.buffer = pipelineResource.buffer.buffer.buffer;
				descriptorWrites[i].buffer.offset = pipelineResource.buffer.passSize * i;
				descriptorWrites[i].buffer.range = pipelineResource.buffer.objectSize;
			}
			else {
				Vulkan::Texture texture;
				if (pipelineResource.textures.count == passCount) {
					texture = pipelineResource.textures.pTextures[i];
				}
				else {
					texture = pipelineResource.textures.pTextures[0];
				}

				descriptorWrites[i].image.imageView = texture.imageView;
				descriptorWrites[i].image.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				descriptorWrites[i].image.sampler = texture.sampler;
			}
		}
		writeInfos.push_back(descriptorWrites);
	}
	UpdateDescriptors(passCount);


}

ShaderResources::ShaderResources(VulkanContext& vulkanContext_) :vulkanContext(vulkanContext_) {
}

ShaderResources::~ShaderResources() {
	for (auto& pipelineRes : pipelineResources) {

		if (isBufferType(pipelineRes.descriptorType)) {
			Vulkan::cleanupBuffer(vulkanContext, pipelineRes.buffer.buffer);
		}
		else {
			for (size_t i = 0; i < pipelineRes.textures.count; ++i) {
				vkDestroySampler(vulkanContext, pipelineRes.textures.pTextures[i].sampler, nullptr);
				Vulkan::cleanupImage(vulkanContext, pipelineRes.textures.pTextures[i]);
			}
		}

	}
	for (auto& descriptorSetLayout : descriptorSetLayouts) {
		Vulkan::cleanupDescriptorSetLayout(vulkanContext, descriptorSetLayout);
	}
	if (descriptorPool != VK_NULL_HANDLE) {
		Vulkan::cleanupDescriptorPool(vulkanContext, descriptorPool);
	}
}

ShaderProgram::ShaderProgram(VkDevice device_) :device(device_) {

}


ShaderProgram::~ShaderProgram() {
	clear();
}

void ShaderProgram::clear() {
	vertexAttributeDescriptions.clear();
	for (auto& pair : shaders) {
		Vulkan::cleanupShaderModule(device, pair.second);
	}
	shaders.clear();
}


void ShaderProgram::load(std::vector<const char*>& shaderPaths) {
	clear();
	for (auto path : shaderPaths) {
		std::ifstream file(path, std::ios::ate | std::ios::binary);
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
}

PipelineLayout::PipelineLayout(VkDevice device_, ShaderResources& shaderResources) :device(device_) {
	pipelineLayout = Vulkan::initPipelineLayout(device, shaderResources.descriptorSetLayouts);
}
PipelineLayout::~PipelineLayout() {
	Vulkan::cleanupPipelineLayout(device, pipelineLayout);
}

Pipeline::Pipeline(VkDevice device_, VkRenderPass renderPass_, ShaderProgram& shaderProgram_, PipelineLayout& pipelineLayout, Vulkan::PipelineInfo& pipelineInfo) :device(device_) {
	std::vector<Vulkan::ShaderModule> shaderModules;
	for (auto& shader : shaderProgram_.shaders) {
		Vulkan::ShaderModule shaderModule;
		shaderModule.stage = shader.first;
		shaderModule.shaderModule = shader.second;
		shaderModules.push_back(shaderModule);
	}
	pipeline = Vulkan::initGraphicsPipeline(device, renderPass_, pipelineLayout.pipelineLayout, shaderModules, shaderProgram_.vertexInputDescription, shaderProgram_.vertexAttributeDescriptions, pipelineInfo);
}

Pipeline::Pipeline(VkDevice device_, VkRenderPass renderPass_, ShaderProgram& shaderProgram_, PipelineLayout& pipelineLayout) :device(device_) {


	Vulkan::PipelineInfo pipelineInfo;
	pipelineInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	pipelineInfo.polygonMode = VK_POLYGON_MODE_FILL;
	pipelineInfo.depthTest = VK_TRUE;
	std::vector<Vulkan::ShaderModule> shaderModules;
	for (auto& shader : shaderProgram_.shaders) {
		Vulkan::ShaderModule shaderModule;
		shaderModule.stage = shader.first;
		shaderModule.shaderModule = shader.second;
		shaderModules.push_back(shaderModule);
	}
	pipeline = Vulkan::initGraphicsPipeline(device, renderPass_, pipelineLayout.pipelineLayout, shaderModules, shaderProgram_.vertexInputDescription, shaderProgram_.vertexAttributeDescriptions, pipelineInfo);

}

Pipeline::~Pipeline() {
	Vulkan::cleanupPipeline(device, pipeline);
}