#pragma once
#include "../../../Common/Vulkan.h"
#include "../../../Common/VulkUtil.h"
#include "../../../Common/VulkanEx.h"
#include <glm/glm.hpp>

struct BlurParms {
	int gBlurRadius;

	// Support up to 11 blur weights.
	float weights[11];
	
};

class BlurFilter {
	VkDevice device{ VK_NULL_HANDLE };
	VkQueue computeQueue{ VK_NULL_HANDLE };
	uint32_t width{ 0 };
	uint32_t height{ 0 };
	VkFormat format{ PREFERRED_IMAGE_FORMAT };
	std::vector<float> CalcGaussWeights(float sigma);
public:
	BlurFilter(VkDevice device_,VkQueue computeQueue_, uint32_t width_, uint32_t height_, VkFormat format_);
	~BlurFilter();
	void OnResize(uint32_t width_, uint32_t height_);
	void Execute(VkCommandBuffer cmd, VkPipeline horzPipeline, VkPipeline vertPipeline,BlurParms*pBlurParms,int blurCount);
};