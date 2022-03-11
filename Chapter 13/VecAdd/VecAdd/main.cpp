#include "../../../Common/VulkApp.h"
#include "../../../Common/VulkUtil.h"
#include "../../../Common/VulkanEx.h"
#include "../../../Common/GeometryGenerator.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/Colors.h"
#include "../../../Common/TextureLoader.h"
#include <memory>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "FrameResource.h"

const int gNumFrameResources = 3;

struct Data
{
	glm::vec3 v1;
	alignas (16)  glm::vec2 v2;
};

struct RenderItem {
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;
	//World matrix of the shape that descripes the object's local space
	//relative to the world space,
	glm::mat4 World = glm::mat4(1.0f);

	glm::mat4 TexTransform = glm::mat4(1.0f);

	int NumFramesDirty = gNumFrameResources;

	uint32_t ObjCBIndex = -1;

	Material* Mat{ nullptr };
	MeshGeometry* Geo{ nullptr };

	uint32_t IndexCount{ 0 };
	uint32_t StartIndexLocation{ 0 };
	uint32_t BaseVertexLocation{ 0 };
};

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	Count
};


class VecAddCSApp : public VulkApp {
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	std::unique_ptr<DescriptorSetLayoutCache> descriptorSetLayoutCache;
	std::unique_ptr<DescriptorSetPoolCache> descriptorSetPoolCache;
	std::unique_ptr<VulkanUniformBuffer> computeBuffer;
	std::unique_ptr<VulkanDescriptorList> computeDescriptors;
	std::unique_ptr<VulkanPipelineLayout> computePipelineLayout;
	std::unique_ptr<VulkanPipeline> computePipeline;
	std::vector<VkCommandPool>computeCommandPools;
	std::vector<VkCommandBuffer>computeCommandBuffers;
	std::vector<VkFence>computeFences;
	



	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, VkPipeline> mPSOs;

	RenderItem* mWavesRitem{ nullptr };

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	const int NumDataElements = 32;

	std::unique_ptr<VulkanBuffer> mInputBufferA;
	std::unique_ptr<VulkanBuffer> mInputBufferB;
	std::unique_ptr<VulkanBuffer> mOutputBuffer;
	std::unique_ptr<VulkanBuffer> mReadbackBuffer;

	PassConstants mMainPassCB;


	glm::vec3 mEyePos = glm::vec3(0.0f);
	glm::mat4 mView = glm::mat4(1.0f);
	glm::mat4 mProj = glm::mat4(1.0f);

	float mTheta = 1.5f * pi;
	float mPhi = pi / 2 - 0.1f;
	float mRadius = 50.0f;

	POINT mLastMousePos;

	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void DoComputeWork();

	void BuildBuffers();

	void BuildPSOs();
	void BuildFrameResources();
public:
	VecAddCSApp(HINSTANCE);
	VecAddCSApp& operator=(const VecAddCSApp& rhs) = delete;
	~VecAddCSApp();
	virtual bool Initialize()override;
};

VecAddCSApp::VecAddCSApp(HINSTANCE hInstance) :VulkApp(hInstance) {
	mAllowWireframe = true;
	mClearValues[0].color = Colors::LightSteelBlue;
	mMSAA = false;
}

VecAddCSApp::~VecAddCSApp() {
	vkDeviceWaitIdle(mDevice);
	for (auto& fence : computeFences)
		Vulkan::cleanupFence(mDevice,fence);
	for(size_t i=0;i<computeCommandBuffers.size();++i)
		Vulkan::cleanupCommandBuffer(mDevice, computeCommandPools[i], computeCommandBuffers[i]);
	for(size_t i=0;i<computeCommandPools.size();++i)
		Vulkan::cleanupCommandPool(mDevice, computeCommandPools[i]);
}

bool VecAddCSApp::Initialize() {
	if (!VulkApp::Initialize())
		return false;

	BuildBuffers();
	BuildPSOs();
	BuildFrameResources();

	//DoComputeWork();

	return true;

}

void VecAddCSApp::BuildBuffers() {
	std::vector<Data> dataA(NumDataElements);
	std::vector<Data> dataB(NumDataElements);
	std::vector<Data> dataC(NumDataElements);
	for (int i = 0; i < NumDataElements; ++i)
	{
		dataA[i].v1 = glm::vec3(i, i, i);
		dataA[i].v2 = glm::vec2(i, 0);

		dataB[i].v1 = glm::vec3(-i, i, 0.0f);
		dataB[i].v2 = glm::vec2(0, -i);

		dataC[i].v1 = glm::vec3(0.0f);
		dataC[i].v2 = glm::vec2(0.0f);
	}

	VkDeviceSize dataASize = sizeof(Data) * NumDataElements;
	VkDeviceSize dataBSize = sizeof(Data) * NumDataElements;
	VkDeviceSize dataCSize = sizeof(Data) * NumDataElements;

	VkDeviceSize dataTotalSize = dataASize + dataBSize + dataCSize;
	Vulkan::Buffer dynamicBuffer;
	std::vector<UniformBufferInfo> bufferInfo;
	UniformBufferBuilder::begin(mDevice, mDeviceProperties, mMemoryProperties, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, true)
		.AddBuffer(dataASize, 1, 1)
		.AddBuffer(dataBSize, 1, 1)
		.AddBuffer(dataCSize, 1, 1)
		.build(dynamicBuffer, bufferInfo);
	memcpy(bufferInfo[0].ptr, dataA.data(), dataASize);
	memcpy(bufferInfo[1].ptr, dataB.data(), dataBSize);
	memcpy(bufferInfo[2].ptr, dataC.data(), dataCSize);
	computeBuffer = std::make_unique<VulkanUniformBuffer>(mDevice, dynamicBuffer, bufferInfo);
}

void VecAddCSApp::BuildPSOs() {
	descriptorSetPoolCache = std::make_unique<DescriptorSetPoolCache>(mDevice);
	descriptorSetLayoutCache = std::make_unique<DescriptorSetLayoutCache>(mDevice);

	std::vector<VkDescriptorSet> descriptors;
	
	VkDescriptorSetLayout descriptorLayout = { VK_NULL_HANDLE };
	
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(descriptors, descriptorLayout,3);

	
	computeDescriptors = std::make_unique<VulkanDescriptorList>(mDevice,descriptors);

	auto& cb = *computeBuffer;
	VkDeviceSize offset = 0;
	for (size_t i = 0; i < descriptors.size(); ++i) {
		VkDescriptorBufferInfo descrInfo{};
		descrInfo.buffer = cb;
		descrInfo.offset = offset;
		descrInfo.range = cb[i].objectSize;
		offset += cb[i].objectSize;
		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), descriptorLayout, descriptors[i])
			.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descrInfo)
			.update();
	}

	VkPipelineLayout layout{ VK_NULL_HANDLE };
	PipelineLayoutBuilder::begin(mDevice)
		.AddDescriptorSetLayout(descriptorLayout)
		.AddDescriptorSetLayout(descriptorLayout)
		.AddDescriptorSetLayout(descriptorLayout)
		.build(layout);
	computePipelineLayout = std::make_unique<VulkanPipelineLayout>(mDevice, layout);


	std::vector<Vulkan::ShaderModule> shaders;
	ShaderProgramLoader::begin(mDevice)
		.AddShaderPath("Shaders/vecadd.comp.spv")
		.load(shaders);

	VkPipeline pipeline{ VK_NULL_HANDLE };
	Vulkan::ShaderModule shader = shaders[0];
	pipeline = Vulkan::initComputePipeline(mDevice, *computePipelineLayout, shader);
	computePipeline = std::make_unique<VulkanPipeline>(mDevice,pipeline);

	Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);

	Vulkan::initCommandPools(mDevice, 3, mQueues.computeQueueFamily, computeCommandPools);
	Vulkan::initCommandBuffers(mDevice, computeCommandPools, computeCommandBuffers);
	for(size_t i=0;i<3;++i)
		computeFences.push_back(Vulkan::initFence(mDevice, VK_FENCE_CREATE_SIGNALED_BIT));
}

void VecAddCSApp::BuildFrameResources() {

}

void VecAddCSApp::DoComputeWork() {
	
	VkCommandBuffer cmd = computeCommandBuffers[mCurrFrame];
	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	pvkBeginCommandBuffer(cmd, &beginInfo);
	pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipeline);
	auto& descriptors = *computeDescriptors;
	VkDescriptorSet descriptor0 = descriptors[0];
	VkDescriptorSet descriptor1 = descriptors[1];
	VkDescriptorSet descriptor2 = descriptors[2];
	VkDescriptorSet descriptorSets[] = { descriptor0,descriptor1,descriptor2 };
	pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *computePipelineLayout, 0, 3, descriptorSets, 0, 0);
	pvkCmdDispatch(cmd, NumDataElements/16,1, 1);
	pvkEndCommandBuffer(cmd);

	VkSubmitInfo computeInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
	computeInfo.commandBufferCount = 1;
	computeInfo.pCommandBuffers = &cmd;

	VkResult res = pvkQueueSubmit(mComputeQueue, 1, &computeInfo, VK_NULL_HANDLE);
	assert(res == VK_SUCCESS);
	VkFence fence = computeFences[mCurrFrame];



}

void VecAddCSApp::Update(const GameTimer& gt) {
	
	VulkApp::Update(gt);
	//Cycle through the circular frame resource array
	//mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	//mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();
	//DoComputeWork();
}

void VecAddCSApp::OnMouseDown(WPARAM btnState, int x, int y) {
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void VecAddCSApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void VecAddCSApp::OnMouseMove(WPARAM btnState, int x, int y) {
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
		float dx = 0.05f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.05f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}


void VecAddCSApp::Draw(const GameTimer&gt) {
	DoComputeWork();
	uint32_t index = 0;
	VkCommandBuffer cmd{ VK_NULL_HANDLE };

	cmd = BeginRender();

	VkViewport viewport = { 0.0f,0.0f,(float)mClientWidth,(float)mClientHeight,0.0f,1.0f };
	pvkCmdSetViewport(cmd, 0, 1, &viewport);
	VkRect2D scissor = { {0,0},{(uint32_t)mClientWidth,(uint32_t)mClientHeight} };
	pvkCmdSetScissor(cmd, 0, 1, &scissor);


	EndRender(cmd);
	
}

void VecAddCSApp::OnResize() {
	VulkApp::OnResize();
	mProj = glm::perspectiveFovLH_ZO(0.25f * pi, (float)mClientWidth, (float)mClientHeight, 1.0f, 1000.0f);
}


int main() {
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		VecAddCSApp theApp(GetModuleHandle(NULL));
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

