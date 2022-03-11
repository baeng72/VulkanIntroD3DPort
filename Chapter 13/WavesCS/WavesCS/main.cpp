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

#include "GpuWaves.h"


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

	uint32_t IndexCount{ 0 };
	uint32_t StartIndexLocation{ 0 };
	uint32_t BaseVertexLocation{ 0 };
};

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	GpuWaves,
	Count
};


class WavesCSApp : public VulkApp {
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	std::unique_ptr<DescriptorSetLayoutCache> descriptorSetLayoutCache;
	std::unique_ptr<DescriptorSetPoolCache> descriptorSetPoolCache;
	std::unique_ptr<VulkanUniformBuffer> uniformBuffer;
	std::unique_ptr<VulkanTextureList> textures;
	std::unique_ptr<VulkanDescriptorList> uniformDescriptors;
	std::unique_ptr<VulkanDescriptorList> textureDescriptors;
	std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
	std::unique_ptr<VulkanPipeline> opaquePipeline;
	std::unique_ptr<VulkanPipeline> wireframePipeline;
	std::unique_ptr<VulkanPipeline> transparentPipeline;
	std::unique_ptr<VulkanPipeline> alphatestedPipeline;

	std::unique_ptr<VulkanUniformBuffer> wavesUniformBuffer;
	std::unique_ptr<VulkanDescriptorList> wavesUniformDescriptors;
	std::unique_ptr<VulkanDescriptorList> wavesTextureDescriptors;
	std::unique_ptr<VulkanPipelineLayout> wavesPipelineLayout;
	std::unique_ptr<VulkanPipeline> wavesUpdatePipeline;
	std::unique_ptr<VulkanPipeline> wavesDisturbPipeline;
	VkDescriptorSetLayout wavesTextureDescriptorLayout{ VK_NULL_HANDLE };//cache copy, descriptor layout cache will handle lifetime

	std::unique_ptr<VulkanDescriptorList> displacementDescriptors;
	VkDescriptorSetLayout displacementDescriptorLayout{ VK_NULL_HANDLE };//cache copy, descriptor layout cache will handle lifetime
	std::unique_ptr<VulkanPipelineLayout> wavesGraphicLayout;
	std::unique_ptr<VulkanPipeline> wavesGraphicPipeline;
	std::unique_ptr<VulkanCommandPool> computeCommandPool;
	std::unique_ptr<VulkanCommandBuffer> computeCommandBuffer;



	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map < std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture> > mTextures;
	std::unordered_map<std::string, VkPipeline> mPSOs;

	RenderItem* mWavesRitem{ nullptr };

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	std::unique_ptr<GpuWaves> mWaves;

	PassConstants mMainPassCB;

	glm::vec3 mEyePos = glm::vec3(0.0f);
	glm::mat4 mView = glm::mat4(1.0f);
	glm::mat4 mProj = glm::mat4(1.0f);

	float mTheta = 1.5f * pi;
	float mPhi = pi / 2 - 0.1f;
	float mRadius = 50.0f;

	POINT mLastMousePos;

	bool mIsWireframe{ false };

	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void AnimateMaterials(const GameTimer& gt);
	void UpdateMaterialsCBs(const GameTimer& gt);
	void UpdateWavesGPU(const GameTimer& gt);

	void LoadTextures();
	void BuildLandGeometry();
	void BuildWavesGeometry();
	void BuildBoxGeometry();
	void BuildPSOs();
	void BuildWavesPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems);

	float GetHillsHeight(float x, float z)const;
	glm::vec3 GetHillsNormal(float x, float z)const;

public:
	WavesCSApp(HINSTANCE hInstance);
	WavesCSApp& operator=(const WavesCSApp& rhs) = delete;
	~WavesCSApp();
	virtual bool Initialize()override;
};


WavesCSApp::WavesCSApp(HINSTANCE hInstance) :VulkApp(hInstance) {
	mAllowWireframe = true;
	mClearValues[0].color = Colors::LightSteelBlue;
	mMSAA = false;
}

WavesCSApp::~WavesCSApp() {
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

bool WavesCSApp::Initialize() {
	if (!VulkApp::Initialize())
		return false;

	mWaves = std::make_unique<GpuWaves>(mDevice, mMemoryProperties, mGraphicsQueue, mCommandBuffer, 256, 256, 0.25f, 0.03f, 2.0f, 0.2f);

	BuildLandGeometry();
	BuildWavesGeometry();
	BuildBoxGeometry();
	BuildMaterials();
	BuildRenderItems();


	BuildPSOs();
	BuildWavesPSOs();
	BuildFrameResources();
	computeCommandPool = std::make_unique<VulkanCommandPool>(mDevice, Vulkan::initCommandPool(mDevice, mQueues.computeQueueFamily));
	computeCommandBuffer = std::make_unique<VulkanCommandBuffer>(mDevice, *computeCommandPool, Vulkan::initCommandBuffer(mDevice, *computeCommandPool));
	return true;
}

void WavesCSApp::BuildLandGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

	//Extra the vertex elements we are interested and apply the height function to each vertex.
	// In addition, color the vertices based on their height so we have 
	// sandy looking beaches, grassy low hills, and snow mountain peaks.
	std::vector<Vertex> vertices(grid.Vertices.size());
	for (size_t i = 0; i < grid.Vertices.size(); ++i) {
		auto& p = grid.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Pos.y = GetHillsHeight(p.x, p.z);
		vertices[i].Normal = GetHillsNormal(p.x, p.z);
		vertices[i].TexC = grid.Vertices[i].TexC;


	}
	uint32_t vbByteSize = (uint32_t)vertices.size() * sizeof(Vertex);
	auto& indices = grid.Indices32;
	uint32_t ibByteSize = (uint32_t)indices.size() * sizeof(uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "landGeo";

	geo->vertexBufferCPU = malloc(vbByteSize);
	memcpy(geo->vertexBufferCPU, vertices.data(), vbByteSize);

	geo->indexBufferCPU = malloc(ibByteSize);
	memcpy(geo->indexBufferCPU, indices.data(), ibByteSize);

	std::vector<uint32_t> vertexLocations;
	VertexBufferBuilder::begin(mDevice, mGraphicsQueue, mCommandBuffer, mMemoryProperties)
		.AddVertices(vbByteSize,(float*) vertices.data())
		.build(geo->vertexBufferGPU,vertexLocations);

	std::vector<uint32_t> indexLocations;
	IndexBufferBuilder::begin(mDevice, mGraphicsQueue, mCommandBuffer, mMemoryProperties)
		.AddIndices(ibByteSize, indices.data())
		.build(geo->indexBufferGPU, indexLocations);

	geo->VertexBufferByteSize = vbByteSize;
	geo->VertexByteStride = sizeof(Vertex);
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (uint32_t)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["landGeo"] = std::move(geo);
}

float WavesCSApp::GetHillsHeight(float x, float z)const
{
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}


glm::vec3 WavesCSApp::GetHillsNormal(float x, float z)const
{
	// n = (-df/dx, 1, -df/dz)
	glm::vec3 n(
		-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
		1.0f,
		-0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

	glm::vec3 unitNormal = glm::normalize(n);
	return unitNormal;
}


void WavesCSApp::BuildBoxGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(8.0f, 8.0f, 8.0f, 3);

	std::vector<Vertex> vertices(box.Vertices.size());
	for (size_t i = 0; i < box.Vertices.size(); ++i)
	{
		auto& p = box.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint32_t> indices = box.Indices32;
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

	geo->VertexBufferByteSize = vbByteSize;
	geo->VertexByteStride = sizeof(Vertex);
	geo->IndexBufferByteSize = ibByteSize;

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

	geo->VertexBufferByteSize = vbByteSize;
	geo->VertexByteStride = sizeof(Vertex);
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (uint32_t)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["box"] = submesh;

	mGeometries["boxGeo"] = std::move(geo);
}

void WavesCSApp::BuildWavesGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, mWaves->RowCount(), mWaves->ColumnCount());

	std::vector<Vertex> vertices(grid.Vertices.size());
	for (size_t i = 0; i < grid.Vertices.size(); ++i)
	{
		vertices[i].Pos = grid.Vertices[i].Position;
		vertices[i].Normal = grid.Vertices[i].Normal;
		vertices[i].TexC = grid.Vertices[i].TexC;
	}

	std::vector<std::uint32_t> indices = grid.Indices32;

	uint32_t vbByteSize = mWaves->VertexCount() * sizeof(Vertex);
	uint32_t ibByteSize = (uint32_t)indices.size() * sizeof(uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

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

	geo->VertexBufferByteSize = vbByteSize;
	geo->VertexByteStride = sizeof(Vertex);
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (uint32_t)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["waterGeo"] = std::move(geo);
}


void WavesCSApp::BuildMaterials() {
	auto grass = std::make_unique<Material>();
	grass->NumFramesDirty = gNumFrameResources;
	grass->Name = "grass";
	grass->MatCBIndex = 0;
	grass->DiffuseSrvHeapIndex = 0;
	grass->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = glm::vec3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;

	// This is not a good water material definition, but we do not have all the rendering
	// tools we need (transparency, environment reflection), so we fake it for now.
	auto water = std::make_unique<Material>();
	water->NumFramesDirty = gNumFrameResources;
	water->Name = "water";
	water->MatCBIndex = 1;
	water->DiffuseSrvHeapIndex = 1;
	water->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 0.5f);
	water->FresnelR0 = glm::vec3(0.2f, 0.2f, 0.2f);
	water->Roughness = 0.0f;

	auto wirefence = std::make_unique<Material>();
	wirefence->NumFramesDirty = gNumFrameResources;
	wirefence->Name = "wirefence";
	wirefence->MatCBIndex = 2;
	wirefence->DiffuseSrvHeapIndex = 2;
	wirefence->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	wirefence->FresnelR0 = glm::vec3(0.1f, 0.1f, 0.1f);
	wirefence->Roughness = 0.25f;

	mMaterials["grass"] = std::move(grass);
	mMaterials["water"] = std::move(water);
	mMaterials["wirefence"] = std::move(wirefence);

}

void WavesCSApp::BuildRenderItems() {
	auto wavesRitem = std::make_unique<RenderItem>();
	wavesRitem->World = MathHelper::Identity4x4();
	wavesRitem->TexTransform = glm::scale(glm::mat4(1.0f), glm::vec3(5.0f, 5.0f, 1.0f));// , XMMatrixScaling(5.0f, 5.0f, 1.0f));
	wavesRitem->DisplacementMapTexelSize.x = 1.0f / mWaves->ColumnCount();
	wavesRitem->DisplacementMapTexelSize.y = 1.0f / mWaves->RowCount();
	wavesRitem->GridSpatialStep = mWaves->SpatialStep();
	wavesRitem->ObjCBIndex = 0;
	wavesRitem->Mat = mMaterials["water"].get();
	wavesRitem->Geo = mGeometries["waterGeo"].get();
	///wavesRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["grid"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mWavesRitem = wavesRitem.get();

	mRitemLayer[(int)RenderLayer::GpuWaves].push_back(wavesRitem.get());

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = MathHelper::Identity4x4();
	//XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	gridRitem->TexTransform = glm::scale(glm::mat4(1.0f), glm::vec3(5.0f));
	gridRitem->ObjCBIndex = 1;
	gridRitem->Mat = mMaterials["grass"].get();
	gridRitem->Geo = mGeometries["landGeo"].get();
	//gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());

	auto boxRitem = std::make_unique<RenderItem>();
	//XMStoreFloat4x4(&boxRitem->World, XMMatrixTranslation(3.0f, 2.0f, -9.0f));
	boxRitem->World = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, 2.0f, -9.0f));
	boxRitem->ObjCBIndex = 2;
	boxRitem->Mat = mMaterials["wirefence"].get();
	boxRitem->Geo = mGeometries["boxGeo"].get();
	//boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(boxRitem.get());

	mAllRitems.push_back(std::move(wavesRitem));
	mAllRitems.push_back(std::move(gridRitem));
	mAllRitems.push_back(std::move(boxRitem));
}

void WavesCSApp::BuildPSOs() {
	descriptorSetPoolCache = std::make_unique<DescriptorSetPoolCache>(mDevice);
	descriptorSetLayoutCache = std::make_unique<DescriptorSetLayoutCache>(mDevice);
	VkDescriptorSet descriptor0 = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptorSets0;

	VkDescriptorSetLayout descriptorLayout0;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(descriptor0, descriptorLayout0);

	VkDescriptorSet descriptor1 = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptorSets1;
	VkDescriptorSetLayout descriptorLayout1;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
		.build(descriptor1, descriptorLayout1);

	VkDescriptorSet descriptor2 = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptorSets2;
	VkDescriptorSetLayout descriptorLayout2;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(descriptor2, descriptorLayout2);

	Vulkan::Buffer dynamicBuffer;
	std::vector<UniformBufferInfo> bufferInfo;
	UniformBufferBuilder::begin(mDevice, mDeviceProperties, mMemoryProperties, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, true)
		.AddBuffer(sizeof(PassConstants), 1, gNumFrameResources)
		.AddBuffer(sizeof(ObjectConstants), mAllRitems.size(), gNumFrameResources)
		.AddBuffer(sizeof(MaterialConstants), mMaterials.size(), gNumFrameResources)
		.build(dynamicBuffer, bufferInfo);
	uniformBuffer = std::make_unique<VulkanUniformBuffer>(mDevice, dynamicBuffer, bufferInfo);

	std::vector<Vulkan::Texture> texturesList;
	TextureLoader::begin(mDevice, mCommandBuffer, mGraphicsQueue, mMemoryProperties)
		.addTexture("../../../Textures/grass.jpg")
		.addTexture("../../../Textures/water1.jpg")
		.addTexture("../../../Textures/WireFence-new.png")
		.load(texturesList);

	VkDescriptorSet descriptor3 = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptorSets3;
	VkDescriptorSetLayout descriptorLayout3 = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(descriptorSets3, descriptorLayout3, 3);

	VkDescriptorSet descriptor4 = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptorSets4;
	VkDescriptorSetLayout descriptorLayout4 = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_VERTEX_BIT)
		.build(descriptorSets4, descriptorLayout4, 3);//extra descriptor for displacement map

	displacementDescriptorLayout = descriptorLayout4;
	displacementDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptorSets4);

	textures = std::make_unique<VulkanTextureList>(mDevice, texturesList);
	textureDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptorSets3);

	std::vector<VkDescriptorSet> descriptors{ descriptor0,descriptor1,descriptor2 };
	uniformDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptors);


	std::vector<VkDescriptorSetLayout> descriptorLayouts = { descriptorLayout0,descriptorLayout1,descriptorLayout2 };
	//std::vector<VkDescriptorBufferInfo> descriptorBufferInfo;
	VkDeviceSize offset = 0;

	for (size_t i = 0; i < descriptors.size(); ++i) {
		//descriptorBufferInfo.clear();
		VkDeviceSize range = bufferInfo[i].objectSize;// bufferInfo[i].objectCount* bufferInfo[i].objectSize* bufferInfo[i].repeatCount;
		VkDeviceSize bufferSize = bufferInfo[i].objectCount * bufferInfo[i].objectSize * bufferInfo[i].repeatCount;
		VkDescriptorBufferInfo descrInfo{};
		descrInfo.buffer = dynamicBuffer.buffer;
		descrInfo.offset = offset;
		descrInfo.range = bufferInfo[i].objectSize;
		//descriptorBufferInfo.push_back(descrInfo);
		offset += bufferSize;
		VkDescriptorSetLayout layout = descriptorLayouts[i];
		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), layout, descriptors[i])
			.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &descrInfo)
			.update();
	}

	for (size_t i = 0; i < texturesList.size(); ++i) {
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = texturesList[i].imageView;
		imageInfo.sampler = texturesList[i].sampler;
		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), descriptorLayout3, descriptorSets3[i])
			.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo)
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
	{
		

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

		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_LINE)
			.setDepthTest(VK_TRUE)
			.build(pipeline);
		wireframePipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);

		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setBlend(VK_TRUE)
			.build(pipeline);
		transparentPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);

		mPSOs["opaque"] = *opaquePipeline;
		mPSOs["opaque_wireframe"] = *wireframePipeline;
		mPSOs["transparent"] = *transparentPipeline;

		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
	}
	{
		std::vector<Vulkan::ShaderModule> shaders;
		VkVertexInputBindingDescription vertexInputDescription = {};
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
		ShaderProgramLoader::begin(mDevice)
			.AddShaderPath("Shaders/waves.vert.spv")
			.AddShaderPath("Shaders/waves.frag.spv")
			.load(shaders, vertexInputDescription, vertexAttributeDescriptions);
		VkPipelineLayout wavesLayout{ VK_NULL_HANDLE };
		PipelineLayoutBuilder::begin(mDevice)
			.AddDescriptorSetLayout(descriptorLayout0)
			.AddDescriptorSetLayout(descriptorLayout1)
			.AddDescriptorSetLayout(descriptorLayout2)
			.AddDescriptorSetLayout(descriptorLayout3)
			.AddDescriptorSetLayout(descriptorLayout4)
			.build(wavesLayout);
		wavesGraphicLayout = std::make_unique<VulkanPipelineLayout>(mDevice, wavesLayout);
		VkPipeline pipeline{ VK_NULL_HANDLE };
		PipelineBuilder::begin(mDevice, wavesLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setBlend(VK_TRUE)
			.build(pipeline);
		wavesGraphicPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["wavesRender"] = *wavesGraphicPipeline;
		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
	}
	{

		std::vector<Vulkan::ShaderModule> shaders;
		VkVertexInputBindingDescription vertexInputDescription = {};
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
		ShaderProgramLoader::begin(mDevice)
			.AddShaderPath("Shaders/alphatest.vert.spv")
			.AddShaderPath("Shaders/alphatest.frag.spv")
			.load(shaders, vertexInputDescription, vertexAttributeDescriptions);

		VkPipeline pipeline{ VK_NULL_HANDLE };
		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_NONE)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setBlend(VK_TRUE)
			.build(pipeline);

		alphatestedPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["alphaTested"] = *alphatestedPipeline;

		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
	}
}


void WavesCSApp::BuildWavesPSOs() {	
	//std::unique_ptr<VulkanPipelineLayout> wavesPipelineLayout;
	//std::unique_ptr<VulkanPipeline> wavesUpdatePipeline;
	//std::unique_ptr<VulkanPipeline> wavesDisturbPipeline;

	Vulkan::Buffer uniformBuffer;
	std::vector<UniformBufferInfo> bufferInfo;
	UniformBufferBuilder::begin(mDevice, mDeviceProperties, mMemoryProperties, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, true)
		.AddBuffer(sizeof(GpuUBO), 1, 1)
		.build(uniformBuffer, bufferInfo);
	wavesUniformBuffer = std::make_unique<VulkanUniformBuffer>(mDevice, uniformBuffer, bufferInfo);

	VkDescriptorSet descriptor0 = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptorSets0;

	VkDescriptorSetLayout descriptorLayout0;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(descriptor0, descriptorLayout0);
	std::vector<VkDescriptorSet> descriptorSetList = { descriptor0 };
	wavesUniformDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptorSetList);

	//update UBO descriptor as it won't change
	VkDeviceSize range = sizeof(GpuUBO);	
	VkDescriptorBufferInfo descrInfo{};
	descrInfo.buffer = uniformBuffer.buffer;
	descrInfo.offset = 0;
	descrInfo.range = range;	
	DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), descriptorLayout0, descriptor0)
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &descrInfo)
		.update();

	//Texture descriptors
	VkDescriptorSet descriptor3 = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptorSets3;
	VkDescriptorSetLayout descriptorLayout3 = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
		.build(descriptorSets3, descriptorLayout3, 3);
	wavesTextureDescriptorLayout = descriptorLayout3;

	
	wavesTextureDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptorSets3);

	//we'll update these on the fly


	VkPipelineLayout layout{ VK_NULL_HANDLE };
	PipelineLayoutBuilder::begin(mDevice)
		.AddDescriptorSetLayout(descriptorLayout0)
		.AddDescriptorSetLayout(descriptorLayout3)
		.build(layout);
	wavesPipelineLayout = std::make_unique<VulkanPipelineLayout>(mDevice, layout);
	{
		std::vector<Vulkan::ShaderModule> shaders;
		ShaderProgramLoader::begin(mDevice)
			.AddShaderPath("Shaders/updatewaves.comp.spv")			
			.load(shaders);

		VkPipeline pipeline{ VK_NULL_HANDLE };
		Vulkan::ShaderModule shader = shaders[0];
		pipeline = Vulkan::initComputePipeline(mDevice, *wavesPipelineLayout, shader);
		wavesUpdatePipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
	}
	{
		std::vector<Vulkan::ShaderModule> shaders;
		ShaderProgramLoader::begin(mDevice)
			.AddShaderPath("Shaders/disturbwaves.comp.spv")
			.load(shaders);

		VkPipeline pipeline{ VK_NULL_HANDLE };
		Vulkan::ShaderModule shader = shaders[0];
		pipeline = Vulkan::initComputePipeline(mDevice, *wavesPipelineLayout, shader);
		wavesDisturbPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
	}

}

void WavesCSApp::BuildFrameResources() {
	for (int i = 0; i < gNumFrameResources; i++) {
		auto& ub = *uniformBuffer;

		PassConstants* pc = (PassConstants*)((uint8_t*)ub[0].ptr + ub[0].objectSize * ub[0].objectCount * i);// ((uint8_t*)pPassCB + passSize * i);
		ObjectConstants* oc = (ObjectConstants*)((uint8_t*)ub[1].ptr + ub[1].objectSize * ub[1].objectCount * i);// ((uint8_t*)pObjectCB + objectSize * mAllRitems.size() * i);
		MaterialConstants* mc = (MaterialConstants*)((uint8_t*)ub[2].ptr + ub[2].objectSize * ub[2].objectCount * i);// ((uint8_t*)pMatCB + matSize * mMaterials.size() * i);
		//Vertex* pWv = (Vertex*)WaveVertexPtrs[i];
		mFrameResources.push_back(std::make_unique<FrameResource>(pc, oc, mc));// , pWv));
	}
}

void WavesCSApp::Draw(const GameTimer& gt) {
	uint32_t index = 0;
	VkCommandBuffer cmd{ VK_NULL_HANDLE };

	UpdateWavesGPU(gt);

	cmd = BeginRender();

	VkViewport viewport = { 0.0f,0.0f,(float)mClientWidth,(float)mClientHeight,0.0f,1.0f };
	pvkCmdSetViewport(cmd, 0, 1, &viewport);
	VkRect2D scissor = { {0,0},{(uint32_t)mClientWidth,(uint32_t)mClientHeight} };
	pvkCmdSetScissor(cmd, 0, 1, &scissor);


	if (mIsWireframe) {
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque_wireframe"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Opaque]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::AlphaTested]);
		
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Transparent]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::GpuWaves]);
	}
	else {

		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Opaque]);
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["alphaTested"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::AlphaTested]);
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["transparent"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Transparent]);
		auto& dd = *displacementDescriptors;//update displacement image
		VkDescriptorSet descriptor = dd[mCurrFrame];
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		imageInfo.imageView = mWaves->DisplacementMap().imageView;
		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), displacementDescriptorLayout, descriptor)
			.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo)
			.update();
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *wavesGraphicLayout, 4, 1, &descriptor, 0, 0);
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["wavesRender"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::GpuWaves]);
	}
	EndRender(cmd);
}

void WavesCSApp::DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems) {
	uint32_t dynamicOffsets[1] = { 0 };
	auto& ub = *uniformBuffer;
	VkDeviceSize passSize = ub[0].objectSize;
	VkDeviceSize objectSize = ub[1].objectSize;
	VkDeviceSize matSize = ub[2].objectSize;
	auto& ud = *uniformDescriptors;
	auto& td = *textureDescriptors;
	VkDescriptorSet descriptor0 = ud[0];//pass constant buffer
	pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
	VkDescriptorSet descriptor1 = ud[1];
	VkDescriptorSet descriptor2 = ud[2];
	VkDescriptorSet descriptors[2] = { descriptor1,descriptor2 };


	for (size_t i = 0; i < ritems.size(); i++) {
		auto ri = ritems[i];
		uint32_t indexOffset = ri->StartIndexLocation;

		const auto vbv = ri->Geo->vertexBufferGPU;
		const auto ibv = ri->Geo->indexBufferGPU;
		pvkCmdBindVertexBuffers(cmd, 0, 1, &vbv.buffer, mOffsets);


		pvkCmdBindIndexBuffer(cmd, ibv.buffer, indexOffset * sizeof(uint32_t), VK_INDEX_TYPE_UINT32);
		uint32_t cbvIndex = ri->ObjCBIndex;


		uint32_t dyoffsets[2] = { (uint32_t)(cbvIndex * objectSize),(uint32_t)(ri->Mat->MatCBIndex * matSize) };




		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 1, 2, descriptors, 2, dyoffsets);
		VkDescriptorSet descriptor3 = td[ri->Mat->DiffuseSrvHeapIndex];// pipelineRes->GetDescriptorSet(3, ri->Mat->MatCBIndex);

		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, dynamicOffsets);//bind PC data once
		pvkCmdDrawIndexed(cmd, ri->IndexCount, 1, 0, ri->BaseVertexLocation, 0);
	}
}





void WavesCSApp::OnResize() {
	VulkApp::OnResize();
	mProj = glm::perspectiveFovLH_ZO(0.25f * pi, (float)mClientWidth, (float)mClientHeight, 1.0f, 1000.0f);
}

void WavesCSApp::OnMouseDown(WPARAM btnState, int x, int y) {
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void WavesCSApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void WavesCSApp::OnMouseMove(WPARAM btnState, int x, int y) {
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


void WavesCSApp::OnKeyboardInput(const GameTimer& gt)
{
	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;

	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('P') & 0x8000)
		saveScreenCap(mDevice, mCommandBuffer, mGraphicsQueue, mSwapchainImages[mIndex], mFormatProperties, mSwapchainFormat.format, mSwapchainExtent, mFrameCount);
}

void WavesCSApp::UpdateWavesGPU(const GameTimer& gt) {
	// Every quarter second, generate a random wave.
	static float t_base = 0.0f;
	auto& ud = *wavesUniformDescriptors;
	auto& td = *wavesTextureDescriptors;
	auto& ub = *wavesUniformBuffer;
	VkDescriptorSet descriptorUB = ud[0];
	VkDescriptorSet descriptorImage = td[mCurrFrame];
	VkDescriptorImageInfo imageInfo[3];
	mWaves->UpdateImageDescriptors(imageInfo);
	DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), wavesTextureDescriptorLayout, descriptorImage)
		.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo[0])
		.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo[1])
		.AddBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo[2])
		.update();
	if ((mTimer.TotalTime() - t_base) >= 0.25f)
	{
		t_base += 0.25f;

		int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
		int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);

		float r = MathHelper::RandF(1.0f, 2.0f);

		mWaves->Disturb(mComputeQueue, *computeCommandBuffer,descriptorUB, descriptorImage, *wavesPipelineLayout, *wavesDisturbPipeline,/*mFences[mCurrFrame],*/ i, j, r, (GpuUBO*)ub[0].ptr);
		//mWaves->Disturb(mCommandList.Get(), mWavesRootSignature.Get(), mPSOs["wavesDisturb"].Get(), i, j, r);
	}

	// Update the wave simulation.
	mWaves->Update(gt, mComputeQueue, *computeCommandBuffer, descriptorUB, descriptorImage, *wavesPipelineLayout, *wavesUpdatePipeline,/*mFences[mCurrFrame],*/ (GpuUBO*)ub[0].ptr);
	//mWaves->Update(gt, mCommandList.Get(), mWavesRootSignature.Get(), mPSOs["wavesUpdate"].Get());


	//update displacement descriptor
	VkDescriptorImageInfo dispInfo{};
	dispInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	dispInfo.imageView = mWaves->DisplacementMap().imageView;
	auto& dd = *displacementDescriptors;
	DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), displacementDescriptorLayout, dd[mCurrFrame])
		.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &dispInfo)		
		.update();
}

void WavesCSApp::Update(const GameTimer& gt) {
	VulkApp::Update(gt);
	OnKeyboardInput(gt);
	UpdateCamera(gt);

	//Cycle through the circular frame resource array
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();


	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialsCBs(gt);
	UpdateMainPassCB(gt);
	
}

void WavesCSApp::UpdateCamera(const GameTimer& gt) {
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	mEyePos = glm::vec3(x, y, z);

	glm::vec3 pos = glm::vec3(x, y, z);
	glm::vec3 target = glm::vec3(0.0f);
	glm::vec3 up = glm::vec3(0.0, 1.0f, 0.0f);

	mView = glm::lookAtLH(pos, target, up);
}



void WavesCSApp::UpdateObjectCBs(const GameTimer& gt) {

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
			objConstants.DisplacementMapTexelSize = e->DisplacementMapTexelSize;
			objConstants.GridSpatialStep = e->GridSpatialStep;
			memcpy((pObjConsts + (objSize * e->ObjCBIndex)), &objConstants, sizeof(objConstants));
			//pObjConsts[e->ObjCBIndex] = objConstants;
			e->NumFramesDirty--;
		}
	}
}

void WavesCSApp::UpdateMainPassCB(const GameTimer& gt) {
	glm::mat4 view = mView;
	glm::mat4 proj = mProj;
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
	mMainPassCB.EyePosW = mEyePos;
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


void WavesCSApp::UpdateMaterialsCBs(const GameTimer& gt) {
	uint8_t* pMatConsts = (uint8_t*)mCurrFrameResource->pMats;
	auto& ub = *uniformBuffer;
	VkDeviceSize objSize = ub[2].objectSize;
	for (auto& e : mMaterials) {
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0) {
			glm::mat4 matTransform = mat->MatTransform;
			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			matConstants.MatTransform = matTransform;
			memcpy((pMatConsts + (objSize * mat->MatCBIndex)), &matConstants, sizeof(MaterialConstants));
			mat->NumFramesDirty--;
		}
	}
}

void WavesCSApp::AnimateMaterials(const GameTimer& gt)
{
	// Scroll the water material texture coordinates.
	auto waterMat = mMaterials["water"].get();

	float& tu = waterMat->MatTransform[3][0];
	float& tv = waterMat->MatTransform[3][1];

	tu += 0.1f * gt.DeltaTime();
	tv += 0.02f * gt.DeltaTime();

	if (tu >= 1.0f)
		tu -= 1.0f;

	if (tv >= 1.0f)
		tv -= 1.0f;

	waterMat->MatTransform[3][0] = tu;
	waterMat->MatTransform[3][1] = tv;

	// Material has changed, so need to update cbuffer.
	waterMat->NumFramesDirty = gNumFrameResources;
}


int main() {
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		WavesCSApp theApp(GetModuleHandle(NULL));
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