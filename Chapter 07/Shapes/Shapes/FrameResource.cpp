#include "FrameResource.h"

FrameResource::FrameResource(VkDevice dev,VkPhysicalDeviceMemoryProperties memoryProperties, VkDescriptorSet descriptorSetPC, VkDescriptorSet descriptorSetOBs,uint32_t minAlignmentSize, uint32_t passCount, uint32_t objectCount) {
	device = dev;
	
	
	initBuffer(device, memoryProperties, sizeof(PassConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, PassCB);
	VkDeviceSize ObjectCBSize = sizeof(ObjectConstants) * objectCount;//this size may need to be aligned to 
	VkDeviceSize dynamicSize = sizeof(ObjectConstants);
	if (minAlignmentSize > 0) {
		dynamicSize = (((uint32_t)sizeof(ObjectConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1));
		ObjectCBSize = objectCount * dynamicSize;
	}
	
	initBuffer(device, memoryProperties, ObjectCBSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, ObjectCB);
	pPCs = (PassConstants*)mapBuffer(dev, PassCB);
	pOCs = (ObjectConstants*)mapBuffer(dev, ObjectCB);
	Fence = initFence(dev, VK_FENCE_CREATE_SIGNALED_BIT);
	
	VkDescriptorBufferInfo bufferInfoPC{};
	bufferInfoPC.buffer = PassCB.buffer;
	bufferInfoPC.offset = 0;
	bufferInfoPC.range = sizeof(PassConstants);
	VkDescriptorBufferInfo bufferInfoOC{};
	bufferInfoOC.buffer = ObjectCB.buffer;
	bufferInfoOC.offset = 0;
	bufferInfoOC.range = dynamicSize;
	std::vector<VkWriteDescriptorSet> descriptorWrites = {
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,descriptorSetPC,0,0,1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,nullptr,&bufferInfoPC,nullptr },
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,descriptorSetOBs,0,0,1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,nullptr,&bufferInfoOC,nullptr }
	};
	updateDescriptorSets(device, descriptorWrites);
}

FrameResource::~FrameResource() {
	

	cleanupFence(device, Fence);
	unmapBuffer(device, PassCB);
	unmapBuffer(device, ObjectCB);
	cleanupBuffer(device, ObjectCB);
	cleanupBuffer(device, PassCB);
	
}