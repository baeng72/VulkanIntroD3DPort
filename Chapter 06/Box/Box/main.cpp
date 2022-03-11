#include "../../Common/VulkApp.h"
#include "../../Common/VulkUtil.h"
#include "../../Common/VulkanEx.h"
#include "../../Common/MathHelper.h"
#include "../../Common/Colors.h"
#include <memory>
#include <array>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Vertex {
    glm::vec3 Pos;
    glm::vec4 Color;
    static VkVertexInputBindingDescription& getInputBindingDescription() {
        static VkVertexInputBindingDescription bindingDescription = { 0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX };
        return bindingDescription;
    }
    static std::vector<VkVertexInputAttributeDescription>& getInputAttributeDescription() {
        static std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {
            {0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,Pos)},
            {1,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(Vertex,Color)},
        };
        return attributeDescriptions;
    }
};

struct ObjectConstants {
    //just a Uniform in Vulkan
    glm::mat4 WorldViewProj = glm::mat4(1.0f);
};

class BoxApp : public VulkApp {
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;
    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
    void BuildPipeline();
    void BuildBoxGeometry();
    void WaitFence();
    //std::unique_ptr<ResourceManager> mResourceManager;
    //std::unique_ptr<VulkanManager> mVulkanManager;
    std::unique_ptr<MeshGeometry> mBoxGeo;
    /*std::vector<VkFence> fences;
    VkFence mFence{ VK_NULL_HANDLE };*/
    size_t pipelineHash{ 0 };
    ObjectConstants* pConstantBuffer{ nullptr };
    glm::mat4 mProj = glm::mat4(1.0f);
    glm::mat4 mWorld = glm::mat4(1.0f);
    glm::mat4 mView = glm::mat4(1.0f);
    float mTheta = 1.5f * pi;
    float mPhi = pi / 4;
    float mRadius = 5.0f;

    POINT mLastMousePos;


public:
    BoxApp(HINSTANCE hInstance);
    BoxApp(const BoxApp& rhs) = delete;
    BoxApp& operator=(const BoxApp& rhs) = delete;
    ~BoxApp();
    virtual bool Initialize()override;
};


BoxApp::BoxApp(HINSTANCE hInstance)
    : VulkApp(hInstance)
{
    mClearValues[0].color = Colors::LightSteelBlue;
    mMSAA = false;
    mDepthBuffer = true;
}

BoxApp::~BoxApp() {
   /* vkDeviceWaitIdle(mDevice);
    
    for (auto fence : fences) {
        Vulkan::cleanupFence(mDevice, fence);
    }*/
    //mResourceManager.reset();
}

bool BoxApp::Initialize() {
    if (!VulkApp::Initialize())
        return false;
    //VMVulkanInfo vulkanInfo;
    //vulkanInfo.device = mDevice;
    //vulkanInfo.commandBuffer = mCommandBuffer;
    //vulkanInfo.memoryProperties = mMemoryProperties;
    //vulkanInfo.queue = mGraphicsQueue;
    //vulkanInfo.formatProperties = mFormatProperties;
    ////mResourceManager = std::make_unique<ResourceManager>(vulkanInfo);
    //mVulkanManager = std::make_unique<VulkanManager>(vulkanInfo);
    BuildPipeline();
    BuildBoxGeometry();
    
    return true;
}

void BoxApp::OnResize() {
    VulkApp::OnResize();
    
    mProj = glm::perspectiveFovLH(0.25f * pi, (float)mClientWidth, (float)mClientHeight, 1.0f, 1000.0f);
}

void BoxApp::OnMouseDown(WPARAM btnState, int x, int y) {
    mLastMousePos.x = x;
    mLastMousePos.y = y;
    SetCapture(mhMainWnd);
}

void BoxApp::OnMouseUp(WPARAM btnState, int x, int y) {
    ReleaseCapture();
}

void BoxApp::OnMouseMove(WPARAM btnState, int x, int y) {
    if ((btnState & MK_LBUTTON) != 0) {
        // Make each pixel correspond to a quarter of a degree.
        float dx = glm::radians(0.25f * static_cast<float>(x - mLastMousePos.x));
        float dy = glm::radians(0.25f * static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);    // Convert Spherical to Cartesian coordinates.
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.005 unit in the scene.
        float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
        float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}
void BoxApp::Update(const GameTimer& gt) {
    VulkApp::Update(gt);
    //WaitFence();
    float x = mRadius * sinf(mPhi) * cosf(mTheta);
    float z = mRadius * sinf(mPhi) * sinf(mTheta);
    float y = mRadius * cosf(mPhi);

    glm::vec3 pos = glm::vec3(x, y, z);
    glm::vec3 target = glm::vec3(0.0f);
    glm::vec3 up = glm::vec3(0.0, 1.0f, 0.0f);

    mView = glm::lookAtLH(pos, target, up);
    //multiply in reverse order. glm is column major, DirectX row major.
    glm::mat4 worldViewProj = mProj * mView * mWorld;// mWorld* mView* mProj;
    /*if (mVulkanManager != nullptr) {
        uint32_t offset = mVulkanManager->GetBufferOffset(pipelineHash, 0);
        offset *= mCurrFrame;
        ObjectConstants* p = (ObjectConstants*)((uint8_t*)pConstantBuffer + offset);

        p->WorldViewProj = worldViewProj;
    }*/
}

void BoxApp::Draw(const GameTimer& gt) {
    uint32_t index = 0;
    VkCommandBuffer cmd{ VK_NULL_HANDLE };

    cmd = BeginRender();

    VkViewport viewport = { 0.0f,0.0f,(float)mClientWidth,(float)mClientHeight,0.0f,1.0f };
    pvkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = { {0,0},{(uint32_t)mClientWidth,(uint32_t)mClientHeight} };
    pvkCmdSetScissor(cmd, 0, 1, &scissor);
    /*VMDrawInfo drawInfo;
    drawInfo.pvkCmdBindDescriptorSets = pvkCmdBindDescriptorSets;
    drawInfo.pvkCmdBindIndexBuffer = pvkCmdBindIndexBuffer;
    drawInfo.pvkCmdBindPipeline = pvkCmdBindPipeline;
    drawInfo.pvkCmdBindVertexBuffers = pvkCmdBindVertexBuffers;
    drawInfo.pvkCmdDrawIndexed = pvkCmdDrawIndexed;
 
 
    mVulkanManager->DrawPart(pipelineHash,mBoxGeo->hash, drawInfo, cmd, mBoxGeo->DrawArgs["box"],1,mCurrFrame);*/
 
    
    EndRender(cmd);
}


void BoxApp::BuildPipeline() {
   
   /* VMDrawObjectInfo doi;
    doi.shaderPaths = { "Shaders/cube.vert.spv","Shaders/cube.frag.spv" };
    doi.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    doi.buffers.push_back({ sizeof(ObjectConstants),VK_SHADER_STAGE_VERTEX_BIT,3,bUniform,true });
    doi.passCount = 3;
    pipelineHash =  mVulkanManager->InitDrawObject("Pipeline", doi, mRenderPass);
    pConstantBuffer = (ObjectConstants*)mVulkanManager->GetBufferPtr(pipelineHash, 0);*/
}


void BoxApp::BuildBoxGeometry() {
    std::array<Vertex, 8> vertices =
    {
        Vertex({ glm::vec3(-1.0f, -1.0f, -1.0f), Colors::colortovec(Colors::White) }),
        Vertex({ glm::vec3(-1.0f, +1.0f, -1.0f), Colors::colortovec(Colors::Black) }),
        Vertex({ glm::vec3(+1.0f, +1.0f, -1.0f), Colors::colortovec(Colors::Red) }),
        Vertex({ glm::vec3(+1.0f, -1.0f, -1.0f), Colors::colortovec(Colors::Green) }),
        Vertex({ glm::vec3(-1.0f, -1.0f, +1.0f), Colors::colortovec(Colors::Blue) }),
        Vertex({ glm::vec3(-1.0f, +1.0f, +1.0f), Colors::colortovec(Colors::Yellow) }),
        Vertex({ glm::vec3(+1.0f, +1.0f, +1.0f), Colors::colortovec(Colors::Cyan) }),
        Vertex({ glm::vec3(+1.0f, -1.0f, +1.0f), Colors::colortovec(Colors::Magenta) })
    };

    std::array<std::uint32_t, 36> indices =
    {
        // front face
        0, 1, 2,
        0, 2, 3,

        // back face
        4, 6, 5,
        4, 7, 6,

        // left face
        4, 5, 1,
        4, 1, 0,

        // right face
        3, 2, 6,
        3, 6, 7,

        // top face
        1, 5, 6,
        1, 6, 2,

        // bottom face
        4, 0, 3,
        4, 3, 7
    };

    VkDeviceSize vertexSize = sizeof(Vertex) * vertices.size();
    VkDeviceSize indexSize = sizeof(uint32_t) * indices.size();

    mBoxGeo = std::make_unique<MeshGeometry>();
    mBoxGeo->vertexBufferCPU = malloc(vertexSize);
    memcpy(mBoxGeo->vertexBufferCPU, vertices.data(), vertexSize);
    mBoxGeo->indexBufferCPU = malloc(indexSize);
    memcpy(mBoxGeo->indexBufferCPU, indices.data(), indexSize);

    
    std::vector<uint32_t> vertexLocations;
    VertexBufferBuilder::begin(mDevice, mBackQueue, mCommandBuffer, mMemoryProperties)
        .AddVertices(vertexSize, (float*)vertices.data())
        .build(mBoxGeo->vertexBufferGPU, vertexLocations);

    std::vector<uint32_t> indexLocations;
    IndexBufferBuilder::begin(mDevice, mBackQueue, mCommandBuffer, mMemoryProperties)
        .AddIndices(indexSize, indices.data())
        .build(mBoxGeo->indexBufferGPU, indexLocations);

    
    
    mBoxGeo->IndexBufferByteSize = (uint32_t)indexSize;
    mBoxGeo->VertexBufferByteSize = (uint32_t)vertexSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (uint32_t)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;
    mBoxGeo->DrawArgs["box"] = submesh;
}


int main() {
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        BoxApp theApp(GetModuleHandle(NULL));
        if (!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch (std::exception& e)
    {

        MessageBoxA(nullptr, e.what(), "Failed", MB_OK);
        return 0;
    }
}