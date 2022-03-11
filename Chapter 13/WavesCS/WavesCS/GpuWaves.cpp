#include "GpuWaves.h"

GpuWaves::GpuWaves(VkDevice device_,VkPhysicalDeviceMemoryProperties memoryProperties_,VkQueue queue_, VkCommandBuffer cmd_, int m, int n, float dx, float dt, float speed, float damping):device(device_),memoryProperties(memoryProperties_) {
	mNumRows = m;
	mNumCols = n;
	assert((m * n) % 256 == 0);

	mVertexCount = m * n;
	mTriangleCount = (m - 1) * (n - 1) * 2;

	mTimeStep = dt;
	mSpatialStep = dx;

	float d = damping * dt + 2.0f;
	float e = (speed * speed) * (dt * dt) / (dx * dx);
	mK[0] = (damping * dt - 2.0f) / d;
	mK[1] = (4.0f - 8.0f * e) / d;
	mK[2] = (2.0f * e) / d;

	BuildResources(queue_, cmd_);
}

GpuWaves::~GpuWaves() {
	Vulkan::cleanupFence(device, fence);
	Vulkan::cleanupImage(device, mPrevSol);
	Vulkan::cleanupImage(device, mCurrSol);
	Vulkan::cleanupImage(device, mNextSol);
}


void GpuWaves::BuildResources(VkQueue queue_, VkCommandBuffer cmd_) {
	Vulkan::ImageProperties texDesc;
	texDesc.width = mNumCols;
	texDesc.height = mNumRows;
	texDesc.mipLevels = 1;
	texDesc.format = VK_FORMAT_R32_SFLOAT;
	texDesc.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	texDesc.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	Vulkan::initImage(device, memoryProperties, texDesc, mPrevSol);
	Vulkan::initImage(device, memoryProperties, texDesc, mCurrSol);
	Vulkan::initImage(device, memoryProperties, texDesc, mNextSol);
	Vulkan::transitionImage(device, queue_, cmd_, mPrevSol.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mPrevSol.mipLevels);
	Vulkan::transitionImage(device, queue_, cmd_, mCurrSol.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mCurrSol.mipLevels);
	Vulkan::transitionImage(device, queue_, cmd_, mNextSol.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mNextSol.mipLevels);

	// Describe the data we want to copy into the default buffer.
	std::vector<float> initData(mNumRows * mNumCols, 0.0f);
	for (int i = 0; i < initData.size(); ++i)
		initData[i] = 0.0f;

	VkDeviceSize bufSize = mNumRows * mNumCols * sizeof(float);
	Vulkan::Buffer stagingBuffer;

	Vulkan::BufferProperties props;
#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_CPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
#endif
	props.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	props.size = bufSize;
	initBuffer(device, memoryProperties, props, stagingBuffer);
	uint8_t* ptr = (uint8_t*)mapBuffer(device, stagingBuffer);
	Vulkan::CopyBufferToImage(device, queue_, cmd_, stagingBuffer, mPrevSol, mNumRows, mNumCols);
	Vulkan::CopyBufferToImage(device, queue_, cmd_, stagingBuffer, mCurrSol, mNumRows, mNumCols);

	Vulkan::unmapBuffer(device, stagingBuffer);
	Vulkan::cleanupBuffer(device, stagingBuffer);

	Vulkan::transitionImage(device, queue_, cmd_, mPrevSol.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, mPrevSol.mipLevels);
	Vulkan::transitionImage(device, queue_, cmd_, mCurrSol.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, mCurrSol.mipLevels);
	Vulkan::transitionImage(device, queue_, cmd_, mNextSol.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, mNextSol.mipLevels);

	//
	fence = Vulkan::initFence(device);
}

void GpuWaves::Update(const GameTimer& gt,VkQueue queue_, VkCommandBuffer cmd_,VkDescriptorSet descriptorSetUBO_, VkDescriptorSet descriptorSetImages_, VkPipelineLayout pipelineLayout_, VkPipeline pipeline_,/*VkFence fence_,*/ GpuUBO* pGpuUBO) {

	static float t = 0.0f;
	//Accumulate time
	t += gt.DeltaTime();
	if (t >= mTimeStep) {
		pGpuUBO->gWaveConstant0 = mK[0];
		pGpuUBO->gWaveConstant1 = mK[1];
		pGpuUBO->gWaveConstant2 = mK[2];
		VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		VkSubmitInfo computeInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		computeInfo.commandBufferCount = 1;
		computeInfo.pCommandBuffers = &cmd_;
		vkBeginCommandBuffer(cmd_, &beginInfo);
		vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
		VkDescriptorSet ds[] = { descriptorSetUBO_,descriptorSetImages_ };
		vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_, 0, 2, ds, 0, 0);
		vkCmdDispatch(cmd_, (uint32_t)(mNumRows / 16), (uint32_t)(mNumCols / 16), 1);
		vkEndCommandBuffer(cmd_);
		VkResult res = vkQueueSubmit(queue_, 1, &computeInfo, fence);
		assert(res == VK_SUCCESS);
		vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
		vkResetFences(device, 1, &fence);

		auto resTemp = mPrevSol;
		mPrevSol = mCurrSol;
		mCurrSol = mNextSol;
		mNextSol = resTemp;

		t = 0.0f;
	}

	
}

void GpuWaves::Disturb(VkQueue queue_, VkCommandBuffer cmd_, VkDescriptorSet descriptorSetUBO_,VkDescriptorSet descriptorSetImages_, VkPipelineLayout pipelineLayout_, VkPipeline pipeline_,/*VkFence fence_,*/ uint32_t i, uint32_t j, float magnitude, GpuUBO* pGpuUBO) {
	glm::ivec2 disturbIndex = { j,i };
	pGpuUBO->gDisturbIndex = disturbIndex;
	pGpuUBO->gDisturbMag = magnitude;
	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	VkSubmitInfo computeInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	computeInfo.commandBufferCount = 1;
	computeInfo.pCommandBuffers = &cmd_;
	vkBeginCommandBuffer(cmd_, &beginInfo);
	vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
	VkDescriptorSet ds[] = { descriptorSetUBO_,descriptorSetImages_ };
	
	vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_, 0, 2, ds, 0, nullptr);
	vkCmdDispatch(cmd_, 1, 1, 1);
	vkEndCommandBuffer(cmd_);
	VkResult res = vkQueueSubmit(queue_, 1, &computeInfo, fence);
	assert(res == VK_SUCCESS);
	vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &fence);
}

void GpuWaves::UpdateImageDescriptors(VkDescriptorImageInfo* pImageInfo)const {
	pImageInfo[0].imageView = mPrevSol.imageView;
	pImageInfo[0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	pImageInfo[1].imageView = mCurrSol.imageView;
	pImageInfo[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	pImageInfo[2].imageView = mNextSol.imageView;
	pImageInfo[2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
}