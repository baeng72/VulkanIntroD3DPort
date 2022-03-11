#include "../../Common/VulkApp.h"
//#include "../../Common/VulkanManager.h"
#include "../../Common/VulkUtil.h"
#include "../../Common/MathHelper.h"
#include "../../Common/Colors.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"
#include <memory>
#include <array>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../../ThirdParty/spirv-reflect/spirv_reflect.h"
#include <fstream>
#include "test.h"
const int gNumFrameResources = 3;

//Lightweight structure stores parameters to draw a shape. 
struct RenderItem {
	RenderItem() = default;
	//World matrix of shape that describes object's local space and relative to the world space, which defines the position orientation, and scale of the object
	glm::mat4 World = glm::mat4(1.0f);
	//Dirty flag indicating we need to update constant (uniform) buffer
	//Apply to all frame buffers (ususally 3).
	int NumFramesDirty = gNumFrameResources;

	//Index into GPU constant (dynamic uniform) buffer corresponding to the ObjectCB for this render item.
	uint32_t ObjCBIndex = -1;

	MeshGeometry* Geo{ nullptr };

	//Primitive topology.
	VkPrimitiveTopology PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	//vkCmdDrawIndexed parameters
	uint32_t IndexCount = 0;
	uint32_t StartIndexLocation = 0;
	uint32_t BaseVertexLocation = 0;
};

struct Vertex {
	glm::vec3 Pos;
	glm::vec4 Color;
	static VkVertexInputBindingDescription& getInputBindingDescription() {
		static VkVertexInputBindingDescription bindingDescription = { 0,sizeof(Vertex),VK_VERTEX_INPUT_RATE_VERTEX };
		return bindingDescription;
	}
	static std::vector<VkVertexInputAttributeDescription>& getInputAttributeDescription() {
		static std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {
			{0,0,VK_FORMAT_R32G32B32_SFLOAT,offsetof(Vertex,Pos)},
			{1,0,VK_FORMAT_R32G32B32A32_SFLOAT,offsetof(Vertex,Color)},
		};
		return attributeDescriptions;
	}
};

class ShapesApp : public VulkApp {
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource{ nullptr };
	int mCurrFrameResourceIndex{ 0 };

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;

	std::unordered_map<std::string, size_t> mPSOs;

	std::vector < std::unique_ptr<RenderItem>> mAllRItems;

	//RenderItems divided by Pipeline object
	std::vector<RenderItem*> mOpaqueRitems;

	//std::unique_ptr<VulkanManager> mVulkanManager;

	PassConstants mMainPassCB;

	uint32_t mPassCbvOffset = 0;

	bool mIsWireframe = 0;

	glm::vec3 mEyePos = glm::vec3(0.0f);
	glm::mat4 mView = glm::mat4(1.0f);
	glm::mat4 mProj = glm::mat4(1.0f);

	float mTheta = 1.5f * pi;
	float mPhi = 0.2f * pi;
	float mRadius = 15.0f;

	POINT mLastMousePos;

	size_t passConstantHash{ 0 };
	size_t objectConstantHash{ 0 };

	Vulkan::Buffer VertexBuffer;
	Vulkan::Buffer IndexBuffer;

	/*Vulkan::Buffer PassCBBuffer;
	Vulkan::Buffer ObjectCBBuffer;
	VkDeviceSize passSize{ 0 };
	VkDeviceSize objectSize{ 0 };
	void* pPassCB{ nullptr };
	void* pObjectCB{ nullptr };
	VkShaderStageFlags passStage{ VK_SHADER_STAGE_VERTEX_BIT };
	VkShaderStageFlags objectStage{ VK_SHADER_STAGE_VERTEX_BIT };
	VkDescriptorType passDescriptorType{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC };
	VkDescriptorType objectDescriptorType{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC };
	std::vector<VkDescriptorBufferInfo> passBufferInfo;
	std::vector<VkDescriptorBufferInfo> objectBufferInfo;

	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	VkDescriptorPool descriptorPool;
	std::vector < std::vector<VkDescriptorSet>> descriptorSets;
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkPipeline opaquePipeline{ VK_NULL_HANDLE };
	VkPipeline wireframePipeline{ VK_NULL_HANDLE };*/

	std::unique_ptr<ShaderResources> pipelineRes;
	std::unique_ptr<ShaderProgram> prog;
	std::unique_ptr<PipelineLayout> pipelineLayout;
	std::unique_ptr<Pipeline> opaquePipeline;
	std::unique_ptr<Pipeline> wireframePipeline;


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
	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildShapeGeometry();
	void BuildRootSignature();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildRenderItems();
	void DrawRenderItems(VkCommandBuffer cmd, size_t hash, const std::vector<RenderItem*>& ritems);
	void DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems);


public:
	ShapesApp(HINSTANCE hInstance);
	ShapesApp(const ShapesApp& rhs) = delete;
	ShapesApp& operator=(const ShapesApp& rhs) = delete;
	~ShapesApp();

	virtual bool Initialize()override;
};

ShapesApp::ShapesApp(HINSTANCE hInstance) :VulkApp(hInstance) {
	mAllowWireframe = true;
	mClearValues[0].color = Colors::LightSteelBlue;
	mMSAA = false;
}

ShapesApp::~ShapesApp() {
	Vulkan::cleanupBuffer(mDevice, IndexBuffer);
	Vulkan::cleanupBuffer(mDevice, VertexBuffer);
	/*Vulkan::cleanupPipeline(mDevice, wireframePipeline);
	Vulkan::cleanupPipeline(mDevice, opaquePipeline);
	Vulkan::cleanupPipelineLayout(mDevice, pipelineLayout);
	for (auto& descriptorSetLayout : descriptorSetLayouts) {
		Vulkan::cleanupDescriptorSetLayout(mDevice, descriptorSetLayout);
	}
	Vulkan::cleanupDescriptorPool(mDevice, descriptorPool);
	Vulkan::unmapBuffer(mDevice, PassCBBuffer);
	Vulkan::cleanupBuffer(mDevice, PassCBBuffer);
	Vulkan::unmapBuffer(mDevice, ObjectCBBuffer);
	Vulkan::cleanupBuffer(mDevice, ObjectCBBuffer);*/
}

bool ShapesApp::Initialize() {
	if (!VulkApp::Initialize())
		return false;

	/*VMVulkanInfo vulkanInfo;
	vulkanInfo.device = mDevice;
	vulkanInfo.commandBuffer = mCommandBuffer;
	vulkanInfo.memoryProperties = mMemoryProperties;
	vulkanInfo.queue = mGraphicsQueue;
	vulkanInfo.formatProperties = mFormatProperties;
	vulkanInfo.deviceProperties = mDeviceProperties;*/
	
	//mVulkanManager = std::make_unique<VulkanManager>(vulkanInfo);

	BuildShapeGeometry();
	BuildRenderItems();

	BuildConstantBuffers();
	BuildPSOs();
	BuildFrameResources();
	
	
	return true;
}


void ShapesApp::OnResize() {
	VulkApp::OnResize();
	mProj = glm::perspectiveFovLH(0.25f * pi, (float)mClientWidth, (float)mClientHeight, 1.0f, 1000.0f);
}


void ShapesApp::Update(const GameTimer& gt) {
	VulkApp::Update(gt);
	OnKeyboardInput(gt);
	UpdateCamera(gt);

	//Cycle through the circular frame resource array
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	/*vkWaitForFences(mDevice, 1, &mCurrFrameResource->Fence, VK_TRUE, UINT64_MAX);
	vkResetFences(mDevice, 1, &mCurrFrameResource->Fence);*/
	UpdateObjectCBs(gt);
	UpdateMainPassCB(gt);

}

void ShapesApp::Draw(const GameTimer& gt) {
	uint32_t index = 0;
	VkCommandBuffer cmd{ VK_NULL_HANDLE };

	cmd = BeginRender();

	VkViewport viewport = { 0.0f,0.0f,(float)mClientWidth,(float)mClientHeight,0.0f,1.0f };
	pvkCmdSetViewport(cmd, 0, 1, &viewport);
	VkRect2D scissor = { {0,0},{(uint32_t)mClientWidth,(uint32_t)mClientHeight} };
	pvkCmdSetScissor(cmd, 0, 1, &scissor);

	if (mIsWireframe) {
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *wireframePipeline);// mPSOs["opaque_wireframe"]);
	}
	else {
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *opaquePipeline);// mPSOs["opaque"]);
	}


	
	
	DrawRenderItems(cmd, mOpaqueRitems);

	//DrawRenderItems(cmd, mIsWireframe ? wireframeHash : solidHash, mOpaqueRitems);

	EndRender(cmd);// , mCurrFrameResource->Fence);


}


void ShapesApp::DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems) {
	//size_t hash = mIsWireframe ? mPSOs["wireframe"] : mPSOs["opaque"];
	/*std::vector<VMDrawObjectPart> parts;
	size_t objectHash = 0;
	for (size_t i = 0; i < ritems.size(); i++) {
		auto ri = ritems[i];
		parts.push_back({ri->IndexCount,ri->StartIndexLocation,ri->BaseVertexLocation,ri->ObjCBIndex});		
		objectHash = ri->Geo->hash;

	}
	
	VMDrawInfo drawInfo;
	drawInfo.pvkCmdBindDescriptorSets = pvkCmdBindDescriptorSets;
	drawInfo.pvkCmdBindIndexBuffer = pvkCmdBindIndexBuffer;
	drawInfo.pvkCmdBindPipeline = pvkCmdBindPipeline;
	drawInfo.pvkCmdBindVertexBuffers = pvkCmdBindVertexBuffers;
	drawInfo.pvkCmdDrawIndexed = pvkCmdDrawIndexed;*/
	//mVulkanManager->DrawParts(hash, objectHash,drawInfo,cmd,  parts,1,mCurrFrame);
	
	//VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	//VkDeviceSize objSize = mCurrFrameResource->ObjectCBSize;
	uint32_t dynamicOffsets[1] = { 0 };
	//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[0][mCurrFrame], 1, dynamicOffsets);//bind PC data once
	pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &pipelineRes->descriptorSets[0][mCurrFrame], 1, dynamicOffsets);//bind PC data once
	pvkCmdBindVertexBuffers(cmd, 0, 1, &VertexBuffer.buffer, mOffsets);
	VkDeviceSize objectSize = pipelineRes->pipelineResources[1].buffer.objectSize;
	for (size_t i = 0; i < ritems.size(); i++) {
		auto ri = ritems[i];
		uint32_t indexOffset = ri->StartIndexLocation;


	//	const auto vbv = ri->Geo->vertexBufferGPU;
		//pvkCmdBindVertexBuffers(cmd, 0, 1, &VertexBuffer.buffer, mOffsets);
		
		pvkCmdBindIndexBuffer(cmd, IndexBuffer.buffer, indexOffset * sizeof(uint32_t), VK_INDEX_TYPE_UINT32);
		uint32_t cbvIndex = ri->ObjCBIndex;


	//	//uint32_t dynamicOffsets[2] = { 0,cbvIndex *objSize};
		dynamicOffsets[0] = (uint32_t)(cbvIndex * objectSize);
		
		//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 1, 1, &descriptorSets[1][mCurrFrame], 1, dynamicOffsets);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 1, 1, &pipelineRes->descriptorSets[1][mCurrFrame], 1, dynamicOffsets);
		pvkCmdDrawIndexed(cmd, ri->IndexCount, 1, 0, ri->BaseVertexLocation, 0);
	}
}

void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y) {
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}


void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y) {
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
		float dx = 0.005f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.005f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 3.0f, 15.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void ShapesApp::OnKeyboardInput(const GameTimer& gt) {
	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;
}

void ShapesApp::UpdateCamera(const GameTimer& gt) {
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	glm::vec3 pos = glm::vec3(x, y, z);
	glm::vec3 target = glm::vec3(0.0f);
	glm::vec3 up = glm::vec3(0.0, 1.0f, 0.0f);

	mView = glm::lookAtLH(pos, target, up);
}

void ShapesApp::UpdateObjectCBs(const GameTimer& gt) {
	//auto currObjectCB = mCurrFrameResource->ObjectCB;
	uint8_t* pObjConsts = (uint8_t*)mCurrFrameResource->pOCs;
	VkDeviceSize objectSize = pipelineRes->pipelineResources[1].buffer.objectSize;
	//VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	//VkDeviceSize objSize = ((uint32_t)sizeof(ObjectConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	//VkDeviceSize objSize = mVulkanManager->GetBufferOffset(objectConstantHash, 0);
	for (auto& e : mAllRItems) {
		//Only update the cbuffer data if the constants have changed.
		//This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0) {
			glm::mat4 world = e->World;
			ObjectConstants objConstants;
			objConstants.World = world;
			//memcpy((pObjConsts + (objSize * e->ObjCBIndex)), &objConstants, sizeof(objConstants));
			uint8_t* ptr = pObjConsts + e->ObjCBIndex * objectSize;
			memcpy(ptr, &objConstants, sizeof(objConstants));
			//pObjConsts[e->ObjCBIndex] = objConstants;
			e->NumFramesDirty--;
		}
	}
}

void ShapesApp::UpdateMainPassCB(const GameTimer& gt) {
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
	memcpy(pPassConstants, &mMainPassCB, sizeof(PassConstants));
}

void ShapesApp::BuildFrameResources() {
	void* pPassCB = pipelineRes->pipelineResources[0].buffer.ptr;
	VkDeviceSize passSize = pipelineRes->pipelineResources[0].buffer.objectSize;
	void* pObjectCB = pipelineRes->pipelineResources[1].buffer.ptr;
	VkDeviceSize objectSize = pipelineRes->pipelineResources[1].buffer.objectSize;

	for (int i = 0; i < gNumFrameResources; i++) {

		//PassConstants* pc = (PassConstants*)mVulkanManager->GetBufferPtr(passConstantHash, 0);
		//ObjectConstants* oc = (ObjectConstants*)((uint8_t*)mVulkanManager->GetBufferPtr(objectConstantHash, 0)+i*mVulkanManager->GetBufferOffset(objectConstantHash,0));

		PassConstants* pc = (PassConstants*)((uint8_t*)pPassCB + passSize * i);
		ObjectConstants* oc = (ObjectConstants*)((uint8_t*)pObjectCB + objectSize*mAllRItems.size() * i);
		mFrameResources.push_back(std::make_unique<FrameResource>(pc,oc));
	}
}

void ShapesApp::BuildConstantBuffers() {
	/*VMBufferInfo passBuffer;
	passBuffer.size = sizeof(PassConstants);
	passBuffer.count = gNumFrameResources;
	passBuffer.isMapped = true;
	passBuffer.stage = VK_SHADER_STAGE_VERTEX_BIT;
	passBuffer.type = bUniformDynamic;
	passConstantHash = mVulkanManager->InitBuffer("PassConstants", passBuffer);

	VMBufferInfo objectBuffer;
	objectBuffer.size = sizeof(ObjectConstants);
	objectBuffer.count = (uint32_t)mAllRItems.size();
	objectBuffer.isMapped = true;
	objectBuffer.stage = VK_SHADER_STAGE_VERTEX_BIT;
	objectBuffer.type = bUniformDynamic;
	objectConstantHash = mVulkanManager->InitBuffer("ObjectConstants", objectBuffer);*/
}

void ShapesApp::BuildShapeGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
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
		vertices[k].Color = Colors::colortovec(Colors::DarkGreen);
		//object.push_back({ box.Vertices[i].Position,Colors::colortovec(Colors::DarkGreen) });
	}
	//writeObj("box", "models", object, box.Indices32);
	//object.clear();
	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Color = Colors::colortovec(Colors::ForestGreen);
		//object.push_back({ grid.Vertices[i].Position,Colors::colortovec(Colors::ForestGreen) });
	}
	//writeObj("grid", "models", object, grid.Indices32);
	//object.clear();
	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Color = Colors::colortovec(Colors::Crimson);
		//object.push_back({ sphere.Vertices[i].Position,Colors::colortovec(Colors::Crimson) });
	}

	//writeObj("sphere", "models", object,sphere.Indices32);
	//object.clear();
	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k) {
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Color = Colors::colortovec(Colors::SteelBlue);
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

	//geo->hash = mVulkanManager->InitObjectBuffersWithData("shapeGeo", vbByteSize, vertices.data(), ibByteSize, indices.data());
	
	Vulkan::BufferProperties props;
#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
#endif
	props.bufferUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	props.size = vbByteSize;
	Vulkan::initBuffer(mDevice, mMemoryProperties, props, VertexBuffer);
	
#ifdef __USE__VMA__
	props.usage = VMA_MEMORY_USAGE_GPU_ONLY;
#else
	props.memoryProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
#endif
	props.bufferUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	props.size = ibByteSize;
	Vulkan::initBuffer(mDevice, mMemoryProperties, props, IndexBuffer);
	
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
	
	CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, VertexBuffer, vbByteSize);
	memcpy(ptr, indices.data(), ibByteSize);
	CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, IndexBuffer, ibByteSize);
	unmapBuffer(mDevice, stagingBuffer);
	cleanupBuffer(mDevice, stagingBuffer);
	

	geo->VertexBufferByteSize = vbByteSize;
	geo->VertexByteStride = sizeof(Vertex);
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);

}

void ShapesApp::BuildPSOs() {
	//DescriptorSet desSet(mDevice);
	//std::vector<std::vector<DescriptorInfo>> desInfos ={ { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,VK_SHADER_STAGE_VERTEX_BIT } }, { { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,VK_SHADER_STAGE_VERTEX_BIT } }};
	//desSet.AddDescriptorSet(desInfos,3);
	// 
	{
		//ShaderResources pipelineRes(mDevice, mDeviceProperties, mMemoryProperties);
		pipelineRes = std::make_unique<ShaderResources>(mDevice, mDeviceProperties, mMemoryProperties);

		std::vector<std::vector<PipelineResource>> pipelineResourceInfos{
			{
				{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,VK_SHADER_STAGE_VERTEX_BIT, sizeof(PassConstants),1,true},
			},
			{
				{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,VK_SHADER_STAGE_VERTEX_BIT,sizeof(ObjectConstants),mAllRItems.size(),true},
			}
		};
		//pipelineRes.AddShaderResources(pipelineResourceInfos, 3);
		pipelineRes->AddShaderResources(pipelineResourceInfos, 3);
		//ShaderProgram prog(mDevice);
		prog = std::make_unique<ShaderProgram>(mDevice);
		std::vector<const char*> shaderPaths = { "Shaders/color.vert.spv","Shaders/color.frag.spv" };
		//prog.load(shaderPaths);
		prog->load(shaderPaths);
		//PipelineLayout pipelineLayout(mDevice, *pipelineRes);
		pipelineLayout = std::make_unique<PipelineLayout>(mDevice, *pipelineRes);
		Vulkan::PipelineInfo pipelineInfo;
		pipelineInfo.cullMode = VK_CULL_MODE_BACK_BIT;
		pipelineInfo.polygonMode = VK_POLYGON_MODE_FILL;
		pipelineInfo.depthTest = VK_TRUE;
		//Pipeline opaquepipeline(mDevice, mRenderPass, prog, pipelineLayout,pipelineInfo);
		opaquePipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);
		pipelineInfo.polygonMode = VK_POLYGON_MODE_LINE;
		//Pipeline wireframe(mDevice, mRenderPass, prog, pipelineLayout, pipelineInfo);
		wireframePipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);
	}
	////Allocte buffers
	//uint32_t passCount = (uint32_t)gNumFrameResources;
	////allocate dynamic uniform buffers 
	//{
	//	VkDeviceSize alignment = mDeviceProperties.limits.minUniformBufferOffsetAlignment > 0 ? mDeviceProperties.limits.minUniformBufferOffsetAlignment : 256;
	//	VkDeviceSize passCBSize = sizeof(PassConstants);
	//	
	//	passCBSize = (((uint32_t)passCBSize + alignment - 1) & ~(alignment - 1));
	//	VkDeviceSize passCBTotalSize = passCount * passCBSize;

	//	Vulkan::BufferProperties props{};
	//	props.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	//	props.bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	//	props.size = passCBTotalSize;

	//	Vulkan::initBuffer(mDevice, mMemoryProperties, props, PassCBBuffer);
	//	pPassCB = Vulkan::mapBuffer(mDevice, PassCBBuffer);
	//	passSize = passCBSize;

	//	passBufferInfo.resize(passCount);

	//	



	//	VkDeviceSize objectCBSize = sizeof(ObjectConstants);
	//	
	//	objectCBSize = (((uint32_t)objectCBSize + alignment - 1) & ~(alignment - 1));
	//	objectSize = objectCBSize;
	//	objectCBSize *= (uint32_t)mAllRItems.size();
	//	VkDeviceSize objectCBTotalSize = passCount * objectCBSize;

	//	props.size = objectCBTotalSize;
	//	Vulkan::initBuffer(mDevice, mMemoryProperties, props, ObjectCBBuffer);
	//	pObjectCB = Vulkan::mapBuffer(mDevice, ObjectCBBuffer);
	//	

	//	objectBufferInfo.resize(passCount);

	//	for (uint32_t i = 0; i < passCount; i++) {
	//		passBufferInfo[i].buffer = PassCBBuffer.buffer;
	//		passBufferInfo[i].offset = passCBSize * i;
	//		passBufferInfo[i].range = passSize;

	//		objectBufferInfo[i].buffer = ObjectCBBuffer.buffer;
	//		objectBufferInfo[i].offset = objectCBSize * i;
	//		objectBufferInfo[i].range = objectSize;
	//	}

	//	

	//}
	//{
	//	//build descriptor
	//	//need pool to allocate passCount * 2 descriptor sets
	//	std::vector<VkDescriptorPoolSize> poolSizes = {
	//		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, passCount*2}
	//	};
	//	descriptorPool = Vulkan::initDescriptorPool(mDevice, poolSizes, passCount*2);
	//	//need 2 descriptor layouts
	//	{
	//		std::vector<VkDescriptorSetLayoutBinding> descriptorBindings{
	//			{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,1,passStage}
	//		};
	//		descriptorSetLayouts.push_back(Vulkan::initDescriptorSetLayout(mDevice, descriptorBindings));
	//	}
	//	{
	//		std::vector<VkDescriptorSetLayoutBinding> descriptorBindings{
	//		{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,1,objectStage}
	//		};
	//		descriptorSetLayouts.push_back(Vulkan::initDescriptorSetLayout(mDevice, descriptorBindings));
	//	}
	//	//need passCount * 2 descriptorSets
	//	descriptorSets.resize(2);
	//	for (size_t i = 0; i < 2; i++) {
	//		descriptorSets[i].resize(passCount);
	//		for (size_t j = 0; j < passCount; j++) {
	//			descriptorSets[i][j]=Vulkan::initDescriptorSet(mDevice, descriptorSetLayouts[i], descriptorPool);

	//		}
	//	}

	//	//update descriptors with buffer info
	//	for (uint32_t i = 0; i < passCount; ++i) {
	//		std::vector<VkWriteDescriptorSet> descriptorWrites = {
	//			{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,descriptorSets[0][i],0,0,1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,nullptr,&passBufferInfo[i],nullptr},
	//			{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,nullptr,descriptorSets[1][i],0,0,1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,nullptr,&objectBufferInfo[i],nullptr}
	//		};
	//		
	//		Vulkan::updateDescriptorSets(mDevice, descriptorWrites);
	//	}
	//}

	//{
	//	//build pipeline objects 
	//	VkVertexInputBindingDescription vertexInputDescription;
	//	std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
	//	std::unordered_map<VkShaderStageFlagBits, VkShaderModule> shaders;
	//	//std::vector<VkShaderModule> shaders;
	//	std::vector<const char*> shaderPaths = { "Shaders/color.vert.spv","Shaders/color.frag.spv" };
	//	for (auto path : shaderPaths) {
	//		std::ifstream file(path, std::ios::ate | std::ios::binary);
	//		assert(file.is_open());


	//		size_t fileSize = (size_t)file.tellg();
	//		std::vector<char> shaderData(fileSize);

	//		file.seekg(0);
	//		file.read(shaderData.data(), fileSize);

	//		file.close();
	//		SpvReflectShaderModule module = {};
	//		SpvReflectResult result = spvReflectCreateShaderModule(shaderData.size(), shaderData.data(), &module);
	//		assert(result == SPV_REFLECT_RESULT_SUCCESS);


	//		VkShaderStageFlagBits stage;
	//		switch (module.shader_stage) {
	//		case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
	//			stage = VK_SHADER_STAGE_VERTEX_BIT;
	//			break;
	//		case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
	//			stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	//			break;
	//		case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
	//			stage = VK_SHADER_STAGE_GEOMETRY_BIT;
	//			break;
	//		case SpvReflectShaderStageFlagBits::SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
	//			stage = VK_SHADER_STAGE_COMPUTE_BIT;
	//			break;
	//		default:
	//			assert(0);
	//			break;
	//		}

	//		if (stage == VK_SHADER_STAGE_VERTEX_BIT) {

	//			vertexAttributeDescriptions.resize(module.input_variable_count);

	//			uint32_t offset = 0;
	//			std::vector<uint32_t> sizes(module.input_variable_count);
	//			std::vector<VkFormat> formats(module.input_variable_count);
	//			for (uint32_t i = 0; i < module.input_variable_count; ++i) {
	//				SpvReflectInterfaceVariable& inputVar = module.input_variables[i];
	//				uint32_t size = 0;
	//				VkFormat format;
	//				switch (inputVar.format) {

	//				case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
	//					format = VK_FORMAT_R32G32B32A32_SFLOAT;
	//					size = sizeof(float) * 4;
	//					break;
	//				case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
	//					format = VK_FORMAT_R32G32B32_SFLOAT;
	//					size = sizeof(float) * 3;
	//					break;
	//				case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
	//					format = VK_FORMAT_R32G32_SFLOAT;
	//					size = sizeof(float) * 2;
	//					break;
	//				default:
	//					assert(0);
	//					break;
	//				}
	//				sizes[inputVar.location] = (uint32_t)size;
	//				formats[inputVar.location] = format;
	//			}

	//			for (uint32_t i = 0; i < module.input_variable_count; i++) {

	//				vertexAttributeDescriptions[i].location = i;
	//				vertexAttributeDescriptions[i].offset = offset;
	//				vertexAttributeDescriptions[i].format = formats[i];
	//				offset += sizes[i];
	//			}
	//			vertexInputDescription.binding = 0;
	//			vertexInputDescription.stride = offset;
	//			vertexInputDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	//		}
	//		spvReflectDestroyShaderModule(&module);
	//		VkShaderModule shader = VK_NULL_HANDLE;

	//		VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	//		createInfo.codeSize = shaderData.size();
	//		createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderData.data());
	//		VkResult res = vkCreateShaderModule(mDevice, &createInfo, nullptr, &shader);
	//		assert(res == VK_SUCCESS);
	//		shaders.insert(std::pair<VkShaderStageFlagBits, VkShaderModule>(stage, shader));
	//		//shaders.push_back(shader);
	//		shaderData.clear();
	//	}
	//	
	//	pipelineLayout = Vulkan::initPipelineLayout(mDevice, descriptorSetLayouts);

	//	Vulkan::PipelineInfo pipelineInfo;
	//	pipelineInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	//	pipelineInfo.polygonMode = VK_POLYGON_MODE_FILL;
	//	pipelineInfo.depthTest = VK_TRUE;

	//	std::vector<Vulkan::ShaderModule> shaderModules;
	//	for (auto& shader : shaders) {
	//		Vulkan::ShaderModule shaderModule;
	//		shaderModule.stage = shader.first;
	//		shaderModule.shaderModule = shader.second;
	//		shaderModules.push_back(shaderModule);
	//	}
	//	
	//	opaquePipeline = Vulkan::initGraphicsPipeline(mDevice, mRenderPass, pipelineLayout, shaderModules, vertexInputDescription, vertexAttributeDescriptions, pipelineInfo);

	//	pipelineInfo.polygonMode = VK_POLYGON_MODE_LINE;
	//	wireframePipeline = Vulkan::initGraphicsPipeline(mDevice, mRenderPass, pipelineLayout, shaderModules, vertexInputDescription, vertexAttributeDescriptions, pipelineInfo);

	//	for (auto& shaderPair : shaders) {
	//		vkDestroyShaderModule(mDevice, shaderPair.second,nullptr);
	//	
	//	}
	//}

	//{
	//	VMDrawObjectInfo doi;
	//	doi.shaderPaths = { "Shaders/color.vert.spv","Shaders/color.frag.spv" };
	//	doi.passCount = gNumFrameResources;
	//	doi.polygonMode = VK_POLYGON_MODE_FILL;
	//	doi.cullMode = VK_CULL_MODE_FRONT_BIT;
	//	
	//	//doi.bufferHashes = { passConstantHash,objectConstantHash };
	//	//doi.descriptorTypes = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC };
	//	doi.descriptorSets = {
	//		{
	//			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,passConstantHash}
	//		},
	//		{
	//				{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,objectConstantHash}
	//		}
	//	};
	//	mPSOs["opaque"] = mVulkanManager->InitDrawObject("opaque", doi, mRenderPass);
	//}
	//{
	//	VMDrawObjectInfo doi;
	//	doi.shaderPaths = { "Shaders/color.vert.spv","Shaders/color.frag.spv" };
	//	doi.passCount = gNumFrameResources;
	//	doi.polygonMode = VK_POLYGON_MODE_LINE;
	//	doi.cullMode = VK_CULL_MODE_FRONT_BIT;
	//	//doi.bufferHashes = { passConstantHash,objectConstantHash };
	//	//doi.descriptorTypes = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC };
	//	doi.descriptorSets = {
	//		{
	//			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,passConstantHash}
	//		},
	//		{
	//				{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,objectConstantHash}
	//		}
	//	};
	//	mPSOs["wireframe"] = mVulkanManager->InitDrawObject("wireframe", doi, mRenderPass);
	//}

}


void ShapesApp::BuildRenderItems() {
	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->World = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f)), glm::vec3(0.0f, 0.5f, 0.0f));
	boxRitem->ObjCBIndex = 0;
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRItems.push_back(std::move(boxRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = glm::mat4(1.0f);// glm::rotate(glm::mat4(1.0f), pi, glm::vec3(0.0f, 0.0f, 1.0f));
	gridRitem->ObjCBIndex = 1;
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mAllRItems.push_back(std::move(gridRitem));

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
		leftCylRitem->Geo = mGeometries["shapeGeo"].get();
		leftCylRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		rightCylRitem->World = leftCylWorld;
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Geo = mGeometries["shapeGeo"].get();
		rightCylRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		leftSphereRitem->World = leftSphereWorld;
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
		leftSphereRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		rightSphereRitem->World = rightSphereWorld;
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
		rightSphereRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		mAllRItems.push_back(std::move(leftCylRitem));
		mAllRItems.push_back(std::move(rightCylRitem));
		mAllRItems.push_back(std::move(leftSphereRitem));
		mAllRItems.push_back(std::move(rightSphereRitem));

	}

	//All the render items are opaque.
	for (auto& e : mAllRItems) {
		mOpaqueRitems.push_back(e.get());
	}
}

int main() {
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		ShapesApp theApp(GetModuleHandle(NULL));
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