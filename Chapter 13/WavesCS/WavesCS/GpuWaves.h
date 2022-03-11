#pragma once
#include "../../../Common/Vulkan.h"
#include "../../../Common/GameTimer.h"
#include <glm/glm.hpp>

struct GpuUBO {
	float gWaveConstant0;
	float gWaveConstant1;
	float gWaveConstant2;
	float gDisturbMag;
	glm::ivec2 gDisturbIndex;
};

class GpuWaves {
	VkDevice device{ VK_NULL_HANDLE };
	VkPhysicalDeviceMemoryProperties memoryProperties;
	VkFence fence{ VK_NULL_HANDLE };
	uint32_t mNumRows;
	uint32_t mNumCols;

	uint32_t mVertexCount;
	uint32_t mTriangleCount;
	// Simulation constants we can precompute.
	float mK[3];

	float mTimeStep;
	float mSpatialStep;
	Vulkan::Image mPrevSol;
	Vulkan::Image mCurrSol;
	Vulkan::Image mNextSol;
	void BuildResources(VkQueue queue_, VkCommandBuffer cmd_);
public:
	GpuWaves(VkDevice device_,VkPhysicalDeviceMemoryProperties memoryProperties_,VkQueue queue_, VkCommandBuffer cmd_, int m, int n, float dx, float dt, float speed, float damping);
	GpuWaves(const GpuWaves& rhs) = delete;
	GpuWaves& operator=(const GpuWaves& rhs) = delete;
	//~GpuWaves() = default;
	~GpuWaves();

	uint32_t RowCount()const { return mNumRows; }
	uint32_t ColumnCount()const {	return mNumCols;}
	uint32_t VertexCount()const { return mVertexCount; }
	uint32_t TriangleCount()const { return mTriangleCount; }
	float Width()const { return mNumCols * mSpatialStep; }
	float Depth()const { return mNumRows * mSpatialStep; }
	float SpatialStep()const {return mSpatialStep;}

	const Vulkan::Image& DisplacementMap()const { return mCurrSol; }

	uint32_t DescriptorCount()const;

	

	void Update(const GameTimer& gt, VkQueue queue_, VkCommandBuffer cmd_, VkDescriptorSet descriptorSetUBO_,VkDescriptorSet descriptorSetImages_, VkPipelineLayout pipelineLayout_, VkPipeline pipeline_,/*VkFence fence,*/ GpuUBO* pGpuUBO);

	void Disturb(VkQueue queue_,VkCommandBuffer cmd_, VkDescriptorSet descriptorSetUBO_,VkDescriptorSet descriptorSetImages_, VkPipelineLayout pipelineLayout, VkPipeline pipeline,/* VkFence fence_,*/ uint32_t i, uint32_t j, float magnitude, GpuUBO* pGpuUBO);


	void UpdateImageDescriptors(VkDescriptorImageInfo* pImageInfo)const;

};
