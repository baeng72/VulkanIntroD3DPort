#include "../../../Common/VulkApp.h"
#include "../../../Common/VulkUtil.h"
#include "../../../Common/GeometryGenerator.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/Colors.h"
#include "FrameResource.h"
#include <memory>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
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
	uint32_t indexCount = 0;
	uint32_t startIndexLocation = 0;
	uint32_t BaseVertexLocation = 0;
};

const float pi = 3.14159265358979323846264338327950288f;
class ShapesApp : public VulkApp {
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource{ nullptr };
	int mCurrFrameResourceIndex{ 0 };
	VkDescriptorSetLayout mDescriptorSetLayoutPC{ VK_NULL_HANDLE };
	VkDescriptorSetLayout	mDescriptorSetLayoutOBs{ VK_NULL_HANDLE };
	VkDescriptorPool		mDescriptorPool{ VK_NULL_HANDLE };
	//VkDescriptorSet			mDescriptorSet{ VK_NULL_HANDLE };
	std::vector<VkDescriptorSet> mDescriptorSetsPC;
	std::vector<VkDescriptorSet> mDescriptorSetsOBs;
	VkPipelineLayout		mPipelineLayout{ VK_NULL_HANDLE };
	

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, VkPipeline> mPSOs;

	std::vector < std::unique_ptr<RenderItem>> mAllRItems;

	//RenderItems divided by Pipeline object
	std::vector<RenderItem*> mOpaqueRitems;

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
	vkDeviceWaitIdle(mDevice);
	for (auto& pair : mGeometries) {
		free(pair.second->indexBufferCPU);
		free(pair.second->vertexBufferCPU);
		cleanupBuffer(mDevice, pair.second->indexBufferGPU);
		cleanupBuffer(mDevice, pair.second->vertexBufferGPU);

	}
	for (auto& pair : mPSOs) {
		VkPipeline pipeline = pair.second;
		cleanupPipeline(mDevice, pipeline);
	}
	cleanupPipelineLayout(mDevice, mPipelineLayout);
	/*for (auto& desc : mDescriptorSets) {
		cleanupDescriptorSet(mDevice, desc);
	}*/
	cleanupDescriptorPool(mDevice, mDescriptorPool);
	cleanupDescriptorSetLayout(mDevice, mDescriptorSetLayoutOBs);
	cleanupDescriptorSetLayout(mDevice, mDescriptorSetLayoutPC);
}

bool ShapesApp::Initialize() {
	if (!VulkApp::Initialize())
		return false;
	BuildDescriptorHeaps();
	BuildShapeGeometry();
	BuildRenderItems();
	BuildFrameResources();
	BuildConstantBuffers();
	BuildRootSignature();
	BuildPSOs();
	return true;
}

void ShapesApp::OnResize() {
	VulkApp::OnResize();
	mProj = glm::perspectiveFovLH(0.25f * pi, (float)mClientWidth, (float)mClientHeight, 1.0f, 1000.0f);
}

void ShapesApp::Update(const GameTimer& gt) {
	OnKeyboardInput(gt);
	UpdateCamera(gt);

	//Cycle through the circular frame resource array
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	vkWaitForFences(mDevice, 1, &mCurrFrameResource->Fence, VK_TRUE, UINT64_MAX);
	vkResetFences(mDevice, 1, &mCurrFrameResource->Fence);
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
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque_wireframe"]);
	}
	else {
		pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque"]);
	}

	

	DrawRenderItems(cmd, mOpaqueRitems);

	EndRender(cmd,mCurrFrameResource->Fence);

	
}

void ShapesApp::DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems) {
	VkDeviceSize obSize = mCurrFrameResource->ObjectCBSize;
	VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	VkDeviceSize objSize = ((uint32_t)sizeof(ObjectConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	uint32_t dynamicOffsets[1] = { 0 };
	pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsPC[mIndex], 1, dynamicOffsets);//bind PC data once
	for (size_t i = 0; i < ritems.size(); i++) {
		auto ri = ritems[i];
		uint32_t indexOffset = ri->startIndexLocation;
		

		const auto vbv = ri->Geo->vertexBufferGPU;
		pvkCmdBindVertexBuffers(cmd, 0, 1, &vbv.buffer, mOffsets);
		const auto ibv = ri->Geo->indexBufferGPU;
		pvkCmdBindIndexBuffer(cmd, ibv.buffer, indexOffset*sizeof(uint32_t), VK_INDEX_TYPE_UINT32);
		uint32_t cbvIndex = ri->ObjCBIndex;
		
			
		//uint32_t dynamicOffsets[2] = { 0,cbvIndex *objSize};
		dynamicOffsets[0] =(uint32_t)( cbvIndex * objSize);
		//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSetsOBs[mIndex], 2, dynamicOffsets);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 1, 1, &mDescriptorSetsOBs[mIndex], 1, dynamicOffsets);
		pvkCmdDrawIndexed(cmd, ri->indexCount, 1, 0, ri->BaseVertexLocation, 0);
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
	auto currObjectCB = mCurrFrameResource->ObjectCB;
	uint8_t* pObjConsts = (uint8_t*)mCurrFrameResource->pOCs;
	VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	VkDeviceSize objSize = ((uint32_t)sizeof(ObjectConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);
	for (auto& e : mAllRItems) {
		//Only update the cbuffer data if the constants have changed.
		//This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0) {
			glm::mat4 world = e->World;
			ObjectConstants objConstants;
			objConstants.World = world;
			memcpy((pObjConsts + (objSize * e->ObjCBIndex)), &objConstants, sizeof(objConstants));
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

void ShapesApp::BuildDescriptorHeaps() {
	//don't really build heaps, just setup descriptors

	std::vector<VkDescriptorSetLayoutBinding> bindings = {
		{0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr}, //Binding 0, uniform (constant) buffer
	//	{1,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,1,VK_SHADER_STAGE_VERTEX_BIT,nullptr}, //Binding 1, uniform (constant) buffer
	};
	mDescriptorSetLayoutPC = initDescriptorSetLayout(mDevice, bindings);
	mDescriptorSetLayoutOBs = initDescriptorSetLayout(mDevice, bindings);
	std::vector<VkDescriptorPoolSize> poolSizes{
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,6},
	//	{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,3},
	};
	uint32_t count = getSwapchainImageCount(mSurfaceCaps);
	mDescriptorPool = initDescriptorPool(mDevice, poolSizes,6);
	mDescriptorSetsPC.resize(count);
	initDescriptorSets(mDevice, mDescriptorSetLayoutPC, mDescriptorPool, mDescriptorSetsPC.data(), count);
	mDescriptorSetsOBs.resize(count);
	initDescriptorSets(mDevice, mDescriptorSetLayoutOBs, mDescriptorPool,mDescriptorSetsOBs.data(), count);
	
	

}

void ShapesApp::BuildFrameResources() {
	for (int i = 0; i < gNumFrameResources; i++) {
		mFrameResources.push_back(std::make_unique<FrameResource>(mDevice, mMemoryProperties, mDescriptorSetsPC[i],mDescriptorSetsOBs[i],(uint32_t) mDeviceProperties.limits.minUniformBufferOffsetAlignment, 1, (uint32_t)mAllRItems.size()));
	}
}

void writeObj(const char* objName, const char* folder, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices) {
	char fname[512];
	char mname[512];
	char tname[512];
	strcpy_s(fname, sizeof(fname), folder);
	strcat_s(fname, sizeof(fname), "/");
	strcpy_s(mname, sizeof(mname), folder);
	strcat_s(mname, sizeof(mname), "/");
	strcpy_s(tname, sizeof(tname), folder);
	strcat_s(tname, sizeof(tname), "/");

	strcat_s(fname, sizeof(fname), objName);
	strcat_s(mname, sizeof(mname), objName);
	strcat_s(tname, sizeof(tname), objName);
	strcat_s(fname, ".obj");
	strcat_s(mname, ".mtl");
	strcat_s(tname, ".jpg");
	std::ofstream file(fname);
	file << "# Mesh obj" << std::endl;
	//file << "mtllib " << objName << ".mtl" << std::endl;
	//file << "o " << objName << std::endl;

	for (auto& vertex : vertices) {
		file << "v " << vertex.Pos.x << " " << vertex.Pos.y << " " << vertex.Pos.z << std::endl;
	}
	/*for (auto& vertex : vertices) {
		file << "vn " << vertex.Normal.x << " " << vertex.Normal.y << " " << vertex.Normal.z << std::endl;
	}
	for (auto& vertex : vertices) {
		file << "vt " << vertex.Tex.u << " " << vertex.Tex.v << std::endl;
	}*/
	int matidx = 0;
	/*if (subsets.size() > 0) {
		std::map<int, std::vector<size_t>> faceSets;
		for (size_t i = 0; i < subsets.size(); i++) {
			faceSets[subsets[i]].push_back(i);
		}
		for (auto& pair : faceSets) {

			file << "usemtl mat" << matidx++ << std::endl;
			for (auto& f : pair.second) {
				size_t i = f * 3;
				file << "f " << indices[i + 0] + 1 << "/" << indices[i + 0] + 1 << "/" << indices[i + 0] + 1 << " " << indices[i + 1] + 1 << "/" << indices[i + 1] + 1 << "/" << indices[i + 1] + 1 << " " << indices[i + 2] + 1 << "/" << indices[i + 2] + 1 << "/" << indices[i + 2] + 1 << std::endl;
			}

		}
	}
	else*/ {
		file << "usemtl " << objName << std::endl;
		for (size_t i = 0; i < indices.size(); i += 3) {
			file << "f " << indices[i + 0] + 1 << "/" << indices[i + 0] + 1 << "/" << indices[i + 0] + 1 << " " << indices[i + 1] + 1 << "/" << indices[i + 1] + 1 << "/" << indices[i + 1] + 1 << " " << indices[i + 2] + 1 << "/" << indices[i + 2] + 1 << "/" << indices[i + 2] + 1 << std::endl;
		}
	}

	std::ofstream mat(mname);
	/*mat << "#Material Count: " << materials.size() << std::endl << std::endl;
	matidx = 0;
	for (auto& m : materials) {
		mat << "newmtl mat" << matidx++ << std::endl;
		mat << "Ns " << m.power << std::endl;
		mat << "Ka " << m.ambient.x << " " << m.ambient.y << " " << m.ambient.z << std::endl;
		mat << "Kd " << m.diffuse.x << " " << m.diffuse.y << " " << m.diffuse.z << std::endl;
		mat << "Ks " << m.specular.x << " " << m.specular.y << " " << m.specular.z << std::endl;
		mat << "Ni 1.45" << std::endl;
		mat << "d 1.0" << std::endl;
		mat << "illum 2" << std::endl;
		if (m.diffuseTex[0]) {
			mat << "map_Kd " << m.diffuseTex << std::endl;
		}
		mat << std::endl;
	}*/
	mat << "newmtl " << objName << std::endl;
	mat << "Ns 323.99994" << std::endl;
	mat << "Ka 1.000 1.00 1.000" << std::endl;
	mat << "Kd 0.8 0.8 0.8" << std::endl;
	mat << "Ks 0.5 0.5 0.5" << std::endl;
	mat << "Ni 1.45" << std::endl;
	mat << "d 1.0" << std::endl;
	mat << "illum 2" << std::endl;
	//mat << "map_Kd " << objName << ".jpg" << std::endl;
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

	void ShapesApp::BuildRootSignature() {
		//build pipeline layout
		std::vector<VkDescriptorSetLayout> layouts = {
			mDescriptorSetLayoutPC,
			mDescriptorSetLayoutOBs
		};
		mPipelineLayout = initPipelineLayout(mDevice, layouts);
	}

void ShapesApp::BuildPSOs() {
	std::vector<ShaderModule> shaders = {
		{initShaderModule(mDevice,"shaders/color.vert.spv"),VK_SHADER_STAGE_VERTEX_BIT},
		{initShaderModule(mDevice,"shaders/color.frag.spv"),VK_SHADER_STAGE_FRAGMENT_BIT}
	};
	VkPipeline opaquePipeline = initGraphicsPipeline(mDevice, mRenderPass, mPipelineLayout, shaders, Vertex::getInputBindingDescription(), Vertex::getInputAttributeDescription(), VK_CULL_MODE_FRONT_BIT,true,mMSAA ? mNumSamples : VK_SAMPLE_COUNT_1_BIT,VK_FALSE,VK_POLYGON_MODE_FILL);
	VkPipeline wireframePipeline = initGraphicsPipeline(mDevice, mRenderPass, mPipelineLayout, shaders, Vertex::getInputBindingDescription(), Vertex::getInputAttributeDescription(), VK_CULL_MODE_FRONT_BIT, true,mMSAA ? mNumSamples :  VK_SAMPLE_COUNT_1_BIT, VK_FALSE, VK_POLYGON_MODE_LINE);
	
	mPSOs["opaque"]=opaquePipeline;
	mPSOs["opaque_wireframe"] = wireframePipeline;


	cleanupShaderModule(mDevice, shaders[0].shaderModule);
	cleanupShaderModule(mDevice, shaders[1].shaderModule);
}

void ShapesApp::BuildRenderItems() {
	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->World = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(2.0f)), glm::vec3(0.0f, 0.5f, 0.0f));
	boxRitem->ObjCBIndex = 0;
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	boxRitem->indexCount = boxRitem->Geo->DrawArgs["box"].indexCount;
	boxRitem->startIndexLocation = boxRitem->Geo->DrawArgs["box"].startIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].baseVertexLocation;
	mAllRItems.push_back(std::move(boxRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = glm::mat4(1.0f);// glm::rotate(glm::mat4(1.0f), pi, glm::vec3(0.0f, 0.0f, 1.0f));
	gridRitem->ObjCBIndex = 1;
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	gridRitem->indexCount = gridRitem->Geo->DrawArgs["grid"].indexCount;
	gridRitem->startIndexLocation = gridRitem->Geo->DrawArgs["grid"].startIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].baseVertexLocation;
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
		leftCylRitem->indexCount = leftCylRitem->Geo->DrawArgs["cylinder"].indexCount;
		leftCylRitem->startIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].startIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].baseVertexLocation;

		rightCylRitem->World = leftCylWorld;
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Geo = mGeometries["shapeGeo"].get();
		rightCylRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		rightCylRitem->indexCount = rightCylRitem->Geo->DrawArgs["cylinder"].indexCount;
		rightCylRitem->startIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].startIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].baseVertexLocation;

		leftSphereRitem->World = leftSphereWorld;
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
		leftSphereRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		leftSphereRitem->indexCount = leftSphereRitem->Geo->DrawArgs["sphere"].indexCount;
		leftSphereRitem->startIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].startIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].baseVertexLocation;

		rightSphereRitem->World = rightSphereWorld;
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
		rightSphereRitem->PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		rightSphereRitem->indexCount = rightSphereRitem->Geo->DrawArgs["sphere"].indexCount;
		rightSphereRitem->startIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].startIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].baseVertexLocation;

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

void ShapesApp::BuildConstantBuffers() {
	

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