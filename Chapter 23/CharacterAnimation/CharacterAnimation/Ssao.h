#pragma once
#include "../../../Common/VulkanEx.h"
#include "../../../Common/MathHelper.h"
#include "FrameResource.h"

class Ssao : public VulkanObject {
    VkQueue queue{ VK_NULL_HANDLE };
    VkCommandBuffer cmd{ VK_NULL_HANDLE };

    VkPhysicalDeviceMemoryProperties memoryProperties;
    VkImageView depthImageView{ VK_NULL_HANDLE };
    VkFormat depthFormat{ VK_FORMAT_UNDEFINED };
    std::unique_ptr<VulkanTexture> mRandomVectorMap;
    std::unique_ptr<VulkanTexture> mNormalMap;
    std::unique_ptr<VulkanTexture> mAmbientMap0;
    std::unique_ptr<VulkanTexture> mAmbientMap1;

    std::unique_ptr<VulkanFramebuffer> normalFramebuffer;
    std::unique_ptr<VulkanRenderPass> normalRenderPass;
    std::unique_ptr<VulkanFramebuffers> ssaoFramebuffers;
    std::unique_ptr<VulkanRenderPass> ssaoRenderPass;


    static const VkFormat AmbientMapFormat = VK_FORMAT_R16_UNORM;
    static const VkFormat NormalMapFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    static const int MaxBlurRadius = 5;

    uint32_t mRenderTargetWidth;
    uint32_t mRenderTargetHeight;

    glm::vec4 mOffsets[14];


    VkViewport mViewport;
    VkRect2D mScissorRect;
    ///<summary>
   /// Blurs the ambient map to smooth out the noise caused by only taking a
   /// few random samples per pixel.  We use an edge preserving blur so that 
   /// we do not blur across discontinuities--we want edges to remain edges.
   ///</summary>
    void BlurAmbientMap(VkCommandBuffer cmd_, FrameResource* currFrame, int blurCount);
    void BlurAmbientMap(VkCommandBuffer cmd_, bool horzBlur);

    void BuildResources();
    void BuildRandomVectorTexture();

    void BuildOffsetVectors();



public:
    Ssao(VkDevice device_, VkPhysicalDeviceMemoryProperties memoryProperties_, VkQueue queue_, VkCommandBuffer cmd_, VkImageView depthImageView_, VkFormat depthFormat_, uint32_t width, uint32_t height);
    Ssao(const Ssao& rhs) = delete;
    ~Ssao();
    void OnResize(uint32_t width_, uint32_t height_);
    Ssao& operator=(const Ssao& rhs) = delete;
    //VkImage getNormalMapImage()const { return mNormalMap->operator VkImage(); }
    VkImageView getNormalMapImageView()const { return mNormalMap->operator VkImageView(); }
    VkSampler getNormalMapSampler()const { return mNormalMap->operator VkSampler(); }
    VkImageView getRandomMapImageView()const { return mRandomVectorMap->operator VkImageView(); }
    VkSampler getRandomMapSampler()const { return mRandomVectorMap->operator VkSampler(); }
    uint32_t SsaoMapWidth()const { return mRenderTargetWidth / 2; }
    uint32_t SsaoMapHeight()const { return mRenderTargetHeight / 2; }
    void GetOffsetVectors(glm::vec4 offsets[14]) {
        std::copy(&mOffsets[0], &mOffsets[14], &offsets[0]);
    }
    std::vector<float> CalcGaussWeights(float sigma);
    VkRenderPass getNormalRenderPass()const { return normalRenderPass->operator VkRenderPass(); }
    VkFramebuffer getNormalFramebuffer()const { return normalFramebuffer->operator VkFramebuffer(); }
    VkRenderPass getSsaoRenderPass()const { return ssaoRenderPass->operator VkRenderPass(); }
    VkFramebuffer getSsaoFramebuffer(int n)const { return ssaoFramebuffers->operator[](n); }
    VkImageView getSsaoImageView(int n)const { return n == 0 ? mAmbientMap0->operator VkImageView() : mAmbientMap1->operator VkImageView(); }
    VkSampler getSsaoImageSampler(int n)const { return n == 0 ? mAmbientMap0->operator VkSampler() : mAmbientMap1->operator VkSampler(); }
    VkImage getSsaoImage(int n)const { return n == 0 ? mAmbientMap0->operator VkImage() : mAmbientMap1->operator VkImage(); }
    VkRect2D getSsaoScissorRect()const { return mScissorRect; }
    VkViewport getSsaoViewport()const { return mViewport; }


};
