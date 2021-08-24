#include "../../../Common/VulkApp.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/Colors.h"
#include "../../../Common/VulkUtil.h"
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

const float pi = 3.14159265358979323846264338327950288f;

class BoxApp : public VulkApp {
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
    void BuildDescriptorHeaps();
    void BuildConstantBuffers();
    void BuildBoxGeometry();
    void BuildRootSignature();
    void BuildPSO();
    VkDescriptorSetLayout   mDescriptorSetLayout{ VK_NULL_HANDLE };
    VkDescriptorPool        mDescriptorPool{ VK_NULL_HANDLE };
    VkDescriptorSet         mDescriptorSet{ VK_NULL_HANDLE };
    VkPipelineLayout        mPipelineLayout{ VK_NULL_HANDLE };
    VkPipeline              mPipeline{ VK_NULL_HANDLE };
    Buffer             mConstantBuffer;
    ObjectConstants*    pConstantBuffer{ nullptr };
    glm::mat4 mProj = glm::mat4(1.0f);
    glm::mat4 mWorld = glm::mat4(1.0f);
    glm::mat4 mView = glm::mat4(1.0f);
    float mTheta = 1.5f * pi;
    float mPhi = pi/4;
    float mRadius = 5.0f;

    POINT mLastMousePos;

    MeshGeometry            mBoxGeo;
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
}

BoxApp::~BoxApp()
{
    free(mBoxGeo.indexBufferCPU);
    free(mBoxGeo.vertexBufferCPU);
    vkDeviceWaitIdle(mDevice);
    cleanupPipeline(mDevice, mPipeline);
    cleanupPipelineLayout(mDevice, mPipelineLayout);
    cleanupBuffer(mDevice, mBoxGeo.indexBufferGPU);
    cleanupBuffer(mDevice, mBoxGeo.vertexBufferGPU);
    unmapBuffer(mDevice, mConstantBuffer);
    cleanupBuffer(mDevice, mConstantBuffer);
    cleanupDescriptorPool(mDevice, mDescriptorPool);
    cleanupDescriptorSetLayout(mDevice, mDescriptorSetLayout);
}

bool BoxApp::Initialize() {
    if (!VulkApp::Initialize())
        return false;
    BuildDescriptorHeaps();
    BuildConstantBuffers();
    BuildBoxGeometry();
    BuildRootSignature();
    BuildPSO();
    return true;

}

void BoxApp::OnResize() {
    VulkApp::OnResize();
    
    mProj = glm::perspectiveFovLH(0.25f * pi,(float) mClientWidth, (float)mClientHeight, 1.0f, 1000.0f);
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
    }else if((btnState & MK_RBUTTON)!=0)
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
void BoxApp::Update(const GameTimer&gt){
    float x = mRadius * sinf(mPhi) * cosf(mTheta);
    float z = mRadius * sinf(mPhi) * sinf(mTheta);
    float y = mRadius * cosf(mPhi);

    glm::vec3 pos = glm::vec3(x, y, z);
    glm::vec3 target = glm::vec3(0.0f);
    glm::vec3 up = glm::vec3(0.0, 1.0f, 0.0f);

    mView = glm::lookAtLH(pos, target, up);
    //multiply in reverse order. glm is column major, DirectX row major.
    glm::mat4 worldViewProj = mProj * mView * mWorld;// mWorld* mView* mProj;
    pConstantBuffer->WorldViewProj = worldViewProj;
}

void BoxApp::Draw(const GameTimer& gt) {
    uint32_t index = 0;
    VkCommandBuffer cmd{ VK_NULL_HANDLE };

    cmd = BeginRender();

    VkViewport viewport = { 0.0f,0.0f,(float)mClientWidth,(float)mClientHeight,0.0f,1.0f };
    pvkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = { {0,0},{(uint32_t)mClientWidth,(uint32_t)mClientHeight} };
    pvkCmdSetScissor(cmd, 0, 1, &scissor);
    pvkCmdBindVertexBuffers(cmd, 0, 1, &mBoxGeo.vertexBufferGPU.buffer, mOffsets);
    pvkCmdBindIndexBuffer(cmd, mBoxGeo.indexBufferGPU.buffer, 0, VK_INDEX_TYPE_UINT32);
    pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeline);
    pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSet, 0, 0);
    pvkCmdDrawIndexed(cmd, mBoxGeo.DrawArgs["box"].indexCount, 1, 0, 0, 0);



    EndRender(cmd);
}

void BoxApp::BuildDescriptorHeaps() {
    //don't really build heaps, just setup descriptors
    
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr}, //Binding 0, uniform (constant) buffer
    };
    mDescriptorSetLayout = initDescriptorSetLayout(mDevice, bindings);
    std::vector<VkDescriptorPoolSize> poolSizes{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1},
    };
    mDescriptorPool = initDescriptorPool(mDevice, poolSizes, 2);
    mDescriptorSet = initDescriptorSet(mDevice, mDescriptorSetLayout, mDescriptorPool);
    
}

void BoxApp::BuildConstantBuffers() {
    
    VkDeviceSize uniformBufferSize = sizeof(ObjectConstants);
    initBuffer(mDevice, mMemoryProperties, uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mConstantBuffer);
    pConstantBuffer = (ObjectConstants*) mapBuffer(mDevice, mConstantBuffer);
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = mConstantBuffer.buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(ObjectConstants);
    std::vector<VkWriteDescriptorSet> descriptorWrites = {
            { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,mDescriptorSet,0,0,1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,nullptr,&bufferInfo,nullptr }
    };
    updateDescriptorSets(mDevice, descriptorWrites);

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

    mBoxGeo.vertexBufferCPU = malloc(vertexSize);
    memcpy(mBoxGeo.vertexBufferCPU, vertices.data(), vertexSize);
    mBoxGeo.indexBufferCPU = malloc(indexSize);
    memcpy(mBoxGeo.indexBufferCPU, indices.data(), indexSize);

    initBuffer(mDevice, mMemoryProperties, vertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mBoxGeo.vertexBufferGPU);
    initBuffer(mDevice, mMemoryProperties, indexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mBoxGeo.indexBufferGPU);

    VkDeviceSize maxSize = std::max(vertexSize, indexSize);
    Buffer stagingBuffer;
    initBuffer(mDevice, mMemoryProperties, maxSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer);
    void* ptr = mapBuffer(mDevice, stagingBuffer);
    //copy vertex data
    memcpy(ptr, vertices.data(), vertexSize);
    CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, mBoxGeo.vertexBufferGPU, vertexSize);
    memcpy(ptr, indices.data(), indexSize);
    CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, mBoxGeo.indexBufferGPU, indexSize);
    unmapBuffer(mDevice, stagingBuffer);
    cleanupBuffer(mDevice, stagingBuffer);
    mBoxGeo.indexBufferByteSize = (uint32_t)indexSize;
    mBoxGeo.vertexBufferByteSize = (uint32_t)vertexSize;

    SubmeshGeometry submesh;
    submesh.indexCount = (uint32_t)indices.size();
    submesh.startIndexLocation = 0;
    submesh.baseVertexLocation = 0;
    mBoxGeo.DrawArgs["box"] = submesh;




}

void BoxApp::BuildRootSignature() {
    //build pipeline layout
    mPipelineLayout = initPipelineLayout(mDevice, mDescriptorSetLayout);
}

void BoxApp::BuildPSO() {
    //build  pipeline
    std::vector<ShaderModule> shaders = {
        {initShaderModule(mDevice, "Shaders/cube.vert.spv"),VK_SHADER_STAGE_VERTEX_BIT},
        {initShaderModule(mDevice,"Shaders/cube.frag.spv"),VK_SHADER_STAGE_FRAGMENT_BIT},
    };
    mPipeline = initGraphicsPipeline(mDevice, mRenderPass, mPipelineLayout, shaders, Vertex::getInputBindingDescription(), Vertex::getInputAttributeDescription(),
        VK_CULL_MODE_BACK_BIT);
    cleanupShaderModule(mDevice, shaders[0].shaderModule);
    cleanupShaderModule(mDevice, shaders[1].shaderModule);
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