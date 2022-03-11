#include "../../../Common/VulkApp.h"
#include "../../../Common/VulkUtil.h"
#include "../../../Common/VulkanEx.h"
#include "../../../Common/GeometryGenerator.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/Colors.h"
#include "../../../Common/TextureLoader.h"
#include "FrameResource.h"
#include "Waves.h"
#include <memory>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


const int gNumFrameResources = 3;


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
	AlphaTestedTreeSprites,
	Count
};


class TreeBillboardApp : public VulkApp {
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

	std::unique_ptr<VulkanPipeline> treeSpritePipeline;
	
	

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map < std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture> > mTextures;
	std::unordered_map<std::string, VkPipeline> mPSOs;

	RenderItem* mWavesRitem{ nullptr };

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	std::unique_ptr<Waves> mWaves;

	Vulkan::Buffer WavesIndexBuffer;
	std::vector<Vulkan::Buffer> WaveVertexBuffers;
	std::vector<void*> WaveVertexPtrs;

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
	void UpdateWaves(const GameTimer& gt);

	void LoadTextures();
	void BuildLandGeometry();
	void BuildWavesGeometry();
	void BuildBoxGeometry();
	void BuildTreeSpritesGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems);

	float GetHillsHeight(float x, float z)const;
	glm::vec3 GetHillsNormal(float x, float z)const;


	
public:
	TreeBillboardApp(HINSTANCE);
	TreeBillboardApp& operator=(const TreeBillboardApp& rhs) = delete;
	~TreeBillboardApp();
	virtual bool Initialize()override;
};

TreeBillboardApp::TreeBillboardApp(HINSTANCE hInstance) :VulkApp(hInstance) {
	mAllowWireframe = true;
	mClearValues[0].color = Colors::LightSteelBlue;
	mMSAA = false;
	mGeometryShader = true;//we need gl_PrimitiveID 
}

TreeBillboardApp::~TreeBillboardApp() {
	vkDeviceWaitIdle(mDevice);
	for (auto& buffer : WaveVertexBuffers) {
		Vulkan::unmapBuffer(mDevice, buffer);
		Vulkan::cleanupBuffer(mDevice, buffer);
	}
	for (auto& pair : mGeometries) {
		free(pair.second->indexBufferCPU);
		if (pair.second->vertexBufferGPU.buffer != mWavesRitem->Geo->vertexBufferGPU.buffer) {
			free(pair.second->vertexBufferCPU);
			cleanupBuffer(mDevice, pair.second->vertexBufferGPU);
		}
		cleanupBuffer(mDevice, pair.second->indexBufferGPU);
	}
}

bool TreeBillboardApp::Initialize() {
	if (!VulkApp::Initialize())
		return false;
	mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

	LoadTextures();

	BuildLandGeometry();
	BuildWavesGeometry();
	BuildBoxGeometry();
	BuildTreeSpritesGeometry();
	BuildMaterials();
	BuildRenderItems();


	BuildPSOs();
	BuildFrameResources();


	return true;
}


void TreeBillboardApp::BuildLandGeometry() {
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
	VertexBufferBuilder::begin(mDevice, mBackQueue, mCommandBuffer, mMemoryProperties)
		.AddVertices(vbByteSize, (float*)vertices.data())
		.build(geo->vertexBufferGPU, vertexLocations);

	std::vector<uint32_t> indexLocations;
	IndexBufferBuilder::begin(mDevice, mBackQueue, mCommandBuffer, mMemoryProperties)
		.AddIndices(ibByteSize, indices.data())
		.build(geo->indexBufferGPU, indexLocations);

//	Vulkan::BufferProperties props;
//#ifdef __USE__VMA__
//	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
//#else
//	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
//#endif
//	props.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
//	props.size = vbByteSize;
//	Vulkan::initBuffer(mDevice, mMemoryProperties, props, geo->vertexBufferGPU);
//
//#ifdef __USE__VMA__
//	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
//#else
//	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
//#endif
//	props.bufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
//	props.size = ibByteSize;
//	Vulkan::initBuffer(mDevice, mMemoryProperties, props, geo->indexBufferGPU);
//
//	VkDeviceSize maxSize = std::max(vbByteSize, ibByteSize);
//	Vulkan::Buffer stagingBuffer;
//
//#ifdef __USE__VMA__
//	props.usage = VMA_MEMORY_USAGE_CPU_ONLY;
//#else
//	props.memoryProps = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
//#endif
//	props.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
//	props.size = maxSize;
//	initBuffer(mDevice, mMemoryProperties, props, stagingBuffer);
//	void* ptr = mapBuffer(mDevice, stagingBuffer);
//	//copy vertex data
//	memcpy(ptr, vertices.data(), vbByteSize);
//
//	CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->vertexBufferGPU, vbByteSize);
//	memcpy(ptr, indices.data(), ibByteSize);
//	CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->indexBufferGPU, ibByteSize);
//	unmapBuffer(mDevice, stagingBuffer);
//	cleanupBuffer(mDevice, stagingBuffer);
//
//	

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


float TreeBillboardApp::GetHillsHeight(float x, float z)const
{
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

glm::vec3 TreeBillboardApp::GetHillsNormal(float x, float z)const
{
	// n = (-df/dx, 1, -df/dz)
	glm::vec3 n(
		-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
		1.0f,
		-0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

	glm::vec3 unitNormal = glm::normalize(n);
	return unitNormal;
}

void TreeBillboardApp::BuildWavesGeometry() {
	std::vector<uint32_t> indices(3 * mWaves->TriangleCount());//3 indices per face
	assert(mWaves->VertexCount() < 0x0000FFFFF);

	//Iterate over each quad.
	int m = mWaves->RowCount();
	int n = mWaves->ColumnCount();
	int k = 0;

	for (int i = 0; i < m - 1; ++i) {
		for (int j = 0; j < n - 1; ++j) {
			indices[k] = i * n + j;
			indices[k + 1] = i * n + j + 1;
			indices[k + 2] = (i + 1) * n + j;

			indices[k + 3] = (i + 1) * n + j;
			indices[k + 4] = i * n + j + 1;
			indices[k + 5] = (i + 1) * n + j + 1;
			k += 6;//next quad
		}
	}

	uint32_t vbByteSize = mWaves->VertexCount() * sizeof(Vertex);
	uint32_t ibByteSize = (uint32_t)indices.size() * sizeof(uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

	//Set dynamically.
	geo->vertexBufferCPU = nullptr;


	geo->indexBufferCPU = malloc(ibByteSize);
	memcpy(geo->indexBufferCPU, indices.data(), ibByteSize);

	Vulkan::BufferProperties props;
#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_CPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
#endif
	props.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	props.size = vbByteSize;
	WaveVertexBuffers.resize(gNumFrameResources);
	WaveVertexPtrs.resize(gNumFrameResources);
	for (size_t i = 0; i < gNumFrameResources; i++) {
		Vulkan::initBuffer(mDevice, mMemoryProperties, props, WaveVertexBuffers[i]);
		WaveVertexPtrs[i] = Vulkan::mapBuffer(mDevice, WaveVertexBuffers[i]);
	}
#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
#endif
	props.bufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	props.size = ibByteSize;
	Vulkan::initBuffer(mDevice, mMemoryProperties, props, WavesIndexBuffer);



	VkDeviceSize maxSize = std::max(vbByteSize, ibByteSize);
	Vulkan::Buffer stagingBuffer;

#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_CPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
#endif
	props.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	props.size = maxSize;
	initBuffer(mDevice, mMemoryProperties, props, stagingBuffer);
	void* ptr = mapBuffer(mDevice, stagingBuffer);
	//copy vertex data

	memcpy(ptr, indices.data(), ibByteSize);
	CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, WavesIndexBuffer, ibByteSize);
	unmapBuffer(mDevice, stagingBuffer);
	cleanupBuffer(mDevice, stagingBuffer);

	//initBuffer(mDevice, mMemoryProperties, ibByteSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, geo->indexBufferGPU);

	//VkDeviceSize maxSize = std::max(vbByteSize, ibByteSize);
	//Buffer stagingBuffer;
	//initBuffer(mDevice, mMemoryProperties, maxSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer);
	//void* ptr = mapBuffer(mDevice, stagingBuffer);
	////copy vertex data

	//memcpy(ptr, indices.data(), ibByteSize);
	//CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->indexBufferGPU, ibByteSize);
	//unmapBuffer(mDevice, stagingBuffer);
	//cleanupBuffer(mDevice, stagingBuffer);

	geo->indexBufferGPU = WavesIndexBuffer;

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



void TreeBillboardApp::BuildBoxGeometry() {
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

	geo->vertexBufferCPU = malloc(vbByteSize);
	memcpy(geo->vertexBufferCPU, vertices.data(), vbByteSize);

	geo->indexBufferCPU = malloc(ibByteSize);
	memcpy(geo->indexBufferCPU, indices.data(), ibByteSize);

	Vulkan::BufferProperties props;
#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_CPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
#endif
	props.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	props.size = vbByteSize;
	Vulkan::initBuffer(mDevice, mMemoryProperties, props, geo->vertexBufferGPU);

#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
#endif
	props.bufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	props.size = ibByteSize;
	Vulkan::initBuffer(mDevice, mMemoryProperties, props, geo->indexBufferGPU);




	VkDeviceSize maxSize = std::max(vbByteSize, ibByteSize);
	Vulkan::Buffer stagingBuffer;

#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_CPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
#endif
	props.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	props.size = maxSize;
	initBuffer(mDevice, mMemoryProperties, props, stagingBuffer);
	void* ptr = mapBuffer(mDevice, stagingBuffer);
	//copy vertex data
	memcpy(ptr, vertices.data(), vbByteSize);
	CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->vertexBufferGPU, vbByteSize);
	memcpy(ptr, indices.data(), ibByteSize);
	CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->indexBufferGPU, ibByteSize);
	unmapBuffer(mDevice, stagingBuffer);
	cleanupBuffer(mDevice, stagingBuffer);

	//initBuffer(mDevice, mMemoryProperties, vbByteSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, geo->vertexBufferGPU);
	//initBuffer(mDevice, mMemoryProperties, ibByteSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, geo->indexBufferGPU);

	//VkDeviceSize maxSize = std::max(vbByteSize, ibByteSize);
	//Buffer stagingBuffer;
	//initBuffer(mDevice, mMemoryProperties, maxSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer);
	//void* ptr = mapBuffer(mDevice, stagingBuffer);
	////copy vertex data
	//memcpy(ptr, vertices.data(), vbByteSize);
	//CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->vertexBufferGPU, vbByteSize);
	//memcpy(ptr, indices.data(), ibByteSize);
	//CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->indexBufferGPU, ibByteSize);
	//unmapBuffer(mDevice, stagingBuffer);
	//cleanupBuffer(mDevice, stagingBuffer);

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

void TreeBillboardApp::BuildTreeSpritesGeometry() {
	struct TreeSpriteVertex
	{
		glm::vec3 Pos;
		glm::vec2 Size;
	};

	static const int treeCount = 16;
	std::array<TreeSpriteVertex, 16> vertices;
	for (UINT i = 0; i < treeCount; ++i)
	{
		float x = MathHelper::RandF(-45.0f, 45.0f);
		float z = MathHelper::RandF(-45.0f, 45.0f);
		float y = GetHillsHeight(x, z);

		// Move tree slightly above land height.
		y += 8.0f;

		vertices[i].Pos = glm::vec3(x, y, z);
		vertices[i].Size = glm::vec2(20.0f, 20.0f);
	}

	std::array<std::uint32_t, 16> indices =
	{
		0, 1, 2, 3, 4, 5, 6, 7,
		8, 9, 10, 11, 12, 13, 14, 15
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(TreeSpriteVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "treeSpritesGeo";

	geo->vertexBufferCPU = malloc(vbByteSize);
	memcpy(geo->vertexBufferCPU, vertices.data(), vbByteSize);

	geo->indexBufferCPU = malloc(ibByteSize);
	memcpy(geo->indexBufferCPU, indices.data(), ibByteSize);

	Vulkan::BufferProperties props;
#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_CPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
#endif
	props.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	props.size = vbByteSize;
	Vulkan::initBuffer(mDevice, mMemoryProperties, props, geo->vertexBufferGPU);

#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
#endif
	props.bufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	props.size = ibByteSize;
	Vulkan::initBuffer(mDevice, mMemoryProperties, props, geo->indexBufferGPU);




	VkDeviceSize maxSize = std::max(vbByteSize, ibByteSize);
	Vulkan::Buffer stagingBuffer;

#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_CPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
#endif
	props.bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	props.size = maxSize;
	initBuffer(mDevice, mMemoryProperties, props, stagingBuffer);
	void* ptr = mapBuffer(mDevice, stagingBuffer);
	//copy vertex data
	memcpy(ptr, vertices.data(), vbByteSize);
	CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->vertexBufferGPU, vbByteSize);
	memcpy(ptr, indices.data(), ibByteSize);
	CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->indexBufferGPU, ibByteSize);
	unmapBuffer(mDevice, stagingBuffer);
	cleanupBuffer(mDevice, stagingBuffer);

	geo->VertexByteStride = sizeof(TreeSpriteVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexBufferByteSize = ibByteSize;


	SubmeshGeometry submesh;
	submesh.IndexCount = (uint32_t)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["points"] = submesh;

	mGeometries["treeSpritesGeo"] = std::move(geo);

}

void TreeBillboardApp::BuildMaterials() {
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

	auto treeSprites = std::make_unique<Material>();
	treeSprites->NumFramesDirty = gNumFrameResources;
	treeSprites->Name = "treeSprites";
	treeSprites->MatCBIndex = 3;
	treeSprites->DiffuseSrvHeapIndex = 3;
	treeSprites->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprites->FresnelR0 = glm::vec3(0.01f, 0.01f, 0.01f);
	treeSprites->Roughness = 0.125f;

	mMaterials["grass"] = std::move(grass);
	mMaterials["water"] = std::move(water);
	mMaterials["wirefence"] = std::move(wirefence);
	mMaterials["treeSprites"] = std::move(treeSprites);
}

void TreeBillboardApp::BuildRenderItems()
{
	auto wavesRitem = std::make_unique<RenderItem>();
	wavesRitem->World = MathHelper::Identity4x4();
	wavesRitem->TexTransform = glm::scale(glm::mat4(1.0f), glm::vec3(5.0f, 5.0f, 1.0f));// , XMMatrixScaling(5.0f, 5.0f, 1.0f));
	wavesRitem->ObjCBIndex = 0;
	wavesRitem->Mat = mMaterials["water"].get();
	wavesRitem->Geo = mGeometries["waterGeo"].get();
	///wavesRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["grid"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mWavesRitem = wavesRitem.get();

	mRitemLayer[(int)RenderLayer::Transparent].push_back(wavesRitem.get());

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

	auto treeSpritesRitem = std::make_unique<RenderItem>();
	treeSpritesRitem->World = MathHelper::Identity4x4();
	treeSpritesRitem->ObjCBIndex = 3;
	treeSpritesRitem->Mat = mMaterials["treeSprites"].get();
	treeSpritesRitem->Geo = mGeometries["treeSpritesGeo"].get();	
	treeSpritesRitem->IndexCount = treeSpritesRitem->Geo->DrawArgs["points"].IndexCount;
	treeSpritesRitem->StartIndexLocation = treeSpritesRitem->Geo->DrawArgs["points"].StartIndexLocation;
	treeSpritesRitem->BaseVertexLocation = treeSpritesRitem->Geo->DrawArgs["points"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeSpritesRitem.get());

	mAllRitems.push_back(std::move(wavesRitem));
	mAllRitems.push_back(std::move(gridRitem));
	mAllRitems.push_back(std::move(boxRitem));
	mAllRitems.push_back(std::move(treeSpritesRitem));
}

void TreeBillboardApp::BuildPSOs() {
	descriptorSetPoolCache = std::make_unique<DescriptorSetPoolCache>(mDevice);
	descriptorSetLayoutCache = std::make_unique<DescriptorSetLayoutCache>(mDevice);

	VkDescriptorSet descriptor0 = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptorSets0;

	VkDescriptorSetLayout descriptorLayout0;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT |VK_SHADER_STAGE_GEOMETRY_BIT| VK_SHADER_STAGE_FRAGMENT_BIT)
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

	VkDescriptorSet descriptor3 = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptorSets3;
	VkDescriptorSetLayout descriptorLayout3 = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(descriptorSets3, descriptorLayout3, 4);//extra descriptor for tree sprite

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
	//put tree sprites at end of texturesList
	std::vector<Vulkan::Texture> treeTexture;
	TextureLoader::begin(mDevice, mCommandBuffer, mGraphicsQueue, mMemoryProperties)
		.addTexture("../../../Textures/treeArray2-1.png")
		.addTexture("../../../Textures/treeArray2-2.png")
		.addTexture("../../../Textures/treeArray2-3.png")
		.setIsArray(true)
		.load(treeTexture);
	texturesList.push_back(treeTexture[0]);

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
	shaders.clear();

	ShaderProgramLoader::begin(mDevice)
		.AddShaderPath("Shaders/alphatest.vert.spv")
		.AddShaderPath("Shaders/alphatest.frag.spv")
		.load(shaders, vertexInputDescription, vertexAttributeDescriptions);

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
	shaders.clear();

	ShaderProgramLoader::begin(mDevice)
		.AddShaderPath("Shaders/treesprite.vert.spv")
		.AddShaderPath("Shaders/treesprite.geom.spv")
		.AddShaderPath("Shaders/treesprite.frag.spv")
		.load(shaders, vertexInputDescription, vertexAttributeDescriptions);

	PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
		.setCullMode(VK_CULL_MODE_NONE)		
		.setDepthTest(VK_TRUE)
		.setBlend(VK_TRUE)
		.setTopology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
		.build(pipeline);
	treeSpritePipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
	mPSOs["treeSprites"] = pipeline;
	for (auto& shader : shaders) {
		Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
	}
	shaders.clear();
}
void TreeBillboardApp::BuildFrameResources() {
	for (int i = 0; i < gNumFrameResources; i++) {
		auto& ub = *uniformBuffer;

		PassConstants* pc = (PassConstants*)((uint8_t*)ub[0].ptr + ub[0].objectSize * ub[0].objectCount * i);// ((uint8_t*)pPassCB + passSize * i);
		ObjectConstants* oc = (ObjectConstants*)((uint8_t*)ub[1].ptr + ub[1].objectSize * ub[1].objectCount * i);// ((uint8_t*)pObjectCB + objectSize * mAllRitems.size() * i);
		MaterialConstants* mc = (MaterialConstants*)((uint8_t*)ub[2].ptr + ub[2].objectSize * ub[2].objectCount * i);// ((uint8_t*)pMatCB + matSize * mMaterials.size() * i);
		Vertex* pWv = (Vertex*)WaveVertexPtrs[i];
		mFrameResources.push_back(std::make_unique<FrameResource>(pc, oc, mc,pWv));
	}
}


void TreeBillboardApp::Draw(const GameTimer& gt) {
	uint32_t index = 0;
	VkCommandBuffer cmd{ VK_NULL_HANDLE };

	cmd = BeginRender();

	VkViewport viewport = { 0.0f,0.0f,(float)mClientWidth,(float)mClientHeight,0.0f,1.0f };
	pvkCmdSetViewport(cmd, 0, 1, &viewport);
	VkRect2D scissor = { {0,0},{(uint32_t)mClientWidth,(uint32_t)mClientHeight} };
	pvkCmdSetScissor(cmd, 0, 1, &scissor);


	if (mIsWireframe) {
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque_wireframe"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Opaque]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::AlphaTested]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Transparent]);
	}
	else {

		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Opaque]);
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["alphaTested"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::AlphaTested]);
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["treeSprites"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites]);
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["transparent"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Transparent]);
	}
	EndRender(cmd);
}

void TreeBillboardApp::DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems) {
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


void TreeBillboardApp::OnResize() {
	VulkApp::OnResize();
	mProj = glm::perspectiveFovLH_ZO(0.25f * pi, (float)mClientWidth, (float)mClientHeight, 1.0f, 1000.0f);
}

void TreeBillboardApp::OnMouseDown(WPARAM btnState, int x, int y) {
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void TreeBillboardApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void TreeBillboardApp::OnMouseMove(WPARAM btnState, int x, int y) {
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
void TreeBillboardApp::OnKeyboardInput(const GameTimer& gt)
{
	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;

	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('P') & 0x8000)
		saveScreenCap(mDevice,mCommandBuffer,mGraphicsQueue, mSwapchainImages[mIndex], mFormatProperties, mSwapchainFormat.format, mSwapchainExtent, mFrameCount);
}

void TreeBillboardApp::Update(const GameTimer& gt) {
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
	UpdateWaves(gt);
}

void TreeBillboardApp::UpdateCamera(const GameTimer& gt) {
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	mEyePos = glm::vec3(x, y, z);

	glm::vec3 pos = glm::vec3(x, y, z);
	glm::vec3 target = glm::vec3(0.0f);
	glm::vec3 up = glm::vec3(0.0, 1.0f, 0.0f);

	mView = glm::lookAtLH(pos, target, up);
}


void TreeBillboardApp::UpdateObjectCBs(const GameTimer& gt) {
	
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
			memcpy((pObjConsts + (objSize * e->ObjCBIndex)), &objConstants, sizeof(objConstants));
			//pObjConsts[e->ObjCBIndex] = objConstants;
			e->NumFramesDirty--;
		}
	}
}
void TreeBillboardApp::UpdateMainPassCB(const GameTimer& gt) {
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

void TreeBillboardApp::UpdateMaterialsCBs(const GameTimer& gt) {
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


void TreeBillboardApp::UpdateWaves(const GameTimer& gt)
{
	// Every quarter second, generate a random wave.
	static float t_base = 0.0f;
	if ((mTimer.TotalTime() - t_base) >= 0.25f)
	{
		t_base += 0.25f;

		int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
		int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);

		float r = MathHelper::RandF(0.2f, 0.5f);

		mWaves->Disturb(i, j, r);
	}

	// Update the wave simulation.
	mWaves->Update(gt.DeltaTime());

	VkDeviceSize waveBufferSize = sizeof(Vertex) * mWaves->VertexCount();

	Vertex* pWaves = mCurrFrameResource->pWavesVB;

	for (int i = 0; i < mWaves->VertexCount(); ++i) {
		pWaves[i].Pos = mWaves->Position(i);
		pWaves[i].Normal = mWaves->Normal(i);
		// Derive tex-coords from position by 
		// mapping [-w/2,w/2] --> [0,1]
		pWaves[i].TexC.x = 0.5f + pWaves[i].Pos.x / mWaves->Width();
		pWaves[i].TexC.y = 0.5f - pWaves[i].Pos.z / mWaves->Depth();
	}
	mWavesRitem->Geo->vertexBufferGPU = WaveVertexBuffers[mCurrFrameResourceIndex];

}





void TreeBillboardApp::AnimateMaterials(const GameTimer& gt)
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

void TreeBillboardApp::LoadTextures() {

}



int main() {
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		TreeBillboardApp theApp(GetModuleHandle(NULL));
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
