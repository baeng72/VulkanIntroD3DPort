#include "../../../Common/VulkApp.h"
#include "../../../Common/VulkUtil.h"
#include "../../../Common/GeometryGenerator.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/Colors.h"
#include "../../../Common/TextureLoader.h"


#include <array>
#include <memory>
#include <fstream>
#include <iostream>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "FrameResource.h"
//#include "ShaderProgram.h"

//#include "vk_descriptors.h"

#include "temp.h"

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
	Mirrors,
	Reflected,
	Transparent,
	Shadow,
	Count
};


class StencilApp : public VulkApp {
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	/*std::unique_ptr<ShaderResources> pipelineRes;
	std::unique_ptr<ShaderProgram> prog;
	std::unique_ptr<PipelineLayout> pipelineLayout;
	std::unique_ptr<Pipeline> opaquePipeline;
	std::unique_ptr<Pipeline> wireframePipeline;
	std::unique_ptr<Pipeline> transparentPipeline;
	std::unique_ptr<Pipeline> markStencilMirrorsPipeline;
	std::unique_ptr<Pipeline> drawStencilReflectionsPipeline;
	std::unique_ptr<Pipeline> shadowPipeline;*/

	std::unique_ptr<DescriptorSetLayoutCache> descriptorSetLayoutCache;
	std::unique_ptr<DescriptorSetPoolCache> descriptorSetPoolCache;	
	std::unique_ptr<UniformBuffer> uniformBuffer;
	std::unique_ptr<VulkanTextureList> textures;
	std::unique_ptr<VulkanDescriptorList> uniformDescriptors;
	std::unique_ptr<VulkanDescriptorList> textureDescriptors;
	std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
	std::unique_ptr<VulkanPipeline> opaquePipeline;
	std::unique_ptr<VulkanPipeline> wireframePipeline;
	std::unique_ptr<VulkanPipeline> transparentPipeline;
	std::unique_ptr<VulkanPipeline> markStencilMirrorsPipeline;
	std::unique_ptr<VulkanPipeline> drawStencilReflectionsPipeline;
	std::unique_ptr<VulkanPipeline> shadowPipeline;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map < std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture> > mTextures;
	std::unordered_map<std::string, VkPipeline> mPSOs;

	// Cache render items of interest.
	RenderItem* mSkullRitem = nullptr;
	RenderItem* mReflectedSkullRitem = nullptr;
	RenderItem* mShadowedSkullRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	PassConstants mMainPassCB;
	PassConstants mReflectedPassCB;

	glm::vec3 mSkullTranslation = { 0.0f, 1.0f, -5.0f };

	glm::vec3 mEyePos = glm::vec3(0.0f);
	glm::mat4 mView = glm::mat4(1.0f);
	glm::mat4 mProj = glm::mat4(1.0f);

	float mTheta = 1.24f * pi;
	float mPhi = 0.42f * pi;
	float mRadius = 12.0f;

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

	void UpdateReflectedPassCB(const GameTimer& gt);

	void LoadTextures();
	void BuildRoomGeometry();
	void BuildSkullGeometry();
	void BuildBoxGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems);

public:
	StencilApp(HINSTANCE hInstance);
	StencilApp(const StencilApp& rhs) = delete;
	StencilApp& operator=(const StencilApp& rhs) = delete;
	~StencilApp();
	virtual bool Initialize()override;

};

StencilApp::StencilApp(HINSTANCE hInstance) :VulkApp(hInstance) {
	mAllowWireframe = true;
	mClearValues[0].color = Colors::LightSteelBlue;
	mMSAA = false;
	mDepthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;//set stencil format
}


StencilApp::~StencilApp() {
	vkDeviceWaitIdle(mDevice);
	for (auto& pair : mGeometries) {
		free(pair.second->indexBufferCPU);



		free(pair.second->vertexBufferCPU);
		Vulkan::cleanupBuffer(mDevice, pair.second->vertexBufferGPU);


		Vulkan::cleanupBuffer(mDevice, pair.second->indexBufferGPU);

	}	
}


bool StencilApp::Initialize() {
	if (!VulkApp::Initialize())
		return false;
	LoadTextures();
	
	BuildRoomGeometry();
	BuildSkullGeometry();
	BuildMaterials();
	BuildRenderItems();
	
	BuildPSOs();
	BuildFrameResources();
	

	return true;
}

void StencilApp::BuildRoomGeometry()
{
	// Create and specify geometry.  For this sample we draw a floor
// and a wall with a mirror on it.  We put the floor, wall, and
// mirror geometry in one vertex buffer.
//
//   |--------------|
//   |              |
//   |----|----|----|
//   |Wall|Mirr|Wall|
//   |    | or |    |
//   /--------------/
//  /   Floor      /
// /--------------/

	std::array<Vertex, 20> vertices =
	{
		// Floor: Observe we tile texture coordinates.
		Vertex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f), // 0 
		Vertex(-3.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
		Vertex(7.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f),
		Vertex(7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f),

		// Wall: Observe we tile texture coordinates, and that we
		// leave a gap in the middle for the mirror.
		Vertex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 4
		Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f),
		Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f),

		Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 8 
		Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f),
		Vertex(7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f),

		Vertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 12
		Vertex(-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f),
		Vertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f),

		// Mirror
		Vertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 16
		Vertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
		Vertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
		Vertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f)
	};

	std::array<std::int32_t, 30> indices =
	{
		// Floor
		0, 1, 2,
		0, 2, 3,

		// Walls
		4, 5, 6,
		4, 6, 7,

		8, 9, 10,
		8, 10, 11,

		12, 13, 14,
		12, 14, 15,

		// Mirror
		16, 17, 18,
		16, 18, 19
	};

	SubmeshGeometry floorSubmesh;
	floorSubmesh.IndexCount = 6;
	floorSubmesh.StartIndexLocation = 0;
	floorSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry wallSubmesh;
	wallSubmesh.IndexCount = 18;
	wallSubmesh.StartIndexLocation = 6;
	wallSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry mirrorSubmesh;
	mirrorSubmesh.IndexCount = 6;
	mirrorSubmesh.StartIndexLocation = 24;
	mirrorSubmesh.BaseVertexLocation = 0;

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "roomGeo";

	geo->vertexBufferCPU = malloc(vbByteSize);
	memcpy(geo->vertexBufferCPU, vertices.data(), vbByteSize);

	geo->indexBufferCPU = malloc(ibByteSize);
	memcpy(geo->indexBufferCPU, indices.data(), ibByteSize);

	Vulkan::BufferProperties props;
#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
#endif
	props.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
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

	/*ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);*/

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["floor"] = floorSubmesh;
	geo->DrawArgs["wall"] = wallSubmesh;
	geo->DrawArgs["mirror"] = mirrorSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}


void StencilApp::BuildSkullGeometry() {
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

	std::vector<Vertex> vertices(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;
	}

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

	Vulkan::BufferProperties props;
#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
#endif
	props.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
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

	/*initBuffer(mDevice, mMemoryProperties, vbByteSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, geo->vertexBufferGPU);
	initBuffer(mDevice, mMemoryProperties, ibByteSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, geo->indexBufferGPU);
	VkDeviceSize maxSize = std::max(vbByteSize, ibByteSize);
	Buffer stagingBuffer;
	initBuffer(mDevice, mMemoryProperties, maxSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, stagingBuffer);
	void* ptr = mapBuffer(mDevice, stagingBuffer);
	memcpy(ptr, vertices.data(), vbByteSize);
	CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->vertexBufferGPU, vbByteSize);
	memcpy(ptr, indices.data(), ibByteSize);
	CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->indexBufferGPU, ibByteSize);
	unmapBuffer(mDevice, stagingBuffer);
	cleanupBuffer(mDevice, stagingBuffer);*/

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;

	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (uint32_t)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["skull"] = submesh;
	mGeometries[geo->Name] = std::move(geo);

}

void StencilApp::BuildPSOs() {
	{
		//DescriptorSetPoolCache poolCache(mDevice);
		//DescriptorSetLayoutCache layoutCache(mDevice);

		descriptorSetPoolCache = std::make_unique<DescriptorSetPoolCache>(mDevice);
		descriptorSetLayoutCache = std::make_unique<DescriptorSetLayoutCache>(mDevice);

		VkDescriptorSet descriptor0 = VK_NULL_HANDLE;
		std::vector<VkDescriptorSet> descriptorSets0;

		VkDescriptorSetLayout descriptorLayout0;
		DescriptorSetBuilder2::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
			.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(descriptor0, descriptorLayout0);

		VkDescriptorSet descriptor1 = VK_NULL_HANDLE;
		std::vector<VkDescriptorSet> descriptorSets1;
		VkDescriptorSetLayout descriptorLayout1;
		DescriptorSetBuilder2::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
			.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
			.build(descriptor1, descriptorLayout1);

		VkDescriptorSet descriptor2 = VK_NULL_HANDLE;
		std::vector<VkDescriptorSet> descriptorSets2;
		VkDescriptorSetLayout descriptorLayout2;
		DescriptorSetBuilder2::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
			.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(descriptor2, descriptorLayout2);

		VkDescriptorSet descriptor3 = VK_NULL_HANDLE;
		std::vector<VkDescriptorSet> descriptorSets3;
		VkDescriptorSetLayout descriptorLayout3 = VK_NULL_HANDLE;
		DescriptorSetBuilder2::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
			.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
			.build(descriptorSets3, descriptorLayout3, 4);

		Vulkan::Buffer dynamicBuffer;
		std::vector<UniformBufferInfo> bufferInfo;
		UniformBufferBuilder::begin(mDevice, mDeviceProperties, mMemoryProperties, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, true)
			.AddBuffer(sizeof(PassConstants), 2, gNumFrameResources)
			.AddBuffer(sizeof(ObjectConstants), mAllRitems.size(), gNumFrameResources)
			.AddBuffer(sizeof(MaterialConstants), mMaterials.size(), gNumFrameResources)
			.build(dynamicBuffer, bufferInfo);
		uniformBuffer = std::make_unique<UniformBuffer>(mDevice,dynamicBuffer, bufferInfo);

		std::vector<Vulkan::Texture> texturesList;
		TextureLoader::begin(mDevice, mCommandBuffer, mGraphicsQueue, mMemoryProperties)
			.addTexture("../../../Textures/bricks3.jpg")
			.addTexture("../../../Textures/checkboard.jpg")
			.addTexture("../../../Textures/ice.jpg")
			.addTexture("../../../Textures/white1x1.jpg")
			.load(texturesList);

		textures = std::make_unique<VulkanTextureList>(mDevice,texturesList);
		textureDescriptors = std::make_unique<VulkanDescriptorList>(mDevice,descriptorSets3);

		std::vector<VkDescriptorSet> descriptors{ descriptor0,descriptor1,descriptor2 };
		uniformDescriptors = std::make_unique<VulkanDescriptorList>(mDevice,descriptors);

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
		pipelineLayout = std::make_unique<VulkanPipelineLayout>(mDevice,layout);

		std::vector<Vulkan::ShaderModule> shaders;
		VkVertexInputBindingDescription vertexInputDescription = {};
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
		ShaderProgramLoader::begin(mDevice)
			.AddShaderPath("Shaders/default.vert.spv")
			.AddShaderPath("Shaders/default.frag.spv")
			.load(shaders, vertexInputDescription, vertexAttributeDescriptions);

		
	//Vulkan::PipelineInfo pipelineInfo;
	//pipelineInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	//pipelineInfo.polygonMode = VK_POLYGON_MODE_FILL;
	//pipelineInfo.depthTest = VK_TRUE;

	//opaquePipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);
		VkPipeline pipeline{ VK_NULL_HANDLE };
		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.build(pipeline);
		opaquePipeline = std::make_unique<VulkanPipeline>(mDevice,pipeline);

		//pipelineInfo.polygonMode = VK_POLYGON_MODE_LINE;
	//wireframePipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);
	// PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_LINE)
			.setDepthTest(VK_TRUE)
			.build(pipeline);
		wireframePipeline = std::make_unique<VulkanPipeline>(mDevice,pipeline);
	//pipelineInfo.polygonMode = VK_POLYGON_MODE_FILL;
	//pipelineInfo.blend = VK_TRUE;
	//transparentPipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);
		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setBlend(VK_TRUE)			
			.build(pipeline);
		transparentPipeline = std::make_unique<VulkanPipeline>(mDevice,pipeline);

		//pipelineInfo.stencilTest = VK_TRUE;
	//pipelineInfo.stencil.failOp = VK_STENCIL_OP_REPLACE;
	//pipelineInfo.stencil.depthFailOp = VK_STENCIL_OP_REPLACE;
	//pipelineInfo.stencil.passOp = VK_STENCIL_OP_REPLACE;
	//pipelineInfo.stencil.compareOp = VK_COMPARE_OP_ALWAYS;
	//pipelineInfo.stencil.writeMask = 0xFF;
	//pipelineInfo.stencil.compareMask = 0xFF;
	//pipelineInfo.stencil.reference = 0xFF;
	//markStencilMirrorsPipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);
		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setBlend(VK_TRUE)			
			.setStencilTest(VK_TRUE)				
			.setStencilState(VK_STENCIL_OP_REPLACE,VK_STENCIL_OP_REPLACE,VK_STENCIL_OP_REPLACE,VK_COMPARE_OP_ALWAYS,0xFF,0xFF,0xFF)	
			.setNoDraw(VK_TRUE)
			.build(pipeline);
		markStencilMirrorsPipeline = std::make_unique<VulkanPipeline>(mDevice,pipeline);

		//pipelineInfo.stencilTest = VK_TRUE;
	//pipelineInfo.stencil.failOp = VK_STENCIL_OP_KEEP;
	//pipelineInfo.stencil.depthFailOp = VK_STENCIL_OP_REPLACE;
	//pipelineInfo.stencil.passOp = VK_STENCIL_OP_REPLACE;
	//pipelineInfo.stencil.compareOp = VK_COMPARE_OP_EQUAL;
	//pipelineInfo.stencil.writeMask = 0xFF;
	//pipelineInfo.stencil.compareMask = 0xFF;
	//pipelineInfo.stencil.reference = 0xFF;
	//drawStencilReflectionsPipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);
		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_BACK_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)			
			.setDepthTest(VK_TRUE)
			.setStencilTest(VK_TRUE)
			.setStencilState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_REPLACE, VK_COMPARE_OP_EQUAL, 0xFF, 0xFF, 0xFF)
			//.setBlend(VK_TRUE)
			.build(pipeline);
		drawStencilReflectionsPipeline = std::make_unique<VulkanPipeline>(mDevice,pipeline);

		//pipelineInfo.stencilTest = VK_TRUE;
	//pipelineInfo.stencil.failOp = VK_STENCIL_OP_KEEP;
	//pipelineInfo.stencil.depthFailOp = VK_STENCIL_OP_KEEP;
	//pipelineInfo.stencil.passOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
	//pipelineInfo.stencil.compareOp = VK_COMPARE_OP_EQUAL;
	//pipelineInfo.stencil.writeMask = 0xFF;
	//pipelineInfo.stencil.compareMask = 0xFF;
	//pipelineInfo.stencil.reference = 0x00;
	//shadowPipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);
		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setStencilTest(VK_TRUE)
			.setStencilState(VK_STENCIL_OP_KEEP, VK_STENCIL_OP_INCREMENT_AND_WRAP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xFF, 0xFF, 0x00)
			.setBlend(VK_TRUE)
			.build(pipeline);
		shadowPipeline = std::make_unique<VulkanPipeline>(mDevice,pipeline);


		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
		/*VkPipeline pipeline{ VK_NULL_HANDLE };
		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.build(pipeline);*/

		/*VkDescriptorBufferInfo bufferInfo{};
		VkDescriptorBufferInfo bufferInfo2{};
		VkDescriptorBufferInfo bufferInfo3{};
		VkDescriptorImageInfo imageInfo{};
		std::vector<VkDescriptorSet> sets;
		std::vector<VkDescriptorSetLayout> layouts;
		DescriptorSetBuilder::begin(&poolCache, &layoutCache)
			.AddBinding(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT,&bufferInfo)
			.AddBinding(1,0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,VK_SHADER_STAGE_VERTEX_BIT,&bufferInfo2)
			.AddBinding(2,0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,VK_SHADER_STAGE_VERTEX_BIT,&bufferInfo3)
			.AddBinding(3,0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT,&imageInfo)
			.build(sets, layouts);*/
			/*Vulkan::Texture textures[] = { *mTextures["bricksTqeex"].get(),*mTextures["checkboardTex"].get(),*mTextures["iceTex"].get(),*mTextures["white1x1Tex"].get() };
			VulkContext vulkContext{ mDevice,mDeviceProperties,mMemoryProperties,mCommandBuffer,mGraphicsQueue };
			ShaderResourceCache shaderRes(vulkContext, &poolCache, &layoutCache);
			shaderRes.AddResource(0, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT, sizeof(PassConstants), 1, gNumFrameResources)
				.AddResource(1, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT, sizeof(ObjectConstants), mAllRitems.size(), gNumFrameResources)
				.AddResource(2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT, sizeof(MaterialConstants), mMaterials.size(), gNumFrameResources)
				.AddResource(3, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, textures, 4)
				.build();
		}*/
	}
	/*{
		DescriptorLayoutCache cache;
		DescriptorAllocator allocator;
		allocator.init(mDevice);
		cache.init(mDevice);
		VkDescriptorBufferInfo buffer{};
		VkDescriptorSet descriptorSet;
		DescriptorBuilder::begin(&cache, &allocator).bind_buffer(0, &buffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT).build(descriptorSet);
	}*/

	//VulkanContext vulkanContext{ mDevice,mDeviceProperties,mMemoryProperties,mCommandBuffer,mGraphicsQueue };
	//pipelineRes = std::make_unique<ShaderResources>(vulkanContext);
	//Vulkan::Texture textures[] = { *mTextures["bricksTex"].get(),*mTextures["checkboardTex"].get(),*mTextures["iceTex"].get(),*mTextures["white1x1Tex"].get()};
	//std::vector<std::vector<ShaderResource>> pipelineResourceInfos{
	//	{
	//		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PassConstants),1,gNumFrameResources,true},
	//	},
	//	{
	//		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,VK_SHADER_STAGE_VERTEX_BIT,sizeof(ObjectConstants),mAllRitems.size(),gNumFrameResources,true},
	//	},
	//	{
	//		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,sizeof(MaterialConstants),mMaterials.size(),gNumFrameResources,true},
	//	},
	//	{
	//		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT,textures,4},
	//	}
	//};
	//pipelineRes->AddShaderResources(pipelineResourceInfos/*, gNumFrameResources*/);

	//prog = std::make_unique<ShaderProgram>(mDevice);
	//std::vector<const char*> shaderPaths = { "Shaders/default.vert.spv","Shaders/default.frag.spv" };
	//prog->load(shaderPaths);

	//pipelineLayout = std::make_unique<PipelineLayout>(mDevice, *pipelineRes);
	//Vulkan::PipelineInfo pipelineInfo;
	//pipelineInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	//pipelineInfo.polygonMode = VK_POLYGON_MODE_FILL;
	//pipelineInfo.depthTest = VK_TRUE;

	//opaquePipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);

	//pipelineInfo.polygonMode = VK_POLYGON_MODE_LINE;
	//wireframePipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);
	//pipelineInfo.polygonMode = VK_POLYGON_MODE_FILL;
	//pipelineInfo.blend = VK_TRUE;
	//transparentPipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);

	//pipelineInfo.stencilTest = VK_TRUE;
	//pipelineInfo.stencil.failOp = VK_STENCIL_OP_REPLACE;
	//pipelineInfo.stencil.depthFailOp = VK_STENCIL_OP_REPLACE;
	//pipelineInfo.stencil.passOp = VK_STENCIL_OP_REPLACE;
	//pipelineInfo.stencil.compareOp = VK_COMPARE_OP_ALWAYS;
	//pipelineInfo.stencil.writeMask = 0xFF;
	//pipelineInfo.stencil.compareMask = 0xFF;
	//pipelineInfo.stencil.reference = 0xFF;
	//markStencilMirrorsPipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);

	//pipelineInfo.stencilTest = VK_TRUE;
	//pipelineInfo.stencil.failOp = VK_STENCIL_OP_KEEP;
	//pipelineInfo.stencil.depthFailOp = VK_STENCIL_OP_REPLACE;
	//pipelineInfo.stencil.passOp = VK_STENCIL_OP_REPLACE;
	//pipelineInfo.stencil.compareOp = VK_COMPARE_OP_EQUAL;
	//pipelineInfo.stencil.writeMask = 0xFF;
	//pipelineInfo.stencil.compareMask = 0xFF;
	//pipelineInfo.stencil.reference = 0xFF;
	//drawStencilReflectionsPipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);



	//pipelineInfo.stencilTest = VK_TRUE;
	//pipelineInfo.stencil.failOp = VK_STENCIL_OP_KEEP;
	//pipelineInfo.stencil.depthFailOp = VK_STENCIL_OP_KEEP;
	//pipelineInfo.stencil.passOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
	//pipelineInfo.stencil.compareOp = VK_COMPARE_OP_EQUAL;
	//pipelineInfo.stencil.writeMask = 0xFF;
	//pipelineInfo.stencil.compareMask = 0xFF;
	//pipelineInfo.stencil.reference = 0x00;
	//shadowPipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);

	mPSOs["opaque"] = *opaquePipeline;
	mPSOs["opaque_wireframe"] = *wireframePipeline;
	mPSOs["markStencilMirrors"] = *markStencilMirrorsPipeline;
	mPSOs["drawStencilReflections"] = *drawStencilReflectionsPipeline;
	mPSOs["shadow"] = *shadowPipeline;
	mPSOs["transparent"] = *transparentPipeline;
}


void StencilApp::BuildFrameResources() {
	/*void* pPassCB = pipelineRes->GetShaderResource(0).buffer.ptr;
	VkDeviceSize passSize = pipelineRes->GetShaderResource(0).buffer.objectSize;
	void* pObjectCB = pipelineRes->GetShaderResource(1).buffer.ptr;
	VkDeviceSize objectSize = pipelineRes->GetShaderResource(1).buffer.objectSize;
	void* pMatCB = pipelineRes->GetShaderResource(2).buffer.ptr;
	VkDeviceSize matSize = pipelineRes->GetShaderResource(2).buffer.objectSize;*/
	
	for (int i = 0; i < gNumFrameResources; i++) {
		auto& ub = *uniformBuffer;
		
		PassConstants* pc = (PassConstants*)((uint8_t*)ub[0].ptr+ub[0].objectSize*ub[0].objectCount*i);// ((uint8_t*)pPassCB + passSize * i);
		ObjectConstants* oc = (ObjectConstants*)((uint8_t*)ub[1].ptr+ub[1].objectSize*ub[1].objectCount*i);// ((uint8_t*)pObjectCB + objectSize * mAllRitems.size() * i);
		MaterialConstants* mc = (MaterialConstants*)((uint8_t*)ub[2].ptr+ub[2].objectSize*ub[2].objectCount*i);// ((uint8_t*)pMatCB + matSize * mMaterials.size() * i);
		
		mFrameResources.push_back(std::make_unique<FrameResource>(pc, oc, mc));
	}
}


void StencilApp::BuildMaterials()
{
	auto bricks = std::make_unique<Material>();
	bricks->NumFramesDirty = gNumFrameResources;
	bricks->Name = "bricks";
	bricks->MatCBIndex = 0;
	bricks->DiffuseSrvHeapIndex = 0;
	bricks->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks->FresnelR0 = glm::vec3(0.05f, 0.05f, 0.05f);
	bricks->Roughness = 0.25f;

	auto checkertile = std::make_unique<Material>();
	checkertile->NumFramesDirty = gNumFrameResources;
	checkertile->Name = "checkertile";
	checkertile->MatCBIndex = 1;
	checkertile->DiffuseSrvHeapIndex = 1;
	checkertile->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	checkertile->FresnelR0 = glm::vec3(0.07f, 0.07f, 0.07f);
	checkertile->Roughness = 0.3f;

	auto icemirror = std::make_unique<Material>();
	icemirror->NumFramesDirty = gNumFrameResources;
	icemirror->Name = "icemirror";
	icemirror->MatCBIndex = 2;
	icemirror->DiffuseSrvHeapIndex = 2;
	icemirror->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 0.3f);
	icemirror->FresnelR0 = glm::vec3(0.1f, 0.1f, 0.1f);
	icemirror->Roughness = 0.5f;

	auto skullMat = std::make_unique<Material>();
	skullMat->NumFramesDirty = gNumFrameResources;
	skullMat->Name = "skullMat";
	skullMat->MatCBIndex = 3;
	skullMat->DiffuseSrvHeapIndex = 3;
	skullMat->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	skullMat->FresnelR0 = glm::vec3(0.05f, 0.05f, 0.05f);
	skullMat->Roughness = 0.3f;

	auto shadowMat = std::make_unique<Material>();
	shadowMat->NumFramesDirty = gNumFrameResources;
	shadowMat->Name = "shadowMat";
	shadowMat->MatCBIndex = 4;
	shadowMat->DiffuseSrvHeapIndex = 3;
	shadowMat->DiffuseAlbedo = glm::vec4(0.0f, 0.0f, 0.0f, 0.5f);
	shadowMat->FresnelR0 = glm::vec3(0.001f, 0.001f, 0.001f);
	shadowMat->Roughness = 0.0f;

	mMaterials["bricks"] = std::move(bricks);
	mMaterials["checkertile"] = std::move(checkertile);
	mMaterials["icemirror"] = std::move(icemirror);
	mMaterials["skullMat"] = std::move(skullMat);
	mMaterials["shadowMat"] = std::move(shadowMat);
}


void StencilApp::BuildRenderItems()
{
	auto floorRitem = std::make_unique<RenderItem>();
	floorRitem->World = MathHelper::Identity4x4();
	floorRitem->TexTransform = MathHelper::Identity4x4();
	floorRitem->ObjCBIndex = 0;
	floorRitem->Mat = mMaterials["checkertile"].get();
	floorRitem->Geo = mGeometries["roomGeo"].get();
	//floorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	floorRitem->IndexCount = floorRitem->Geo->DrawArgs["floor"].IndexCount;
	floorRitem->StartIndexLocation = floorRitem->Geo->DrawArgs["floor"].StartIndexLocation;
	floorRitem->BaseVertexLocation = floorRitem->Geo->DrawArgs["floor"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(floorRitem.get());

	auto wallsRitem = std::make_unique<RenderItem>();
	wallsRitem->World = MathHelper::Identity4x4();
	wallsRitem->TexTransform = MathHelper::Identity4x4();
	wallsRitem->ObjCBIndex = 1;
	wallsRitem->Mat = mMaterials["bricks"].get();
	wallsRitem->Geo = mGeometries["roomGeo"].get();
	//wallsRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wallsRitem->IndexCount = wallsRitem->Geo->DrawArgs["wall"].IndexCount;
	wallsRitem->StartIndexLocation = wallsRitem->Geo->DrawArgs["wall"].StartIndexLocation;
	wallsRitem->BaseVertexLocation = wallsRitem->Geo->DrawArgs["wall"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(wallsRitem.get());

	auto skullRitem = std::make_unique<RenderItem>();
	skullRitem->World = MathHelper::Identity4x4();
	skullRitem->TexTransform = MathHelper::Identity4x4();
	skullRitem->ObjCBIndex = 2;
	skullRitem->Mat = mMaterials["skullMat"].get();
	skullRitem->Geo = mGeometries["skullGeo"].get();
	//skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
	skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
	skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;
	mSkullRitem = skullRitem.get();
	mRitemLayer[(int)RenderLayer::Opaque].push_back(skullRitem.get());

	// Reflected skull will have different world matrix, so it needs to be its own render item.
	auto reflectedSkullRitem = std::make_unique<RenderItem>();
	*reflectedSkullRitem = *skullRitem;
	reflectedSkullRitem->ObjCBIndex = 3;
	mReflectedSkullRitem = reflectedSkullRitem.get();
	mRitemLayer[(int)RenderLayer::Reflected].push_back(reflectedSkullRitem.get());

	// Shadowed skull will have different world matrix, so it needs to be its own render item.
	auto shadowedSkullRitem = std::make_unique<RenderItem>();
	*shadowedSkullRitem = *skullRitem;
	shadowedSkullRitem->ObjCBIndex = 4;
	shadowedSkullRitem->Mat = mMaterials["shadowMat"].get();
	mShadowedSkullRitem = shadowedSkullRitem.get();
	mRitemLayer[(int)RenderLayer::Shadow].push_back(shadowedSkullRitem.get());

	auto mirrorRitem = std::make_unique<RenderItem>();
	mirrorRitem->World = MathHelper::Identity4x4();
	mirrorRitem->TexTransform = MathHelper::Identity4x4();
	mirrorRitem->ObjCBIndex = 5;
	mirrorRitem->Mat = mMaterials["icemirror"].get();
	mirrorRitem->Geo = mGeometries["roomGeo"].get();
	//mirrorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	mirrorRitem->IndexCount = mirrorRitem->Geo->DrawArgs["mirror"].IndexCount;
	mirrorRitem->StartIndexLocation = mirrorRitem->Geo->DrawArgs["mirror"].StartIndexLocation;
	mirrorRitem->BaseVertexLocation = mirrorRitem->Geo->DrawArgs["mirror"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Mirrors].push_back(mirrorRitem.get());
	mRitemLayer[(int)RenderLayer::Transparent].push_back(mirrorRitem.get());

	mAllRitems.push_back(std::move(floorRitem));
	mAllRitems.push_back(std::move(wallsRitem));
	mAllRitems.push_back(std::move(skullRitem));
	mAllRitems.push_back(std::move(reflectedSkullRitem));
	mAllRitems.push_back(std::move(shadowedSkullRitem));
	mAllRitems.push_back(std::move(mirrorRitem));
}


void StencilApp::LoadTextures()
{
	//auto bricksTex = std::make_unique<Texture>();
	//bricksTex->Name = "bricksTex";
	//bricksTex->FileName = "../../../Textures/bricks3.jpg";
	//loadTexture(mDevice, mCommandBuffer, mGraphicsQueue, mMemoryProperties, bricksTex->FileName.c_str(), *bricksTex);


	//auto checkboardTex = std::make_unique<Texture>();
	//checkboardTex->Name = "checkboardTex";
	//checkboardTex->FileName = "../../../Textures/checkboard.jpg";
	//loadTexture(mDevice, mCommandBuffer, mGraphicsQueue, mMemoryProperties, checkboardTex->FileName.c_str(), *checkboardTex);

	//auto iceTex = std::make_unique<Texture>();
	//iceTex->Name = "iceTex";
	//iceTex->FileName = "../../../Textures/ice.jpg";
	//loadTexture(mDevice, mCommandBuffer, mGraphicsQueue, mMemoryProperties, iceTex->FileName.c_str(), *iceTex);

	//auto white1x1Tex = std::make_unique<Texture>();
	//white1x1Tex->Name = "white1x1Tex";
	//white1x1Tex->FileName = "../../../Textures/white1x1.jpg";
	//loadTexture(mDevice, mCommandBuffer, mGraphicsQueue, mMemoryProperties, white1x1Tex->FileName.c_str(), *white1x1Tex);

	//mTextures[bricksTex->Name] = std::move(bricksTex);
	//mTextures[checkboardTex->Name] = std::move(checkboardTex);
	//mTextures[iceTex->Name] = std::move(iceTex);
	//mTextures[white1x1Tex->Name] = std::move(white1x1Tex);
}

void StencilApp::Update(const GameTimer& gt) {
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
	UpdateReflectedPassCB(gt);

}

void StencilApp::UpdateCamera(const GameTimer& gt) {
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	mEyePos = glm::vec3(x, y, z);

	glm::vec3 pos = glm::vec3(x, y, z);
	glm::vec3 target = glm::vec3(0.0f);
	glm::vec3 up = glm::vec3(0.0, 1.0f, 0.0f);

	mView = glm::lookAtLH(pos, target, up);
}

void StencilApp::UpdateObjectCBs(const GameTimer& gt) {
	//auto currObjectCB = mCurrFrameResource->ObjectCB;
	uint8_t* pObjConsts = (uint8_t*)mCurrFrameResource->pOCs;
	//VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	//VkDeviceSize objSize = ((uint32_t)sizeof(ObjectConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	//VkDeviceSize objSize = pipelineRes->GetShaderResource(1).buffer.objectSize;
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
void StencilApp::UpdateMainPassCB(const GameTimer& gt) {
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

void StencilApp::UpdateMaterialsCBs(const GameTimer& gt) {
	//VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	//VkDeviceSize objSize = ((uint32_t)sizeof(MaterialConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	uint8_t* pMatConsts = (uint8_t*)mCurrFrameResource->pMats;
	//VkDeviceSize objSize = pipelineRes->GetShaderResource(2).buffer.objectSize;
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

			//memcpy(&mCurrFrameResource->pMats[mat->MatCBIndex] , &matConstants, sizeof(MaterialConstants));
			memcpy((pMatConsts + (objSize * mat->MatCBIndex)), &matConstants, sizeof(MaterialConstants));
			mat->NumFramesDirty--;

		}
	}
}


void StencilApp::UpdateReflectedPassCB(const GameTimer& gt)
{
	mReflectedPassCB = mMainPassCB;

	glm::vec4 mirrorPlane = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f); // xy plane
	glm::mat4 R = MathHelper::reflect(mirrorPlane);// XMMatrixReflect(mirrorPlane);

	// Reflect the lighting.
	for (int i = 0; i < 3; ++i)
	{
		glm::vec3 lightDir = mMainPassCB.Lights[i].Direction;
		glm::vec3 reflectedLightDir = R * glm::vec4(lightDir, 0.0f);// XMVector3TransformNormal(lightDir, R);
		mReflectedPassCB.Lights[i].Direction = reflectedLightDir;
	}
	PassConstants* pRPC = mCurrFrameResource->pPCs;
	//VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	//VkDeviceSize objSize = ((uint32_t)sizeof(PassConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	//VkDeviceSize objSize = pipelineRes->GetShaderResource(0).buffer.objectSize;
	auto& ub = *uniformBuffer;
	VkDeviceSize objSize = ub[0].objectSize;
	memcpy(((uint8_t*)pRPC) + objSize, &mReflectedPassCB, sizeof(PassConstants));
	// Reflected pass stored in index 1
	//auto currPassCB = mCurrFrameResource->PassCB.get();
	//currPassCB->CopyData(1, mReflectedPassCB);
}

void StencilApp::OnResize() {
	VulkApp::OnResize();
	mProj = glm::perspectiveFovLH_ZO(0.25f * pi, (float)mClientWidth, (float)mClientHeight, 1.0f, 1000.0f);
}

void StencilApp::OnKeyboardInput(const GameTimer& gt)
{
	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;

	if (GetAsyncKeyState('P') & 0x8000) {


		saveScreenCap(mDevice, mCommandBuffer, mGraphicsQueue, mSwapchainImages[mIndex], mFormatProperties, mSwapchainFormat.format, {(uint32_t)mClientWidth,(uint32_t)mClientHeight}, mFrameCount);
	}
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('A') & 0x8000)
		mSkullTranslation.x -= 1.0f * dt;

	if (GetAsyncKeyState('D') & 0x8000)
		mSkullTranslation.x += 1.0f * dt;

	if (GetAsyncKeyState('W') & 0x8000)
		mSkullTranslation.y += 1.0f * dt;

	if (GetAsyncKeyState('S') & 0x8000)
		mSkullTranslation.y -= 1.0f * dt;

	// Don't let user move below ground plane.
	mSkullTranslation.y = MathHelper::Max(mSkullTranslation.y, 0.0f);

	// Update the new world matrix.
	//XMMATRIX skullRotate = XMMatrixRotationY(0.5f * MathHelper::Pi);
	glm::mat4 skullRotate = glm::rotate(glm::mat4(1.0f), 0.5f * pi, glm::vec3(0.0f, 1.0f, 0.0f));
	//XMMATRIX skullScale = XMMatrixScaling(0.45f, 0.45f, 0.45f);
	glm::mat4 skullScale = glm::scale(glm::mat4(1.0f), glm::vec3(0.45f, 0.45f, 0.45f));
	//XMMATRIX skullOffset = XMMatrixTranslation(mSkullTranslation.x, mSkullTranslation.y, mSkullTranslation.z);
	glm::mat4 skullOffset = glm::translate(glm::mat4(1.0f), mSkullTranslation);
	//XMMATRIX skullWorld = skullRotate * skullScale * skullOffset;
	glm::mat4 skullWorld = skullOffset * skullScale * skullRotate;
	//XMStoreFloat4x4(&mSkullRitem->World, skullWorld);
	mSkullRitem->World = skullWorld;

	// Update reflection world matrix.
	//XMVECTOR mirrorPlane = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // xy plane
	glm::vec4 mirrorPlane = glm::vec4(0.0f, 0.0f, 1.0f, 0.0f);
	//XMMATRIX R = XMMatrixReflect(mirrorPlane);
	glm::mat4 R = MathHelper::reflect(mirrorPlane);
	//XMStoreFloat4x4(&mReflectedSkullRitem->World, skullWorld * R);
	mReflectedSkullRitem->World = R * skullWorld;

	// Update shadow world matrix.
	//XMVECTOR shadowPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f); // xz plane
	glm::vec4 shadowPlane = glm::vec4(0.0f, 1.0f, 0.0f, 0.0f);
	//XMVECTOR toMainLight = -XMLoadFloat3(&mMainPassCB.Lights[0].Direction);
	glm::vec3 toMainLight = -mMainPassCB.Lights[0].Direction;
	//XMMATRIX S = XMMatrixShadow(shadowPlane, toMainLight);
	glm::mat4 S = MathHelper::shadowMatrix(glm::vec4(toMainLight, 0.0f), shadowPlane);

	//XMMATRIX shadowOffsetY = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
	glm::mat4 shadowOffsetY = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.001f, 0.0f));
	//XMStoreFloat4x4(&mShadowedSkullRitem->World, skullWorld * S * shadowOffsetY);
	mShadowedSkullRitem->World = shadowOffsetY * S * skullWorld;//Order?
	mSkullRitem->NumFramesDirty = gNumFrameResources;
	mReflectedSkullRitem->NumFramesDirty = gNumFrameResources;
	mShadowedSkullRitem->NumFramesDirty = gNumFrameResources;
}

void StencilApp::AnimateMaterials(const GameTimer& gt) {

}

void StencilApp::OnMouseDown(WPARAM btnState, int x, int y) {
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void StencilApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void StencilApp::OnMouseMove(WPARAM btnState, int x, int y) {
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

void StencilApp::Draw(const GameTimer& gt) {
	uint32_t index = 0;
	VkCommandBuffer cmd{ VK_NULL_HANDLE };

	cmd = BeginRender();

	VkViewport viewport = { 0.0f,0.0f,(float)mClientWidth,(float)mClientHeight,0.0f,1.0f };
	pvkCmdSetViewport(cmd, 0, 1, &viewport);
	VkRect2D scissor = { {0,0},{(uint32_t)mClientWidth,(uint32_t)mClientHeight} };
	pvkCmdSetScissor(cmd, 0, 1, &scissor);

	//VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	//VkDeviceSize passSize = ((uint32_t)sizeof(PassConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	//uint32_t dynamicOffsets[1] = { 0 };
	//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsPC[mIndex], 1, dynamicOffsets);//bind PC data once
	//uint32_t dynamicOffsets2[1] = { (uint32_t)passSize };
	auto& ub = *uniformBuffer;
	VkDeviceSize passSize = ub[0].objectSize;
	uint32_t dynamicOffsets[1] = { (uint32_t)( mCurrFrame*2 * passSize)};
	//VkDescriptorSet descriptor = pipelineRes->GetDescriptorSet(0, mCurrFrame);
	auto& ud = *uniformDescriptors;
	VkDescriptorSet descriptor = ud[0];
	pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor, 1, dynamicOffsets);//bind PC data once
	//VkDeviceSize passSize = pipelineRes->GetShaderResource(0).buffer.objectSize;
	
	
	uint32_t dynamicOffsets2[1] = { (uint32_t)(mCurrFrame * 2 * passSize + passSize )};

	if (mIsWireframe) {
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque_wireframe"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Opaque]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Mirrors]);
		//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsPC[mIndex], 1, dynamicOffsets2);//bind PC data once
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor, 1, dynamicOffsets2);//bind PC data once
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Reflected]);
		//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsPC[mIndex], 1, dynamicOffsets);//bind PC data once
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor, 1, dynamicOffsets);//bind PC data once
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Transparent]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Shadow]);
	}
	else {

		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Opaque]);
		
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["markStencilMirrors"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Mirrors]);

		
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor, 1, dynamicOffsets2);//bind PC data once
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["drawStencilReflections"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Reflected]);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor, 1, dynamicOffsets);//bind PC data once
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["transparent"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Transparent]);
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["shadow"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Shadow]);
	}






	EndRender(cmd);


}

void StencilApp::DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems) {
	//	VkDeviceSize obSize = mCurrFrameResource->ObjectCBSize;
	//VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	//VkDeviceSize objSize = ((uint32_t)sizeof(ObjectConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	//VkDeviceSize matSize = ((uint32_t)sizeof(Material) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	uint32_t dynamicOffsets[1] = { 0 };
	//uint32_t count = getSwapchainImageCount(mSurfaceCaps);
	//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsPC[mIndex], 1, dynamicOffsets);//bind PC data once
	//VkDeviceSize objectSize = pipelineRes->GetShaderResource(1).buffer.objectSize;
	auto& ub = *uniformBuffer;
	VkDeviceSize objectSize = ub[1].objectSize;
	//VkDeviceSize matSize = pipelineRes->GetShaderResource(2).buffer.objectSize;
	VkDeviceSize matSize = ub[2].objectSize;
	//VkDescriptorSet descriptor2 = pipelineRes->GetDescriptorSet(1, mCurrFrame);
	//VkDescriptorSet descriptor3 = pipelineRes->GetDescriptorSet(2, mCurrFrame);
	//VkDescriptorSet descriptor4 = pipelineRes->GetDescriptorSet(3, mCurrFrame);
	auto& ud = *uniformDescriptors;
	auto& td = *textureDescriptors;
	VkDescriptorSet descriptor1 = ud[1];
	VkDescriptorSet descriptor2 = ud[2];
	VkDescriptorSet descriptors[2] = { descriptor1,descriptor2 };
	for (size_t i = 0; i < ritems.size(); i++) {
		auto ri = ritems[i];
		uint32_t indexOffset = ri->StartIndexLocation;
		
		//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 3, 1, &mDescriptorSetsTextures[ri->Mat->DiffuseSrvHeapIndex], 0, 0);//bin texture descriptor
		const auto vbv = ri->Geo->vertexBufferGPU;
		pvkCmdBindVertexBuffers(cmd, 0, 1, &vbv.buffer, mOffsets);
		const auto ibv = ri->Geo->indexBufferGPU;
		pvkCmdBindIndexBuffer(cmd, ibv.buffer, indexOffset * sizeof(uint32_t), VK_INDEX_TYPE_UINT32);
		uint32_t cbvIndex = ri->ObjCBIndex;
		//uint32_t dynamicOffsets[2] = { 0,cbvIndex *objSize};

		//dynamicOffsets[0] = (uint32_t)(cbvIndex * objSize);
		//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsOBs[mIndex], 2, dynamicOffsets);
		//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 1, 1, &mDescriptorSetsOBs[mIndex], 1, dynamicOffsets);
		//dynamicOffsets[0] = (uint32_t)(ri->Mat->MatCBIndex * matSize);
		//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 2, 1, &mDescriptorSetsMats[mIndex], 1, dynamicOffsets);
		uint32_t dyoffsets[2] = { (uint32_t)(cbvIndex * objectSize),(uint32_t)(ri->Mat->MatCBIndex * matSize) };
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 1, 2, descriptors, 2, dyoffsets);
		//bind texture

		//VkDescriptorSet descriptor4 = pipelineRes->GetDescriptorSet(3, ri->Mat->MatCBIndex);
		VkDescriptorSet descriptor3 = td[ri->Mat->DiffuseSrvHeapIndex];
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, nullptr);//bind PC data once
		pvkCmdDrawIndexed(cmd, ri->IndexCount, 1, 0, ri->BaseVertexLocation, 0);
	}
}

int main() {
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		StencilApp theApp(GetModuleHandle(NULL));
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

