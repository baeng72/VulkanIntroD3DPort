#include "Ssao.h"
#define SSAO_DIM 256
Ssao::Ssao(VkDevice device_, VkPhysicalDeviceMemoryProperties memoryProperties_, VkQueue queue_, VkCommandBuffer cmd_, VkImageView depthImageView_, VkFormat depthFormat_, uint32_t width_, uint32_t height_) :VulkanObject(device_),
memoryProperties(memoryProperties_), queue(queue_), cmd(cmd_), depthImageView(depthImageView_), depthFormat(depthFormat_) {
    OnResize(width_, height_);
    BuildOffsetVectors();
    BuildRandomVectorTexture();
}

Ssao::~Ssao() {

}

std::vector<float> Ssao::CalcGaussWeights(float sigma) {
    float twoSigma2 = 2.0f * sigma * sigma;


    // Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
    // For example, for sigma = 3, the width of the bell curve is 
    int blurRadius = (int)ceil(2.0f * sigma);

    assert(blurRadius <= MaxBlurRadius);

    std::vector<float> weights;
    weights.resize(2 * blurRadius + 1);

    float weightSum = 0.0f;

    for (int i = -blurRadius; i <= blurRadius; ++i)
    {
        float x = (float)i;

        weights[i + blurRadius] = expf(-x * x / twoSigma2);

        weightSum += weights[i + blurRadius];
    }

    // Divide by the sum so all the weights add up to 1.0.
    for (int i = 0; i < weights.size(); ++i)
    {
        weights[i] /= weightSum;
    }

    return weights;
}

void Ssao::OnResize(uint32_t width_, uint32_t height_) {
    if (mRenderTargetWidth != width_ || mRenderTargetHeight != height_) {
        mRenderTargetWidth = width_;
        mRenderTargetHeight = height_;

        //We render to ambient map at half the resolution
        mViewport.x = 0.0f;
        mViewport.y = 0.0f;
        mViewport.width = mRenderTargetWidth / 2.0f;
        mViewport.height = mRenderTargetHeight / 2.0f;
        mViewport.minDepth = 0.0f;
        mViewport.maxDepth = 1.0f;

        mScissorRect = { {0,0},{mRenderTargetWidth / 2,mRenderTargetHeight / 2} };

        BuildResources();
    }
}
void Ssao::BuildResources() {
    mRandomVectorMap = std::make_unique<VulkanTexture>(device, TextureBuilder::begin(device, memoryProperties)
        .setDimensions(SSAO_DIM, SSAO_DIM)
        .setFormat(VK_FORMAT_R8G8B8A8_UNORM)
        .setImageUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
        .setSamplerAddressMode(VK_SAMPLER_ADDRESS_MODE_REPEAT)
        .build());

    mNormalMap = std::make_unique<VulkanTexture>(device, TextureBuilder::begin(device, memoryProperties)
        .setDimensions(mRenderTargetWidth, mRenderTargetHeight)
        .setFormat(NormalMapFormat)
        .setImageUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        .build());
    normalRenderPass = std::make_unique<VulkanRenderPass>(device, RenderPassBuilder::begin(device)
        .setColorFormat(NormalMapFormat)
        .setDepthFormat(depthFormat)
        .setColorInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED)
        .setColorFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .setDepthInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED)
        .setDepthFinalLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        .setDepthLoadOp(VK_ATTACHMENT_LOAD_OP_CLEAR)
        .setDepthStoreOp(VK_ATTACHMENT_STORE_OP_STORE)
        .build());
    std::vector<VkFramebuffer> framebuffers;
    FramebufferBuilder::begin(device)
        .setDimensions(mRenderTargetWidth, mRenderTargetHeight)
        .setColorImageView(*mNormalMap)
        .setDepthImageView(depthImageView)
        .setRenderPass(*normalRenderPass)
        .build(framebuffers);
    normalFramebuffer = std::make_unique<VulkanFramebuffer>(device, framebuffers[0]);

    mAmbientMap0 = std::make_unique<VulkanTexture>(device, TextureBuilder::begin(device, memoryProperties)
        .setDimensions(mRenderTargetWidth / 2, mRenderTargetHeight / 2)
        .setFormat(AmbientMapFormat)
        .setImageUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        .build());

    mAmbientMap1 = std::make_unique<VulkanTexture>(device, TextureBuilder::begin(device, memoryProperties)
        .setDimensions(mRenderTargetWidth / 2, mRenderTargetHeight / 2)
        .setFormat(AmbientMapFormat)
        .setImageUsage(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        .build());
    Vulkan::transitionImage(device, queue, cmd, mAmbientMap1->operator VkImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    ssaoRenderPass = std::make_unique<VulkanRenderPass>(device, RenderPassBuilder::begin(device)
        .setColorFormat(AmbientMapFormat)
        .setColorInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED)
        .setColorFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .setColorLoadOp(VK_ATTACHMENT_LOAD_OP_CLEAR)
        .setColorStoreOp(VK_ATTACHMENT_STORE_OP_STORE)
        .build());

    std::vector<VkImageView> colorImageViews = { *mAmbientMap0,*mAmbientMap1 };
    FramebufferBuilder::begin(device)
        .setDimensions(mRenderTargetWidth / 2, mRenderTargetHeight / 2)
        .setColorImageViews(colorImageViews)
        .setRenderPass(*ssaoRenderPass)
        .build(framebuffers);
    ssaoFramebuffers = std::make_unique<VulkanFramebuffers>(device, framebuffers);




}

void Ssao::BuildRandomVectorTexture() {
    uint32_t* initData = new uint32_t[SSAO_DIM * SSAO_DIM];
    for (int i = 0; i < SSAO_DIM; ++i) {
        for (int j = 0; j < SSAO_DIM; ++j) {
            // Random vector in [0,1].  We will decompress in shader to [-1,1].
            glm::vec4 v = glm::vec4(MathHelper::RandF(), MathHelper::RandF(), MathHelper::RandF(), 0.0);
            uint8_t r = (uint8_t)(v.x * 255);
            uint8_t g = (uint8_t)(v.y * 255);
            uint8_t b = (uint8_t)(v.z * 255);
            uint8_t a = (uint8_t)(v.w * 255);
            uint32_t c = (static_cast<uint32_t>(a) << 24) |
                (static_cast<uint32_t>(r) << 16) |
                (static_cast<uint32_t>(g) << 8) |
                static_cast<uint32_t>(b);
            initData[i * SSAO_DIM + j] = c;

        }
    }
    VkDeviceSize buffSize = sizeof(uint32_t) * SSAO_DIM * SSAO_DIM;
    Vulkan::Buffer stagingBuffer = StagingBufferBuilder::begin(device, memoryProperties)
        .setSize(buffSize)
        .build();

    uint8_t* ptr = (uint8_t*)mapBuffer(device, stagingBuffer);
    memcpy(ptr, initData, buffSize);
    VulkanTexture& texture = *mRandomVectorMap;
    const Vulkan::Image& image = texture;
    Vulkan::transitionImage(device, queue, cmd, image.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image.mipLevels);
    CopyBufferToImage(device, queue, cmd, stagingBuffer, (Vulkan::Image&)image, SSAO_DIM, SSAO_DIM, 0, 0);
    //even if lod not enabled, need to transition, so use this code.
    //Vulkan::generateMipMaps(device, queue, cmd, (Vulkan::Image&)image);
    Vulkan::transitionImage(device, queue, cmd, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, image.mipLevels);
    unmapBuffer(device, stagingBuffer);
    cleanupBuffer(device, stagingBuffer);

    delete[] initData;
}

void Ssao::BuildOffsetVectors()
{
    // Start with 14 uniformly distributed vectors.  We choose the 8 corners of the cube
    // and the 6 center points along each cube face.  We always alternate the points on 
    // opposites sides of the cubes.  This way we still get the vectors spread out even
    // if we choose to use less than 14 samples.

    // 8 cube corners
    mOffsets[0] = glm::vec4(+1.0f, +1.0f, +1.0f, 0.0f);
    mOffsets[1] = glm::vec4(-1.0f, -1.0f, -1.0f, 0.0f);

    mOffsets[2] = glm::vec4(-1.0f, +1.0f, +1.0f, 0.0f);
    mOffsets[3] = glm::vec4(+1.0f, -1.0f, -1.0f, 0.0f);

    mOffsets[4] = glm::vec4(+1.0f, +1.0f, -1.0f, 0.0f);
    mOffsets[5] = glm::vec4(-1.0f, -1.0f, +1.0f, 0.0f);

    mOffsets[6] = glm::vec4(-1.0f, +1.0f, -1.0f, 0.0f);
    mOffsets[7] = glm::vec4(+1.0f, -1.0f, +1.0f, 0.0f);

    // 6 centers of cube faces
    mOffsets[8] = glm::vec4(-1.0f, 0.0f, 0.0f, 0.0f);
    mOffsets[9] = glm::vec4(+1.0f, 0.0f, 0.0f, 0.0f);

    mOffsets[10] = glm::vec4(0.0f, -1.0f, 0.0f, 0.0f);
    mOffsets[11] = glm::vec4(0.0f, +1.0f, 0.0f, 0.0f);

    mOffsets[12] = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
    mOffsets[13] = glm::vec4(0.0f, 0.0f, +1.0f, 0.0f);

    for (int i = 0; i < 14; ++i)
    {
        // Create random lengths in [0.25, 1.0].
        float s = MathHelper::RandF(0.25f, 1.0f);

        glm::vec4 v = s * glm::normalize(mOffsets[i]);

        mOffsets[i] = v;
    }
}

