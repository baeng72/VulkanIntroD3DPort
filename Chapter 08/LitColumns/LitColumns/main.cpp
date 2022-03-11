#include "../../../Common/VulkApp.h"
#include "../../Common/VulkUtil.h"
#include "../../../Common/MathHelper.h"
#include "../../../Common/Colors.h"
#include "../../../Common/GeometryGenerator.h"
#include "FrameResource.h"


#include <memory>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ShaderProgram.h"

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

class LitColumnsApp :public  VulkApp {
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource{ nullptr };
	int mCurrFrameResourceIndex{ 0 };

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map < std::string, std::unique_ptr<Material>> mMaterials;
	//std::unordered_map<std::string, std::unique_ptr<Texture> > mTextures;

	std::unique_ptr<ShaderResources> pipelineRes;
	std::unique_ptr<ShaderProgram> prog;
	std::unique_ptr<PipelineLayout> pipelineLayout;
	std::unique_ptr<Pipeline> opaquePipeline;
	std::unique_ptr<Pipeline> wireframePipeline;

	std::unordered_map<std::string, VkPipeline> mPSOs;

	std::vector < std::unique_ptr<RenderItem>> mAllRitems;

	std::vector<RenderItem*> mOpaqueRitems;

	PassConstants mMainPassCB;

	bool mIsWireframe = false;

	glm::vec3 mEyePos = glm::vec3(0.0f);
	glm::mat4 mView = glm::mat4(1.0f);
	glm::mat4 mProj = glm::mat4(1.0f);

	float mTheta = 1.5f * pi;
	float mPhi = 0.2f*pi;
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
	void UpdateMaterialsCBs(const GameTimer& gt);

	void BuildShapeGeometry();
	void BuildSkullGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems);
public:
	LitColumnsApp(HINSTANCE hInstance);
	LitColumnsApp(const LitColumnsApp& rhs) = delete;
	LitColumnsApp& operator=(const LitColumnsApp& rhs) = delete;
	~LitColumnsApp();
	virtual bool Initialize()override;
	

};

LitColumnsApp::LitColumnsApp(HINSTANCE hInstance) :VulkApp(hInstance) {
	mAllowWireframe = true;
	mClearValues[0].color = Colors::LightSteelBlue;
	mMSAA = false;
}

LitColumnsApp::~LitColumnsApp() {
	vkDeviceWaitIdle(mDevice);
	for (auto& pair : mGeometries) {
		free(pair.second->indexBufferCPU);
		free(pair.second->vertexBufferCPU);
		cleanupBuffer(mDevice, pair.second->vertexBufferGPU);
		cleanupBuffer(mDevice, pair.second->indexBufferGPU);

	}


}


bool LitColumnsApp::Initialize() {
	if (!VulkApp::Initialize())
		return false;
	
	BuildShapeGeometry();
	BuildSkullGeometry();
	BuildMaterials();
	BuildRenderItems();

	BuildPSOs();
	BuildFrameResources();
	
	return true;
}

void LitColumnsApp::BuildShapeGeometry() {
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 0.5f, 1.5f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//We are concatenating all the geometry into one big vertex/index buffer.
	uint32_t boxVertexOffset = 0;
	uint32_t gridVertexOffset = (uint32_t)box.Vertices.size();
	uint32_t sphereVertexOffset = gridVertexOffset + (uint32_t)grid.Vertices.size();
	uint32_t cylinderVertexOffset = sphereVertexOffset + (uint32_t)sphere.Vertices.size();

	//cache the starting index for each object in the concatenated index buffer.
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
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
	}

	std::vector<std::uint32_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

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



	//initBuffer(mDevice, mMemoryProperties, vbByteSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, geo->vertexBufferGPU);
	//initBuffer(mDevice, mMemoryProperties, ibByteSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, geo->indexBufferGPU);
	//VkDeviceSize maxSize = std::max(vbByteSize, ibByteSize);
	//Buffer stagingBuffer;
	//initBuffer(mDevice, mMemoryProperties, maxSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer);

	//void* ptr = mapBuffer(mDevice, stagingBuffer);
	//memcpy(ptr, vertices.data(), vbByteSize);
	//CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->vertexBufferGPU, vbByteSize);
	//memcpy(ptr, indices.data(), ibByteSize);
	//CopyBufferTo(mDevice, mGraphicsQueue, mCommandBuffer, stagingBuffer, geo->indexBufferGPU, ibByteSize);
	//unmapBuffer(mDevice, stagingBuffer);
	//cleanupBuffer(mDevice, stagingBuffer);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void LitColumnsApp::BuildSkullGeometry() {
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


void LitColumnsApp::BuildMaterials() {
	auto bricks0 = std::make_unique<Material>();
	bricks0->NumFramesDirty = gNumFrameResources;
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseAlbedo = Colors::colortovec(Colors::ForestGreen);
	bricks0->FresnelR0 = glm::vec3(0.2f);
	bricks0->Roughness = 0.1f;

	auto stone0 = std::make_unique<Material>();
	stone0->NumFramesDirty = gNumFrameResources;
	stone0->Name = "stone0";
	stone0->MatCBIndex = 1;
	//stone0->DiffuseSrvHeapIndex = 1;
	stone0->DiffuseAlbedo = Colors::colortovec(Colors::LightSteelBlue);
	stone0->FresnelR0 = glm::vec3(0.05f);
	stone0->Roughness = 0.3f;

	auto tile0 = std::make_unique<Material>();
	tile0->NumFramesDirty = gNumFrameResources;
	tile0->Name = "tile0";
	tile0->MatCBIndex = 2;
	//tile0->DiffuseSrvHeapIndex = 2;
	tile0->DiffuseAlbedo = Colors::colortovec(Colors::LightGray);
	tile0->FresnelR0 = glm::vec3(0.02f);
	tile0->Roughness = 0.2f;

	auto skullMat = std::make_unique<Material>();
	skullMat->NumFramesDirty = gNumFrameResources;
	skullMat->Name = "skullMat";
	skullMat->MatCBIndex = 3;
	//skullMat->DiffuseSrvHeapIndex = 3;
	skullMat->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	skullMat->FresnelR0 = glm::vec3(0.05f);
	skullMat->Roughness = 0.3f;

	mMaterials["bricks0"] = std::move(bricks0);
	mMaterials["stone0"] = std::move(stone0);
	mMaterials["tile0"] = std::move(tile0);
	mMaterials["skullMat"] = std::move(skullMat);
}

void LitColumnsApp::BuildRenderItems() {
	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->World = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5, 0.0f)), glm::vec3(2.0f, 2.0f, 2.0f));
	boxRitem->TexTransform = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
	boxRitem->ObjCBIndex = 0;
	boxRitem->Mat = mMaterials["stone0"].get();
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = glm::mat4(1.0f);
	gridRitem->ObjCBIndex = 1;
	gridRitem->Mat = mMaterials["tile0"].get();
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mAllRitems.push_back(std::move(gridRitem));

	auto skullRitem = std::make_unique<RenderItem>();
	skullRitem->World = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(0.5f));
	//XMStoreFloat4x4(&skullRitem->World, XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(0.0f, 1.0f, 0.0f));
	skullRitem->TexTransform = MathHelper::Identity4x4();
	skullRitem->ObjCBIndex = 2;
	skullRitem->Mat = mMaterials["skullMat"].get();
	skullRitem->Geo = mGeometries["skullGeo"].get();
	//skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
	skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
	skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;
	mAllRitems.push_back(std::move(skullRitem));

	glm::mat4 brickTexTransform = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, 1.0f, 1.0f));
	UINT objCBIndex = 3;
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

		//XMStoreFloat4x4(&leftCylRitem->World, rightCylWorld);
		leftCylRitem->World = rightCylWorld;
		//XMStoreFloat4x4(&leftCylRitem->TexTransform, brickTexTransform);
		leftCylRitem->TexTransform = brickTexTransform;
		leftCylRitem->ObjCBIndex = objCBIndex++;
		leftCylRitem->Mat = mMaterials["bricks0"].get();
		leftCylRitem->Geo = mGeometries["shapeGeo"].get();
		//leftCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		//XMStoreFloat4x4(&rightCylRitem->World, leftCylWorld);
		rightCylRitem->World = leftCylWorld;
		//XMStoreFloat4x4(&rightCylRitem->TexTransform, brickTexTransform);
		rightCylRitem->TexTransform = brickTexTransform;
		rightCylRitem->ObjCBIndex = objCBIndex++;
		rightCylRitem->Mat = mMaterials["bricks0"].get();
		rightCylRitem->Geo = mGeometries["shapeGeo"].get();
		//rightCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		//XMStoreFloat4x4(&leftSphereRitem->World, leftSphereWorld);
		leftSphereRitem->World = leftSphereWorld;
		leftSphereRitem->TexTransform = MathHelper::Identity4x4();
		leftSphereRitem->ObjCBIndex = objCBIndex++;
		leftSphereRitem->Mat = mMaterials["stone0"].get();
		leftSphereRitem->Geo = mGeometries["shapeGeo"].get();
		//leftSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		//XMStoreFloat4x4(&rightSphereRitem->World, rightSphereWorld);
		rightSphereRitem->World = rightSphereWorld;
		rightSphereRitem->TexTransform = MathHelper::Identity4x4();
		rightSphereRitem->ObjCBIndex = objCBIndex++;
		rightSphereRitem->Mat = mMaterials["stone0"].get();
		rightSphereRitem->Geo = mGeometries["shapeGeo"].get();
		//rightSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
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



void LitColumnsApp::BuildPSOs() {
	pipelineRes = std::make_unique<ShaderResources>(mDevice, mDeviceProperties, mMemoryProperties);

	std::vector<std::vector<ShaderResource>> pipelineResourceInfos{
		{
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(PassConstants),1,true},
		},
		{
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,VK_SHADER_STAGE_VERTEX_BIT,sizeof(ObjectConstants),mAllRitems.size(),true},
		},
		{
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,sizeof(MaterialConstants),mMaterials.size(),true},
		},
	};
	pipelineRes->AddShaderResources(pipelineResourceInfos, gNumFrameResources);

	prog = std::make_unique<ShaderProgram>(mDevice);
	std::vector<const char*> shaderPaths = { "Shaders/default.vert.spv","Shaders/default.frag.spv" };
	prog->load(shaderPaths);

	pipelineLayout = std::make_unique<PipelineLayout>(mDevice, *pipelineRes);
	Vulkan::PipelineInfo pipelineInfo;
	pipelineInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	pipelineInfo.polygonMode = VK_POLYGON_MODE_FILL;
	pipelineInfo.depthTest = VK_TRUE;

	opaquePipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);

	pipelineInfo.polygonMode = VK_POLYGON_MODE_LINE;
	wireframePipeline = std::make_unique<Pipeline>(mDevice, mRenderPass, *prog, *pipelineLayout, pipelineInfo);

	mPSOs["opaque"] = *opaquePipeline;
	mPSOs["opaque_wireframe"] = *wireframePipeline;


}

void LitColumnsApp::BuildFrameResources() {

	void* pPassCB = pipelineRes->GetShaderResource(0).buffer.ptr;
	VkDeviceSize passSize = pipelineRes->GetShaderResource(0).buffer.objectSize;
	void* pObjectCB = pipelineRes->GetShaderResource(1).buffer.ptr;
	VkDeviceSize objectSize = pipelineRes->GetShaderResource(1).buffer.objectSize;
	void* pMatCB = pipelineRes->GetShaderResource(2).buffer.ptr;
	VkDeviceSize matSize = pipelineRes->GetShaderResource(2).buffer.objectSize;
	
	for (int i = 0; i < gNumFrameResources; i++) {

		//PassConstants* pc = (PassConstants*)mVulkanManager->GetBufferPtr(passConstantHash, 0);
		//ObjectConstants* oc = (ObjectConstants*)((uint8_t*)mVulkanManager->GetBufferPtr(objectConstantHash, 0)+i*mVulkanManager->GetBufferOffset(objectConstantHash,0));

		PassConstants* pc = (PassConstants*)((uint8_t*)pPassCB + passSize * i);
		ObjectConstants* oc = (ObjectConstants*)((uint8_t*)pObjectCB + objectSize * mAllRitems.size() * i);
		MaterialConstants* mc = (MaterialConstants*)((uint8_t*)pMatCB + matSize * mMaterials.size() * i);
		mFrameResources.push_back(std::make_unique<FrameResource>(pc, oc, mc));
	}
}


void LitColumnsApp::OnResize() {
	VulkApp::OnResize();
	mProj = glm::perspectiveFovLH(0.25f * pi, (float)mClientWidth, (float)mClientHeight, 1.0f, 1000.0f);
}

void LitColumnsApp::Update(const GameTimer& gt) {
	VulkApp::Update(gt);
	OnKeyboardInput(gt);
	UpdateCamera(gt);

	//Cycle through the circular frame resource array
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	
	UpdateObjectCBs(gt);
	UpdateMaterialsCBs(gt);
	UpdateMainPassCB(gt);
}

void LitColumnsApp::OnKeyboardInput(const GameTimer& gt)
{
	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;
}

void LitColumnsApp::UpdateCamera(const GameTimer& gt) {
	float x = mRadius * sinf(mPhi) * cosf(mTheta);
	float z = mRadius * sinf(mPhi) * sinf(mTheta);
	float y = mRadius * cosf(mPhi);

	mEyePos = glm::vec3(x, y, z);

	glm::vec3 pos = glm::vec3(x, y, z);
	glm::vec3 target = glm::vec3(0.0f);
	glm::vec3 up = glm::vec3(0.0, 1.0f, 0.0f);

	mView = glm::lookAtLH(pos, target, up);
}


void LitColumnsApp::UpdateObjectCBs(const GameTimer& gt) {

	uint8_t* pObjConsts = (uint8_t*)mCurrFrameResource->pOCs;
	VkDeviceSize objectSize = pipelineRes->GetShaderResource(1).buffer.objectSize;

	for (auto& e : mAllRitems) {
		//Only update the cbuffer data if the constants have changed.
		//This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0) {
			glm::mat4 world = e->World;
			ObjectConstants objConstants;
			objConstants.World = world;

			uint8_t* ptr = pObjConsts + e->ObjCBIndex * objectSize;
			memcpy(ptr, &objConstants, sizeof(objConstants));

			e->NumFramesDirty--;
		}
	}
}


void LitColumnsApp::UpdateMainPassCB(const GameTimer& gt) {
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
	mMainPassCB.AmbientLight = glm::vec4(0.25f, 0.25f, 0.35f, 1.0f);
	mMainPassCB.Lights[0].Direction = glm::vec3(0.57735f, -0.57735f, 0.577735f);
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };
	memcpy(pPassConstants, &mMainPassCB, sizeof(PassConstants));
}


void LitColumnsApp::UpdateMaterialsCBs(const GameTimer& gt) {
	/*VkDeviceSize minAlignmentSize = mDeviceProperties.limits.minUniformBufferOffsetAlignment;
	VkDeviceSize objSize = ((uint32_t)sizeof(MaterialConstants) + minAlignmentSize - 1) & ~(minAlignmentSize - 1);*/
	uint8_t* pMatConsts = (uint8_t*)mCurrFrameResource->pMats;	
	VkDeviceSize objSize = pipelineRes->GetShaderResource(2).buffer.objectSize;
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


void LitColumnsApp::OnMouseDown(WPARAM btnState, int x, int y) {
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void LitColumnsApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}

void LitColumnsApp::OnMouseMove(WPARAM btnState, int x, int y) {
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

void LitColumnsApp::Draw(const GameTimer& gt) {
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

	

	EndRender(cmd);// , mCurrFrameResource->Fence);


}

void LitColumnsApp::DrawRenderItems(VkCommandBuffer cmd, const std::vector<RenderItem*>& ritems) {
	uint32_t dynamicOffsets[1] = { 0 };
	VkDescriptorSet descriptor = pipelineRes->GetDescriptorSet(0, mCurrFrame);
	pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor, 1, dynamicOffsets);//bind PC data once
	//pvkCmdBindVertexBuffers(cmd, 0, 1, &VertexBuffer.buffer, mOffsets);
	VkDeviceSize objectSize = pipelineRes->GetShaderResource(1).buffer.objectSize;
	VkDeviceSize matSize = pipelineRes->GetShaderResource(2).buffer.objectSize;
	VkDescriptorSet descriptor2 = pipelineRes->GetDescriptorSet(1, mCurrFrame);
	VkDescriptorSet descriptor3 = pipelineRes->GetDescriptorSet(2, mCurrFrame);
	for (size_t i = 0; i < ritems.size(); i++) {
		auto ri = ritems[i];
		uint32_t indexOffset = ri->StartIndexLocation;

		const auto vbv = ri->Geo->vertexBufferGPU;
		const auto ibv = ri->Geo->indexBufferGPU;
		pvkCmdBindVertexBuffers(cmd, 0, 1, &vbv.buffer, mOffsets);


		pvkCmdBindIndexBuffer(cmd, ibv.buffer, indexOffset * sizeof(uint32_t), VK_INDEX_TYPE_UINT32);
		uint32_t cbvIndex = ri->ObjCBIndex;



		dynamicOffsets[0] = (uint32_t)(cbvIndex * objectSize);

		//VkDescriptorSet descriptor2 = pipelineRes->GetDescriptorSet(1, mCurrFrame);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 1, 1, &descriptor2, 1, dynamicOffsets);
		//VkDescriptorSet descriptor3 = pipelineRes->GetDescriptorSet(2, mCurrFrame);
		dynamicOffsets[0] = (uint32_t)(ri->Mat->MatCBIndex * matSize);
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor3, 1, dynamicOffsets);
		pvkCmdDrawIndexed(cmd, ri->IndexCount, 1, 0, ri->BaseVertexLocation, 0);
	}
}


int main() {
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		LitColumnsApp theApp(GetModuleHandle(NULL));
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
