#include "FrameResource.h"
FrameResource::FrameResource(VkDevice dev, VkPhysicalDeviceMemoryProperties memoryProperties, std::vector<VkDescriptorSet>& descriptorSets, uint32_t minAlignmentSize, uint32_t passCount, uint32_t objectCount, uint32_t materialCount) {
	device = dev;
	initBuffer(device, memoryProperties, sizeof(PassConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, PassCB);
	VkDeviceSize ObjectCBSize = sizeof(ObjectConstants) * objectCount;//this size may need to be aligned to 
	VkDeviceSize objectDynamicSize = sizeof(ObjectConstants);
	if (minAlignmentSize > 0) {
		objectDynamicSize = (((uint32_t)sizeof(ObjectConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1));
		ObjectCBSize = objectCount * objectDynamicSize;
	}

	initBuffer(device, memoryProperties, ObjectCBSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, ObjectCB);
	VkDeviceSize matSize = sizeof(MaterialConstants) * materialCount;
	VkDeviceSize materialDynamicSize = sizeof(MaterialConstants);
	if (minAlignmentSize > 0) {
		materialDynamicSize = (((uint32_t)sizeof(MaterialConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1));
		matSize = materialCount * materialDynamicSize;
	}
	initBuffer(device, memoryProperties, matSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, MaterialCB);

	pPCs = (PassConstants*)mapBuffer(dev, PassCB);
	pOCs = (ObjectConstants*)mapBuffer(dev, ObjectCB);
	pMats = (MaterialConstants*)mapBuffer(dev, MaterialCB);

	Fence = initFence(dev, VK_FENCE_CREATE_SIGNALED_BIT);

	VkDescriptorBufferInfo bufferInfoPC{};
	bufferInfoPC.buffer = PassCB.buffer;
	bufferInfoPC.offset = 0;
	bufferInfoPC.range = sizeof(PassConstants);
	VkDescriptorBufferInfo bufferInfoOC{};
	bufferInfoOC.buffer = ObjectCB.buffer;
	bufferInfoOC.offset = 0;
	bufferInfoOC.range = objectDynamicSize;
	VkDescriptorBufferInfo bufferInfoMats{};
	bufferInfoMats.buffer = MaterialCB.buffer;
	bufferInfoMats.offset = 0;
	bufferInfoMats.range = materialDynamicSize;
	std::vector<VkWriteDescriptorSet> descriptorWrites = {
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,descriptorSets[0],0,0,1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,nullptr,&bufferInfoPC,nullptr },
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,descriptorSets[1],0,0,1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,nullptr,&bufferInfoOC,nullptr },
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,descriptorSets[2],0,0,1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,nullptr,&bufferInfoMats,nullptr }
	};
	updateDescriptorSets(device, descriptorWrites);
}

FrameResource::~FrameResource() {

	cleanupFence(device, Fence);


	unmapBuffer(device, MaterialCB);
	unmapBuffer(device, PassCB);
	unmapBuffer(device, ObjectCB);

	cleanupBuffer(device, MaterialCB);
	cleanupBuffer(device, ObjectCB);
	cleanupBuffer(device, PassCB);
}