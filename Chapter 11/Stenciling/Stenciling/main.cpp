#include "../../../Common/VulkApp.h"
#include "../../../Common/VulkUtil.h"
#include "../../../Common/GeometryGenerator.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/Colors.h"
#include "../../../Common/TextureLoader.h"
#include "FrameResource.h"

#include <array>
#include <memory>
#include <fstream>
#include <iostream>
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
	Mirrors,
	Reflected,
	Transparent,
	Shadow,
	Count
};

const float pi = 3.14159265358979323846264338327950288f;

class StencilApp : public VulkApp {
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	VkDescriptorSetLayout	mDescriptorSetLayoutPC{ VK_NULL_HANDLE };
	VkDescriptorSetLayout	mDescriptorSetLayoutOBs{ VK_NULL_HANDLE };
	VkDescriptorSetLayout  mDescriptorSetLayoutMats{ VK_NULL_HANDLE };
	VkDescriptorSetLayout mDescriptorSetLayoutTextures{ VK_NULL_HANDLE };
	VkDescriptorPool		mDescriptorPool{ VK_NULL_HANDLE };
	VkDescriptorPool		mDescriptorPoolTexture{ VK_NULL_HANDLE };
	std::vector<VkDescriptorSet> mDescriptorSetsPC;
	std::vector<VkDescriptorSet> mDescriptorSetsOBs;
	std::vector<VkDescriptorSet> mDescriptorSetsMats;
	std::vector<VkDescriptorSet> mDescriptorSetsTextures;
	VkPipelineLayout		mPipelineLayout{ VK_NULL_HANDLE };

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
	void BuildRootSignature();
	void BuildDescriptorHeaps();
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
		cleanupBuffer(mDevice, pair.second->vertexBufferGPU);
		

		cleanupBuffer(mDevice, pair.second->indexBufferGPU);

	}
	for (auto& pair : mPSOs) {
		VkPipeline pipeline = pair.second;
		cleanupPipeline(mDevice, pipeline);
	}
	for (auto& pair : mTextures) {
		cleanupImage(mDevice, *pair.second);
	}
	cleanupPipelineLayout(mDevice, mPipelineLayout);
	cleanupDescriptorPool(mDevice, mDescriptorPoolTexture);
	cleanupDescriptorPool(mDevice, mDescriptorPool);
	cleanupDescriptorSetLayout(mDevice, mDescriptorSetLayoutTextures);
	cleanupDescriptorSetLayout(mDevice, mDescriptorSetLayoutMats);
	cleanupDescriptorSetLayout(mDevice, mDescriptorSetLayoutOBs);
	cleanupDescriptorSetLayout(mDevice, mDescriptorSetLayoutPC);
}


bool StencilApp::Initialize() {
	if (!VulkApp::Initialize())
		return false;
	LoadTextures();
	BuildDescriptorHeaps();
	BuildRoomGeometry();	
	BuildSkullGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildRootSignature();

	BuildFrameResources();
	BuildPSOs();

	return true;
}

void StencilApp::BuildDescriptorHeaps() {
	std::vector<VkDescriptorSetLayoutBinding> bindings = {
		{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,1,VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}, //Binding 0, uniform (constant) buffer

	};
	mDescriptorSetLayoutPC = initDescriptorSetLayout(mDevice, bindings);
	mDescriptorSetLayoutOBs = initDescriptorSetLayout(mDevice, bindings);
	mDescriptorSetLayoutMats = initDescriptorSetLayout(mDevice, bindings);

	std::vector<VkDescriptorPoolSize> poolSizes{
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,9},
	};
	mDescriptorPool = initDescriptorPool(mDevice, poolSizes, 9);
	uint32_t count = getSwapchainImageCount(mSurfaceCaps);
	mDescriptorSetsPC.resize(count);
	initDescriptorSets(mDevice, mDescriptorSetLayoutPC, mDescriptorPool, mDescriptorSetsPC.data(), count);
	mDescriptorSetsOBs.resize(count);
	initDescriptorSets(mDevice, mDescriptorSetLayoutOBs, mDescriptorPool, mDescriptorSetsOBs.data(), count);
	mDescriptorSetsMats.resize(count);
	initDescriptorSets(mDevice, mDescriptorSetLayoutOBs, mDescriptorPool, mDescriptorSetsMats.data(), count);

	bindings = {
		{0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_FRAGMENT_BIT,nullptr}
	};
	uint32_t texCount = 4;
	mDescriptorSetLayoutTextures = initDescriptorSetLayout(mDevice, bindings);
	poolSizes = {
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,texCount},
	};
	mDescriptorPoolTexture = initDescriptorPool(mDevice, poolSizes, texCount + 1);
	mDescriptorSetsTextures.resize(texCount);
	initDescriptorSets(mDevice, mDescriptorSetLayoutTextures, mDescriptorPoolTexture, mDescriptorSetsTextures.data(), texCount);


	std::vector<VkDescriptorImageInfo> imageInfos(texCount);

	std::vector<VkWriteDescriptorSet> descriptorWrites;
	Texture* tex{ nullptr };
	for (uint32_t i = 0; i < texCount; i++) {
		imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		switch (i) {
		case 0:
			tex = mTextures["bricksTex"].get();
			break;
		case 1:
			tex = mTextures["checkboardTex"].get();
			break;
		case 2:
			tex = mTextures["iceTex"].get();
			break;
		case 3:
			tex = mTextures["white1x1Tex"].get();
			break;
		}
		if (tex != nullptr) {
			imageInfos[i].imageView = tex->imageView;
			imageInfos[i].sampler = tex->sampler;

			descriptorWrites.push_back(
				{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,mDescriptorSetsTextures[i],0,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&imageInfos[i],nullptr,nullptr });

		}


	}
	updateDescriptorSets(mDevice, descriptorWrites);
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
	floorSubmesh.indexCount = 6;
	floorSubmesh.startIndexLocation = 0;
	floorSubmesh.baseVertexLocation = 0;

	SubmeshGeometry wallSubmesh;
	wallSubmesh.indexCount = 18;
	wallSubmesh.startIndexLocation = 6;
	wallSubmesh.baseVertexLocation = 0;

	SubmeshGeometry mirrorSubmesh;
	mirrorSubmesh.indexCount = 6;
	mirrorSubmesh.startIndexLocation = 24;
	mirrorSubmesh.baseVertexLocation = 0;

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "roomGeo";

	geo->vertexBufferCPU = malloc(vbByteSize);
	memcpy(geo->vertexBufferCPU, vertices.data(), vbByteSize);

	geo->indexBufferCPU = malloc(ibByteSize);
	memcpy(geo->indexBufferCPU, indices.data(), ibByteSize);

	initBuffer(mDevice, mMemoryProperties, vbByteSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, geo->vertexBufferGPU);
	initBuffer(mDevice, mMemoryProperties, ibByteSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, geo->indexBufferGPU);

	VkDeviceSize maxSize = std::max(vbByteSize, ibByteSize);
	Buffer stagingBuffer;
	initBuffer(mDevice, mMemoryProperties, maxSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer);
	void* ptr = mapBuffer(mDevice, stagingBuffer);
	//copy vertex data
	memcpy(ptr, vertices.data(), vbByteSize);
	CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->vertexBufferGPU, vbByteSize);
	memcpy(ptr, indices.data(), ibByteSize);
	CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->indexBufferGPU, ibByteSize);
	unmapBuffer(mDevice, stagingBuffer);
	cleanupBuffer(mDevice, stagingBuffer);

	/*ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);*/

	geo->vertexByteStride = sizeof(Vertex);
	geo->vertexBufferByteSize = vbByteSize;	
	geo->indexBufferByteSize = ibByteSize;

	geo->DrawArgs["floor"] = floorSubmesh;
	geo->DrawArgs["wall"] = wallSubmesh;
	geo->DrawArgs["mirror"] = mirrorSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void StencilApp::BuildSkullGeometry()
{
	std::ifstream fin("Models/skull.txt");

	if (!fin)
	{
		MessageBox(0, L"Models/skull.txt not found.", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;

	std::vector<Vertex> vertices(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

		// Model does not have texture coordinates, so just zero them out.
		vertices[i].TexC = { 0.0f, 0.0f };
	}

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::int32_t> indices(3 * tcount);
	for (UINT i = 0; i < tcount; ++i)
	{
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	fin.close();

	//
	// Pack the indices of all the meshes into one index buffer.
	//

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "skullGeo";

	geo->vertexBufferCPU = malloc(vbByteSize);
	memcpy(geo->vertexBufferCPU, vertices.data(), vbByteSize);

	geo->indexBufferCPU = malloc(ibByteSize);
	memcpy(geo->indexBufferCPU, indices.data(), ibByteSize);

	initBuffer(mDevice, mMemoryProperties, vbByteSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, geo->vertexBufferGPU);
	initBuffer(mDevice, mMemoryProperties, ibByteSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, geo->indexBufferGPU);

	VkDeviceSize maxSize = std::max(vbByteSize, ibByteSize);
	Buffer stagingBuffer;
	initBuffer(mDevice, mMemoryProperties, maxSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer);
	void* ptr = mapBuffer(mDevice, stagingBuffer);
	//copy vertex data
	memcpy(ptr, vertices.data(), vbByteSize);
	CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->vertexBufferGPU, vbByteSize);
	memcpy(ptr, indices.data(), ibByteSize);
	CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->indexBufferGPU, ibByteSize);
	unmapBuffer(mDevice, stagingBuffer);
	cleanupBuffer(mDevice, stagingBuffer);


	//ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	//CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	//ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	//CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	//geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
	//	mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	//geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
	//	mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->vertexByteStride = sizeof(Vertex);
	geo->vertexBufferByteSize = vbByteSize;
	//geo->indexFormat = DXGI_FORMAT_R32_UINT;
	geo->indexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.indexCount = (UINT)indices.size();
	submesh.startIndexLocation = 0;
	submesh.baseVertexLocation = 0;

	geo->DrawArgs["skull"] = submesh;

	mGeometries[geo->Name] = std::move(geo);
}

void StencilApp::BuildPSOs() {
	{
		std::vector<ShaderModule> shaders = {
				{initShaderModule(mDevice,"shaders/default.vert.spv"),VK_SHADER_STAGE_VERTEX_BIT},
				{initShaderModule(mDevice,"shaders/default.frag.spv"),VK_SHADER_STAGE_FRAGMENT_BIT}
		};
		VkPipeline opaquePipeline = initGraphicsPipeline(mDevice, mRenderPass, mPipelineLayout, shaders, Vertex::getInputBindingDescription(), Vertex::getInputAttributeDescription(), VK_CULL_MODE_FRONT_BIT, true, mMSAA ? mNumSamples : VK_SAMPLE_COUNT_1_BIT, VK_FALSE, VK_POLYGON_MODE_FILL);
		VkPipeline wireframePipeline = initGraphicsPipeline(mDevice, mRenderPass, mPipelineLayout, shaders, Vertex::getInputBindingDescription(), Vertex::getInputAttributeDescription(), VK_CULL_MODE_FRONT_BIT, true, mMSAA ? mNumSamples : VK_SAMPLE_COUNT_1_BIT, VK_FALSE, VK_POLYGON_MODE_LINE);
		

		mPSOs["opaque"] = opaquePipeline;
		mPSOs["opaque_wireframe"] = wireframePipeline;
		

		VkStencilOpState stencil{};
		stencil.compareOp = VK_COMPARE_OP_ALWAYS;
		stencil.failOp = VK_STENCIL_OP_REPLACE;
		stencil.depthFailOp = VK_STENCIL_OP_REPLACE;
		stencil.passOp = VK_STENCIL_OP_REPLACE;
		stencil.writeMask = 0xFF;
		stencil.compareMask = 0xFF;
		stencil.reference = 0xFF;
		VkPipeline mirrorPipeline = initGraphicsPipeline(mDevice, mRenderPass, mPipelineLayout, shaders, Vertex::getInputBindingDescription(), Vertex::getInputAttributeDescription(), VK_CULL_MODE_FRONT_BIT,false, &stencil,true, mMSAA ? mNumSamples : VK_SAMPLE_COUNT_1_BIT, VK_TRUE, VK_POLYGON_MODE_FILL);
		mPSOs["markStencilMirrors"] = mirrorPipeline;

		stencil.failOp = VK_STENCIL_OP_KEEP;
		stencil.depthFailOp = VK_STENCIL_OP_REPLACE;
		stencil.passOp = VK_STENCIL_OP_REPLACE;
		stencil.compareOp = VK_COMPARE_OP_EQUAL;
		stencil.writeMask = 0xFF;
		stencil.compareMask = 0xFF;
		stencil.reference = 0xFF;
		VkPipeline reflectionPipeline = initGraphicsPipeline(mDevice, mRenderPass, mPipelineLayout, shaders, Vertex::getInputBindingDescription(), Vertex::getInputAttributeDescription(), VK_CULL_MODE_BACK_BIT,true, &stencil, mMSAA ? mNumSamples : VK_SAMPLE_COUNT_1_BIT, VK_FALSE, VK_POLYGON_MODE_FILL);
		mPSOs["drawStencilReflections"] = reflectionPipeline;

		stencil.failOp = VK_STENCIL_OP_KEEP;
		stencil.depthFailOp = VK_STENCIL_OP_KEEP;
		stencil.passOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
		stencil.compareOp = VK_COMPARE_OP_EQUAL;
		stencil.writeMask = 0xFF;
		stencil.compareMask = 0xFF;
		stencil.reference = 0x00;
		VkPipeline shadowPipeline = initGraphicsPipeline(mDevice, mRenderPass, mPipelineLayout, shaders, Vertex::getInputBindingDescription(), Vertex::getInputAttributeDescription(), VK_CULL_MODE_FRONT_BIT, true, &stencil, mMSAA ? mNumSamples : VK_SAMPLE_COUNT_1_BIT, VK_TRUE, VK_POLYGON_MODE_FILL);
		mPSOs["shadow"] = shadowPipeline;


		VkPipeline transPipeline = initGraphicsPipeline(mDevice, mRenderPass, mPipelineLayout, shaders, Vertex::getInputBindingDescription(), Vertex::getInputAttributeDescription(), VK_CULL_MODE_FRONT_BIT, true, mMSAA ? mNumSamples : VK_SAMPLE_COUNT_1_BIT, VK_TRUE, VK_POLYGON_MODE_FILL);
		mPSOs["transparent"] = transPipeline;



		cleanupShaderModule(mDevice, shaders[0].shaderModule);
		cleanupShaderModule(mDevice, shaders[1].shaderModule); 
	}


}

void StencilApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		std::vector<VkDescriptorSet> descriptorSets{
			mDescriptorSetsPC[i],
			mDescriptorSetsOBs[i],
			mDescriptorSetsMats[i]
		};
		mFrameResources.push_back(std::make_unique<FrameResource>(mDevice, mMemoryProperties, descriptorSets, (uint32_t)mDeviceProperties.limits.minUniformBufferOffsetAlignment,
			2/*main & reflected passes*/, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
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
	floorRitem->IndexCount = floorRitem->Geo->DrawArgs["floor"].indexCount;
	floorRitem->StartIndexLocation = floorRitem->Geo->DrawArgs["floor"].startIndexLocation;
	floorRitem->BaseVertexLocation = floorRitem->Geo->DrawArgs["floor"].baseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(floorRitem.get());

	auto wallsRitem = std::make_unique<RenderItem>();
	wallsRitem->World = MathHelper::Identity4x4();
	wallsRitem->TexTransform = MathHelper::Identity4x4();
	wallsRitem->ObjCBIndex = 1;
	wallsRitem->Mat = mMaterials["bricks"].get();
	wallsRitem->Geo = mGeometries["roomGeo"].get();
	//wallsRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wallsRitem->IndexCount = wallsRitem->Geo->DrawArgs["wall"].indexCount;
	wallsRitem->StartIndexLocation = wallsRitem->Geo->DrawArgs["wall"].startIndexLocation;
	wallsRitem->BaseVertexLocation = wallsRitem->Geo->DrawArgs["wall"].baseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(wallsRitem.get());

	auto skullRitem = std::make_unique<RenderItem>();
	skullRitem->World = MathHelper::Identity4x4();
	skullRitem->TexTransform = MathHelper::Identity4x4();
	skullRitem->ObjCBIndex = 2;
	skullRitem->Mat = mMaterials["skullMat"].get();
	skullRitem->Geo = mGeometries["skullGeo"].get();
	//skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].indexCount;
	skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].startIndexLocation;
	skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].baseVertexLocation;
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
	mirrorRitem->IndexCount = mirrorRitem->Geo->DrawArgs["mirror"].indexCount;
	mirrorRitem->StartIndexLocation = mirrorRitem->Geo->DrawArgs["mirror"].startIndexLocation;
	mirrorRitem->BaseVertexLocation = mirrorRitem->Geo->DrawArgs["mirror"].baseVertexLocation;
	mRitemLayer[(int)RenderLayer::Mirrors].push_back(mirrorRitem.get());
	mRitemLayer[(int)RenderLayer::Transparent].push_back(mirrorRitem.get());

	mAllRitems.push_back(std::move(floorRitem));
	mAllRitems.push_back(std::move(wallsRitem));
	mAllRitems.push_back(std::move(skullRitem));
	mAllRitems.push_back(std::move(reflectedSkullRitem));
	mAllRitems.push_back(std::move(shadowedSkullRitem));
	mAllRitems.push_back(std::move(mirrorRitem));
}

void StencilApp::Draw(const GameTimer& gt) {
	uint32_t index = 0;
	VkCommandBuffer cmd{ VK_NULL_HANDLE };

	cmd = BeginRender();

	VkViewport viewport = { 0.0f,0.0f,(float)mClientWidth,(float)mClientHeight,0.0f,1.0f };
	pvkCmdSetViewport(cmd, 0, 1, &viewport);
	VkRect2D scissor = { {0,0},{(uint32_t)mClientWidth,(uint32_t)mClientHeight} };
	pvkCmdSetScissor(cmd, 0, 1, &scissor);

	VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	VkDeviceSize passSize = ((uint32_t)sizeof(PassConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	uint32_t dynamicOffsets[1] = { 0 };	
	pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsPC[mIndex], 1, dynamicOffsets);//bind PC data once
	uint32_t dynamicOffsets2[1] = { (uint32_t)passSize };
	
	if (mIsWireframe) {
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque_wireframe"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Opaque]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Mirrors]);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsPC[mIndex], 1, dynamicOffsets2);//bind PC data once
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Reflected]);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsPC[mIndex], 1, dynamicOffsets);//bind PC data once
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Transparent]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Shadow]);
	}
	else {

		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Opaque]);
		//vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_BIT, 1);
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["markStencilMirrors"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Mirrors]);
		
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsPC[mIndex], 1, dynamicOffsets2);//bind PC data once
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["drawStencilReflections"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Reflected]);
		//vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_BIT, 0);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsPC[mIndex], 1, dynamicOffsets);//bind PC data once
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["transparent"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Transparent]);
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["shadow"]);
		DrawRenderItems(cmd, mRitemLayer[(int)RenderLayer::Shadow]);
	}






	EndRender(cmd, mCurrFrameResource->Fence);


}
void StencilApp::DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems) {
	//	VkDeviceSize obSize = mCurrFrameResource->ObjectCBSize;
	VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	VkDeviceSize objSize = ((uint32_t)sizeof(ObjectConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	VkDeviceSize matSize = ((uint32_t)sizeof(Material) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	uint32_t dynamicOffsets[1] = { 0 };
	uint32_t count = getSwapchainImageCount(mSurfaceCaps);
	//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsPC[mIndex], 1, dynamicOffsets);//bind PC data once

	for (size_t i = 0; i < ritems.size(); i++) {
		auto ri = ritems[i];
		uint32_t indexOffset = ri->StartIndexLocation;

		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 3, 1, &mDescriptorSetsTextures[ri->Mat->DiffuseSrvHeapIndex], 0, 0);//bin texture descriptor
		const auto vbv = ri->Geo->vertexBufferGPU;
		pvkCmdBindVertexBuffers(cmd, 0, 1, &vbv.buffer, mOffsets);
		const auto ibv = ri->Geo->indexBufferGPU;
		pvkCmdBindIndexBuffer(cmd, ibv.buffer, indexOffset * sizeof(uint32_t), VK_INDEX_TYPE_UINT32);
		uint32_t cbvIndex = ri->ObjCBIndex;
		//uint32_t dynamicOffsets[2] = { 0,cbvIndex *objSize};

		dynamicOffsets[0] = (uint32_t)(cbvIndex * objSize);
		//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsOBs[mIndex], 2, dynamicOffsets);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 1, 1, &mDescriptorSetsOBs[mIndex], 1, dynamicOffsets);
		dynamicOffsets[0] = (uint32_t)(ri->Mat->MatCBIndex * matSize);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 2, 1, &mDescriptorSetsMats[mIndex], 1, dynamicOffsets);

		pvkCmdDrawIndexed(cmd, ri->IndexCount, 1, 0, ri->BaseVertexLocation, 0);
	}
}

void StencilApp::BuildRootSignature() {
	//build pipeline layout
	std::vector<VkDescriptorSetLayout> layouts = {
		mDescriptorSetLayoutPC,
		mDescriptorSetLayoutOBs,
		mDescriptorSetLayoutMats,
		mDescriptorSetLayoutTextures
	};
	mPipelineLayout = initPipelineLayout(mDevice, layouts);
}
void StencilApp::OnResize() {
	VulkApp::OnResize();
	mProj = glm::perspectiveFovLH_ZO(0.25f * pi, (float)mClientWidth, (float)mClientHeight, 1.0f, 1000.0f);
}
void StencilApp::LoadTextures()
{
	auto bricksTex = std::make_unique<Texture>();
	bricksTex->Name = "bricksTex";
	bricksTex->FileName = "../../../Textures/bricks3.jpg";
	loadTexture(mDevice, mCommandBuffer, mGraphicsQueue, mFormatProperties, mMemoryProperties, bricksTex->FileName.c_str(), *bricksTex);
	

	auto checkboardTex = std::make_unique<Texture>();
	checkboardTex->Name = "checkboardTex";
	checkboardTex->FileName = "../../../Textures/checkboard.jpg";
	loadTexture(mDevice, mCommandBuffer, mGraphicsQueue, mFormatProperties, mMemoryProperties, checkboardTex->FileName.c_str(), *checkboardTex);

	auto iceTex = std::make_unique<Texture>();
	iceTex->Name = "iceTex";
	iceTex->FileName = "../../../Textures/ice.jpg";
	loadTexture(mDevice, mCommandBuffer, mGraphicsQueue, mFormatProperties, mMemoryProperties, iceTex->FileName.c_str(), *iceTex);
	
	auto white1x1Tex = std::make_unique<Texture>();
	white1x1Tex->Name = "white1x1Tex";
	white1x1Tex->FileName = "../../../Textures/white1x1.jpg";
	loadTexture(mDevice, mCommandBuffer, mGraphicsQueue, mFormatProperties, mMemoryProperties, white1x1Tex->FileName.c_str(), *white1x1Tex);

	mTextures[bricksTex->Name] = std::move(bricksTex);
	mTextures[checkboardTex->Name] = std::move(checkboardTex);
	mTextures[iceTex->Name] = std::move(iceTex);
	mTextures[white1x1Tex->Name] = std::move(white1x1Tex);
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
void StencilApp::OnKeyboardInput(const GameTimer& gt)
{
	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;

	if (GetAsyncKeyState('P') & 0x8000) {
		
		
		saveScreenCap(mDevice, mCommandBuffer, mGraphicsQueue, mSwapchainImages[mIndex], mMemoryProperties, mFormatProperties, mSwapchainFormat.format,{(uint32_t)mClientWidth,(uint32_t)mClientHeight}, mFrameCount);
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
	glm::mat4 S = MathHelper::shadowMatrix(glm::vec4(toMainLight,0.0f), shadowPlane);

	//XMMATRIX shadowOffsetY = XMMatrixTranslation(0.0f, 0.001f, 0.0f);
	glm::mat4 shadowOffsetY = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.001f, 0.0f));
	//XMStoreFloat4x4(&mShadowedSkullRitem->World, skullWorld * S * shadowOffsetY);
	mShadowedSkullRitem->World = shadowOffsetY * S * skullWorld;//Order?
	mSkullRitem->NumFramesDirty = gNumFrameResources;
	mReflectedSkullRitem->NumFramesDirty = gNumFrameResources;
	mShadowedSkullRitem->NumFramesDirty = gNumFrameResources;
}

void StencilApp::AnimateMaterials(const GameTimer&gt) {

}

void StencilApp::Update(const GameTimer& gt) {
	OnKeyboardInput(gt);
	UpdateCamera(gt);

	//Cycle through the circular frame resource array
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	vkWaitForFences(mDevice, 1, &mCurrFrameResource->Fence, VK_TRUE, UINT64_MAX);
	vkResetFences(mDevice, 1, &mCurrFrameResource->Fence);

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
	auto currObjectCB = mCurrFrameResource->ObjectCB;
	uint8_t* pObjConsts = (uint8_t*)mCurrFrameResource->pOCs;
	VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	VkDeviceSize objSize = ((uint32_t)sizeof(ObjectConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
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
	VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	VkDeviceSize objSize = ((uint32_t)sizeof(MaterialConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	uint8_t* pMatConsts = (uint8_t*)mCurrFrameResource->pMats;
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
		mReflectedPassCB.Lights[i].Direction= reflectedLightDir;
	}
	PassConstants* pRPC = mCurrFrameResource->pPCs;
	VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	VkDeviceSize objSize = ((uint32_t)sizeof(PassConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	memcpy(((uint8_t*)pRPC)+objSize, &mReflectedPassCB, sizeof(PassConstants));
	// Reflected pass stored in index 1
	//auto currPassCB = mCurrFrameResource->PassCB.get();
	//currPassCB->CopyData(1, mReflectedPassCB);
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

