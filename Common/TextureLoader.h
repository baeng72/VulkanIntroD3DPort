#pragma once
#include "vulkan.h"
//only loads jpg/png
void loadTexture(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue, VkFormatProperties formatProperties, VkPhysicalDeviceMemoryProperties memoryProperties, const char* path, Image& image, bool enableLod=false);

void saveScreenCap(VkDevice device, VkCommandBuffer cmd, VkQueue queue, VkImage srcImage, VkPhysicalDeviceMemoryProperties& memoryProperties, VkFormatProperties& formatProperties, VkFormat colorFormat, VkExtent2D extent, uint32_t index);