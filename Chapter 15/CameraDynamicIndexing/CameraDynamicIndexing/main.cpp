#include "../../../Common/VulkApp.h"
#include "../../../Common/VulkUtil.h"
#include "../../../Common/VulkanEx.h"
#include "../../../Common/GeometryGenerator.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/Colors.h"
#include "../../../Common/TextureLoader.h"
#include "../../../Common/Camera.h"
#include <memory>
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

	uint32_t IndexCount{ 0 };
	uint32_t StartIndexLocation{ 0 };
	uint32_t BaseVertexLocation{ 0 };
};

class CameraAndDynamicIndexingApp : public VulkApp {
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
	std::unique_ptr<VulkanSampler> sampler;
	std::unique_ptr<VulkanDescriptorList> storageDescriptors;
	std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
	std::unique_ptr<VulkanPipeline> opaquePipeline;
	std::unique_ptr<VulkanPipeline> wireframePipeline;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map < std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture> > mTextures;
	std::unordered_map<std::string, VkPipeline> mPSOs;

	bool mIsWireframe{ false };

	RenderItem* mWavesRitem{ nullptr };

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	std::vector<RenderItem*> mOpaqueRitems;

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
	void BuildShapeGeometry();
	void DrawRenderItems(VkCommandBuffer, const std::vector<RenderItem*>& ritems);
public:
	CameraAndDynamicIndexingApp(HINSTANCE hInstance);
	CameraAndDynamicIndexingApp& operator=(const CameraAndDynamicIndexingApp& rhs) = delete;
	~CameraAndDynamicIndexingApp();
	virtual bool Initialize()override;
};

CameraAndDynamicIndexingApp::CameraAndDynamicIndexingApp(HINSTANCE hInstance) :VulkApp(hInstance) {
	mAllowWireframe = true;
	mClearValues[0].color = Colors::LightSteelBlue;
	mMSAA = false;
	mDepthBuffer = true;
}

CameraAndDynamicIndexingApp::~CameraAndDynamicIndexingApp() {
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


bool CameraAndDynamicIndexingApp::Initialize() {
	if (!VulkApp::Initialize())
		return false;

	mCamera.SetPosition(0.0f, 2.0f, -15.0f);


	LoadTextures();
	BuildShapeGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildBuffers();
	BuildDescriptors();
	BuildPSOs();
	BuildFrameResources();

	return true;

}

void CameraAndDynamicIndexingApp::LoadTextures() {
	std::vector<Vulkan::Image> texturesList;
	ImageLoader::begin(mDevice, mCommandBuffer, mGraphicsQueue, mMemoryProperties)
		.addImage("../../../Textures/bricks.jpg")
		.addImage("../../../Textures/stone.jpg")
		.addImage("../../../Textures/tile.jpg")
		.addImage("../../../Textures/WoodCrate01.jpg")
		.load(texturesList);

	textures = std::make_unique<VulkanImageList>(mDevice, texturesList);
	auto bricksTex = std::make_unique<Texture>();
	bricksTex->image = texturesList[0].image;
	bricksTex->imageView = texturesList[0].imageView;
	//bricksTex->sampler = texturesList[0].sampler;
	bricksTex->Name = "bricksTex";

	auto stoneTex = std::make_unique<Texture>();
	stoneTex->Name = "stoneTex";
	stoneTex->image = texturesList[1].image;
	stoneTex->imageView = texturesList[1].imageView;
	//stoneTex->sampler = texturesList[1].sampler;

	auto tileTex = std::make_unique<Texture>();
	tileTex->Name = "tileTex";
	tileTex->image = texturesList[2].image;
	tileTex->imageView = texturesList[2].imageView;
	//tileTex->sampler = texturesList[2].sampler;

	auto crateTex = std::make_unique<Texture>();
	crateTex->Name = "crateTex";
	crateTex->image = texturesList[3].image;
	crateTex->imageView = texturesList[3].imageView;
	//crateTex->sampler = texturesList[2].sampler;

	mTextures[bricksTex->Name] = std::move(bricksTex);
	mTextures[stoneTex->Name] = std::move(stoneTex);
	mTextures[tileTex->Name] = std::move(tileTex);
	mTextures[crateTex->Name] = std::move(crateTex);

	Vulkan::SamplerProperties sampProps;
	sampProps.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler = std::make_unique<VulkanSampler>(mDevice, Vulkan::initSampler(mDevice, sampProps));

	

}

void CameraAndDynamicIndexingApp::BuildBuffers() {
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

void CameraAndDynamicIndexingApp::BuildDescriptors() {
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
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT)
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
		.AddBinding(0,VK_DESCRIPTOR_TYPE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT,(uint32_t)*textures)
		.build(descriptor3, descriptorLayout3);

	descriptors = { descriptor3 };
	textureDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptors);

	VkDeviceSize offset = 0;
	descriptors = { descriptor0,descriptor1};
	std::vector<VkDescriptorSetLayout> descriptorLayouts = { descriptorLayout0,descriptorLayout1};
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
			.AddBinding(0,VK_DESCRIPTOR_TYPE_SAMPLER,&sampInfo)
			.AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos.data(),(uint32_t) tl)
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

void CameraAndDynamicIndexingApp::BuildPSOs() {
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

	for (auto& shader : shaders) {
		Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
	}
}

void CameraAndDynamicIndexingApp::BuildFrameResources() {
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

void CameraAndDynamicIndexingApp::BuildMaterials()
{
	auto bricks0 = std::make_unique<Material>();
	bricks0->NumFramesDirty = gNumFrameResources;
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = glm::vec3(0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.1f;

	auto stone0 = std::make_unique<Material>();
	stone0->NumFramesDirty = gNumFrameResources;
	stone0->Name = "stone0";
	stone0->MatCBIndex = 1;
	stone0->DiffuseSrvHeapIndex = 1;
	stone0->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	stone0->FresnelR0 = glm::vec3(0.05f, 0.05f, 0.05f);
	stone0->Roughness = 0.3f;

	auto tile0 = std::make_unique<Material>();
	tile0->NumFramesDirty = gNumFrameResources;
	tile0->Name = "tile0";	
	tile0->MatCBIndex = 2;
	tile0->DiffuseSrvHeapIndex = 2;
	tile0->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	tile0->FresnelR0 = glm::vec3(0.02f, 0.02f, 0.02f);
	tile0->Roughness = 0.3f;

	auto crate0 = std::make_unique<Material>();
	crate0->NumFramesDirty = gNumFrameResources;
	crate0->Name = "crate0";
	crate0->MatCBIndex = 3;
	crate0->DiffuseSrvHeapIndex = 3;
	crate0->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	crate0->FresnelR0 = glm::vec3(0.05f, 0.05f, 0.05f);
	crate0->Roughness = 0.2f;

	mMaterials["bricks0"] = std::move(bricks0);
	mMaterials["stone0"] = std::move(stone0);
	mMaterials["tile0"] = std::move(tile0);
	mMaterials["crate0"] = std::move(crate0);
}

void CameraAndDynamicIndexingApp::BuildShapeGeometry() {
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
	uint32_t boxVertexOffset = 0;
	uint32_t gridVertexOffset = (uint32_t)box.Vertices.size();
	uint32_t sphereVertexOffset = gridVertexOffset + (uint32_t)grid.Vertices.size();
	uint32_t cylinderVertexOffset = sphereVertexOffset + (uint32_t)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	uint32_t boxIndexOffset = 0;
	uint32_t gridIndexOffset = (uint32_t)box.Indices32.size();
	uint32_t sphereIndexOffset = gridIndexOffset + (uint32_t)grid.Indices32.size();
	uint32_t cylinderIndexOffset = sphereIndexOffset + (uint32_t)sphere.Indices32.size();

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (uint32_t)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (uint32_t)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (uint32_t)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (uint32_t)cylinder.Indices32.size();
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

	uint32_t k = 0;
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

	const uint32_t vbByteSize = (uint32_t)vertices.size() * sizeof(Vertex);
	const uint32_t ibByteSize = (uint32_t)indices.size() * sizeof(std::uint32_t);

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

	geo->VertexBufferByteSize = (uint32_t)vbByteSize;
	geo->VertexByteStride = sizeof(Vertex);
	geo->IndexBufferByteSize = ibByteSize;



	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);


}

void CameraAndDynamicIndexingApp::BuildRenderItems()
{
	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->World = glm::translate(glm::scale(glm::mat4(1.0f),glm::vec3(2.0f)),glm::vec3(0.0f,1.0f,0.0f));
	boxRitem->TexTransform = glm::scale(glm::mat4(1.0f),glm::vec3(1.0f, 1.0f, 1.0f));
	boxRitem->ObjCBIndex = 0;
	boxRitem->Mat = mMaterials["crate0"].get();
	boxRitem->Geo = mGeometries["shapeGeo"].get();	
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = glm::mat4(1.0f);
	gridRitem->TexTransform =glm::scale(glm::mat4(1.0f),glm::vec3(8.0f, 8.0f, 1.0f));
	gridRitem->ObjCBIndex = 1;
	gridRitem->Mat = mMaterials["tile0"].get();
	gridRitem->Geo = mGeometries["shapeGeo"].get();	
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mAllRitems.push_back(std::move(gridRitem));

	glm::mat4 brickTexTransform = glm::scale(glm::mat4(1.0f),glm::vec3(1.0f, 1.0f, 1.0f));
	uint32_t objCBIndex = 2;
	for (int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		glm::mat4 leftCylWorld = glm::translate(glm::mat4(1.0f),glm::vec3(-5.0f, 1.5f, -10.0f + i * 5.0f));
		glm::mat4 rightCylWorld = glm::translate(glm::mat4(1.0f),glm::vec3(+5.0f, 1.5f, -10.0f + i * 5.0f));

		glm::mat4 leftSphereWorld = glm::translate(glm::mat4(1.0f),glm::vec3(-5.0f, 3.5f, -10.0f + i * 5.0f));
		glm::mat4 rightSphereWorld = glm::translate(glm::mat4(1.0f),glm::vec3(+5.0f, 3.5f, -10.0f + i * 5.0f));

		leftCylRitem->World = rightCylWorld;
		leftCylRitem->TexTransform = brickTexTransform;
		leftCylRitem->ObjCBIndex = objCBIndex++;
		leftCylRitem->Mat = mMaterials["bricks0"].get();
		leftCylRitem->Geo = mGeometries["shapeGeo"].get();		
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		rightCylRitem->World = leftCylWorld;
		rightCylRitem->TexTransform =  brickTexTransform;
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Mat = mMaterials["bricks0"].get();
		rightCylRitem->Geo = mGeometries["shapeGeo"].get();		
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		leftSphereRitem->World = leftSphereWorld;
		leftSphereRitem->TexTransform = glm::mat4(1.0f);
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Mat = mMaterials["stone0"].get();
		leftSphereRitem->Geo = mGeometries["shapeGeo"].get();		
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		
		rightSphereRitem->World = rightSphereWorld;
		rightSphereRitem->TexTransform = glm::mat4(1.0f);
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Mat = mMaterials["stone0"].get();
		rightSphereRitem->Geo = mGeometries["shapeGeo"].get();		
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		mAllRitems.push_back(std::move(leftCylRitem));
		mAllRitems.push_back(std::move(rightCylRitem));
		mAllRitems.push_back(std::move(leftSphereRitem));
		mAllRitems.push_back(std::move(rightSphereRitem));
	}

	// All the render items are opaque.
	for (auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());
}


void CameraAndDynamicIndexingApp::Update(const GameTimer& gt) {
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

void CameraAndDynamicIndexingApp::UpdateMainPassCB(const GameTimer& gt) {
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


void  CameraAndDynamicIndexingApp::UpdateObjectCBs(const GameTimer& gt) {

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

void CameraAndDynamicIndexingApp::UpdateMaterialsBuffer(const GameTimer& gt) {
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

void CameraAndDynamicIndexingApp::AnimateMaterials(const GameTimer&gt) {

}

void CameraAndDynamicIndexingApp::OnResize() {
	VulkApp::OnResize();
	mCamera.SetLens(0.25f * MathHelper::Pi, (float)mClientWidth / (float)mClientHeight, 1.0f, 1000.0f);
}


void CameraAndDynamicIndexingApp::OnMouseDown(WPARAM btnState, int x, int y) {
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void CameraAndDynamicIndexingApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void CameraAndDynamicIndexingApp::OnMouseMove(WPARAM btnState, int x, int y) {
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

void CameraAndDynamicIndexingApp::OnKeyboardInput(const GameTimer& gt)
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

void CameraAndDynamicIndexingApp::Draw(const GameTimer& gt) {
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
		DrawRenderItems(cmd, mOpaqueRitems);
		
	}
	else {

		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque"]);
		DrawRenderItems(cmd, mOpaqueRitems);
	}
	EndRender(cmd);
}
void CameraAndDynamicIndexingApp::DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems) {
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
		uint32_t dyoffsets[2] = { (uint32_t)(cbvIndex * objectSize)};
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 1, 1, &descriptor1, 1, dyoffsets);
		pvkCmdDrawIndexed(cmd, ri->IndexCount, 1, 0, ri->BaseVertexLocation, 0);
	}
}



int main() {
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		CameraAndDynamicIndexingApp theApp(GetModuleHandle(NULL));
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