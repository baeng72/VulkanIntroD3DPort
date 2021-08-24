#include "../../../Common/VulkApp.h"
#include "../../../Common/VulkUtil.h"
#include "../../../Common/GeometryGenerator.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/Colors.h"
#include "../../../Common/TextureLoader.h"
#include "FrameResource.h"

#include <memory>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

const int gNumFrameResources = 3;

struct RenderItem {
	RenderItem() = default;

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


const float pi = 3.14159265358979323846264338327950288f;
class CrateApp : public VulkApp {
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

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	//Render items divided by PSO.
	std::vector<RenderItem*> mOpaqueRitems;

	PassConstants mMainPassCB;

	glm::vec3 mEyePos = glm::vec3(0.0f);
	glm::mat4 mView = glm::mat4(1.0f);
	glm::mat4 mProj = glm::mat4(1.0f);

	float mTheta = 1.3f * pi;
	float mPhi = 0.4f * pi;
	float mRadius = 2.5f;

	POINT mLastMousePos;

	bool mIsWireframe = false;

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

	void LoadTextures();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	//void BuildConstantBuffers();
	void BuildShapeGeometry();	
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems);

public:
	CrateApp(HINSTANCE hInstance);
	CrateApp(const CrateApp& rhs) = delete;
	CrateApp& operator=(const CrateApp& rhs) = delete;
	~CrateApp();

	virtual bool Initialize()override;

	
};
CrateApp::CrateApp(HINSTANCE hInstance) :VulkApp(hInstance) {
	mAllowWireframe = true;
	mClearValues[0].color = Colors::LightSteelBlue;
	mMSAA = false;
}

CrateApp::~CrateApp() {
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
		cleanupImage(mDevice,*pair.second);
	}
	cleanupPipelineLayout(mDevice, mPipelineLayout);
	cleanupDescriptorPool(mDevice, mDescriptorPoolTexture);
	cleanupDescriptorPool(mDevice, mDescriptorPool);
	cleanupDescriptorSetLayout(mDevice, mDescriptorSetLayoutTextures);
	cleanupDescriptorSetLayout(mDevice, mDescriptorSetLayoutMats);
	cleanupDescriptorSetLayout(mDevice, mDescriptorSetLayoutOBs);
	cleanupDescriptorSetLayout(mDevice, mDescriptorSetLayoutPC);
}

bool CrateApp::Initialize() {
	if (!VulkApp::Initialize())
		return false;

	LoadTextures();
	BuildDescriptorHeaps();
	BuildShapeGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildRootSignature();
	BuildFrameResources();
	
	BuildPSOs();
	return true;
}

void CrateApp::BuildDescriptorHeaps() {
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
	mDescriptorSetLayoutTextures = initDescriptorSetLayout(mDevice, bindings);
	poolSizes={
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,3},
	};
	mDescriptorPoolTexture = initDescriptorPool(mDevice, poolSizes, 4);
	mDescriptorSetsTextures.resize(count);
	initDescriptorSets(mDevice, mDescriptorSetLayoutTextures, mDescriptorPoolTexture, mDescriptorSetsTextures.data(), count);
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	auto tex = mTextures["woodCrateTex"].get();

	imageInfo.imageView = tex->imageView;
	imageInfo.sampler = tex->sampler;
	for (uint32_t i = 0; i < count; i++) {
		std::vector<VkWriteDescriptorSet> descriptorWrites = {
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,mDescriptorSetsTextures[i],0,0,1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,&imageInfo,nullptr,nullptr },
			
		};
		updateDescriptorSets(mDevice, descriptorWrites);
	}
	
	

}

void CrateApp::BuildShapeGeometry() {

	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);

	SubmeshGeometry boxSubmesh;
	boxSubmesh.indexCount = (UINT)box.Indices32.size();
	boxSubmesh.startIndexLocation = 0;
	boxSubmesh.baseVertexLocation = 0;


	std::vector<Vertex> vertices(box.Vertices.size());

	for (size_t i = 0; i < box.Vertices.size(); ++i)
	{
		vertices[i].Pos = box.Vertices[i].Position;
		vertices[i].Normal = box.Vertices[i].Normal;
		vertices[i].TexC = box.Vertices[i].TexC;
	}

	std::vector<std::uint32_t> indices = box.Indices32;

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

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

	geo->vertexBufferByteSize = vbByteSize;
	geo->vertexByteStride = sizeof(Vertex);
	geo->indexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.indexCount = (uint32_t)indices.size();
	submesh.startIndexLocation = 0;
	submesh.baseVertexLocation = 0;

	geo->DrawArgs["box"] = submesh;

	mGeometries[geo->Name] = std::move(geo);
}

void CrateApp::BuildPSOs() {
	std::vector<ShaderModule> shaders = {
		{initShaderModule(mDevice,"shaders/default.vert.spv"),VK_SHADER_STAGE_VERTEX_BIT},
		{initShaderModule(mDevice,"shaders/default.frag.spv"),VK_SHADER_STAGE_FRAGMENT_BIT}
	};
	VkPipeline opaquePipeline = initGraphicsPipeline(mDevice, mRenderPass, mPipelineLayout, shaders, Vertex::getInputBindingDescription(), Vertex::getInputAttributeDescription(), VK_CULL_MODE_FRONT_BIT, true, mMSAA ? mNumSamples : VK_SAMPLE_COUNT_1_BIT, VK_FALSE, VK_POLYGON_MODE_FILL);
	VkPipeline wireframePipeline = initGraphicsPipeline(mDevice, mRenderPass, mPipelineLayout, shaders, Vertex::getInputBindingDescription(), Vertex::getInputAttributeDescription(), VK_CULL_MODE_FRONT_BIT, true, mMSAA ? mNumSamples : VK_SAMPLE_COUNT_1_BIT, VK_FALSE, VK_POLYGON_MODE_LINE);

	
	mPSOs["opaque"] = opaquePipeline;
	mPSOs["opaque_wireframe"] = wireframePipeline;


	cleanupShaderModule(mDevice, shaders[0].shaderModule);
	cleanupShaderModule(mDevice, shaders[1].shaderModule);
}

void CrateApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		std::vector<VkDescriptorSet> descriptorSets{
			mDescriptorSetsPC[i],
			mDescriptorSetsOBs[i],
			mDescriptorSetsMats[i]
		};
		mFrameResources.push_back(std::make_unique<FrameResource>(mDevice,mMemoryProperties,descriptorSets,(uint32_t)mDeviceProperties.limits.minUniformBufferOffsetAlignment,
			1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
	}
}

void CrateApp::BuildMaterials()
{
	auto woodCrate = std::make_unique<Material>();
	woodCrate->Name = "woodCrate";
	woodCrate->NumFramesDirty = gNumFrameResources;
	woodCrate->MatCBIndex = 0;
	woodCrate->DiffuseSrvHeapIndex = 0;
	woodCrate->DiffuseAlbedo = glm::vec4(1.0f);
	woodCrate->FresnelR0 = glm::vec3(0.05f);
	woodCrate->Roughness = 0.2f;

	mMaterials["woodCrate"] = std::move(woodCrate);
}

void CrateApp::BuildRenderItems()
{
	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->NumFramesDirty = gNumFrameResources;
	boxRitem->ObjCBIndex = 0;
	boxRitem->Mat = mMaterials["woodCrate"].get();
	boxRitem->Geo = mGeometries["boxGeo"].get();
	//boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].indexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].startIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].baseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem));

	// All the render items are opaque.
	for (auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());
}



void CrateApp::OnResize() {
	VulkApp::OnResize();
	mProj = glm::perspectiveFovLH_ZO(0.25f * pi, (float)mClientWidth, (float)mClientHeight, 1.0f, 1000.0f);
}

void CrateApp::Update(const GameTimer& gt) {
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

}

void CrateApp::Draw(const GameTimer& gt) {
	uint32_t index = 0;
	VkCommandBuffer cmd{ VK_NULL_HANDLE };

	cmd = BeginRender();

	VkViewport viewport = { 0.0f,0.0f,(float)mClientWidth,(float)mClientHeight,0.0f,1.0f };
	pvkCmdSetViewport(cmd, 0, 1, &viewport);
	VkRect2D scissor = { {0,0},{(uint32_t)mClientWidth,(uint32_t)mClientHeight} };
	pvkCmdSetScissor(cmd, 0, 1, &scissor);


	if (mIsWireframe) {
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque_wireframe"]);
	}
	else {
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque"]);
	}


	
	DrawRenderItems(cmd, mOpaqueRitems);

	EndRender(cmd, mCurrFrameResource->Fence);


}

void CrateApp::DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems) {
	//	VkDeviceSize obSize = mCurrFrameResource->ObjectCBSize;
	VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	VkDeviceSize objSize = ((uint32_t)sizeof(ObjectConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	VkDeviceSize matSize = ((uint32_t)sizeof(Material) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	uint32_t dynamicOffsets[1] = { 0 };
	pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 3, 1, &mDescriptorSetsTextures[mIndex], 0, 0);//bin texture descriptor
	pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsPC[mIndex], 1, dynamicOffsets);//bind PC data once

	for (size_t i = 0; i < ritems.size(); i++) {
		auto ri = ritems[i];
		uint32_t indexOffset = ri->StartIndexLocation;


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

void CrateApp::OnMouseDown(WPARAM btnState, int x, int y) {
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void CrateApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void CrateApp::OnMouseMove(WPARAM btnState, int x, int y) {
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
void CrateApp::OnKeyboardInput(const GameTimer& gt)
{
	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;

	const float dt = gt.DeltaTime();
}

void CrateApp::UpdateCamera(const GameTimer& gt) {
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	mEyePos = glm::vec3(x, y, z);

	glm::vec3 pos = glm::vec3(x, y, z);
	glm::vec3 target = glm::vec3(0.0f);
	glm::vec3 up = glm::vec3(0.0, 1.0f, 0.0f);

	mView = glm::lookAtLH(pos, target, up);
}

void CrateApp::UpdateObjectCBs(const GameTimer& gt) {
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
void CrateApp::UpdateMainPassCB(const GameTimer& gt) {
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

void CrateApp::UpdateMaterialsCBs(const GameTimer& gt) {
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

void CrateApp::AnimateMaterials(const GameTimer& gt) {

}

void CrateApp::BuildRootSignature() {
	//build pipeline layout
	std::vector<VkDescriptorSetLayout> layouts = {
		mDescriptorSetLayoutPC,
		mDescriptorSetLayoutOBs,
		mDescriptorSetLayoutMats,
		mDescriptorSetLayoutTextures
	};
	mPipelineLayout = initPipelineLayout(mDevice, layouts);
}

void CrateApp::LoadTextures() {
	auto woodCrateTex = std::make_unique<Texture>();
	woodCrateTex->Name = "woodCrateTex";
	woodCrateTex->FileName = "../../../Textures/WoodCrate01.jpg";
	loadTexture(mDevice,mCommandBuffer,mGraphicsQueue,mFormatProperties,mMemoryProperties,woodCrateTex->FileName.c_str(), *woodCrateTex);
	mTextures[woodCrateTex->Name] = std::move(woodCrateTex);
}

int main() {
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		CrateApp theApp(GetModuleHandle(NULL));
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

