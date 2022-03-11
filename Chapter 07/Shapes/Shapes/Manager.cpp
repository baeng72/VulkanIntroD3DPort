//#include "Manager.h"
//#include "../../Common/TextureLoader.h"
//
//Manager::Manager(VulkanInfo& vulkanInfo_) {
//	vulkanInfo = vulkanInfo_;
//}
//
//Manager::~Manager() {
//	for (auto& iter : DiffuseMaps) {
//		Vulkan::cleanupImage(vulkanInfo, iter.second.image);
//	}
//	for (auto& iter : SpecularMaps) {
//		Vulkan::cleanupImage(vulkanInfo, iter.second.image);
//	}
//	for (auto& iter : NormalMaps) {
//		Vulkan::cleanupImage(vulkanInfo, iter.second.image);
//	}
//	for (auto& iter : TextureArrays) {
//		for (auto& textureDesc : iter.second) {
//			Vulkan::cleanupImage(vulkanInfo, textureDesc.image);
//		}
//	}
//	for (auto& iter : AlphaMaps) {
//		for (auto& textureDesc : iter.second) {
//			Vulkan::cleanupImage(vulkanInfo, textureDesc.image);
//		}
//	}
//	for (auto& iter : LightMaps) {
//		Vulkan::cleanupImage(vulkanInfo, iter.second.image);
//	}
//	for (auto& iter : UniformBuffers) {
//		BufferDesc& bufferDesc = iter.second;
//		if (bufferDesc.ptr != nullptr)
//			Vulkan::unmapBuffer(vulkanInfo, bufferDesc.buffer);
//		Vulkan::cleanupBuffer(vulkanInfo, bufferDesc.buffer);
//	}
//	for (auto& iter : DynamicUniformBuffers) {
//		BufferDesc& bufferDesc = iter.second;
//		if (bufferDesc.ptr != nullptr)
//			Vulkan::unmapBuffer(vulkanInfo, bufferDesc.buffer);
//		Vulkan::cleanupBuffer(vulkanInfo, bufferDesc.buffer);
//	}
//	for (auto& iter : StorageBuffers) {
//		BufferDesc& bufferDesc = iter.second;
//		if (bufferDesc.ptr != nullptr)
//			Vulkan::unmapBuffer(vulkanInfo, bufferDesc.buffer);
//		Vulkan::cleanupBuffer(vulkanInfo, bufferDesc.buffer);
//	}
//	for (auto& iter : Shaders) {
//		Vulkan::cleanupShaderModule(vulkanInfo, iter.second.vertShader.shaderModule);
//		Vulkan::cleanupShaderModule(vulkanInfo, iter.second.fragShader.shaderModule);
//	}
//	for (auto& iter : VertexBuffers) {
//		Vulkan::cleanupBuffer(vulkanInfo, iter.second);
//	}
//	for (auto& iter : IndexBuffers) {
//		Vulkan::cleanupBuffer(vulkanInfo, iter.second);
//	}
//	for (auto& iter : Pipelines) {
//		Vulkan::cleanupPipeline(vulkanInfo, iter.second.pipeline);
//		Vulkan::cleanupPipelineLayout(vulkanInfo, iter.second.pipelineLayout);
//	}
//	for (auto& iter : Descriptors) {
//		Vulkan::cleanupDescriptorSetLayout(vulkanInfo, iter.second.descriptorSetLayout);
//		Vulkan::cleanupDescriptorPool(vulkanInfo, iter.second.descriptorPool);
//	}
//}
//
//void Manager::LoadTexture(TextureDesc&textureDesc,const char*path, VkShaderStageFlagBits shaderStages, bool enableLod) {
//	Vulkan::Texture texture;
//	loadTexture(vulkanInfo, vulkanInfo, vulkanInfo, vulkanInfo.memoryProperties, path, texture, enableLod);
//	textureDesc.image = texture;
//	textureDesc.stage = shaderStages;
//	textureDesc.imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//	textureDesc.imageInfo.imageView = texture.imageView;
//	textureDesc.imageInfo.sampler = texture.sampler;
//}
//
//size_t Manager::LoadDiffuseMap(const char* path, VkShaderStageFlagBits shaderStages, bool enableLod) {
//	size_t index = ++indexCount;
//	TextureDesc textureDesc;
//	LoadTexture(textureDesc, path, shaderStages, enableLod);
//	assert(DiffuseMaps.find(index) == DiffuseMaps.end());
//	DiffuseMaps.insert(std::pair<size_t, TextureDesc>(index, textureDesc));
//	return index;
//}
//size_t Manager::LoadSpecularMap(const char* path, VkShaderStageFlagBits shaderStages, bool enableLod) {
//	size_t index = ++indexCount;
//	TextureDesc textureDesc;
//	LoadTexture(textureDesc, path, shaderStages, enableLod);
//	assert(SpecularMaps.find(index) == SpecularMaps.end());
//	SpecularMaps.insert(std::pair<size_t, TextureDesc>(index, textureDesc));
//	return index;
//}
//
//size_t Manager::LoadNormalMap(const char* path, VkShaderStageFlagBits shaderStages, bool enableLod) {
//	size_t index = ++indexCount;
//	TextureDesc textureDesc;
//	LoadTexture(textureDesc, path, shaderStages, enableLod);
//	assert(NormalMaps.find(index) == NormalMaps.end());
//	NormalMaps.insert(std::pair<size_t, TextureDesc>(index, textureDesc));
//	return index;
//}
//
//void Manager::FreeDiffuseMap(size_t index) {
//	if (DiffuseMaps.find(index) != DiffuseMaps.end()) {
//		TextureDesc& textureDesc = DiffuseMaps[index];
//		Vulkan::cleanupImage(vulkanInfo, textureDesc.image);
//		DiffuseMaps.erase(index);
//	}
//}
//
//void Manager::FreeSpecularMap(size_t index) {
//	if (SpecularMaps.find(index) != SpecularMaps.end()) {
//		TextureDesc& textureDesc = SpecularMaps[index];
//		Vulkan::cleanupImage(vulkanInfo, textureDesc.image);
//		SpecularMaps.erase(index);
//	}
//}
//
//
//void Manager::FreeNormalMap(size_t index) {
//	if (NormalMaps.find(index) != NormalMaps.end()) {
//		TextureDesc& textureDesc = NormalMaps[index];
//		Vulkan::cleanupImage(vulkanInfo, textureDesc.image);
//		NormalMaps.erase(index);
//	}
//}
//
//size_t Manager::LoadTextureArray(std::vector<const char*>& paths, VkShaderStageFlagBits shaderStages, bool enableLod) {
//	size_t index = ++indexCount;
//	std::vector<TextureDesc> textureDescs;
//	for (auto& path : paths) {
//		Vulkan::Texture texture;
//		loadTexture(vulkanInfo, vulkanInfo, vulkanInfo, vulkanInfo.memoryProperties, path, texture, enableLod);
//		TextureDesc textureDesc;
//		textureDesc.image = texture;
//		textureDesc.stage = shaderStages;
//		textureDesc.imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//		textureDesc.imageInfo.imageView = texture.imageView;
//		textureDesc.imageInfo.sampler = texture.sampler;
//		textureDescs.push_back(textureDesc);
//	}
//	TextureArrays.insert(std::pair<size_t, std::vector<TextureDesc>>(index, textureDescs));
//	return index;
//}
//
//void Manager::FreeTextureArray(size_t index) {
//	if (TextureArrays.find(index) != TextureArrays.end()) {
//		for (auto& textureDesc : TextureArrays[index]) {
//			Vulkan::cleanupImage(vulkanInfo, textureDesc.image);
//		}
//		TextureArrays.erase(index);
//	}
//}
//
//size_t Manager::LoadLightMapFromMemory(uint8_t* pImageData, VkFormat format, uint32_t imageWidth, uint32_t imageHeight, VkShaderStageFlags shaderStages, bool enableLod) {
//	size_t index = ++indexCount;
//	VkDeviceSize pixsize = 4;
//	if (format == VK_FORMAT_R8_UNORM)
//		pixsize = 1;
//	else if (format == VK_FORMAT_R16_UNORM)
//		pixsize = 2;
//	VkDeviceSize imageSize = (uint64_t)imageWidth * (uint64_t)imageHeight * pixsize;
//	Vulkan::Texture texture;
//	Vulkan::TextureProperties props;
//	props.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
//	props.height = imageHeight;
//	props.width = imageWidth;
//	props.mipLevels = enableLod ? 0 : 1;
//	props.imageUsage = (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
//#ifdef __USE__VMA__
//	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
//#else
//	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
//#endif
//	props.format = format;
//
//	Vulkan::initTexture(vulkanInfo, vulkanInfo.memoryProperties, props, texture);
//	Vulkan::transitionImage(vulkanInfo, vulkanInfo, vulkanInfo, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.mipLevels);
//
//	Vulkan::Buffer stagingBuffer;
//	Vulkan::BufferProperties bufProps;
//#ifdef __USE__VMA__
//	bufProps.usage = VMA_MEMORY_USAGE_CPU_ONLY;
//#else
//	bufProps.memoryProps = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
//#endif
//	bufProps.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
//	bufProps.size = imageSize;
//	initBuffer(vulkanInfo, vulkanInfo.memoryProperties, bufProps, stagingBuffer);
//	void* ptr = mapBuffer(vulkanInfo, stagingBuffer);
//	memcpy(ptr, pImageData, imageSize);
//	Vulkan::CopyBufferToImage(vulkanInfo, vulkanInfo, vulkanInfo, stagingBuffer, texture, imageWidth, imageHeight);
//	Vulkan::generateMipMaps(vulkanInfo, vulkanInfo, vulkanInfo, texture);
//	Vulkan::unmapBuffer(vulkanInfo, stagingBuffer);
//	Vulkan::cleanupBuffer(vulkanInfo, stagingBuffer);
//
//	TextureDesc textureDesc;
//	textureDesc.image = texture;
//	textureDesc.stage = shaderStages;
//	textureDesc.imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//	textureDesc.imageInfo.imageView = texture.imageView;
//	textureDesc.imageInfo.sampler = texture.sampler;
//
//	if (LightMaps.find(index) != LightMaps.end()) {
//		Vulkan::cleanupImage(vulkanInfo, LightMaps[index].image);
//	}
//	LightMaps[index] = textureDesc;
//	return index;
//}
//
//void Manager::FreeLightMap(size_t index) {
//	if (LightMaps.find(index) != LightMaps.end()) {
//		TextureDesc& textureDesc = LightMaps[index];
//		Vulkan::cleanupImage(vulkanInfo, textureDesc.image);
//		LightMaps.erase(index);
//	}
//}
//
//size_t Manager::InitObjectBuffersWithData(size_t oldIndex,void* vertices, VkDeviceSize vertexSize, uint32_t* indices, VkDeviceSize indexSize) {
//	
//	size_t index=0;
//	if (oldIndex > 0)
//		index = oldIndex;
//	else
//		index=++indexCount;
//	//assert(VertexBuffers.find(hash) == VertexBuffers.end());
//	//assert(IndexBuffers.find(hash) == IndexBuffers.end());
//	Vulkan::Buffer vertexBuffer;
//	Vulkan::Buffer indexBuffer;
//	Vulkan::BufferProperties props;
//#ifdef __USE__VMA__
//	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
//#else
//	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
//#endif
//	props.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
//	props.size = vertexSize;
//	Vulkan::initBuffer(vulkanInfo, vulkanInfo.memoryProperties, props, vertexBuffer);
//	props.bufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
//	props.size = indexSize;
//	Vulkan::initBuffer(vulkanInfo, vulkanInfo.memoryProperties, props, indexBuffer);
//	VkDeviceSize maxSize = std::max(vertexSize, indexSize);
//	Vulkan::Buffer stagingBuffer;
//#ifdef __USE__VMA__
//	props.usage = VMA_MEMORY_USAGE_CPU_ONLY;
//#else
//	props.memoryProps = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
//#endif
//	props.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
//	props.size = maxSize;
//	initBuffer(vulkanInfo, vulkanInfo.memoryProperties, props, stagingBuffer);
//	void* ptr = mapBuffer(vulkanInfo, stagingBuffer);
//	//copy vertex data
//	memcpy(ptr, vertices, vertexSize);
//	CopyBufferTo(vulkanInfo, vulkanInfo, vulkanInfo, stagingBuffer, vertexBuffer, vertexSize);
//	memcpy(ptr, indices, indexSize);
//	CopyBufferTo(vulkanInfo, vulkanInfo, vulkanInfo, stagingBuffer, indexBuffer, indexSize);
//
//
//	unmapBuffer(vulkanInfo, stagingBuffer);
//	cleanupBuffer(vulkanInfo, stagingBuffer);
//
//	//Allow updating of vertex & index buffers
//	if (VertexBuffers.find(index) != VertexBuffers.end()) {
//		Vulkan::Buffer& oldBuffer = VertexBuffers[index];
//		Vulkan::cleanupBuffer(vulkanInfo, oldBuffer);
//	}
//	if (IndexBuffers.find(index) != IndexBuffers.end()) {
//		Vulkan::Buffer& oldBuffer = IndexBuffers[index];
//		Vulkan::cleanupBuffer(vulkanInfo, oldBuffer);
//	}
//	VertexBuffers[index] = vertexBuffer;
//	IndexBuffers[index] = indexBuffer;
//	IndexCounts[index] = (uint32_t)(indexSize / sizeof(uint32_t));
//
//	
//
//	return index;
//
//}
//
//
//
//
//size_t Manager::InitVertexBuffer(size_t oldIndex,VkDeviceSize vertexSize) {
//	size_t index = 0;
//	if (oldIndex > 0)
//		index = oldIndex;
//	else
//		index= ++indexCount;
//	Vulkan::Buffer vertexBuffer;
//	Vulkan::BufferProperties props;
//#ifdef __USE__VMA__
//	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
//#else
//	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
//#endif
//	props.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
//	props.size = vertexSize;
//	Vulkan::initBuffer(vulkanInfo, vulkanInfo.memoryProperties, props, vertexBuffer);
//	//Allow updating of vertex & index buffers
//	if (VertexBuffers.find(index) != VertexBuffers.end()) {
//		Vulkan::Buffer& oldBuffer = VertexBuffers[index];
//		Vulkan::cleanupBuffer(vulkanInfo, oldBuffer);
//	}
//	VertexBuffers[index] = vertexBuffer;
//	return index;
//}
//
//
//size_t Manager::InitIndexBuffer(size_t oldIndex,VkDeviceSize indexSize) {
//	size_t index = 0;
//	if (oldIndex > 0)
//		index = oldIndex;
//	else
//		index=++indexCount;
//	Vulkan::Buffer indexBuffer;
//	Vulkan::BufferProperties props;
//#ifdef __USE__VMA__
//	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
//#else
//	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
//#endif
//	props.bufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
//	props.size = indexSize;
//	/*Vulkan::initBuffer(vulkanInfo, vulkanInfo.memoryProperties, props, indexBuffer);
//	if (IndexBuffers.find(hash) != IndexBuffers.end()) {
//		Vulkan::Buffer& oldBuffer = IndexBuffers[hash];
//		Vulkan::cleanupBuffer(vulkanInfo, oldBuffer);
//	}*/
//
//	IndexBuffers[index] = indexBuffer;
//	IndexCounts[index] = (uint32_t)(indexSize / sizeof(uint32_t));
//	return index;
//}
//
//void Manager::FreeObjectBuffers(size_t index) {
//	vkQueueWaitIdle(vulkanInfo);
//	if (VertexBuffers.find(index) != VertexBuffers.end()) {
//		Vulkan::cleanupBuffer(vulkanInfo, VertexBuffers[index]);
//		VertexBuffers.erase(index);
//	}
//	if (IndexBuffers.find(index) != IndexBuffers.end()) {
//		Vulkan::cleanupBuffer(vulkanInfo, IndexBuffers[index]);
//		IndexBuffers.erase(index);
//	}
//	IndexCounts[index] = 0;
//}
//
//
//void Manager::UpdateVertexBuffer(size_t index, void* ptrData, VkDeviceSize dataSize) {
//	assert(VertexBuffers.find(index) != VertexBuffers.end());
//	Vulkan::Buffer stagingBuffer;
//	Vulkan::BufferProperties props;
//#ifdef __USE__VMA__
//	props.usage = VMA_MEMORY_USAGE_CPU_ONLY;
//#else
//	props.memoryProps = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
//#endif
//	props.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
//	props.size = dataSize;
//	initBuffer(vulkanInfo, vulkanInfo.memoryProperties, props, stagingBuffer);
//	void* ptr = mapBuffer(vulkanInfo, stagingBuffer);
//	//copy vertex data
//	memcpy(ptr, ptrData, dataSize);
//	Vulkan::Buffer& vertexBuffer = VertexBuffers[index];
//	CopyBufferTo(vulkanInfo, vulkanInfo, vulkanInfo, stagingBuffer, vertexBuffer, dataSize);
//	unmapBuffer(vulkanInfo, stagingBuffer);
//	cleanupBuffer(vulkanInfo, stagingBuffer);
//}
//
//
//void Manager::UpdateIndexBuffer(size_t index, uint32_t* ptrData, VkDeviceSize dataSize) {
//	assert(IndexBuffers.find(index) != IndexBuffers.end());
//	Vulkan::Buffer stagingBuffer;
//	Vulkan::BufferProperties props;
//#ifdef __USE__VMA__
//	props.usage = VMA_MEMORY_USAGE_CPU_ONLY;
//#else
//	props.memoryProps = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
//#endif
//	props.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
//	props.size = dataSize;
//	initBuffer(vulkanInfo, vulkanInfo.memoryProperties, props, stagingBuffer);
//	void* ptr = mapBuffer(vulkanInfo, stagingBuffer);
//	//copy index data
//	memcpy(ptr, ptrData, dataSize);
//	Vulkan::Buffer& vertexBuffer = IndexBuffers[index];
//	CopyBufferTo(vulkanInfo, vulkanInfo, vulkanInfo, stagingBuffer, vertexBuffer, dataSize);
//	unmapBuffer(vulkanInfo, stagingBuffer);
//	cleanupBuffer(vulkanInfo, stagingBuffer);
//}
//
//uint32_t Manager::GetIndexCount(size_t index) {
//	assert(IndexCounts.find(index) != IndexCounts.end());
//	return IndexCounts[index];
//}
//
//
//size_t Manager::InitInstanceBuffer(VkDeviceSize instanceSize, uint32_t instanceCount, VkShaderStageFlagBits stage) {
//	size_t index =  ++indexCount;
//	Vulkan::Buffer instanceBuffer;
//	Vulkan::BufferProperties props;
//	VkDeviceSize instanceBufferSize = instanceSize * instanceCount;
//#ifdef __USE__VMA__
//	props.usage = VMA_MEMORY_USAGE_CPU_ONLY;
//#else
//	props.memoryProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
//#endif
//	props.bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
//	props.size = instanceBufferSize;
//	Vulkan::initBuffer(vulkanInfo, vulkanInfo.memoryProperties, props, instanceBuffer);
//	void* ptr = Vulkan::mapBuffer(vulkanInfo, instanceBuffer);
//	BufferDesc bufferDesc{};
//	bufferDesc.buffer = instanceBuffer;
//	bufferDesc.stage = stage;
//	bufferDesc.bufferInfo.buffer = instanceBuffer.buffer;
//	bufferDesc.bufferInfo.range = instanceBufferSize;
//	bufferDesc.ptr = ptr;
//	if (InstanceBuffers.find(index) != InstanceBuffers.end()) {
//		Vulkan::cleanupBuffer(vulkanInfo, InstanceBuffers[index].buffer);
//	}
//	InstanceBuffers[index] = bufferDesc;
//	InstanceCounts[index] = instanceCount;
//	return index;
//}
//
//size_t Manager::InitStorageBuffer(VkDeviceSize storageSize, uint32_t storageCount, VkShaderStageFlagBits stage) {
//	size_t index = ++indexCount;
//	Vulkan::Buffer storageBuffer;
//	Vulkan::BufferProperties props;
//	VkDeviceSize storageBufferSize = storageSize * storageCount;
//#ifdef __USE__VMA__
//	props.usage = VMA_MEMORY_USAGE_CPU_ONLY;
//#else
//	props.memoryProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
//#endif
//	props.bufferUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
//	props.size = storageBufferSize;
//	Vulkan::initBuffer(vulkanInfo, vulkanInfo.memoryProperties, props, storageBuffer);
//	void* ptr = Vulkan::mapBuffer(vulkanInfo, storageBuffer);
//	BufferDesc bufferDesc{};
//	bufferDesc.buffer = storageBuffer;
//	bufferDesc.stage = stage;
//	bufferDesc.bufferInfo.buffer = storageBuffer.buffer;
//	bufferDesc.bufferInfo.range = storageBufferSize;
//	bufferDesc.ptr = ptr;
//	if (StorageBuffers.find(index) != StorageBuffers.end()) {
//		Vulkan::cleanupBuffer(vulkanInfo, StorageBuffers[index].buffer);
//	}
//	StorageBuffers[index] = bufferDesc;
//	return index;
//}
//
//size_t Manager::InitDynamicUniformBuffer(VkDeviceSize uniformSize, uint32_t uniformCount, VkShaderStageFlagBits stage) {
//	size_t index = ++indexCount;
//	Vulkan::Buffer uniformBuffer;
//	Vulkan::BufferProperties props;
//	auto minAlignmentSize = vulkanInfo.deviceProperties.limits.minUniformBufferOffsetAlignment;
//	VkDeviceSize objSize = uniformSize;
//	if (minAlignmentSize) {
//		objSize = (((VkDeviceSize)uniformSize) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
//	}
//
//	VkDeviceSize uniformBufferSize = objSize * uniformCount;//careful with uniform count > 1, need to align on boundary (256 bytes?), implement later
//#ifdef __USE__VMA__
//	props.usage = VMA_MEMORY_USAGE_CPU_ONLY;
//#else
//	props.memoryProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
//#endif
//	props.bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
//	props.size = uniformBufferSize;
//	Vulkan::initBuffer(vulkanInfo, vulkanInfo.memoryProperties, props, uniformBuffer);
//	void* ptr = Vulkan::mapBuffer(vulkanInfo, uniformBuffer);
//	DynamicBufferDesc bufferDesc{};
//	bufferDesc.buffer = uniformBuffer;
//	bufferDesc.stage = stage;
//	bufferDesc.bufferInfo.buffer = uniformBuffer.buffer;
//	bufferDesc.bufferInfo.range = uniformBufferSize;
//	bufferDesc.ptr = ptr;
//	bufferDesc.objectSize = objSize;
//	if (DynamicUniformBuffers.find(index) != DynamicUniformBuffers.end()) {
//		Vulkan::cleanupBuffer(vulkanInfo, DynamicUniformBuffers[index].buffer);
//	}
//	DynamicUniformBuffers[index] = bufferDesc;	
//	return index;
//}