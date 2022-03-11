#include "TextureLoader.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
//#include <ktx.h>
//#include <ktxvulkan.h>
using namespace Vulkan;

void loadTexture(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkPhysicalDeviceMemoryProperties memoryProperties, uint8_t* pixels, uint32_t width, uint32_t height, VkFormat format, Vulkan::Texture& image, bool enableLod) {
	assert(pixels);
	VkDeviceSize imageSize = (uint64_t)width * (uint64_t)height * 4;
	TextureProperties props;
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
	initTexture(device, memoryProperties, props, image);
	transitionImage(device, queue, commandBuffer, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image.mipLevels);

	VkDeviceSize maxSize = imageSize;
	BufferProperties bufProps;
	bufProps.size = maxSize;
	bufProps.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
#ifdef __USE__VMA__
	bufProps.usage = VMA_MEMORY_USAGE_CPU_ONLY;
#else
	bufProps.memoryProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
#endif
	Buffer stagingBuffer;
	
	initBuffer(device, memoryProperties, bufProps, stagingBuffer);
	void* ptr = mapBuffer(device, stagingBuffer);
	//copy image data using staging buffer
	memcpy(ptr, pixels, imageSize);
	stbi_image_free(pixels);
	CopyBufferToImage(device, queue, commandBuffer, stagingBuffer, image, width, height);

	//even if lod not enabled, need to transition, so use this code.
	generateMipMaps(device, queue, commandBuffer, image);

	unmapBuffer(device, stagingBuffer);
	cleanupBuffer(device, stagingBuffer);
}


void loadTexture(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue,/*VkFormatProperties formatProperties,*/ VkPhysicalDeviceMemoryProperties memoryProperties, const char* path, Texture& image, bool enableLod) {
	int texWidth, texHeight, texChannels;
	stbi_uc* texPixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	assert(texPixels);
	loadTexture(device, commandBuffer, queue, memoryProperties, texPixels, texWidth, texHeight, PREFERRED_FORMAT, image, enableLod);
//	VkDeviceSize imageSize = (uint64_t)texWidth * (uint64_t)texHeight * 4;
//	TextureProperties props;
//	props.format = PREFERRED_IMAGE_FORMAT;
//	props.imageUsage = (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
//#ifdef __USE__VMA__
//	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
//#else
//	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
//#endif
//	props.width = (uint32_t)texWidth;
//	props.height = (uint32_t)texHeight;
//	props.mipLevels = enableLod ? 0 : 1;
//	//props.samplerProps.filter = VK_FILTER_NEAREST;
//	props.samplerProps.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//	initTexture(device, memoryProperties, props, image);
//
//	//enableLod ? initTextureImage(device, PREFERRED_IMAGE_FORMAT, formatProperties, memoryProperties, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, (uint32_t)texWidth, (uint32_t)texHeight, image) :
////		initTextureImageNoMip(device, PREFERRED_IMAGE_FORMAT, formatProperties, memoryProperties, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, (uint32_t)texWidth, (uint32_t)texHeight, image);
//	transitionImage(device, queue, commandBuffer, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image.mipLevels);
//
//	VkDeviceSize maxSize = imageSize;
//	BufferProperties bufProps;
//	bufProps.size = maxSize;
//	bufProps.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
//#ifdef __USE__VMA__
//	bufProps.usage = VMA_MEMORY_USAGE_CPU_ONLY;
//#else
//	bufProps.memoryProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
//#endif
//	Buffer stagingBuffer;
//	//initBuffer(device, memoryProperties, maxSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer);
//	initBuffer(device, memoryProperties, bufProps, stagingBuffer);
//	void* ptr = mapBuffer(device, stagingBuffer);
//	//copy image data using staging buffer
//	memcpy(ptr, texPixels, imageSize);
//	stbi_image_free(texPixels);
//	CopyBufferToImage(device, queue, commandBuffer, stagingBuffer, image, texWidth, texHeight);
//
//	//even if lod not enabled, need to transition, so use this code.
//	generateMipMaps(device, queue, commandBuffer, image);
//
//	unmapBuffer(device, stagingBuffer);
//	cleanupBuffer(device, stagingBuffer);

}

//void loadTexture_ktx(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkPhysicalDeviceMemoryProperties memoryProperties, const char* path, Vulkan::Texture& texture, bool enableLod) {
//	ktxResult result;
//	ktxTexture* ktxTexture;
//	result = ktxTexture_CreateFromNamedFile(path, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &ktxTexture);
//	assert(result == KTX_SUCCESS);
//	ktx_uint8_t* pixels = ktxTexture_GetData(ktxTexture);
//	VkDeviceSize imageSize = (VkDeviceSize)ktxTexture_GetDataSize(ktxTexture);
//	//get properties 
//
//	TextureProperties props;
//	props.format = PREFERRED_IMAGE_FORMAT;
//	props.imageUsage = (VkImageUsageFlagBits)(VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
//	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
//	props.width = (uint32_t)ktxTexture->baseWidth;
//	props.height = (uint32_t)ktxTexture->baseHeight;
//	props.mipLevels = enableLod ? ktxTexture->numLevels : 1;
//	props.samplerProps.addressMode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
//	initTexture(device, memoryProperties, props, texture);
//	transitionImage(device, queue, commandBuffer, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture.mipLevels);
//
//	BufferProperties bufProps;
//	bufProps.size = imageSize;
//	bufProps.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
//	bufProps.memoryProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
//
//	Buffer stagingBuffer;
//	initBuffer(device, memoryProperties, bufProps, stagingBuffer);
//	void* ptr = mapBuffer(device, stagingBuffer);
//	memcpy(ptr, pixels, imageSize);
//
//	std::vector<VkBufferImageCopy> bufferCopyRegions;
//	uint32_t offset = 0;
//
//	for (uint32_t i = 0; i < texture.mipLevels; i++) {
//		// Calculate offset into staging buffer for the current mip level
//		ktx_size_t offset;
//		KTX_error_code ret = ktxTexture_GetImageOffset(ktxTexture, i, 0, 0, &offset);
//		assert(ret == KTX_SUCCESS);
//		// Setup a buffer image copy structure for the current mip level
//		VkBufferImageCopy bufferCopyRegion = {};
//		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//		bufferCopyRegion.imageSubresource.mipLevel = i;
//		bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
//		bufferCopyRegion.imageSubresource.layerCount = 1;
//		bufferCopyRegion.imageExtent.width = ktxTexture->baseWidth >> i;
//		bufferCopyRegion.imageExtent.height = ktxTexture->baseHeight >> i;
//		bufferCopyRegion.imageExtent.depth = 1;
//		bufferCopyRegion.bufferOffset = offset;
//		bufferCopyRegions.push_back(bufferCopyRegion);
//	}
//
//	Vulkan::CopyBufferToImage(device, queue, commandBuffer, stagingBuffer, texture, bufferCopyRegions);
//
//	unmapBuffer(device, stagingBuffer);
//	cleanupBuffer(device, stagingBuffer);
//
//	transitionImage(device, queue, commandBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, texture.mipLevels);
//}
#ifdef __USE__VMA__
void saveScreenCap(VkDevice device, VkCommandBuffer cmd, VkQueue queue, VkImage srcImage, VkFormatProperties& formatProperties, VkFormat colorFormat, VkExtent2D extent, uint32_t index) {
#else
void saveScreenCap(VkDevice device, VkCommandBuffer cmd, VkQueue queue, VkImage srcImage,	VkPhysicalDeviceMemoryProperties& memoryProperties, VkFormat colorFormat, VkExtent2D extent, uint32_t index) {
#endif
	//cribbed from Sascha Willems code.

	bool supportsBlit = (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) && (formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT);
	Image dstImage;
	VkImageCreateInfo imageCI{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.format = colorFormat;
	imageCI.extent = { extent.width,extent.height,1 };
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 1;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_LINEAR;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCI.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	VkResult res = VK_SUCCESS;
#ifdef __USE__VMA__
	Vulkan::initImage(device, imageCI, dstImage,true);

#else
	res = vkCreateImage(device, &imageCI, nullptr, &dstImage.image);
	assert(res == VK_SUCCESS);

	VkMemoryRequirements memReqs{};
	vkGetImageMemoryRequirements(device, dstImage.image, &memReqs);
	VkMemoryAllocateInfo memAllocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, memoryProperties, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	res = vkAllocateMemory(device, &memAllocInfo, nullptr, &dstImage.memory);
	assert(res == VK_SUCCESS);
	res = vkBindImageMemory(device, dstImage.image, dstImage.memory, 0);
	assert(res == VK_SUCCESS);
#endif




	transitionImage(device, queue, cmd, dstImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	transitionImage(device, queue, cmd, srcImage, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	res = vkBeginCommandBuffer(cmd, &beginInfo);
	assert(res == VK_SUCCESS);



	if (supportsBlit) {
		VkOffset3D blitSize;
		blitSize.x = extent.width;
		blitSize.y = extent.height;
		blitSize.z = 1;

		VkImageBlit imageBlitRegion{};
		imageBlitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlitRegion.srcSubresource.layerCount = 1;
		imageBlitRegion.srcOffsets[1] = blitSize;
		imageBlitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlitRegion.dstSubresource.layerCount = 1;
		imageBlitRegion.dstOffsets[1];

		vkCmdBlitImage(cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			dstImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&imageBlitRegion,
			VK_FILTER_NEAREST);
	}
	else {
		VkImageCopy imageCopyRegion{};

		imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopyRegion.srcSubresource.layerCount = 1;
		imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageCopyRegion.dstSubresource.layerCount = 1;
		imageCopyRegion.extent.width = extent.width;
		imageCopyRegion.extent.height = extent.height;
		imageCopyRegion.extent.depth = 1;

		vkCmdCopyImage(cmd,
			srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			dstImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&imageCopyRegion);
	}

	res = vkEndCommandBuffer(cmd);
	assert(res == VK_SUCCESS);

	VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;

	VkFence fence = initFence(device);


	res = vkQueueSubmit(queue, 1, &submitInfo, fence);
	assert(res == VK_SUCCESS);

	res = vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
	assert(res == VK_SUCCESS);


	vkDestroyFence(device, fence, nullptr);


	transitionImage(device, queue, cmd, dstImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

	transitionImage(device, queue, cmd, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VkImageSubresource subResource{ VK_IMAGE_ASPECT_COLOR_BIT,0,0 };
	VkSubresourceLayout subResourceLayout;
	vkGetImageSubresourceLayout(device, dstImage.image, &subResource, &subResourceLayout);

	bool colorSwizzle = false;
	if (!supportsBlit)
	{
		std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
		colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), colorFormat) != formatsBGR.end());
	}

	uint8_t* data{ nullptr };
#ifdef __USE__VMA__
	data =(uint8_t*) dstImage.allocationInfo.pMappedData;
#else
	vkMapMemory(device, dstImage.memory, 0, VK_WHOLE_SIZE, 0, (void**)&data);
#endif
	data += subResourceLayout.offset;
	char fname[512];
	_itoa_s(index, fname, sizeof(fname), 10);
	char path[512];
	sprintf_s(path, "%s.jpg", fname);

	if (colorSwizzle) {
		uint32_t* ppixel = (uint32_t*)data;
		//must be a better way to do this
		for (uint32_t i = 0; i < extent.height; i++) {
			for (uint32_t j = 0; j < extent.width; j++) {

				uint32_t pix = ppixel[i * extent.width + j];
				uint8_t a = (pix & 0xFF000000) >> 24;
				uint8_t r = (pix & 0x00FF0000) >> 16;
				uint8_t g = (pix & 0x0000FF00) >> 8;
				uint8_t b = (pix & 0x000000FF);
				uint32_t newPix = (a << 24) | (b << 16) | (g << 8) | r;
				ppixel[i * extent.width + j] = newPix;

			}
		}
	}
	stbi_write_jpg(path, extent.width, extent.height, 4, data, 100);
#ifdef __USE__VMA__

	
#else
	vkUnmapMemory(device, dstImage.memory);
#endif

	cleanupImage(device, dstImage);


}

