#include "BlurFilter.h"

BlurFilter::BlurFilter(VkDevice device_,VkQueue computeQueue_, uint32_t width_, uint32_t height_, VkFormat format_) :device(device_),computeQueue(computeQueue_),width(width_),height(height_),format(format_){

}

BlurFilter::~BlurFilter() {

}

void BlurFilter::OnResize(uint32_t width_, uint32_t height) {

}
std::vector<float> BlurFilter::CalcGaussWeights(float sigma) {
	float twoSigma2 = 2.0f * sigma * sigma;
	//gaussian 'normal' curve
	int blurRadius = (int)ceil(2.0f * sigma);

	std::vector<float> weights(2 * blurRadius + 1);

	float weightSum = 0.0f;

	for (int i = -blurRadius; i <= blurRadius; ++i) {
		float x = (float)i;
		weights[i + blurRadius] = expf(-x * x / twoSigma2);
		weightSum += weights[i + blurRadius];
	}

	//Divide by the sum so all the weights add up to 1.0
	for (int i = 0; i < weights.size(); ++i) {
		weights[i] /= weightSum;
	}
	return weights;
}

void BlurFilter::Execute(VkCommandBuffer cmd, VkPipeline horzPipeline, VkPipeline vertPipeline,BlurParms*pBlurParms, int blurCount) {
	auto weights = CalcGaussWeights(2.5f);
	int blurRadius = (int)weights.size() / 2;
	pBlurParms->gBlurRadius = blurRadius;
	size_t count = std::min(weights.size(),(size_t) 11);
	memcpy(pBlurParms->weights, weights.data(), sizeof(float) * count);
	


}