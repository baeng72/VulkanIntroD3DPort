#pragma once
#include "vulkan.h"
//only loads jpg/png
void loadTexture(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue,/* VkFormatProperties formatProperties,*/ VkPhysicalDeviceMemoryProperties memoryProperties, const char* path, Vulkan::Texture& image, bool enableLod = false);
void loadTexture(VkDevice device, VkCommandBuffer commandBuffer, VkQueue queue,/* VkFormatProperties formatProperties,*/ VkPhysicalDeviceMemoryProperties memoryProperties, uint8_t*pixels,uint32_t width,uint32_t height,VkFormat format, Vulkan::Texture& image, bool enableLod = false);
#ifdef __USE__VMA__
void saveScreenCap(VkDevice device, VkCommandBuffer cmd, VkQueue queue, VkImage srcImage, VkFormatProperties& formatProperties, VkFormat colorFormat, VkExtent2D extent, uint32_t index);
#else
void saveScreenCap(VkDevice device, VkCommandBuffer cmd, VkQueue queue, VkImage srcImage, VkPhysicalDeviceMemoryProperties& memoryProperties, VkFormatProperties& formatProperties, VkFormat colorFormat, VkExtent2D extent, uint32_t index);
#endif