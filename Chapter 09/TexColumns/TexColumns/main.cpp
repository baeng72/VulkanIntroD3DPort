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


const float pi = 3.14159265358979323846264338327950288f;

class TexColumnsApp : public VulkApp {
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

	float mTheta = 1.5f * pi;
	float mPhi = 0.2f * pi;
	float mRadius = 15.0f;

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
	TexColumnsApp(HINSTANCE hInstance);
	TexColumnsApp(const TexColumnsApp& rhs) = delete;
	TexColumnsApp& operator=(const TexColumnsApp& rhs) = delete;
	~TexColumnsApp();

	virtual bool Initialize()override;


};

TexColumnsApp::TexColumnsApp(HINSTANCE hInstance) :VulkApp(hInstance) {
	mAllowWireframe = true;
	mClearValues[0].color = Colors::LightSteelBlue;
	mMSAA = false;
}

TexColumnsApp::~TexColumnsApp() {
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

bool TexColumnsApp::Initialize() {
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

void TexColumnsApp::BuildDescriptorHeaps() {
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
	poolSizes = {
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,count},
	};
	mDescriptorPoolTexture = initDescriptorPool(mDevice, poolSizes, count+1);
		mDescriptorSetsTextures.resize(count);
	initDescriptorSets(mDevice, mDescriptorSetLayoutTextures, mDescriptorPoolTexture, mDescriptorSetsTextures.data(), count);
	
	
	std::vector<VkDescriptorImageInfo> imageInfos(count);
	
	std::vector<VkWriteDescriptorSet> descriptorWrites;
	Texture* tex{ nullptr };
	for (uint32_t i = 0; i < count; i++) {
		imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		switch (i) {
		case 0:
			tex = mTextures["bricksTex"].get();
			break;
		case 1:
			tex = mTextures["stoneTex"].get();
			break;
		case 2:
			tex = mTextures["tileTex"].get();
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

void TexColumnsApp::BuildShapeGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.50, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//concat all geometry into one big vertex/index buffer.
	//cache the vertex offsets to each object in concatenated vetex buffer.
	uint32_t boxVertexOffset = 0;
	uint32_t gridVertexOffset = (uint32_t)box.Vertices.size();
	uint32_t sphereVertexOffset = gridVertexOffset + (uint32_t)grid.Vertices.size();
	uint32_t cylinderVertexOffset = sphereVertexOffset + (uint32_t)sphere.Vertices.size();

	//Cache the starting index for each object in the concatenated index buffer.
	uint32_t boxIndexOffset = 0;
	uint32_t gridIndexOffset = (uint32_t)box.Indices32.size();
	uint32_t sphereIndexOffset = gridIndexOffset + (uint32_t)grid.Indices32.size();
	uint32_t cylinderIndexOffset = sphereIndexOffset + (uint32_t)sphere.Indices32.size();


	//Define the SubmeshGeometry that cover different
	//regions of the vertex/index buffers.
	SubmeshGeometry boxSubmesh;
	boxSubmesh.indexCount = (uint32_t)box.Indices32.size();
	boxSubmesh.startIndexLocation = boxIndexOffset;
	boxSubmesh.baseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.indexCount = (uint32_t)grid.Indices32.size();
	gridSubmesh.startIndexLocation = gridIndexOffset;
	gridSubmesh.baseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.indexCount = (uint32_t)sphere.Indices32.size();
	sphereSubmesh.startIndexLocation = sphereIndexOffset;
	sphereSubmesh.baseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.indexCount = (uint32_t)cylinder.Indices32.size();
	cylinderSubmesh.startIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.baseVertexLocation = cylinderVertexOffset;

	//extract the vertex elements we are interested in and pack the
	//vertices of all the meshes into one vertex buffer.
	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);
	//std::vector<Vertex> object;
	uint32_t k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
		//object.push_back({ box.Vertices[i].Position,Colors::colortovec(Colors::DarkGreen) });
	}
	//writeObj("box", "models", object, box.Indices32);
	//object.clear();
	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
		//object.push_back({ grid.Vertices[i].Position,Colors::colortovec(Colors::ForestGreen) });
	}
	//writeObj("grid", "models", object, grid.Indices32);
	//object.clear();
	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
		//object.push_back({ sphere.Vertices[i].Position,Colors::colortovec(Colors::Crimson) });
	}

	//writeObj("sphere", "models", object,sphere.Indices32);
	//object.clear();
	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
		//object.push_back({ cylinder.Vertices[i].Position,Colors::colortovec(Colors::Crimson) });
	}
	//writeObj("cylinder", "models", object, sphere.Indices32);

	std::vector<uint32_t> indices;
	indices.insert(indices.end(), std::begin(box.Indices32), std::end(box.Indices32));
	indices.insert(indices.end(), std::begin(grid.Indices32), std::end(grid.Indices32));
	indices.insert(indices.end(), std::begin(sphere.Indices32), std::end(sphere.Indices32));
	indices.insert(indices.end(), std::begin(cylinder.Indices32), std::end(cylinder.Indices32));

	const uint32_t vbByteSize = (uint32_t)vertices.size() * sizeof(Vertex);
	const uint32_t ibByteSize = (uint32_t)indices.size() * sizeof(uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

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

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);



}

void TexColumnsApp::BuildPSOs() {
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

void TexColumnsApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		std::vector<VkDescriptorSet> descriptorSets{
			mDescriptorSetsPC[i],
			mDescriptorSetsOBs[i],
			mDescriptorSetsMats[i]
		};
		mFrameResources.push_back(std::make_unique<FrameResource>(mDevice, mMemoryProperties, descriptorSets, (uint32_t)mDeviceProperties.limits.minUniformBufferOffsetAlignment,
			1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
	}
}


void TexColumnsApp::BuildMaterials()
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

	mMaterials["bricks0"] = std::move(bricks0);
	mMaterials["stone0"] = std::move(stone0);
	mMaterials["tile0"] = std::move(tile0);
}

void TexColumnsApp::BuildRenderItems() {
	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->World = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f)), glm::vec3(0.0f, 0.5f, 0.0f));
	boxRitem->ObjCBIndex = 0;
	boxRitem->Mat = mMaterials["stone0"].get();
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	//boxRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].indexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].startIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].baseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = glm::mat4(1.0f);// glm::rotate(glm::mat4(1.0f), pi, glm::vec3(0.0f, 0.0f, 1.0f));
	gridRitem->ObjCBIndex = 1;
	gridRitem->Mat = mMaterials["tile0"].get();
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	//gridRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].indexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].startIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].baseVertexLocation;
	mAllRitems.push_back(std::move(gridRitem));

	uint32_t objCBIndex = 2;
	for (int i = 0; i < 5; i++) {
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		glm::mat4 leftCylWorld = glm::translate(glm::mat4(1.0f), glm::vec3(-5.0f, 1.5f, -10.0f + i * 5.0f));
		glm::mat4 rightCylWorld = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 1.5f, -10.0f + i * 5.0f));

		glm::mat4 leftSphereWorld = glm::translate(glm::mat4(1.0f), glm::vec3(-5.0f, 3.5f, -10.0f + i * 5.0f));
		glm::mat4 rightSphereWorld = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 3.5f, -10.0f + i * 5.0f));


		leftCylRitem->World = rightCylWorld;
		leftCylRitem->ObjCBIndex = objCBIndex++;
		leftCylRitem->Mat = mMaterials["bricks0"].get();
		leftCylRitem->Geo = mGeometries["shapeGeo"].get();
		//leftCylRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].indexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].startIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].baseVertexLocation;

		rightCylRitem->World = leftCylWorld;
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Mat = mMaterials["bricks0"].get();
		rightCylRitem->Geo = mGeometries["shapeGeo"].get();
		//rightCylRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].indexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].startIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].baseVertexLocation;

		leftSphereRitem->World = leftSphereWorld;
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Mat = mMaterials["stone0"].get();
		leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
		//leftSphereRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].indexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].startIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].baseVertexLocation;

		rightSphereRitem->World = rightSphereWorld;
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Mat = mMaterials["stone0"].get();
		rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
		//rightSphereRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].indexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].startIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].baseVertexLocation;

		mAllRitems.push_back(std::move(leftCylRitem));
		mAllRitems.push_back(std::move(rightCylRitem));
		mAllRitems.push_back(std::move(leftSphereRitem));
		mAllRitems.push_back(std::move(rightSphereRitem));

	}

	//All the render items are opaque.
	for (auto& e : mAllRitems) {
		mOpaqueRitems.push_back(e.get());
	}
}
void TexColumnsApp::BuildRootSignature() {
	//build pipeline layout
	std::vector<VkDescriptorSetLayout> layouts = {
		mDescriptorSetLayoutPC,
		mDescriptorSetLayoutOBs,
		mDescriptorSetLayoutMats,
		mDescriptorSetLayoutTextures
	};
	mPipelineLayout = initPipelineLayout(mDevice, layouts);
}

void TexColumnsApp::LoadTextures() {
	auto bricksTex = std::make_unique<Texture>();
	bricksTex->Name = "bricksTex";
	bricksTex->FileName = "../../../Textures/bricks.jpg";
	loadTexture(mDevice, mCommandBuffer, mGraphicsQueue, mFormatProperties, mMemoryProperties, bricksTex->FileName.c_str(), *bricksTex);	
	mTextures[bricksTex->Name] = std::move(bricksTex);
	auto stoneTex = std::make_unique<Texture>();
	stoneTex->Name = "stoneTex";
	stoneTex->FileName = "../../../Textures/stone.jpg";
	loadTexture(mDevice, mCommandBuffer, mGraphicsQueue, mFormatProperties, mMemoryProperties, stoneTex->FileName.c_str(), *stoneTex);
	mTextures[stoneTex->Name] = std::move(stoneTex);
	auto tileTex = std::make_unique<Texture>();
	tileTex->Name = "tileTex";
	tileTex->FileName = "../../../Textures/tile.jpg";
	loadTexture(mDevice, mCommandBuffer, mGraphicsQueue, mFormatProperties, mMemoryProperties, tileTex->FileName.c_str(), *tileTex);
	mTextures[tileTex->Name] = std::move(tileTex);
}


void TexColumnsApp::OnResize() {
	VulkApp::OnResize();
	mProj = glm::perspectiveFovLH_ZO(0.25f * pi, (float)mClientWidth, (float)mClientHeight, 1.0f, 1000.0f);
}

void TexColumnsApp::Update(const GameTimer& gt) {
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

void TexColumnsApp::Draw(const GameTimer& gt) {
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

void TexColumnsApp::DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems) {
	//	VkDeviceSize obSize = mCurrFrameResource->ObjectCBSize;
	VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	VkDeviceSize objSize = ((uint32_t)sizeof(ObjectConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	VkDeviceSize matSize = ((uint32_t)sizeof(Material) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	uint32_t dynamicOffsets[1] = { 0 };
	uint32_t count = getSwapchainImageCount(mSurfaceCaps);
	pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsPC[mIndex], 1, dynamicOffsets);//bind PC data once

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

void TexColumnsApp::OnMouseDown(WPARAM btnState, int x, int y) {
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void TexColumnsApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void TexColumnsApp::OnMouseMove(WPARAM btnState, int x, int y) {
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
void TexColumnsApp::OnKeyboardInput(const GameTimer& gt)
{
	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;

	const float dt = gt.DeltaTime();
}

void TexColumnsApp::UpdateCamera(const GameTimer& gt) {
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	mEyePos = glm::vec3(x, y, z);

	glm::vec3 pos = glm::vec3(x, y, z);
	glm::vec3 target = glm::vec3(0.0f);
	glm::vec3 up = glm::vec3(0.0, 1.0f, 0.0f);

	mView = glm::lookAtLH(pos, target, up);
}

void TexColumnsApp::UpdateObjectCBs(const GameTimer& gt) {
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
void TexColumnsApp::UpdateMainPassCB(const GameTimer& gt) {
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
	mMainPassCB.Lights[0].Strength = { 0.8f, 0.8f, 0.8f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };
	memcpy(pPassConstants, &mMainPassCB, sizeof(PassConstants));
}

void TexColumnsApp::UpdateMaterialsCBs(const GameTimer& gt) {
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

void TexColumnsApp::AnimateMaterials(const GameTimer& gt) {

}



int main() {
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		TexColumnsApp theApp(GetModuleHandle(NULL));
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

