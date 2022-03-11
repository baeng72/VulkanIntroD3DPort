#include "../../../Common/VulkApp.h"
#include "../../../Common/VulkUtil.h"
#include "../../../Common/VulkanEx.h"
#include "../../../Common/GeometryGenerator.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/Colors.h"
#include "../../../Common/TextureLoader.h"
#include "../../../Common/Camera.h"
#include <memory>
#include <fstream>
#include <iostream>
#include <sstream>
#include "FrameResource.h"

const int gNumFrameResources = 3;

struct RenderItem {
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;
	//World matrix of the shape that descripes the object's local space
	//relative to the world space,
	glm::mat4 World = glm::mat4(1.0f);

	glm::mat4 TexTransform = glm::mat4(1.0f);

	// Used for GPU waves render items.
	glm::vec2 DisplacementMapTexelSize = { 1.0f, 1.0f };
	float GridSpatialStep = 1.0f;

	int NumFramesDirty = gNumFrameResources;

	uint32_t ObjCBIndex = -1;

	Material* Mat{ nullptr };
	MeshGeometry* Geo{ nullptr };

	bool Visible = true;
	//BoundingBox Bounds;
	AABB Bounds;

	uint32_t IndexCount{ 0 };
	uint32_t StartIndexLocation{ 0 };
	uint32_t BaseVertexLocation{ 0 };
};

enum class RenderLayer : int
{
	Opaque = 0,
	Highlight,
	Count
};

class PickingApp : public VulkApp {
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	std::unique_ptr<DescriptorSetLayoutCache> descriptorSetLayoutCache;
	std::unique_ptr<DescriptorSetPoolCache> descriptorSetPoolCache;
	std::unique_ptr<VulkanUniformBuffer> uniformBuffer;
	std::unique_ptr<VulkanImageList> textures;
	std::unique_ptr<VulkanUniformBuffer> storageBuffer;
	std::unique_ptr<VulkanDescriptorList> uniformDescriptors;
	std::unique_ptr<VulkanDescriptorList> textureDescriptors;
	std::unique_ptr<VulkanDescriptorList> storageDescriptors;
	std::unique_ptr<VulkanSampler> sampler;
	std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
	std::unique_ptr<VulkanPipeline> opaquePipeline;
	std::unique_ptr<VulkanPipeline> wireframePipeline;
	std::unique_ptr<VulkanPipeline> highlightPipeline;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map < std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture> > mTextures;
	std::unordered_map<std::string, VkPipeline> mPSOs;

	bool mIsWireframe{ false };

	RenderItem* mWavesRitem{ nullptr };

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	RenderItem* mPickedRitem = nullptr;

	PassConstants mMainPassCB;

	Camera mCamera;

	POINT mLastMousePos;

	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);

	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void AnimateMaterials(const GameTimer& gt);
	void UpdateMaterialsBuffer(const GameTimer& gt);
	void LoadTextures();
	void BuildBuffers();
	void BuildDescriptors();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void BuildCarGeometry();
	void DrawRenderItems(VkCommandBuffer, const std::vector<RenderItem*>& ritems);
	void Pick(int sx, int sy);
public:
	PickingApp(HINSTANCE hInstance);
	PickingApp(const PickingApp& rhs) = delete;
	PickingApp& operator=(const PickingApp& rhs) = delete;
	~PickingApp();
	virtual bool Initialize()override;
};


PickingApp::PickingApp(HINSTANCE hInstance) :VulkApp(hInstance) {
	mAllowWireframe = true;
	mClearValues[0].color = Colors::LightSteelBlue;
	mMSAA = false;
	mDepthBuffer = true;
}

PickingApp::~PickingApp() {
	vkDeviceWaitIdle(mDevice);

	for (auto& pair : mGeometries) {
		free(pair.second->indexBufferCPU);
		//if (pair.second->vertexBufferGPU.buffer != mWavesRitem->Geo->vertexBufferGPU.buffer) {
		free(pair.second->vertexBufferCPU);
		cleanupBuffer(mDevice, pair.second->vertexBufferGPU);
		//}
		cleanupBuffer(mDevice, pair.second->indexBufferGPU);
	}
}



bool PickingApp::Initialize() {
	if (!VulkApp::Initialize())
		return false;

	//mCamera.SetPosition(0.0f, 2.0f, -15.0f);
	mCamera.LookAt(glm::vec3(5.0f, 4.0f, -15.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));


	LoadTextures();
	BuildCarGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildBuffers();
	BuildDescriptors();
	BuildPSOs();
	BuildFrameResources();

	return true;

}

void PickingApp::LoadTextures()
{
	std::vector<Vulkan::Image> texturesList;
	ImageLoader::begin(mDevice, mCommandBuffer, mGraphicsQueue, mMemoryProperties)
		.addImage("../../../Textures/white1x1.jpg")
		.addImage("../../../Textures/bricks.jpg")
		.addImage("../../../Textures/stone.jpg")
		.addImage("../../../Textures/tile.jpg")
		.load(texturesList);
	textures = std::make_unique<VulkanImageList>(mDevice, texturesList);
	Vulkan::SamplerProperties sampProps;
	sampProps.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler = std::make_unique<VulkanSampler>(mDevice, Vulkan::initSampler(mDevice, sampProps));
}

void PickingApp::BuildCarGeometry()
{
	std::ifstream fin("Models/car.txt");

	if (!fin)
	{
		MessageBox(0, L"Models/car.txt not found.", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;

	glm::vec3 vMin(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	glm::vec3 vMax(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);

	

	std::vector<Vertex> vertices(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

		glm::vec3 P = vertices[i].Pos;

		vertices[i].TexC = { 0.0f, 0.0f };

		vMin = MathHelper::vectorMin(vMin, P);
		vMax = MathHelper::vectorMax(vMax, P);
	}

	AABB bounds(vMin,vMax);
	

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::uint32_t> indices(3 * tcount);
	for (UINT i = 0; i < tcount; ++i)
	{
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	fin.close();

	//
	// Pack the indices of all the meshes into one index buffer.
	//

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "carGeo";

	geo->vertexBufferCPU = malloc(vbByteSize);
	memcpy(geo->vertexBufferCPU, vertices.data(), vbByteSize);

	geo->indexBufferCPU = malloc(ibByteSize);
	memcpy(geo->indexBufferCPU, indices.data(), ibByteSize);

	std::vector<uint32_t> vertexLocations;
	VertexBufferBuilder::begin(mDevice, mGraphicsQueue, mCommandBuffer, mMemoryProperties)
		.AddVertices(vbByteSize, (float*)vertices.data())
		.build(geo->vertexBufferGPU, vertexLocations);

	std::vector<uint32_t> indexLocations;
	IndexBufferBuilder::begin(mDevice, mGraphicsQueue, mCommandBuffer, mMemoryProperties)
		.AddIndices(ibByteSize, indices.data())
		.build(geo->indexBufferGPU, indexLocations);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	submesh.Bounds = bounds;

	geo->DrawArgs["car"] = submesh;

	mGeometries[geo->Name] = std::move(geo);
}
void PickingApp::BuildBuffers() {
	Vulkan::Buffer dynamicBuffer;
	//pass and object constants are dynamic uniform buffers
	std::vector<UniformBufferInfo> bufferInfo;
	UniformBufferBuilder::begin(mDevice, mDeviceProperties, mMemoryProperties, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, true)
		.AddBuffer(sizeof(PassConstants), 1, gNumFrameResources)
		.AddBuffer(sizeof(ObjectConstants), mAllRitems.size(), gNumFrameResources)
		.build(dynamicBuffer, bufferInfo);
	uniformBuffer = std::make_unique<VulkanUniformBuffer>(mDevice, dynamicBuffer, bufferInfo);

	UniformBufferBuilder::begin(mDevice, mDeviceProperties, mMemoryProperties, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, true)
		.AddBuffer(sizeof(MaterialData), mMaterials.size(), gNumFrameResources)
		.build(dynamicBuffer, bufferInfo);
	storageBuffer = std::make_unique<VulkanUniformBuffer>(mDevice, dynamicBuffer, bufferInfo);
}
void PickingApp::BuildDescriptors() {
	descriptorSetPoolCache = std::make_unique<DescriptorSetPoolCache>(mDevice);
	descriptorSetLayoutCache = std::make_unique<DescriptorSetLayoutCache>(mDevice);

	VkDescriptorSet descriptor0 = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorLayout0;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(descriptor0, descriptorLayout0);

	VkDescriptorSet descriptor1 = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorLayout1;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(descriptor1, descriptorLayout1);

	std::vector<VkDescriptorSet> descriptors{ descriptor0,descriptor1 };
	uniformDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptors);

	VkDescriptorSet descriptor2 = VK_NULL_HANDLE;

	VkDescriptorSetLayout descriptorLayout2;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(descriptor2, descriptorLayout2);

	descriptors = { descriptor2 };
	storageDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptors);

	VkDescriptorSet descriptor3 = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorLayout3 = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, (uint32_t)*textures)
		.build(descriptor3, descriptorLayout3);

	descriptors = { descriptor3 };
	textureDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptors);

	VkDeviceSize offset = 0;
	descriptors = { descriptor0,descriptor1 };
	std::vector<VkDescriptorSetLayout> descriptorLayouts = { descriptorLayout0,descriptorLayout1 };
	auto& ub = *uniformBuffer;
	for (int i = 0; i < (int)descriptors.size(); ++i) {
		//descriptorBufferInfo.clear();
		VkDeviceSize range = ub[i].objectSize;// bufferInfo[i].objectCount* bufferInfo[i].objectSize* bufferInfo[i].repeatCount;
		VkDeviceSize bufferSize = ub[i].objectCount * ub[i].objectSize * ub[i].repeatCount;
		VkDescriptorBufferInfo descrInfo{};
		descrInfo.buffer = ub;
		descrInfo.offset = offset;
		descrInfo.range = ub[i].objectSize;
		//descriptorBufferInfo.push_back(descrInfo);
		offset += bufferSize;
		VkDescriptorSetLayout layout = descriptorLayouts[i];
		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), layout, descriptors[i])
			.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &descrInfo)
			.update();
	}
	{
		auto& sb = *storageBuffer;
		VkDescriptorBufferInfo descrInfo{};
		descrInfo.buffer = sb;
		descrInfo.range = sb[0].objectCount * sb[0].objectSize * sb[0].repeatCount;
		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), descriptorLayout2, descriptor2)
			.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descrInfo)
			.update();
	}
	{
		auto& samp = *sampler;
		VkDescriptorImageInfo sampInfo;
		sampInfo.sampler = samp;
		auto& tl = *textures;
		std::vector<VkDescriptorImageInfo> imageInfos(tl);
		for (int i = 0; i < (int)tl; ++i) {
			imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfos[i].imageView = tl[i].imageView;

		}
		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), descriptorLayout3, descriptor3)
			.AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, &sampInfo)
			.AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos.data(), (uint32_t)tl)
			.update();


	}
	VkPipelineLayout layout{ VK_NULL_HANDLE };
	PipelineLayoutBuilder::begin(mDevice)
		.AddDescriptorSetLayout(descriptorLayout0)
		.AddDescriptorSetLayout(descriptorLayout1)
		.AddDescriptorSetLayout(descriptorLayout2)
		.AddDescriptorSetLayout(descriptorLayout3)
		.build(layout);
	pipelineLayout = std::make_unique<VulkanPipelineLayout>(mDevice, layout);
}

void PickingApp::BuildPSOs() {
	std::vector<Vulkan::ShaderModule> shaders;
	VkVertexInputBindingDescription vertexInputDescription = {};
	std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
	ShaderProgramLoader::begin(mDevice)
		.AddShaderPath("Shaders/default.vert.spv")
		.AddShaderPath("Shaders/default.frag.spv")
		.load(shaders, vertexInputDescription, vertexAttributeDescriptions);


	VkPipeline pipeline{ VK_NULL_HANDLE };
	PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
		.setCullMode(VK_CULL_MODE_FRONT_BIT)
		.setPolygonMode(VK_POLYGON_MODE_FILL)
		.setDepthTest(VK_TRUE)
		.build(pipeline);
	opaquePipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
	mPSOs["opaque"] = *opaquePipeline;
	PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
		.setCullMode(VK_CULL_MODE_FRONT_BIT)
		.setPolygonMode(VK_POLYGON_MODE_LINE)
		.setDepthTest(VK_TRUE)
		.build(pipeline);
	wireframePipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
	mPSOs["opaque_wireframe"] = *wireframePipeline;

	PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
		.setCullMode(VK_CULL_MODE_FRONT_BIT)
		.setPolygonMode(VK_POLYGON_MODE_FILL)
		.setDepthTest(VK_TRUE)
		.setDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL)
		.setBlend(VK_TRUE)
		//.setBlendState(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD)
		.build(pipeline);
	highlightPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
	mPSOs["highlight"] = *highlightPipeline;

	for (auto& shader : shaders) {
		Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
	}
}
void PickingApp::BuildFrameResources() {
	for (int i = 0; i < gNumFrameResources; i++) {
		auto& ub = *uniformBuffer;
		auto& sb = *storageBuffer;
		PassConstants* pc = (PassConstants*)((uint8_t*)ub[0].ptr + ub[0].objectSize * ub[0].objectCount * i);// ((uint8_t*)pPassCB + passSize * i);
		ObjectConstants* oc = (ObjectConstants*)((uint8_t*)ub[1].ptr + ub[1].objectSize * ub[1].objectCount * i);// ((uint8_t*)pObjectCB + objectSize * mAllRitems.size() * i);
		//MaterialConstants* mc = (MaterialConstants*)((uint8_t*)ub[2].ptr + ub[2].objectSize * ub[2].objectCount * i);// ((uint8_t*)pMatCB + matSize * mMaterials.size() * i);
		MaterialData* md = (MaterialData*)((uint8_t*)sb[0].ptr + sb[0].objectSize * sb[0].objectCount * i);
		//Vertex* pWv = (Vertex*)WaveVertexPtrs[i];
		mFrameResources.push_back(std::make_unique<FrameResource>(pc, oc, md));// , pWv));
	}
}

void PickingApp::BuildMaterials()
{
	auto gray0 = std::make_unique<Material>();
	gray0->NumFramesDirty = gNumFrameResources;
	gray0->Name = "gray0";
	gray0->MatCBIndex = 0;
	gray0->DiffuseSrvHeapIndex = 0;
	gray0->DiffuseAlbedo = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
	gray0->FresnelR0 = glm::vec3(0.04f, 0.04f, 0.04f);
	gray0->Roughness = 0.0f;

	auto highlight0 = std::make_unique<Material>();
	highlight0->NumFramesDirty = gNumFrameResources;
	highlight0->Name = "highlight0";
	highlight0->MatCBIndex = 1;
	highlight0->DiffuseSrvHeapIndex = 0;
	highlight0->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 0.0f, 0.6f);
	highlight0->FresnelR0 = glm::vec3(0.06f, 0.06f, 0.06f);
	highlight0->Roughness = 0.0f;


	mMaterials["gray0"] = std::move(gray0);
	mMaterials["highlight0"] = std::move(highlight0);
}


void PickingApp::BuildRenderItems()
{
	auto carRitem = std::make_unique<RenderItem>();
	
	carRitem->World = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f));// , XMMatrixScaling(1.0f, 1.0f, 1.0f)* XMMatrixTranslation(0.0f, 1.0f, 0.0f));
	//XMStoreFloat4x4(&carRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	carRitem->TexTransform = glm::mat4(1.0f);
	carRitem->ObjCBIndex = 0;
	carRitem->Mat = mMaterials["gray0"].get();
	carRitem->Geo = mGeometries["carGeo"].get();
	
	carRitem->Bounds = carRitem->Geo->DrawArgs["car"].Bounds;
	carRitem->IndexCount = carRitem->Geo->DrawArgs["car"].IndexCount;
	carRitem->StartIndexLocation = carRitem->Geo->DrawArgs["car"].StartIndexLocation;
	carRitem->BaseVertexLocation = carRitem->Geo->DrawArgs["car"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(carRitem.get());

	auto pickedRitem = std::make_unique<RenderItem>();
	pickedRitem->World = MathHelper::Identity4x4();
	pickedRitem->TexTransform = MathHelper::Identity4x4();
	pickedRitem->ObjCBIndex = 1;
	pickedRitem->Mat = mMaterials["highlight0"].get();
	pickedRitem->Geo = mGeometries["carGeo"].get();	

	// Picked triangle is not visible until one is picked.
	pickedRitem->Visible = false;

	// DrawCall parameters are filled out when a triangle is picked.
	pickedRitem->IndexCount = 0;
	pickedRitem->StartIndexLocation = 0;
	pickedRitem->BaseVertexLocation = 0;
	mPickedRitem = pickedRitem.get();
	mRitemLayer[(int)RenderLayer::Highlight].push_back(pickedRitem.get());


	mAllRitems.push_back(std::move(carRitem));
	mAllRitems.push_back(std::move(pickedRitem));
}



void PickingApp::OnResize() {
	VulkApp::OnResize();
	mCamera.SetLens(0.25f * MathHelper::Pi, (float)mClientWidth / (float)mClientHeight, 1.0f, 1000.0f);
}

void PickingApp::OnMouseDown(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		mLastMousePos.x = x;
		mLastMousePos.y = y;
		SetCapture(mhMainWnd);
	}
	else if ((btnState & MK_RBUTTON) != 0) {
		Pick(x, y);
	}
}

void PickingApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void PickingApp::OnMouseMove(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		// Make each pixel correspond to a quarter of a degree.
		float dx = glm::radians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = glm::radians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mCamera.Pitch(dx);
		mCamera.RotateY(dy);
	}


	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void PickingApp::OnKeyboardInput(const GameTimer& gt)
{

	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;

	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(10.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-10.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f * dt);

	mCamera.UpdateViewMatrix();
}

void PickingApp::Update(const GameTimer& gt) {
	VulkApp::Update(gt);
	OnKeyboardInput(gt);

	//Cycle through the circular frame resource array
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialsBuffer(gt);
	UpdateMainPassCB(gt);
}

void PickingApp::UpdateMainPassCB(const GameTimer& gt) {
	glm::mat4 view = mCamera.GetView();
	glm::mat4 proj = mCamera.GetProj();
	proj[1][1] *= -1;
	glm::mat4 viewProj = proj * view;//reverse for column major matrices view * proj
	glm::mat4 invView = glm::inverse(view);
	glm::mat4 invProj = glm::inverse(proj);
	glm::mat4 invViewProj = glm::inverse(viewProj);

	PassConstants* pPassConstants = mCurrFrameResource->pPCs;
	mMainPassCB.View = view;
	mMainPassCB.Proj = proj;
	mMainPassCB.ViewProj = viewProj;
	mMainPassCB.InvView = invView;
	mMainPassCB.InvProj = invProj;
	mMainPassCB.InvViewProj = invViewProj;
	mMainPassCB.EyePosW = mCamera.GetPosition();
	mMainPassCB.RenderTargetSize = glm::vec2(mClientWidth, mClientHeight);
	mMainPassCB.InvRenderTargetSize = glm::vec2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	memcpy(pPassConstants, &mMainPassCB, sizeof(PassConstants));
}


void  PickingApp::UpdateObjectCBs(const GameTimer& gt) {

	uint8_t* pObjConsts = (uint8_t*)mCurrFrameResource->pOCs;
	auto& ub = *uniformBuffer;
	VkDeviceSize objSize = ub[1].objectSize;
	for (auto& e : mAllRitems) {
		//Only update the cbuffer data if the constants have changed.
		//This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0) {
			glm::mat4 world = e->World;
			ObjectConstants objConstants;
			objConstants.World = world;
			objConstants.TexTransform = e->TexTransform;
			objConstants.MaterialIndex = e->Mat->MatCBIndex;
			memcpy((pObjConsts + (objSize * e->ObjCBIndex)), &objConstants, sizeof(objConstants));
			//pObjConsts[e->ObjCBIndex] = objConstants;
			e->NumFramesDirty--;
		}
	}
}

void PickingApp::UpdateMaterialsBuffer(const GameTimer& gt) {
	uint8_t* pMatConsts = (uint8_t*)mCurrFrameResource->pMats;
	auto& sb = *storageBuffer;
	VkDeviceSize objSize = sb[0].objectSize;
	for (auto& e : mMaterials) {
		Material* mat = e.second.get();

		if (mat->NumFramesDirty > 0) {
			glm::mat4 matTransform = mat->MatTransform;
			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.FresnelR0 = mat->FresnelR0;
			matData.Roughness = mat->Roughness;
			matData.MatTransform = matTransform;
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
			memcpy((pMatConsts + (objSize * mat->MatCBIndex)), &matData, sizeof(MaterialData));
			mat->NumFramesDirty--;
		}
	}
}

void PickingApp::AnimateMaterials(const GameTimer& gt) {

}
void PickingApp::Draw(const GameTimer& gt) {
	uint32_t index = 0;
	VkCommandBuffer cmd{ VK_NULL_HANDLE };



	cmd = BeginRender();

	VkViewport viewport = { 0.0f,0.0f,(float)mClientWidth,(float)mClientHeight,0.0f,1.0f };
	pvkCmdSetViewport(cmd, 0, 1, &viewport);
	VkRect2D scissor = { {0,0},{(uint32_t)mClientWidth,(uint32_t)mClientHeight} };
	pvkCmdSetScissor(cmd, 0, 1, &scissor);


	auto& ub = *uniformBuffer;
	VkDeviceSize passSize = ub[0].objectSize;
	auto& ud = *uniformDescriptors;
	//bind descriptors that don't change during pass
	VkDescriptorSet descriptor0 = ud[0];//pass constant buffer
	uint32_t dynamicOffsets[1] = { mCurrFrame * (uint32_t)passSize };
	pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
	//bind storage buffer
	auto& sb = *storageBuffer;
	VkDeviceSize matSize = sb[0].objectSize * sb[0].objectCount;
	auto& sd = *storageDescriptors;
	VkDescriptorSet descriptor2 = sd[0];
	dynamicOffsets[0] = mCurrFrame * (uint32_t)matSize;
	pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
	//bind texture array
	auto& td = *textureDescriptors;
	VkDescriptorSet descriptor3 = td[0];
	pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
	if (mIsWireframe) {
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque_wireframe"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Opaque]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Highlight]);
	}
	else {

		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Opaque]);
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["highlight"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Highlight]);
	}
	EndRender(cmd);
}

void PickingApp::DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems) {
	auto& ub = *uniformBuffer;
	VkDeviceSize objectSize = ub[1].objectSize;
	auto& ud = *uniformDescriptors;
	VkDescriptorSet descriptor1 = ud[1];


	for (size_t i = 0; i < ritems.size(); i++) {
		auto ri = ritems[i];
		uint32_t indexOffset = ri->StartIndexLocation;

		const auto vbv = ri->Geo->vertexBufferGPU;
		const auto ibv = ri->Geo->indexBufferGPU;
		pvkCmdBindVertexBuffers(cmd, 0, 1, &vbv.buffer, mOffsets);

		pvkCmdBindIndexBuffer(cmd, ibv.buffer, indexOffset * sizeof(uint32_t), VK_INDEX_TYPE_UINT32);
		uint32_t cbvIndex = ri->ObjCBIndex;
		uint32_t dyoffsets[2] = { (uint32_t)(cbvIndex * objectSize) };
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 1, 1, &descriptor1, 1, dyoffsets);
		pvkCmdDrawIndexed(cmd, ri->IndexCount, 1, 0, ri->BaseVertexLocation, 0);
	}
}


void PickingApp::Pick(int x, int y) {
	glm::mat4 proj = mCamera.GetProj();

	//compute picking ray in view space
	//x, y are in screen space, then need to be converted to normalized space [-1,-1] which is 2 units wide and high and has center at 0,0, then use projection matrix to go back to view space
	float vx = (+2.0f * x / mClientWidth - 1.0f) / proj[0][0];
	float vy = (-2.0f * y / mClientHeight + 1.0f) / proj[1][1];

	//Ray definition in view space
	glm::vec4 rayOrigin = glm::vec4(0, 0, 0.0f,1.0f);//set last component as it's a point
	glm::vec4 rayDir = glm::vec4(vx, vy,1.0f,0.0f);//not a point, but a vector

	glm::mat4 view = mCamera.GetView();
	//glm::mat4 invView = glm::inverse(mCamera.GetView());
	// 
	//assume nothing is picked to start, so the picked render-item is invisible.
	mPickedRitem->Visible = false;

	//check if we picked an opaque render item. 
	for (auto ri : mRitemLayer[(int)RenderLayer::Opaque]) {
		auto geo = ri->Geo;

		// Skip invisible render-items.
		if (ri->Visible == false)
			continue;
		//glm::mat4 invWorld = glm::inverse(ri->World);
		//transform ray to vi space of mesh
		glm::mat4 worldView = view * ri->World;
		glm::mat4 toLocal = glm::inverse(worldView);

		glm::vec3 localOrigin = toLocal * rayOrigin;
		glm::vec3 localDir = glm::normalize(toLocal * rayDir);
		//if we hit bounding box of mesh, we might have picked a Mesh trianble.
		//so do the ray/trianble tests
		float tmin = 0.0f;
		if (ri->Bounds.Intersects(localOrigin, localDir, tmin)) {
			// NOTE: For the demo, we know what to cast the vertex/index data to.  If we were mixing
			// formats, some metadata would be needed to figure out what to cast it to.
			auto vertices = (Vertex*)geo->vertexBufferCPU;
			auto indices = (std::uint32_t*)geo->indexBufferCPU;
			UINT triCount = ri->IndexCount / 3;
			
			Ray ray(localOrigin, localDir);
			float t = 0.0f;
			uint32_t tri = UINT32_MAX;
			if (ray.IntersectMesh<Vertex>(vertices, indices, ri->IndexCount,tri, t)) {
				
				//This is the new nearest picked triangle
				tmin = t;
				uint32_t pickedTriangle = tri;
				mPickedRitem->Visible = true;
				mPickedRitem->IndexCount = 3;
				mPickedRitem->BaseVertexLocation = 0;
				//Picked render item needs same world matrix as object picked
				mPickedRitem->World = ri->World;
				mPickedRitem->NumFramesDirty = gNumFrameResources;

				//offset to the picked triangle in the mesh index buffer.
				mPickedRitem->StartIndexLocation = 3 * tri;
				
			}
		}

	}

}

int main() {
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		PickingApp theApp(GetModuleHandle(NULL));
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