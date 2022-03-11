#pragma once
#include <unordered_map>
#include <../../Common/Vulkan.h>

#include "../../ThirdParty/spirv-reflect/spirv_reflect.h"

struct DescriptorInfo {
	VkDescriptorType descriptorType{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER };
	VkShaderStageFlags shaderStage{ VK_SHADER_STAGE_VERTEX_BIT };
};



struct DescriptorSet {
	VkDevice device{ VK_NULL_HANDLE };
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	VkDescriptorPool descriptorPool;
	std::vector < std::vector<VkDescriptorSet>> descriptorSets;

	void AddDescriptorSet(std::vector<std::vector<DescriptorInfo>>& descriptorSetInfos,uint32_t passCount) {
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
			poolSizes.push_back({ pair.first,pair.second*passCount });
			maxSet = std::max(pair.second, maxSet);
		}
		descriptorPool = Vulkan::initDescriptorPool(device, poolSizes, passCount * maxSet);
		
		for (auto& descriptorSetInfo : descriptorSetInfos) {
			std::vector<VkDescriptorSetLayoutBinding> descriptorBindings;
			uint32_t binding = 0;
			
			for (auto& descriptorInfo : descriptorSetInfo) {
				descriptorBindings.push_back({ binding,descriptorInfo.descriptorType,1,descriptorInfo.shaderStage });
			}
			VkDescriptorSetLayout descriptorSetLayout = Vulkan::initDescriptorSetLayout(device, descriptorBindings);
			descriptorSetLayouts.push_back(descriptorSetLayout);
			std::vector<VkDescriptorSet> descriptorSet;
			for (uint32_t i = 0; i < passCount;++i) {
				descriptorSet.push_back(Vulkan::initDescriptorSet(device, descriptorSetLayout, descriptorPool));
			}
			descriptorSets.push_back(descriptorSet);			
		}
		
	}
	DescriptorSet(VkDevice device) {
		this->device = device;
	}
	~DescriptorSet() {
		for (auto& descriptorSetLayout : descriptorSetLayouts) {
			Vulkan::cleanupDescriptorSetLayout(device, descriptorSetLayout);
		}
		if (descriptorPool != VK_NULL_HANDLE) {
			Vulkan::cleanupDescriptorPool(device, descriptorPool);
		}
	}
};



struct BufferInfo {
	size_t size{ 0 };
	size_t count{ 0 };
	bool isMapped{ true };
};


struct TextureInfo {
	size_t width{ 0 };
	size_t height{ 0 };
	uint8_t* pixels{ nullptr };
};
enum PipelineResourceType{prtBuffer,prtTexture};
struct PipelineResource :public DescriptorInfo{
	PipelineResourceType type{ prtBuffer };
	union {
		struct {
			size_t size;
			size_t count;
			bool isMapped;
		}buffer;
		struct {
			size_t width;
			size_t height;
			uint8_t *pixels;

		}texture;		
		
	};
	PipelineResource(VkDescriptorType descriptorType_, VkShaderStageFlags shaderStage_,  size_t size_, size_t count_, bool isMapped_) {
		descriptorType = descriptorType_;
		shaderStage = shaderStage_;
		type = prtBuffer;
		buffer.size = size_;
		buffer.count = count_;
		buffer.isMapped = isMapped_;
	}
	PipelineResource(VkDescriptorType descriptorType_, VkShaderStageFlags shaderStage_, size_t width_, size_t height_, uint8_t* pixels_) {
		descriptorType = descriptorType_;
		shaderStage = shaderStage_;
		type = prtTexture;
		texture.width = width_;
		texture.height = height_;
		texture.pixels = pixels_;
	}
};

struct ShaderRes {
	VkDescriptorType descriptorType;
	union {
		struct {
			Vulkan::Buffer buffer;
			void* ptr;
			VkDeviceSize objectSize;
			VkDeviceSize passSize;
			VkDeviceSize totalSize;
		}buffer;
		Vulkan::Texture texture;
	};
	
	
	ShaderRes() { descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM; }
	ShaderRes(VkDescriptorType descriptorType_, Vulkan::Buffer& buffer_, void* ptr, VkDeviceSize objectSize,VkDeviceSize passSize, VkDeviceSize totalSize) {
		descriptorType = descriptorType_;
		buffer.buffer = buffer_;
		buffer.ptr = ptr;
		buffer.objectSize = objectSize;
		buffer.passSize = passSize;
		buffer.totalSize = totalSize;
	}
	ShaderRes(VkDescriptorType descriptorType_, Vulkan::Texture& texture_) {
		descriptorType = descriptorType_;
		texture = texture_;
	}
};
//enum PipelineResourceType{prtBuffer,prtTexture};
//struct PipelineResource :public DescriptorInfo{
//	PipelineResourceType type{ prtBuffer };
//	size_t size{ 0 };
//	size_t count{ 1 };
//	bool isMapped{ true };
//	size_t width;
//	size_t height;
//	uint8_t* pixels{ nullptr };
//};
struct DescriptorWriteInfo{
	VkDescriptorType descriptorType;
	union {
		VkDescriptorBufferInfo buffer;
		VkDescriptorImageInfo image;
	};
	DescriptorWriteInfo() {
		descriptorType = VK_DESCRIPTOR_TYPE_MAX_ENUM;
	}
};

struct ShaderResources {
	VkDevice device{ VK_NULL_HANDLE };

	VkPhysicalDeviceProperties& deviceProperties;
	VkPhysicalDeviceMemoryProperties& memoryProperties;
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	VkDescriptorPool descriptorPool;
	std::vector < std::vector<VkDescriptorSet>> descriptorSets;
	std::vector<ShaderRes> pipelineResources;
	std::vector<std::vector< VkDescriptorBufferInfo>> bufferInfo;
	std::vector<std::vector<DescriptorWriteInfo>> writeInfos;

	void AddDescriptorSet(std::vector<std::vector<DescriptorInfo>>& descriptorSetInfos, uint32_t passCount) {
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
		descriptorPool = Vulkan::initDescriptorPool(device, poolSizes, passCount * maxSet);

		for (auto& descriptorSetInfo : descriptorSetInfos) {
			std::vector<VkDescriptorSetLayoutBinding> descriptorBindings;
			uint32_t binding = 0;

			for (auto& descriptorInfo : descriptorSetInfo) {
				descriptorBindings.push_back({ binding,descriptorInfo.descriptorType,1,descriptorInfo.shaderStage });
			}
			VkDescriptorSetLayout descriptorSetLayout = Vulkan::initDescriptorSetLayout(device, descriptorBindings);
			descriptorSetLayouts.push_back(descriptorSetLayout);
			std::vector<VkDescriptorSet> descriptorSet;
			for (uint32_t i = 0; i < passCount; ++i) {
				descriptorSet.push_back(Vulkan::initDescriptorSet(device, descriptorSetLayout, descriptorPool));
			}
			descriptorSets.push_back(descriptorSet);
		}
	}
	void AddBuffer(PipelineResource& res, uint32_t passCount, ShaderRes& pipeRes) {
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
			alignment = deviceProperties.limits.minUniformBufferOffsetAlignment;
			break;

		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
		case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
			usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			alignment = deviceProperties.limits.minStorageBufferOffsetAlignment;
			break;
		}
		VkDeviceSize objectSize = (res.buffer.size + alignment - 1) & ~(alignment - 1);
		VkDeviceSize bufferSize = objectSize * res.buffer.count;
		VkDeviceSize range = bufferSize;
		VkDeviceSize totalSize = passCount * bufferSize;
		props.size = totalSize;
		Vulkan::initBuffer(device, memoryProperties, props, buffer);
		void* ptr = nullptr;
		if (res.buffer.isMapped) {
			ptr = Vulkan::mapBuffer(device, buffer);
		}
		pipeRes = { descriptorType,buffer,ptr,objectSize,range,totalSize };
	}
	void AddTexture(PipelineResource& res, ShaderRes& pipeRes) {

	}
	void updateDescriptors(uint32_t passCount) {
		for (size_t j = 0; j < passCount; ++j) {
			std::vector<VkWriteDescriptorSet> descriptorWrites(writeInfos.size());
			for (uint32_t i = 0; i < writeInfos.size(); ++i) {				
				if (isBufferType(writeInfos[i][j].descriptorType))
					descriptorWrites[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,descriptorSets[i][j],0,0,1,writeInfos[i][j].descriptorType,nullptr,&writeInfos[i][j].buffer,nullptr };
				else
					descriptorWrites[i] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,descriptorSets[i][j],0,0,1,writeInfos[i][j].descriptorType,&writeInfos[i][j].image,nullptr,nullptr };
			}
			Vulkan::updateDescriptorSets(device, descriptorWrites);
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
	void AddShaderResources(std::vector<std::vector<PipelineResource>>& pipelineResData, uint32_t passCount) {
		std::vector<std::vector<DescriptorInfo>> descriptorSetInfos;
		for (auto& pipelineResource : pipelineResData) {
			std::vector<DescriptorInfo> descriptorSetInfo;
			//std::vector<PipelineRes> pipelineResList;
			for (auto& res : pipelineResource) {
				descriptorSetInfo.push_back(res);
				ShaderRes pipeRes;
				switch (res.type) {
				case prtBuffer:
					AddBuffer(res, passCount, pipeRes);
					break;
				case prtTexture:
					AddTexture(res, pipeRes);
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
			}
			writeInfos.push_back(descriptorWrites);
		}
		updateDescriptors(passCount);


	}
	ShaderResources(VkDevice device_, VkPhysicalDeviceProperties& deviceProperties_, VkPhysicalDeviceMemoryProperties memoryProperties_) :device(device_), deviceProperties(deviceProperties_), memoryProperties(memoryProperties_) {
	}
	bool isBufferType(VkDescriptorType descriptorType) {
		return descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
			|| descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
			|| descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
			|| descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
	
	}
	~ShaderResources() {
		for (auto& pipelineRes : pipelineResources) {
			
			if (isBufferType(pipelineRes.descriptorType)) {
				Vulkan::cleanupBuffer(device, pipelineRes.buffer.buffer);
			}
			else {
				Vulkan::cleanupImage(device, pipelineRes.texture);
			}
			
		}
		for (auto& descriptorSetLayout : descriptorSetLayouts) {
			Vulkan::cleanupDescriptorSetLayout(device, descriptorSetLayout);
		}
		if (descriptorPool != VK_NULL_HANDLE) {
			Vulkan::cleanupDescriptorPool(device, descriptorPool);
		}
	}

};

struct ShaderProgram {
	VkDevice device{ VK_NULL_HANDLE };
	VkVertexInputBindingDescription vertexInputDescription;
	std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
	std::unordered_map<VkShaderStageFlagBits, VkShaderModule> shaders;
	ShaderProgram(VkDevice device_) :device(device_) {

	}
	~ShaderProgram() {
		clear();
	}
	void clear() {
		vertexAttributeDescriptions.clear();
		for (auto& pair : shaders) {
			Vulkan::cleanupShaderModule(device, pair.second);
		}
		shaders.clear();
	}
	void load(std::vector<const char*>& shaderPaths) {
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
};

struct PipelineLayout {
	VkDevice device{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	PipelineLayout(VkDevice device_, ShaderResources& shaderResources):device(device_) {
		pipelineLayout = Vulkan::initPipelineLayout(device, shaderResources.descriptorSetLayouts);
	}
	~PipelineLayout() {
		Vulkan::cleanupPipelineLayout(device, pipelineLayout);
	}
	operator VkPipelineLayout()const { return pipelineLayout; }
};


class Pipeline {
	VkDevice device{ VK_NULL_HANDLE };	
	VkPipeline pipeline{ VK_NULL_HANDLE };
	

public:
	Pipeline(VkDevice device_, VkRenderPass renderPass_, ShaderProgram& shaderProgram_, PipelineLayout& pipelineLayout, Vulkan::PipelineInfo& pipelineInfo):device(device_) {
		std::vector<Vulkan::ShaderModule> shaderModules;
		for (auto& shader : shaderProgram_.shaders) {
			Vulkan::ShaderModule shaderModule;
			shaderModule.stage = shader.first;
			shaderModule.shaderModule = shader.second;
			shaderModules.push_back(shaderModule);
		}
		pipeline = Vulkan::initGraphicsPipeline(device, renderPass_, pipelineLayout.pipelineLayout, shaderModules, shaderProgram_.vertexInputDescription, shaderProgram_.vertexAttributeDescriptions, pipelineInfo);
	}
	Pipeline(VkDevice device_,VkRenderPass renderPass_,ShaderProgram&shaderProgram_,PipelineLayout&pipelineLayout):device(device_){
	

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
	~Pipeline() {
		Vulkan::cleanupPipeline(device, pipeline);
	}
	operator VkPipeline()const { return pipeline; }
};
