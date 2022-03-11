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
#include "CubeRenderTarget.h"
const int gNumFrameResources = 3;

const uint32_t CubeMapSize = 512;

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
	OpaqueDynamicReflectors,
	Sky,
	Count
};

class DynamicCubeMapApp : public VulkApp {
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	std::unique_ptr<DescriptorSetLayoutCache> descriptorSetLayoutCache;
	std::unique_ptr<DescriptorSetPoolCache> descriptorSetPoolCache;
	std::unique_ptr<VulkanUniformBuffer> uniformBuffer;
	std::unique_ptr<VulkanImageList> textures;
	std::unique_ptr<VulkanImage> cubeMapTexture;
	std::unique_ptr<VulkanUniformBuffer> storageBuffer;
	std::unique_ptr<VulkanDescriptorList> uniformDescriptors;
	std::unique_ptr<VulkanDescriptorList> textureDescriptors;
	std::unique_ptr<VulkanDescriptorList> storageDescriptors;
	std::unique_ptr<VulkanDescriptor> cubeMapDescriptor;
	std::unique_ptr<VulkanSampler> sampler;
	std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
	std::unique_ptr<VulkanPipelineLayout> cubeMapPipelineLayout;
	std::unique_ptr<VulkanPipeline> opaquePipeline;
	std::unique_ptr<VulkanPipeline> wireframePipeline;
	std::unique_ptr<VulkanPipeline> cubeMapPipeline;
	std::unique_ptr<VulkanPipeline> offscreenOpaquePipeline;
	std::unique_ptr<VulkanPipeline> offscreenCubeMapPipeline;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map < std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture> > mTextures;
	std::unordered_map<std::string, VkPipeline> mPSOs;

	bool mIsWireframe{ false };

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	RenderItem* mSkullRitem = nullptr;

	std::unique_ptr<CubeRenderTarget> mDynamicCubeMap;

	PassConstants mMainPassCB;

	Camera mCamera;
	Camera mCubeMapCamera[6];

	POINT mLastMousePos;

	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);

	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialsBuffer(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateCubeMapFacePassCBs();

	void AnimateMaterials(const GameTimer& gt);
	
	void LoadTextures();
	void BuildBuffers();
	void BuildDescriptors();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void BuildShapeGeometry();
	void BuildSkullGeometry();
	void DrawRenderItems(VkCommandBuffer, VkPipelineLayout layout, const std::vector<RenderItem*>& ritems);
	void BuildCubeFaceCamera(float x, float y, float z);
public:
	DynamicCubeMapApp(HINSTANCE hInstance);
	DynamicCubeMapApp(const DynamicCubeMapApp& rhs) = delete;
	DynamicCubeMapApp& operator=(const DynamicCubeMapApp& rhs) = delete;
	~DynamicCubeMapApp();
	virtual bool Initialize()override;
};


DynamicCubeMapApp::DynamicCubeMapApp(HINSTANCE hInstance) :VulkApp(hInstance) {
	mAllowWireframe = true;
	mClearValues[0].color = Colors::LightSteelBlue;
	mMSAA = false;
	mDepthBuffer = true;
}

DynamicCubeMapApp::~DynamicCubeMapApp() {
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

bool DynamicCubeMapApp::Initialize()
{
	if (!VulkApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	

	mCamera.SetPosition(0.0f, 2.0f, -15.0f);

	BuildCubeFaceCamera(0.0f, 2.0f, 0.0f);

	mDynamicCubeMap = std::make_unique<CubeRenderTarget>(mDevice, mMemoryProperties, mGraphicsQueue, mCommandBuffer, CubeMapSize, CubeMapSize, mSwapchainFormat.format);// PREFERRED_IMAGE_FORMAT);

	LoadTextures();
	//BuildRootSignature();
	//BuildDescriptorHeaps();
	//BuildCubeDepthStencil();
	//BuildShadersAndInputLayout();
	BuildSkullGeometry();
	BuildShapeGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildBuffers();
	BuildDescriptors();
	BuildPSOs();
	BuildFrameResources();
	

	

	return true;
}

void DynamicCubeMapApp::LoadTextures() {
	std::vector<Vulkan::Image> texturesList;
	ImageLoader::begin(mDevice, mCommandBuffer, mGraphicsQueue, mMemoryProperties)
		.addImage("../../../Textures/bricks2.jpg")
		.addImage("../../../Textures/tile.jpg")
		.addImage("../../../Textures/white1x1.jpg")
		.load(texturesList);
	textures = std::make_unique<VulkanImageList>(mDevice, texturesList);
	ImageLoader::begin(mDevice, mCommandBuffer, mGraphicsQueue, mMemoryProperties)
		.addImage("../../../Textures/grasscube1024-posx.jpg")
		.addImage("../../../Textures/grasscube1024-negx.jpg")
		.addImage("../../../Textures/grasscube1024-posy.jpg")
		.addImage("../../../Textures/grasscube1024-negy.jpg")
		.addImage("../../../Textures/grasscube1024-posz.jpg")
		.addImage("../../../Textures/grasscube1024-negz.jpg")
		.setIsCube(true)
		.load(texturesList);
	cubeMapTexture = std::make_unique <VulkanImage>(mDevice, texturesList[0]);
	Vulkan::SamplerProperties sampProps;
	sampProps.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler = std::make_unique<VulkanSampler>(mDevice, Vulkan::initSampler(mDevice, sampProps));
}

void DynamicCubeMapApp::BuildBuffers() {
	Vulkan::Buffer dynamicBuffer;
	//pass and object constants are dynamic uniform buffers
	std::vector<UniformBufferInfo> bufferInfo;
	UniformBufferBuilder::begin(mDevice, mDeviceProperties, mMemoryProperties, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, true)
		.AddBuffer(sizeof(PassConstants), 7, gNumFrameResources)//1 for main pass, 6 for cube camera passes
		.AddBuffer(sizeof(ObjectConstants), mAllRitems.size(), gNumFrameResources)
		.build(dynamicBuffer, bufferInfo);
	uniformBuffer = std::make_unique<VulkanUniformBuffer>(mDevice, dynamicBuffer, bufferInfo);

	UniformBufferBuilder::begin(mDevice, mDeviceProperties, mMemoryProperties, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, true)
		.AddBuffer(sizeof(MaterialData), mMaterials.size(), gNumFrameResources)
		.build(dynamicBuffer, bufferInfo);
	storageBuffer = std::make_unique<VulkanUniformBuffer>(mDevice, dynamicBuffer, bufferInfo);
}

void DynamicCubeMapApp::BuildShapeGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
	}

	std::vector<std::uint32_t> indices;
	indices.insert(indices.end(), std::begin(box.Indices32), std::end(box.Indices32));
	indices.insert(indices.end(), std::begin(grid.Indices32), std::end(grid.Indices32));
	indices.insert(indices.end(), std::begin(sphere.Indices32), std::end(sphere.Indices32));
	indices.insert(indices.end(), std::begin(cylinder.Indices32), std::end(cylinder.Indices32));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

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

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}


void DynamicCubeMapApp::BuildSkullGeometry() {
	std::ifstream fin("Models/skull.txt");

	if (!fin)
	{
		MessageBox(0, L"Models/skull.txt not found.", 0, 0);
		return;
	}

	uint32_t vcount = 0;
	uint32_t tcount = 0;
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

		glm::vec3 p = vertices[i].Pos;
		glm::vec3 spherePos = glm::normalize(p);

		float theta = atan2f(spherePos.z, spherePos.x);

		//Put in [0, 2pi].
		if (theta < 0.0f)
			theta += MathHelper::Pi;

		float phi = acosf(spherePos.y);

		float u = theta / (2.0f * MathHelper::Pi);
		float v = phi / MathHelper::Pi;

		vertices[i].TexC = { u,v };

		vMin = MathHelper::vectorMin(vMin, p);
		vMax = MathHelper::vectorMax(vMax, p);
	}

	/*BoundingBox bounds;
	bounds.Center = 0.5f * (vMin + vMax);
	bounds.Extents = 0.5f * (vMax - vMin);*/
	AABB bounds(vMin, vMax);

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::uint32_t> indices(3 * tcount);
	for (uint32_t i = 0; i < tcount; ++i)
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
	geo->Name = "skullGeo";

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
	submesh.IndexCount = (uint32_t)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	submesh.Bounds = bounds;

	geo->DrawArgs["skull"] = submesh;
	mGeometries[geo->Name] = std::move(geo);

}


void DynamicCubeMapApp::BuildMaterials()
{
	auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	bricks0->NumFramesDirty = gNumFrameResources;
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = glm::vec3(0.1f, 0.1f, 0.1f);
	bricks0->Roughness = 0.3f;

	auto tile0 = std::make_unique<Material>();
	tile0->NumFramesDirty = gNumFrameResources;
	tile0->Name = "tile0";
	tile0->MatCBIndex = 1;
	tile0->DiffuseSrvHeapIndex = 1;
	tile0->DiffuseAlbedo = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
	tile0->FresnelR0 = glm::vec3(0.2f, 0.2f, 0.2f);
	tile0->Roughness = 0.1f;

	auto mirror0 = std::make_unique<Material>();
	mirror0->NumFramesDirty = gNumFrameResources;
	mirror0->Name = "mirror0";
	mirror0->MatCBIndex = 2;
	mirror0->DiffuseSrvHeapIndex = 2;
	mirror0->DiffuseAlbedo = glm::vec4(0.0f, 0.0f, 0.1f, 1.0f);
	mirror0->FresnelR0 = glm::vec3(0.98f, 0.97f, 0.95f);
	mirror0->Roughness = 0.1f;

	auto skullMat = std::make_unique<Material>();
	skullMat->NumFramesDirty = gNumFrameResources;
	skullMat->Name = "skullMat";

	skullMat->MatCBIndex = 3;
	skullMat->DiffuseSrvHeapIndex = 2;
	skullMat->DiffuseAlbedo = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
	skullMat->FresnelR0 = glm::vec3(0.2f, 0.2f, 0.2f);
	skullMat->Roughness = 0.2f;

	auto sky = std::make_unique<Material>();
	sky->NumFramesDirty = gNumFrameResources;
	sky->Name = "sky";
	sky->MatCBIndex = 4;
	sky->DiffuseSrvHeapIndex = 3;
	sky->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	sky->FresnelR0 = glm::vec3(0.1f, 0.1f, 0.1f);
	sky->Roughness = 1.0f;

	mMaterials["bricks0"] = std::move(bricks0);
	mMaterials["tile0"] = std::move(tile0);
	mMaterials["mirror0"] = std::move(mirror0);
	mMaterials["skullMat"] = std::move(skullMat);
	mMaterials["sky"] = std::move(sky);
}

void DynamicCubeMapApp::BuildRenderItems()
{
	auto skyRitem = std::make_unique<RenderItem>();
	skyRitem->World = glm::scale(glm::mat4(1.0f), glm::vec3(5000.0f, 5000.0f, 5000.0f));
	skyRitem->TexTransform = MathHelper::Identity4x4();
	skyRitem->ObjCBIndex = 0;
	skyRitem->Mat = mMaterials["sky"].get();
	skyRitem->Geo = mGeometries["shapeGeo"].get();
	skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sphere"].IndexCount;
	skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Sky].push_back(skyRitem.get());
	mAllRitems.push_back(std::move(skyRitem));

	

	auto skullRitem = std::make_unique<RenderItem>();
	skullRitem->World = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(0.4f, 0.4f, 0.4f)), glm::vec3(0.0f, 2.0f, 0.0f));
	skullRitem->TexTransform = MathHelper::Identity4x4();
	skullRitem->ObjCBIndex = 1;
	skullRitem->Mat = mMaterials["skullMat"].get();
	skullRitem->Geo = mGeometries["skullGeo"].get();
	skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
	skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
	skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;

	mSkullRitem = skullRitem.get();

	mRitemLayer[(int)RenderLayer::Opaque].push_back(skullRitem.get());
	mAllRitems.push_back(std::move(skullRitem));

	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->World = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 1.0f, 2.0f)), glm::vec3(0.0f, 0.5f, 0.0f));
	boxRitem->TexTransform = glm::mat4(1.0f);
	boxRitem->ObjCBIndex = 2;
	boxRitem->Mat = mMaterials["bricks0"].get();
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem.get());
	mAllRitems.push_back(std::move(boxRitem));

	auto globeRitem = std::make_unique<RenderItem>();
	globeRitem->World =  glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 2.0f, 2.0f)), glm::vec3(0.0f, 1.0f, 0.0f));
	globeRitem->TexTransform = glm::mat4(1.0f);
	globeRitem->ObjCBIndex = 3;
	globeRitem->Mat = mMaterials["mirror0"].get();
	globeRitem->Geo = mGeometries["shapeGeo"].get();
	
	globeRitem->IndexCount = globeRitem->Geo->DrawArgs["sphere"].IndexCount;
	globeRitem->StartIndexLocation = globeRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	globeRitem->BaseVertexLocation = globeRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::OpaqueDynamicReflectors].push_back(globeRitem.get());
	mAllRitems.push_back(std::move(globeRitem));

	

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = MathHelper::Identity4x4();
	gridRitem->TexTransform = glm::scale(glm::mat4(1.0f), glm::vec3(8.0f, 8.0f, 1.0f));
	gridRitem->ObjCBIndex = 4;
	gridRitem->Mat = mMaterials["tile0"].get();
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());
	mAllRitems.push_back(std::move(gridRitem));

	glm::mat4 brickTexTransform = glm::scale(glm::mat4(1.0f), glm::vec3(1.5f, 2.0f, 1.0f));
	UINT objCBIndex = 5;
	for (int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		glm::mat4 leftCylWorld = glm::translate(glm::mat4(1.0f), glm::vec3(-5.0f, 1.5f, -10.0f + i * 5.0f));
		glm::mat4 rightCylWorld = glm::translate(glm::mat4(1.0f), glm::vec3(+5.0f, 1.5f, -10.0f + i * 5.0f));

		glm::mat4 leftSphereWorld = glm::translate(glm::mat4(1.0f), glm::vec3(-5.0f, 3.5f, -10.0f + i * 5.0f));
		glm::mat4 rightSphereWorld = glm::translate(glm::mat4(1.0f), glm::vec3(+5.0f, 3.5f, -10.0f + i * 5.0f));

		leftCylRitem->World = rightCylWorld;
		leftCylRitem->TexTransform = brickTexTransform;
		leftCylRitem->ObjCBIndex = objCBIndex++;
		leftCylRitem->Mat = mMaterials["bricks0"].get();
		leftCylRitem->Geo = mGeometries["shapeGeo"].get();
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		rightCylRitem->World = leftCylWorld;
		rightCylRitem->TexTransform = brickTexTransform;
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Mat = mMaterials["bricks0"].get();
		rightCylRitem->Geo = mGeometries["shapeGeo"].get();
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		leftSphereRitem->World = leftSphereWorld;
		leftSphereRitem->TexTransform = MathHelper::Identity4x4();// glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));// MathHelper::Identity4x4();
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Mat = mMaterials["mirror0"].get();
		leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		rightSphereRitem->World = rightSphereWorld;
		rightSphereRitem->TexTransform = MathHelper::Identity4x4();
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Mat = mMaterials["mirror0"].get();
		rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		mRitemLayer[(int)RenderLayer::Opaque].push_back(leftCylRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(rightCylRitem.get());
#define __MORE__REF__
#ifdef __MORE__REF__
		mRitemLayer[(int)RenderLayer::OpaqueDynamicReflectors].push_back(leftSphereRitem.get());
		mRitemLayer[(int)RenderLayer::OpaqueDynamicReflectors].push_back(rightSphereRitem.get());
#else
		mRitemLayer[(int)RenderLayer::Opaque].push_back(leftSphereRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(rightSphereRitem.get());
#endif

		mAllRitems.push_back(std::move(leftCylRitem));
		mAllRitems.push_back(std::move(rightCylRitem));
		mAllRitems.push_back(std::move(leftSphereRitem));
		mAllRitems.push_back(std::move(rightSphereRitem));
	}
}

void DynamicCubeMapApp::BuildDescriptors() {
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


	//Build stuff for CubeMap
	VkDescriptorSet descriptor4 = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorLayout4 = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, (uint32_t)1)
		.build(descriptor4, descriptorLayout4);
	descriptors = { descriptor3,descriptor4 };
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
	{

		VkDescriptorImageInfo imageInfo{};

		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = *cubeMapTexture;


		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), descriptorLayout4, descriptor4)

			.AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo, (uint32_t)1)
			.update();
	}
	VkPipelineLayout layout{ VK_NULL_HANDLE };
	PipelineLayoutBuilder::begin(mDevice)
		.AddDescriptorSetLayout(descriptorLayout0)
		.AddDescriptorSetLayout(descriptorLayout1)
		.AddDescriptorSetLayout(descriptorLayout2)
		.AddDescriptorSetLayout(descriptorLayout3)
		.AddDescriptorSetLayout(descriptorLayout4)
		.build(layout);
	pipelineLayout = std::make_unique<VulkanPipelineLayout>(mDevice, layout);

	////Build stuff for CubeMap
	VkDescriptorSet descriptor5 = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorLayout5 = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		//.AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, (uint32_t)1)
		.build(descriptor5, descriptorLayout5);

	//this is for the cubemap created by offscreen pass
	cubeMapDescriptor = std::make_unique<VulkanDescriptor>(mDevice, descriptor5);
	{
		/*auto& samp = *sampler;
		VkDescriptorImageInfo sampInfo;
		sampInfo.sampler = samp;
		///auto& ct = *cubeMapTexture;*/

		VkDescriptorImageInfo imageInfo{};
		
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = mDynamicCubeMap->CubeMapImageView();


		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), descriptorLayout5, descriptor5)
			//.AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, &sampInfo)
			.AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo, (uint32_t)1)
			.update();
	}
	VkPipelineLayout cubeMapLayout{ VK_NULL_HANDLE };
	PipelineLayoutBuilder::begin(mDevice)
		.AddDescriptorSetLayout(descriptorLayout0)
		.AddDescriptorSetLayout(descriptorLayout1)
		.AddDescriptorSetLayout(descriptorLayout2)
		.AddDescriptorSetLayout(descriptorLayout3)
		.AddDescriptorSetLayout(descriptorLayout5)
		.build(cubeMapLayout);
	cubeMapPipelineLayout = std::make_unique<VulkanPipelineLayout>(mDevice, cubeMapLayout);

	

}
void DynamicCubeMapApp::BuildPSOs() {
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
		mPSOs["opaque"] = *opaquePipeline;
		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_LINE)
			.setDepthTest(VK_TRUE)
			.build(pipeline);
		wireframePipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["opaque_wireframe"] = *wireframePipeline;
		PipelineBuilder::begin(mDevice, *pipelineLayout, mDynamicCubeMap->getRenderPass(), shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.build(pipeline);
		offscreenOpaquePipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["offscreen_opaque"] = *offscreenOpaquePipeline;

		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
	}
	{
		std::vector<Vulkan::ShaderModule> shaders;
		VkVertexInputBindingDescription vertexInputDescription = {};
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
		ShaderProgramLoader::begin(mDevice)
			.AddShaderPath("Shaders/sky.vert.spv")
			.AddShaderPath("Shaders/sky.frag.spv")
			.load(shaders, vertexInputDescription, vertexAttributeDescriptions);
		VkPipeline pipeline = VK_NULL_HANDLE;
		PipelineBuilder::begin(mDevice, *cubeMapPipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_NONE)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL)
			.build(pipeline);
		cubeMapPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);

		mPSOs["sky"] = *cubeMapPipeline;
		PipelineBuilder::begin(mDevice, *cubeMapPipelineLayout, mDynamicCubeMap->getRenderPass(), shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_NONE)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL)
			.build(pipeline);
		offscreenCubeMapPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["offscreen_sky"] = *offscreenCubeMapPipeline;
		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
	}
}

void DynamicCubeMapApp::BuildFrameResources() {
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

void DynamicCubeMapApp::BuildCubeFaceCamera(float x, float y, float z)
{
	// Generate the cube map about the given position.
	glm::vec3 center(x, y, z);
	glm::vec3 worldUp(0.0f, 1.0f, 0.0f);

	// Look along each coordinate axis.
	glm::vec3 targets[6] =
	{
		glm::vec3(x + 1.0f, y, z), // +X
		glm::vec3(x - 1.0f, y, z), // -X
		glm::vec3(x, y + 1.0f, z), // +Y
		glm::vec3(x, y - 1.0f, z), // -Y
		glm::vec3(x, y, z + 1.0f), // +Z
		glm::vec3(x, y, z - 1.0f)  // -Z
	};

	// Use world up vector (0,1,0) for all directions except +Y/-Y.  In these cases, we
	// are looking down +Y or -Y, so we need a different "up" vector.
	glm::vec3 ups[6] =
	{
		glm::vec3(0.0f, 1.0f, 0.0f),  // +X
		glm::vec3(0.0f, 1.0f, 0.0f),  // -X
		glm::vec3(0.0f, 0.0f, -1.0f), // +Y
		glm::vec3(0.0f, 0.0f, +1.0f), // -Y
		glm::vec3(0.0f, 1.0f, 0.0f),	 // +Z
		glm::vec3(0.0f, 1.0f, 0.0f)	 // -Z
	};

	for (int i = 0; i < 6; ++i)
	{
		mCubeMapCamera[i].LookAt(center, targets[i], ups[i]);
		mCubeMapCamera[i].SetLens(0.5f * MathHelper::Pi, 1.0f, 0.1f, 1000.0f);
		mCubeMapCamera[i].UpdateViewMatrix();
	}
}


void DynamicCubeMapApp::Draw(const GameTimer& gt) {
	uint32_t index = 0;

	auto& ub = *uniformBuffer;
	VkDeviceSize passSize = ub[0].objectSize;
	VkDeviceSize passCount = ub[0].objectCount;
	auto& ud = *uniformDescriptors;
	//bind descriptors that don't change during pass
	VkDescriptorSet descriptor0 = ud[0];//pass constant buffer
	uint32_t dynamicOffsets[1] = { mCurrFrame * (uint32_t)passSize * (uint32_t)passCount };
	
	//bind storage buffer
	auto& sb = *storageBuffer;
	VkDeviceSize matSize = sb[0].objectSize * sb[0].objectCount;
	auto& sd = *storageDescriptors;
	VkDescriptorSet descriptor2 = sd[0];
	auto& td = *textureDescriptors;
	VkDescriptorSet descriptor3 = td[0];
	VkDescriptorSet descriptor4 = td[1];
	VkCommandBuffer cmd = BeginRender(false);//don't want to start main render pass
	//Offscreen renderpass
	{
		uint32_t w = mDynamicCubeMap->getWidth();
		uint32_t h = mDynamicCubeMap->getHeight();
		VkRenderPassBeginInfo renderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
		renderPassBeginInfo.clearValueCount = sizeof(mClearValues) / sizeof(mClearValues[0]);
		renderPassBeginInfo.pClearValues = mClearValues;
		renderPassBeginInfo.renderPass = mDynamicCubeMap->getRenderPass();
		renderPassBeginInfo.framebuffer = mDynamicCubeMap->getFrameBuffer();
		renderPassBeginInfo.renderArea = { 0,0,(uint32_t)w,(uint32_t)h };
		
		
		VkViewport viewport = { 0.0f,0.0f,(float)w,(float)h,0.0f,1.0f };
		pvkCmdSetViewport(cmd, 0, 1, &viewport);
		VkRect2D scissor = { {0,0},{(uint32_t)w,(uint32_t)h} };
		pvkCmdSetScissor(cmd, 0, 1, &scissor);
		for (uint32_t face = 0; face < 6; face++) {
			pvkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			VkDeviceSize offset = passSize * passCount * mCurrFrame + passSize * (face + 1);
			dynamicOffsets[0] = (uint32_t)offset;

			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["offscreen_opaque"]);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *cubeMapPipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *cubeMapPipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *cubeMapPipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *cubeMapPipelineLayout, 4, 1, &descriptor4, 0, 0);//bind PC data once
			DrawRenderItems(cmd, *pipelineLayout, mRitemLayer[(int)RenderLayer::Opaque]);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *cubeMapPipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *cubeMapPipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *cubeMapPipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *cubeMapPipelineLayout, 4, 1, &descriptor4, 0, 0);//bind PC data once
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["offscreen_sky"]);
			DrawRenderItems(cmd, *cubeMapPipelineLayout, mRitemLayer[(int)RenderLayer::Sky]);
			pvkCmdEndRenderPass(cmd);
			//VkResult res = pvkEndCommandBuffer(cmd);

			//assert(res == VK_SUCCESS);
			//transition image, 
			Vulkan::transitionImageNoSubmit(cmd, mDynamicCubeMap->getRenderTarget(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			//copy it to cubemap
			VkImageSubresourceRange cubeFaceSubresourceRange = {};
			cubeFaceSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			cubeFaceSubresourceRange.baseMipLevel = 0;
			cubeFaceSubresourceRange.levelCount = 1;
			cubeFaceSubresourceRange.baseArrayLayer = face;
			cubeFaceSubresourceRange.layerCount = 1;
			Vulkan::transitionImageNoSubmit(cmd,  mDynamicCubeMap->CubeMapImage(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,6);
			// Copy region for transfer from framebuffer to cube face
			VkImageCopy copyRegion = {};

			copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.srcSubresource.baseArrayLayer = 0;
			copyRegion.srcSubresource.mipLevel = 0;
			copyRegion.srcSubresource.layerCount = 1;
			copyRegion.srcOffset = { 0, 0, 0 };

			copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.dstSubresource.baseArrayLayer = face;
			copyRegion.dstSubresource.mipLevel = 0;
			copyRegion.dstSubresource.layerCount = 1;
			copyRegion.dstOffset = { 0, 0, 0 };

			copyRegion.extent.width = CubeMapSize;
			copyRegion.extent.height = CubeMapSize;
			copyRegion.extent.depth = 1;

			// Put image copy into command buffer
			vkCmdCopyImage(
				cmd,
				mDynamicCubeMap->getRenderTarget(),
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				mDynamicCubeMap->CubeMapImage(),
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&copyRegion);

			Vulkan::transitionImageNoSubmit(cmd, mDynamicCubeMap->getRenderTarget(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			Vulkan::transitionImageNoSubmit(cmd, mDynamicCubeMap->CubeMapImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,6);

			/*VkResult res = pvkEndCommandBuffer(cmd);

			assert(res == VK_SUCCESS);*/
		}
	}

	//start main render pass now
	pvkCmdBeginRenderPass(cmd, &mRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	
	dynamicOffsets[0] = (uint32_t)(mCurrFrame * passSize * passCount);
	

	VkViewport viewport = { 0.0f,0.0f,(float)mClientWidth,(float)mClientHeight,0.0f,1.0f };
	pvkCmdSetViewport(cmd, 0, 1, &viewport);
	VkRect2D scissor = { {0,0},{(uint32_t)mClientWidth,(uint32_t)mClientHeight} };
	pvkCmdSetScissor(cmd, 0, 1, &scissor);


	//auto& ub = *uniformBuffer;
	//VkDeviceSize passSize = ub[0].objectSize;
	//auto& ud = *uniformDescriptors;
	////bind descriptors that don't change during pass
	//VkDescriptorSet descriptor0 = ud[0];//pass constant buffer
	//uint32_t dynamicOffsets[1] = { mCurrFrame * (uint32_t)passSize };
	//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
	////bind storage buffer
	//auto& sb = *storageBuffer;
	//VkDeviceSize matSize = sb[0].objectSize * sb[0].objectCount;
	//auto& sd = *storageDescriptors;
	//VkDescriptorSet descriptor2 = sd[0];
	////dynamicOffsets[0] = mCurrFrame * (uint32_t)matSize;
	////pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
	////bind texture array
	//auto& td = *textureDescriptors;
	//VkDescriptorSet descriptor3 = td[0];
	//VkDescriptorSet descriptor4 = td[1];
	////pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
	auto& ct = *cubeMapDescriptor;
	VkDescriptorSet descriptor5 = ct;
	if (mIsWireframe) {
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque_wireframe"]);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 4, 1, &descriptor5, 0, 0);//bind PC data once
		DrawRenderItems(cmd, *pipelineLayout, mRitemLayer[(int)RenderLayer::OpaqueDynamicReflectors]);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 4, 1, &descriptor4, 0, 0);//bind PC data once
		DrawRenderItems(cmd, *pipelineLayout, mRitemLayer[(int)RenderLayer::Opaque]);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 4, 1, &descriptor4, 0, 0);//bind PC data once
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["sky"]);
		DrawRenderItems(cmd, *cubeMapPipelineLayout, mRitemLayer[(int)RenderLayer::Sky]);
	}
	else
	{
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque"]);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 4, 1, &descriptor5, 0, 0);//bind PC data once
		DrawRenderItems(cmd, *pipelineLayout, mRitemLayer[(int)RenderLayer::OpaqueDynamicReflectors]);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 4, 1, &descriptor4, 0, 0);//bind PC data once
		

		DrawRenderItems(cmd, *pipelineLayout, mRitemLayer[(int)RenderLayer::Opaque]);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 4, 1, &descriptor4, 0, 0);//bind PC data once
		
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["sky"]);
		DrawRenderItems(cmd, *cubeMapPipelineLayout, mRitemLayer[(int)RenderLayer::Sky]);
	}
	EndRender(cmd);
}
void DynamicCubeMapApp::DrawRenderItems(VkCommandBuffer cmd, VkPipelineLayout layout, const std::vector<RenderItem*>& ritems) {
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
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &descriptor1, 1, dyoffsets);
		pvkCmdDrawIndexed(cmd, ri->IndexCount, 1, 0, ri->BaseVertexLocation, 0);
	}
}




void DynamicCubeMapApp::Update(const GameTimer& gt) {
	VulkApp::Update(gt);
	OnKeyboardInput(gt);

	//
	// Animate the skull around the center sphere.
	//

	auto skullScale = glm::scale(glm::mat4(1.0f),glm::vec3(0.2f, 0.2f, 0.2f));
	auto skullOffset = glm::translate(glm::mat4(1.0f),glm::vec3(3.0f, 2.0f, 0.0f));
	auto skullLocalRotate = glm::rotate(glm::mat4(1.0f),(2.0f * gt.TotalTime()),glm::vec3(0.0f,1.0f,0.0f));
	auto skullGlobalRotate = glm::rotate(glm::mat4(1.0f),(0.5f * gt.TotalTime()),glm::vec3(0.0f,1.0f,0.0f));
	mSkullRitem->World = skullGlobalRotate*skullOffset * skullLocalRotate * skullScale;
	mSkullRitem->NumFramesDirty = gNumFrameResources;

	//Cycle through the circular frame resource array
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialsBuffer(gt);
	UpdateMainPassCB(gt);

}
void DynamicCubeMapApp::UpdateMainPassCB(const GameTimer& gt) {
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

	UpdateCubeMapFacePassCBs();
}
void DynamicCubeMapApp::UpdateCubeMapFacePassCBs()
{
	//auto currPassCB = mCurrFrameResource->PassCB.get();
	auto currPassCB = mCurrFrameResource->pPCs;
	// Cube map pass cbuffers are stored in elements 1-6.
	//currPassCB->CopyData(1 + i, cubeFacePassCB);
	auto& ub = *uniformBuffer;
	auto size = ub[0].objectSize;
	for (int i = 0; i < 6; ++i)
	{
		PassConstants cubeFacePassCB = mMainPassCB;

		auto view = mCubeMapCamera[i].GetView();
		auto proj = mCubeMapCamera[i].GetProj();
		proj[1][1] *= -1;
		auto viewProj = proj * view;// XMMatrixMultiply(view, proj);
		//auto viewDet = XMMatrixDeterminant(view);
		//XMMATRIX invView = XMMatrixInverse(&viewDet, view);
		auto invView = glm::inverse(view);
		//auto projDet = XMMatrixDeterminant(proj);
		//XMMATRIX invProj = XMMatrixInverse(&projDet, proj);
		auto invProj = glm::inverse(proj);
		//auto viewProjDet = XMMatrixDeterminant(viewProj);
		//XMMATRIX invViewProj = XMMatrixInverse(&viewProjDet, viewProj);
		auto invViewProj = glm::inverse(viewProj);

		//XMStoreFloat4x4(&cubeFacePassCB.View, XMMatrixTranspose(view));
		cubeFacePassCB.View = view;
		//XMStoreFloat4x4(&cubeFacePassCB.InvView, XMMatrixTranspose(invView));
		cubeFacePassCB.InvView = invView;
		//XMStoreFloat4x4(&cubeFacePassCB.Proj, XMMatrixTranspose(proj));
		cubeFacePassCB.Proj = proj;
		//XMStoreFloat4x4(&cubeFacePassCB.InvProj, XMMatrixTranspose(invProj));
		cubeFacePassCB.InvProj = invProj;
		//XMStoreFloat4x4(&cubeFacePassCB.ViewProj, XMMatrixTranspose(viewProj));
		cubeFacePassCB.ViewProj = viewProj;
		//XMStoreFloat4x4(&cubeFacePassCB.InvViewProj, XMMatrixTranspose(invViewProj));
		cubeFacePassCB.InvViewProj = invViewProj;
		//cubeFacePassCB.EyePosW = mCubeMapCamera[i].GetPosition3f();
		cubeFacePassCB.EyePosW = mCubeMapCamera[i].GetPosition();
		//cubeFacePassCB.RenderTargetSize = XMFLOAT2((float)CubeMapSize, (float)CubeMapSize);
		cubeFacePassCB.RenderTargetSize = glm::vec2((float)CubeMapSize, (float)CubeMapSize);
		//cubeFacePassCB.InvRenderTargetSize = XMFLOAT2(1.0f / CubeMapSize, 1.0f / CubeMapSize);
		cubeFacePassCB.InvRenderTargetSize = glm::vec2(1.0f / CubeMapSize, 1.0f / CubeMapSize);

		
		memcpy((uint8_t*)currPassCB + (i + 1) * size, &cubeFacePassCB, size);
	}
}


void  DynamicCubeMapApp::UpdateObjectCBs(const GameTimer& gt) {

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

void DynamicCubeMapApp::UpdateMaterialsBuffer(const GameTimer& gt) {
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

void DynamicCubeMapApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();
	if (GetAsyncKeyState('P') & 0x8000)
		saveScreenCap(mDevice, mCommandBuffer, mGraphicsQueue, mSwapchainImages[mIndex], mFormatProperties, mSwapchainFormat.format, mSwapchainExtent, mFrameCount);

	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;



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



void DynamicCubeMapApp::AnimateMaterials(const GameTimer& gt)
{

}

void DynamicCubeMapApp::OnResize()
{
	VulkApp::OnResize();

	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}


void DynamicCubeMapApp::OnMouseDown(WPARAM btnState, int x, int y) {
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void DynamicCubeMapApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void DynamicCubeMapApp::OnMouseMove(WPARAM btnState, int x, int y) {
	if ((btnState & MK_LBUTTON) != 0) {
		// Make each pixel correspond to a quarter of a degree.
		float dx = glm::radians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = glm::radians(0.25f * static_cast<float>(y - mLastMousePos.y));

		mCamera.Pitch(dy);
		mCamera.RotateY(dx);
	}


	mLastMousePos.x = x;
	mLastMousePos.y = y;
}



int main() {
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		DynamicCubeMapApp theApp(GetModuleHandle(NULL));
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
