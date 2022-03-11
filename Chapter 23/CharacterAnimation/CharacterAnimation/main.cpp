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
#include <future>
#include "FrameResource.h"
#include "ShadowMap.h"
#include "Ssao.h"
#include "SkinnedData.h"
#include "LoadM3d.h"




const int gNumFrameResources = 3;
//
//class PipelineLayoutCache : public VulkanObject {
//	struct PipelineLayoutInfo {
//		std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
//		size_t hash()const;
//	};
//	struct PipelineLayoutHash {
//		size_t operator() (const PipelineLayoutInfo& k)const{
//			return k.hash();
//		} 
//	};
//	std::unordered_map<PipelineLayoutInfo, VkPipelineLayout, PipelineLayoutHash> layoutCache;
//	
//	PipelineLayoutCache() = delete;
//public:
//	PipelineLayoutCache(VkDevice device_) :VulkanObject(device_) {}
//	~PipelineLayoutCache() {};
//	VkPipelineLayout create(std::vector<VkDescriptorSetLayout>& descriptorSets){
//		PipelineLayoutInfo layoutInfo;
//		uint32_t descriptorCount = (uint32_t)descriptorSets.size();
//		layoutInfo.descriptorSetLayouts.insert(layoutInfo.descriptorSetLayouts.begin(),descriptorSets.begin(), descriptorSets.end());
//		VkPipelineLayout pipelineLayout;
//		auto it = layoutCache.find(layoutInfo);
//		if (it != layoutCache.end()) {
//			pipelineLayout = (*it).second;
//		}
//		else {
//			pipelineLayout = Vulkan::initPipelineLayout(device, descriptorSets);
//			layoutCache[layoutInfo] = pipelineLayout;
//		}
//		return pipelineLayout;
//	}
//
//};


#define MAX_SETS 16
struct Binder {
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkDescriptorSet descriptorSets[MAX_SETS];
	uint32_t descriptorSetCount{ 0 };
	uint32_t dynamicOffsetsBase[MAX_SETS];	//hold base memory spot for dynamic offsets, need to be updated to be used, even if no change, call update(descriptorIndex)
	uint32_t dynamicOffsets[MAX_SETS];
	uint32_t dynamicOffsetCount{ 0 };
	uint32_t firstSet{ 0 };//index of first set, might not be zero

	uint32_t addDescriptorSet(VkDescriptorSet descriptorSet) {//insert set in required order
		assert(descriptorSetCount < MAX_SETS);
		uint32_t index = descriptorSetCount++;
		descriptorSets[index] = descriptorSet;
		return index;
	}
	uint32_t setDescriptorDynamicBase(uint32_t dynamicOffsetBase) {//set base for a dynamic offset, no checking yet, so must be in required order
		assert(dynamicOffsetCount < MAX_SETS);
		uint32_t index = dynamicOffsetCount++;
		dynamicOffsetsBase[index] = dynamicOffsetBase;
		return index;
	}
	void updateDynamicOffset(uint32_t dynamicOffsetIndex,uint32_t multiple=1, uint32_t offset=0) {
		dynamicOffsets[dynamicOffsetIndex] = dynamicOffsetsBase[dynamicOffsetIndex] * multiple + offset;//multiply by object add optional offset offset
	}
	
	void Bind(VkCommandBuffer cmd_) {
		vkCmdBindDescriptorSets(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, firstSet, descriptorSetCount, descriptorSets, dynamicOffsetCount, dynamicOffsets);
	}
};


struct Descriptors{
	VkDescriptorSet objectDescriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSet boneDescriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSet passDescriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSet materialDescriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSet textureArrayDescriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSet cubeMapDescriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSet shadowMapDescriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSet ssaoMapDescriptorSet{ VK_NULL_HANDLE };
};

struct SkinnedModelInstance
{


    SkinnedData* SkinnedInfo = nullptr;
    std::vector<glm::mat4> FinalTransforms;
    std::string ClipName;
    float TimePos = 0.0f;

    // Called every frame and increments the time position, interpolates the 
    // animations for each bone based on the current animation clip, and 
    // generates the final transforms which are ultimately set to the effect
    // for processing in the vertex shader.
    void UpdateSkinnedAnimation(float dt)
    {
        TimePos += dt;

        // Loop animation
        if (TimePos > SkinnedInfo->GetClipEndTime(ClipName))
            TimePos = 0.0f;

        // Compute the final transforms for this time position.
        SkinnedInfo->GetFinalTransforms(ClipName, TimePos, FinalTransforms);
    }
};

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

    // Only applicable to skinned render-items.
    uint32_t SkinnedCBIndex = -1;

    // nullptr if this render-item is not animated by skinned mesh.
    SkinnedModelInstance* SkinnedModelInst = nullptr;
};

enum class RenderLayer : int
{
    Opaque = 0,
    SkinnedOpaque,
    Debug,
    Sky,
    Count
};


class SkinnedMeshApp : public VulkApp
{
    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

	std::unique_ptr<DescriptorSetLayoutCache> descriptorSetLayoutCache;
	std::unique_ptr<DescriptorSetPoolCache> descriptorSetPoolCache;
	std::unique_ptr<VulkanUniformBuffer> uniformBuffer;
	std::unique_ptr<VulkanImageList> textures;
	std::unique_ptr<VulkanImageList> skinnedTextures;
	std::unique_ptr<VulkanImage> cubeMapTexture;
	std::unique_ptr<VulkanUniformBuffer> storageBuffer;
	std::unique_ptr<VulkanDescriptorList> uniformDescriptors;
	std::unique_ptr<VulkanDescriptorList> textureDescriptors;
	std::unique_ptr<VulkanDescriptorList> storageDescriptors;
	std::unique_ptr<VulkanDescriptorList> shadowDescriptors;
	std::unique_ptr<VulkanDescriptorList> ssaoUniformDescriptors;
	std::unique_ptr<VulkanDescriptorList> ssaoTextureDescriptors;
	std::unique_ptr<VulkanSampler> sampler;
	std::unique_ptr<VulkanPipelineLayout> pipelineLayout;
	
	std::unique_ptr<VulkanPipelineLayout> cubeMapPipelineLayout;
	std::unique_ptr<VulkanPipelineLayout> shadowPipelineLayout;
	std::unique_ptr<VulkanPipelineLayout> debugPipelineLayout;
	std::unique_ptr<VulkanPipelineLayout> ssaoPipelineLayout;
	std::unique_ptr<VulkanPipelineLayout> drawNormalsPipelineLayout;
	std::unique_ptr<VulkanPipeline> opaquePipeline;
	std::unique_ptr<VulkanPipeline> skinnedOpaquePipeline;
	std::unique_ptr<VulkanPipeline> wireframePipeline;
	std::unique_ptr<VulkanPipeline> skinnedWireframePipeline;
	std::unique_ptr<VulkanPipeline> opaqueFlatPipeline;
	std::unique_ptr<VulkanPipeline> skinnedOpaqueFlatPipeline;
	std::unique_ptr<VulkanPipeline> noSsaoPipeline;
	std::unique_ptr<VulkanPipeline> skinnedNoSsaoPipeline;
	std::unique_ptr<VulkanPipeline> cubeMapPipeline;
	std::unique_ptr<VulkanPipeline> shadowPipeline;
	std::unique_ptr<VulkanPipeline> skinnedShadowPipeline;
	std::unique_ptr<VulkanPipeline> debugPipeline;
	std::unique_ptr<VulkanPipeline> ssaoPipeline;
	std::unique_ptr<VulkanPipeline> ssaoBlurHorzPipeline;
	std::unique_ptr<VulkanPipeline> ssaoBlurVertPipeline;

	std::unique_ptr<VulkanPipeline> drawNormalsPipeline;
	std::unique_ptr<VulkanPipeline> skinnedDrawNormalsPipeline;

	Descriptors descriptorSets;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map < std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture> > mTextures;
	std::unordered_map<std::string, VkPipeline> mPSOs;

	bool mIsWireframe{ false };
	bool mIsFlatShader{ false };
	bool mNoSsao{ false };

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	float lastDelta = 0.0f;
	std::unordered_map<std::string, int> deltaMap;

	PassConstants mMainPassCB;  // index 0 of pass cbuffer.
	PassConstants mShadowPassCB;// index 1 of pass cbuffer.

	std::unique_ptr<SkinnedModelInstance> mSkinnedModelInst;
	SkinnedData mSkinnedInfo;
	std::vector<M3DLoader::Subset> mSkinnedSubsets;
	std::vector<M3DLoader::M3dMaterial> mSkinnedMats;
	std::vector<std::string> mSkinnedTextureNames;

	Camera mCamera;

	std::unique_ptr<ShadowMap> mShadowMap;

	std::unique_ptr<Ssao> mSsao;

	Sphere mSceneBounds;

	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	glm::vec3 mLightPosW;
	glm::mat4 mLightView = MathHelper::Identity4x4();
	glm::mat4 mLightProj = MathHelper::Identity4x4();
	glm::mat4 mShadowTransform = MathHelper::Identity4x4();

	float mLightRotationAngle = 0.0f;
	glm::vec3 mBaseLightDirections[3] = {
		glm::vec3(0.57735f, -0.57735f, 0.57735f),
		glm::vec3(-0.57735f, -0.57735f, 0.57735f),
		glm::vec3(0.0f, -0.707f, -0.707f)
	};
	glm::vec3 mRotatedLightDirections[3];

	POINT mLastMousePos;

	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);

	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateSkinnedCBs(const GameTimer& gt);
	void UpdateShadowTransform(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateShadowPassCB(const GameTimer& gt);
	void UpdateSsaoCB(const GameTimer& gt);
	void UpdateMaterialsBuffer(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);

	void LoadTextures();
	void LoadSkinnedModel();
	void BuildBuffers();
	void BuildDescriptors();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void BuildShapeGeometry();	
	void DrawRenderItems(VkCommandBuffer, VkPipelineLayout layout, const std::vector<RenderItem*>& ritems);
	void DrawSceneToShadowMap();
	void DrawNormalsAndDepth();

public:
	SkinnedMeshApp(HINSTANCE hInstance);
	SkinnedMeshApp(const SkinnedMeshApp& rhs) = delete;
	SkinnedMeshApp& operator=(const SkinnedMeshApp& rhs) = delete;
	~SkinnedMeshApp();

	virtual bool Initialize()override;
};

SkinnedMeshApp::SkinnedMeshApp(HINSTANCE hInstance) :VulkApp(hInstance) {
	mAllowWireframe = true;
	mClearValues[0].color = Colors::LightSteelBlue;
	mMSAA = false;
	mDepthBuffer = true;
	mDepthImageUsage = VK_IMAGE_USAGE_SAMPLED_BIT;//hack to allow sampling depth buffer
	// Estimate the scene bounding sphere manually since we know how the scene was constructed.
	// The grid is the "widest object" with a width of 20 and depth of 30.0f, and centered at
	// the world space origin.  In general, you need to loop over every world space vertex
	// position and compute the bounding sphere.
	mSceneBounds.Center = glm::vec3(0.0f, 0.0f, 0.0f);
	mSceneBounds.Radius = sqrtf(10.0f * 10.0f + 15.0f * 15.0f);
}

SkinnedMeshApp::~SkinnedMeshApp() {
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

bool SkinnedMeshApp::Initialize() {
	if (!VulkApp::Initialize())
		return false;


	mCamera.SetPosition(0.0f, 2.0f, -15.0f);

	mSsao = std::make_unique<Ssao>(mDevice, mMemoryProperties, mBackQueue, mCommandBuffer, mDepthImage.imageView, mDepthFormat, mClientWidth, mClientHeight);
	mShadowMap = std::make_unique<ShadowMap>(mDevice, mMemoryProperties, mBackQueue, mCommandBuffer, 2048, 2048);

	LoadSkinnedModel();
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

void SkinnedMeshApp::LoadSkinnedModel()
{
	std::vector<M3DLoader::SkinnedVertex> vertices;
	std::vector<std::uint32_t> indices;

	M3DLoader m3dLoader;
	m3dLoader.LoadM3d("Models\\soldier.m3d", vertices, indices,
		mSkinnedSubsets, mSkinnedMats, mSkinnedInfo);

	mSkinnedModelInst = std::make_unique<SkinnedModelInstance>();
	mSkinnedModelInst->SkinnedInfo = &mSkinnedInfo;
	mSkinnedModelInst->FinalTransforms.resize(mSkinnedInfo.BoneCount());
	mSkinnedModelInst->ClipName = "Take1";
	mSkinnedModelInst->TimePos = 0.0f;

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(SkinnedVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "Models\\soldier.m3d";

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

	
	geo->VertexByteStride = sizeof(SkinnedVertex);
	geo->VertexBufferByteSize = vbByteSize;	
	geo->IndexBufferByteSize = ibByteSize;

	for (uint32_t i = 0; i < (uint32_t)mSkinnedSubsets.size(); ++i)
	{
		SubmeshGeometry submesh;
		std::string name = "sm_" + std::to_string(i);

		submesh.IndexCount = (uint32_t)mSkinnedSubsets[i].FaceCount * 3;
		submesh.StartIndexLocation = mSkinnedSubsets[i].FaceStart * 3;
		submesh.BaseVertexLocation = 0;

		geo->DrawArgs[name] = submesh;
	}

	mGeometries[geo->Name] = std::move(geo);
}

void SkinnedMeshApp::LoadTextures() {
	std::vector<Vulkan::Image> texturesList;
	ImageLoader::begin(mDevice, mCommandBuffer, mBackQueue, mMemoryProperties)
		.addImage("../../../Textures/bricks2.png")
		.addImage("../../../Textures/bricks2_nmap.png")
		.addImage("../../../Textures/tile.png")
		.addImage("../../../Textures/tile_nmap.png")
		.addImage("../../../Textures/white1x1.png")
		.addImage("../../../Textures/default_nmap.png")
		.load(texturesList);
	textures = std::make_unique<VulkanImageList>(mDevice, texturesList);

	std::vector<std::string> texFilenames;
	// Add skinned model textures to list so we can reference by name later.
	for (size_t i = 0; i < mSkinnedMats.size(); ++i)
	{
		std::string diffuseName = mSkinnedMats[i].DiffuseMapName;
		std::string normalName = mSkinnedMats[i].NormalMapName;

		std::string diffuseFilename = "../../../Textures/" + diffuseName;
		std::string normalFilename = "../../../Textures/" + normalName;

		// strip off extension
		diffuseName = diffuseName.substr(0, diffuseName.find_last_of("."));
		normalName = normalName.substr(0, normalName.find_last_of("."));

		mSkinnedTextureNames.push_back(diffuseName);
		texFilenames.push_back(diffuseFilename);

		mSkinnedTextureNames.push_back(normalName);
		texFilenames.push_back(normalFilename);
	}
	ImageLoader loader = ImageLoader::begin(mDevice, mCommandBuffer, mBackQueue, mMemoryProperties);
	for (auto& skinnedName : mSkinnedTextureNames) {
		std::string path = "../../../Textures/"+skinnedName + ".png";
		loader.addImage(path.c_str());
	}
	loader.load(texturesList);
	skinnedTextures = std::make_unique<VulkanImageList>(mDevice, texturesList);


	ImageLoader::begin(mDevice, mCommandBuffer, mBackQueue, mMemoryProperties)
		.addImage("../../../Textures/desertcube1024-posx.png")
		.addImage("../../../Textures/desertcube1024-negx.png")
		.addImage("../../../Textures/desertcube1024-posy.png")
		.addImage("../../../Textures/desertcube1024-negy.png")
		.addImage("../../../Textures/desertcube1024-posz.png")
		.addImage("../../../Textures/desertcube1024-negz.png")
		.setIsCube(true)
		.load(texturesList);
	cubeMapTexture = std::make_unique <VulkanImage>(mDevice, texturesList[0]);
	Vulkan::SamplerProperties sampProps;
	sampProps.addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler = std::make_unique<VulkanSampler>(mDevice, Vulkan::initSampler(mDevice, sampProps));
	
}

void SkinnedMeshApp::BuildBuffers() {
	Vulkan::Buffer dynamicBuffer;
	//pass and object constants are dynamic uniform buffers
	std::vector<UniformBufferInfo> bufferInfo;
	UniformBufferBuilder::begin(mDevice, mDeviceProperties, mMemoryProperties, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, true)
		.AddBuffer(sizeof(ObjectConstants), mAllRitems.size(), gNumFrameResources)		
		.AddBuffer(sizeof(PassConstants), 2, gNumFrameResources)//one for main, one for shadow		
		.AddBuffer(sizeof(SsaoConstants), 1, gNumFrameResources)
		.AddBuffer(sizeof(glm::mat4) * 96, 1, gNumFrameResources)
		.build(dynamicBuffer, bufferInfo);
	uniformBuffer = std::make_unique<VulkanUniformBuffer>(mDevice, dynamicBuffer, bufferInfo);

	UniformBufferBuilder::begin(mDevice, mDeviceProperties, mMemoryProperties, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, true)
		.AddBuffer(sizeof(MaterialData), mMaterials.size(), gNumFrameResources)
		.build(dynamicBuffer, bufferInfo);
	storageBuffer = std::make_unique<VulkanUniformBuffer>(mDevice, dynamicBuffer, bufferInfo);
}

void SkinnedMeshApp::BuildDescriptors() {
	descriptorSetPoolCache = std::make_unique<DescriptorSetPoolCache>(mDevice); 
	descriptorSetLayoutCache = std::make_unique<DescriptorSetLayoutCache>(mDevice);

	VkDescriptorSet objectDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout objectDescriptorSetLayout = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(objectDescriptorSet, objectDescriptorSetLayout);

	VkDescriptorSet boneDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout boneDescriptorSetLayout = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT)
		.build(boneDescriptorSet, boneDescriptorSetLayout);

	VkDescriptorSet passCBDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout passCBDescriptorSetLayout = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(passCBDescriptorSet, passCBDescriptorSetLayout);

	VkDescriptorSet ssaoCBDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout ssaoCBDescriptorSetLayout = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(ssaoCBDescriptorSet, ssaoCBDescriptorSetLayout);
	

	std::vector<VkDescriptorSet> descriptors{ objectDescriptorSet, boneDescriptorSet, passCBDescriptorSet };
	uniformDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptors);

	VkDescriptorSet materialDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout materialDescriptorSetLayout = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(materialDescriptorSet, materialDescriptorSetLayout);

	descriptors = { materialDescriptorSet };
	storageDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptors);

	uint32_t textureCount = (uint32_t)(*textures + *skinnedTextures); //combine object and skinned textures

	VkDescriptorSet textureArraySet = VK_NULL_HANDLE;
	VkDescriptorSetLayout textureArraySetLayout = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
		.AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, textureCount)
		.build(textureArraySet, textureArraySetLayout);

	//cube map texture
	VkDescriptorSet cubeMapDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout cubeMapDescriptorSetLayout = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_SHADER_STAGE_FRAGMENT_BIT, (uint32_t)1)
		.build(cubeMapDescriptorSet, cubeMapDescriptorSetLayout);

	//ssao map
	VkDescriptorSet ssaoAmbientMap0Set = VK_NULL_HANDLE;
	VkDescriptorSetLayout ssaoAmbientMap0SetLayout = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (uint32_t)1)
		.build(ssaoAmbientMap0Set, ssaoAmbientMap0SetLayout);

	descriptors = { textureArraySet, cubeMapDescriptorSet, ssaoAmbientMap0Set };
	textureDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptors);

	VkDescriptorSet shadowDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout shadowDescriptorSetLayout = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (uint32_t)1)
		.build(shadowDescriptorSet, shadowDescriptorSetLayout);
	descriptors = { shadowDescriptorSet };
	shadowDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptors);

	//descriptor for ssao uniform buffer
	/*VkDescriptorSet ssaoUniformDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout ssaoUniformDescriptorSetLayout = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
		.build(ssaoUniformDescriptorSet, ssaoUniformDescriptorSetLayout);
	descriptors = { ssaoUniformDescriptorSet };
	ssaoUniformDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptors);*/
	descriptors = { ssaoCBDescriptorSet };
	ssaoUniformDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptors);

	//ssao textures
	VkDescriptorSet ssaoNormalMapSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout ssaoNormalMapSetLayout = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (uint32_t)1)
		.build(ssaoNormalMapSet, ssaoNormalMapSetLayout);
	VkDescriptorSet ssaoDepthMapSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout ssaoDepthMapSetLayout = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (uint32_t)1)
		.build(ssaoDepthMapSet, ssaoDepthMapSetLayout);
	VkDescriptorSet ssaoRandomVecMapSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout ssaoRandomVecMapSetLayout = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (uint32_t)1)
		.build(ssaoRandomVecMapSet, ssaoRandomVecMapSetLayout);
	VkDescriptorSet ssaoAmbientMap1Set = VK_NULL_HANDLE;
	VkDescriptorSetLayout ssaoAmbientMap1SetLayout = VK_NULL_HANDLE;
	DescriptorSetBuilder::begin(descriptorSetPoolCache.get(), descriptorSetLayoutCache.get())
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, (uint32_t)1)
		.build(ssaoAmbientMap1Set, ssaoAmbientMap1SetLayout);
	descriptors = { ssaoNormalMapSet,ssaoDepthMapSet,ssaoRandomVecMapSet,ssaoAmbientMap1Set};
	ssaoTextureDescriptors = std::make_unique<VulkanDescriptorList>(mDevice, descriptors);

	VkDeviceSize offset = 0;
	descriptors = { objectDescriptorSet, passCBDescriptorSet , ssaoCBDescriptorSet, boneDescriptorSet };
	std::vector<VkDescriptorSetLayout> descriptorLayouts = { objectDescriptorSetLayout,passCBDescriptorSetLayout,ssaoCBDescriptorSetLayout,boneDescriptorSetLayout };
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
		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), materialDescriptorSetLayout, materialDescriptorSet)
			.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &descrInfo)
			.update();
	}

	//update texture array 
	{
		auto& samp = *sampler;
		VkDescriptorImageInfo sampInfo;
		sampInfo.sampler = samp;
		auto& tl = *textures;
		std::vector<VkDescriptorImageInfo> imageInfos(textureCount);

		for (int i = 0; i < (int)tl; ++i) {
			imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfos[i].imageView = tl[i].imageView;
		}
		auto& st = *skinnedTextures;
		for (int i = 0; i < (int)st; ++i) {
			imageInfos[i + tl].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageInfos[i + tl].imageView = st[i].imageView;
		}
		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), textureArraySetLayout, textureArraySet)
			.AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLER, &sampInfo)
			.AddBinding(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfos.data(), (uint32_t)textureCount)
			.update();
	}
	//update cubemap descriptor set
	{

		VkDescriptorImageInfo imageInfo{};

		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = *cubeMapTexture;


		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), cubeMapDescriptorSetLayout, cubeMapDescriptorSet)
			.AddBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &imageInfo, (uint32_t)1)
			.update();
	}

	//update shadowmap descriptor
	{
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = mShadowMap->getRenderTargetView();
		imageInfo.sampler = mShadowMap->getRenderTargetSampler();
		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), shadowDescriptorSetLayout, shadowDescriptorSet)
			.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo, (uint32_t)1)
			.update();
	}
	//update ssao render target (used above in different shader)
	{
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = mSsao->getSsaoImageView(0);
		imageInfo.sampler = mSsao->getSsaoImageSampler(0);
		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), ssaoAmbientMap0SetLayout, ssaoAmbientMap0Set)
			.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo, (uint32_t)1)
			.update();
	}

	{
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = mSsao->getSsaoImageView(1);
		imageInfo.sampler = mSsao->getSsaoImageSampler(1);
		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), ssaoAmbientMap1SetLayout, ssaoAmbientMap1Set)
			.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo, (uint32_t)1)
			.update();
	}
	{
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = mSsao->getNormalMapImageView();
		imageInfo.sampler = mSsao->getNormalMapSampler();
		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), ssaoNormalMapSetLayout, ssaoNormalMapSet)
			.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo, (uint32_t)1)
			.update();

		auto& samp = *sampler;
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = mDepthImage.imageView;// mShadowMap->getRenderTargetView();
		imageInfo.sampler = samp;//resuse sampler
		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), ssaoDepthMapSetLayout, ssaoDepthMapSet)
			.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo, (uint32_t)1)
			.update();

		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = mSsao->getRandomMapImageView();
		imageInfo.sampler = mSsao->getRandomMapSampler();
		DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), ssaoRandomVecMapSetLayout, ssaoRandomVecMapSet)
			.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &imageInfo, (uint32_t)1)
			.update();

	}
	//{
	//	//update ssao uniformdescriptors
	//	auto& ub = *uniformBuffer;


	//	VkDeviceSize range = ub[3].objectSize;// bufferInfo[i].objectCount* bufferInfo[i].objectSize* bufferInfo[i].repeatCount;
	//	VkDeviceSize bufferSize = ub[3].objectCount * ub[3].objectSize * ub[3].repeatCount;
	//	VkDescriptorBufferInfo descrInfo{};
	//	descrInfo.buffer = ub;
	//	descrInfo.offset = offset;
	//	descrInfo.range = ub[3].objectSize;
	//	//descriptorBufferInfo.push_back(descrInfo);
	//	offset += bufferSize;		
	//	DescriptorSetUpdater::begin(descriptorSetLayoutCache.get(), ssaoCBDescriptorSetLayout, ssaoCBDescriptorSet)
	//		.AddBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &descrInfo)
	//		.update();

	//}
	
	VkPipelineLayout layout{ VK_NULL_HANDLE };
	PipelineLayoutBuilder::begin(mDevice)
		.AddDescriptorSetLayout(objectDescriptorSetLayout)
		.AddDescriptorSetLayout(boneDescriptorSetLayout)
		.AddDescriptorSetLayout(passCBDescriptorSetLayout)
		.AddDescriptorSetLayout(materialDescriptorSetLayout)
		.AddDescriptorSetLayout(textureArraySetLayout)
		.build(layout);
	shadowPipelineLayout = std::make_unique<VulkanPipelineLayout>(mDevice, layout);

	PipelineLayoutBuilder::begin(mDevice)
		.AddDescriptorSetLayout(objectDescriptorSetLayout)
		.AddDescriptorSetLayout(boneDescriptorSetLayout)
		.AddDescriptorSetLayout(passCBDescriptorSetLayout)
		.AddDescriptorSetLayout(materialDescriptorSetLayout)
		.AddDescriptorSetLayout(textureArraySetLayout)
		.AddDescriptorSetLayout(cubeMapDescriptorSetLayout)
		.build(layout);
	cubeMapPipelineLayout = std::make_unique<VulkanPipelineLayout>(mDevice, layout);

	PipelineLayoutBuilder::begin(mDevice)
		.AddDescriptorSetLayout(objectDescriptorSetLayout)
		.AddDescriptorSetLayout(boneDescriptorSetLayout)
		.AddDescriptorSetLayout(passCBDescriptorSetLayout)
		.AddDescriptorSetLayout(materialDescriptorSetLayout)
		.AddDescriptorSetLayout(textureArraySetLayout)
		.AddDescriptorSetLayout(cubeMapDescriptorSetLayout)
		.AddDescriptorSetLayout(shadowDescriptorSetLayout)
		.AddDescriptorSetLayout(ssaoAmbientMap0SetLayout)
		.build(layout);
	pipelineLayout = std::make_unique<VulkanPipelineLayout>(mDevice, layout);

	
	//cache for later
	descriptorSets.objectDescriptorSet = objectDescriptorSet;
	descriptorSets.boneDescriptorSet = boneDescriptorSet;
	descriptorSets.passDescriptorSet = passCBDescriptorSet;
	descriptorSets.materialDescriptorSet = materialDescriptorSet;
	descriptorSets.textureArrayDescriptorSet = textureArraySet;
	descriptorSets.cubeMapDescriptorSet = cubeMapDescriptorSet;
	descriptorSets.shadowMapDescriptorSet = shadowDescriptorSet;
	descriptorSets.ssaoMapDescriptorSet = ssaoAmbientMap0Set;

	PipelineLayoutBuilder::begin(mDevice)
		.AddDescriptorSetLayout(objectDescriptorSetLayout)
		.AddDescriptorSetLayout(boneDescriptorSetLayout)
		.AddDescriptorSetLayout(passCBDescriptorSetLayout)
		.AddDescriptorSetLayout(materialDescriptorSetLayout)
		.AddDescriptorSetLayout(textureArraySetLayout)
		.AddDescriptorSetLayout(cubeMapDescriptorSetLayout)
		.AddDescriptorSetLayout(shadowDescriptorSetLayout)
		.AddDescriptorSetLayout(ssaoAmbientMap0SetLayout)
		.build(layout);
	debugPipelineLayout = std::make_unique<VulkanPipelineLayout>(mDevice, layout);

	PipelineLayoutBuilder::begin(mDevice)
		.AddDescriptorSetLayout(ssaoCBDescriptorSetLayout)
		.AddDescriptorSetLayout(ssaoNormalMapSetLayout)
		.AddDescriptorSetLayout(ssaoDepthMapSetLayout)
		.AddDescriptorSetLayout(ssaoRandomVecMapSetLayout)
		.build(layout);
	ssaoPipelineLayout = std::make_unique<VulkanPipelineLayout>(mDevice, layout);

	PipelineLayoutBuilder::begin(mDevice)
		.AddDescriptorSetLayout(objectDescriptorSetLayout)
		.AddDescriptorSetLayout(boneDescriptorSetLayout)
		.AddDescriptorSetLayout(passCBDescriptorSetLayout)
		.AddDescriptorSetLayout(materialDescriptorSetLayout)
		.AddDescriptorSetLayout(textureArraySetLayout)
		.build(layout);
	drawNormalsPipelineLayout = std::make_unique<VulkanPipelineLayout>(mDevice, layout);


}

void SkinnedMeshApp::BuildPSOs() {
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
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 3)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 1)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0)
			.build(pipeline);
		opaquePipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["opaque"] = *opaquePipeline;

		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 3)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0)
			.build(pipeline);
		opaqueFlatPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["opaqueFlat"] = *opaqueFlatPipeline;
		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 3)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 1)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0)
			.build(pipeline);
		noSsaoPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["opaqueNoSsao"] = *noSsaoPipeline;
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
	{
		std::vector<Vulkan::ShaderModule> shaders;
		VkVertexInputBindingDescription vertexInputDescription = {};
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
		ShaderProgramLoader::begin(mDevice)
			.AddShaderPath("Shaders/defaultskinned.vert.spv")
			.AddShaderPath("Shaders/defaultskinned.frag.spv")
			.load(shaders, vertexInputDescription, vertexAttributeDescriptions);


		VkPipeline pipeline{ VK_NULL_HANDLE };
		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 3)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 1)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0)
			.build(pipeline);
		skinnedOpaquePipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["skinned_opaque"] = *skinnedOpaquePipeline;

		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 3)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0)
			.build(pipeline);
		skinnedOpaqueFlatPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["skinned_opaqueFlat"] = *skinnedOpaqueFlatPipeline;
		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 3)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 1)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0)
			.build(pipeline);
		skinnedNoSsaoPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["skinned_opaqueNoSsao"] = *skinnedNoSsaoPipeline;
		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_LINE)
			.setDepthTest(VK_TRUE)
			.build(pipeline);
		skinnedWireframePipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["skinned_opaque_wireframe"] = *skinnedWireframePipeline;
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
		PipelineBuilder::begin(mDevice, *pipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_BACK_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL)
			.build(pipeline);
		cubeMapPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);

		mPSOs["sky"] = *cubeMapPipeline;

		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
	}
	{
		std::vector<Vulkan::ShaderModule> shaders;
		VkVertexInputBindingDescription vertexInputDescription = {};
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
		ShaderProgramLoader::begin(mDevice)
			.AddShaderPath("Shaders/shadow.vert.spv")
			.AddShaderPath("Shaders/shadow.frag.spv")
			.load(shaders, vertexInputDescription, vertexAttributeDescriptions);
		VkPipeline pipeline = VK_NULL_HANDLE;

		PipelineBuilder::begin(mDevice, *shadowPipelineLayout, mShadowMap->getRenderPass(), shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_BACK_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL)
			.build(pipeline);
		shadowPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);

		mPSOs["shadow_opaque"] = *shadowPipeline;

		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
	}
	{
		std::vector<Vulkan::ShaderModule> shaders;
		VkVertexInputBindingDescription vertexInputDescription = {};
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
		ShaderProgramLoader::begin(mDevice)
			.AddShaderPath("Shaders/shadowskinned.vert.spv")
			.AddShaderPath("Shaders/shadowskinned.frag.spv")
			.load(shaders, vertexInputDescription, vertexAttributeDescriptions);
		VkPipeline pipeline = VK_NULL_HANDLE;

		PipelineBuilder::begin(mDevice, *shadowPipelineLayout, mShadowMap->getRenderPass(), shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_BACK_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL)
			.build(pipeline);
		skinnedShadowPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);

		mPSOs["skinned_shadow_opaque"] = *skinnedShadowPipeline;

		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
	}
	{
		std::vector<Vulkan::ShaderModule> shaders;
		VkVertexInputBindingDescription vertexInputDescription = {};
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
		ShaderProgramLoader::begin(mDevice)
			.AddShaderPath("Shaders/debug.vert.spv")
			.AddShaderPath("Shaders/debug.frag.spv")
			.load(shaders, vertexInputDescription, vertexAttributeDescriptions);
		VkPipeline pipeline = VK_NULL_HANDLE;
		PipelineBuilder::begin(mDevice, *debugPipelineLayout, mRenderPass, shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL)
			.build(pipeline);
		debugPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["debug"] = *debugPipeline;

		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
	}
	{
		//pipeline for normals?
		std::vector<Vulkan::ShaderModule> shaders;
		VkVertexInputBindingDescription vertexInputDescription = {};
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
		ShaderProgramLoader::begin(mDevice)
			.AddShaderPath("Shaders/DrawNormals.vert.spv")
			.AddShaderPath("Shaders/DrawNormals.frag.spv")
			.load(shaders, vertexInputDescription, vertexAttributeDescriptions);
		VkPipeline pipeline = VK_NULL_HANDLE;
		PipelineBuilder::begin(mDevice, *drawNormalsPipelineLayout, mSsao->getNormalRenderPass(), shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setDepthCompareOp(VK_COMPARE_OP_LESS)
			.build(pipeline);
		drawNormalsPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["drawNormals"] = *drawNormalsPipeline;

		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
	}
	{
		//pipeline for normals?
		std::vector<Vulkan::ShaderModule> shaders;
		VkVertexInputBindingDescription vertexInputDescription = {};
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
		ShaderProgramLoader::begin(mDevice)
			.AddShaderPath("Shaders/DrawNormalsSkinned.vert.spv")
			.AddShaderPath("Shaders/DrawNormalsSkinned.frag.spv")
			.load(shaders, vertexInputDescription, vertexAttributeDescriptions);
		VkPipeline pipeline = VK_NULL_HANDLE;
		PipelineBuilder::begin(mDevice, *drawNormalsPipelineLayout, mSsao->getNormalRenderPass(), shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setDepthTest(VK_TRUE)
			.setDepthCompareOp(VK_COMPARE_OP_LESS)
			.build(pipeline);
		skinnedDrawNormalsPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["skinned_drawNormals"] = *skinnedDrawNormalsPipeline;

		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
	}
	{
		//pipeline for ssao
		std::vector<Vulkan::ShaderModule> shaders;
		VkVertexInputBindingDescription vertexInputDescription = {};
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
		ShaderProgramLoader::begin(mDevice)
			.AddShaderPath("Shaders/Ssao.vert.spv")
			.AddShaderPath("Shaders/Ssao.frag.spv")
			.load(shaders, vertexInputDescription, vertexAttributeDescriptions);
		VkPipeline pipeline = VK_NULL_HANDLE;
		PipelineBuilder::begin(mDevice, *ssaoPipelineLayout, mSsao->getSsaoRenderPass(), shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.build(pipeline);
		ssaoPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["ssao"] = *ssaoPipeline;

		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
	}
	{
		//pipeline for ssao
		std::vector<Vulkan::ShaderModule> shaders;
		VkVertexInputBindingDescription vertexInputDescription = {};
		std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
		ShaderProgramLoader::begin(mDevice)
			.AddShaderPath("Shaders/SsaoBlur.vert.spv")
			.AddShaderPath("Shaders/SsaoBlur.frag.spv")
			.load(shaders, vertexInputDescription, vertexAttributeDescriptions);
		VkPipeline pipeline = VK_NULL_HANDLE;
		PipelineBuilder::begin(mDevice, *ssaoPipelineLayout, mSsao->getSsaoRenderPass(), shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 1)
			.build(pipeline);
		ssaoBlurHorzPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["ssaoBlurHorz"] = *ssaoBlurHorzPipeline;
		PipelineBuilder::begin(mDevice, *ssaoPipelineLayout, mSsao->getSsaoRenderPass(), shaders, vertexInputDescription, vertexAttributeDescriptions)
			.setCullMode(VK_CULL_MODE_FRONT_BIT)
			.setPolygonMode(VK_POLYGON_MODE_FILL)
			.setSpecializationConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0)
			.build(pipeline);
		ssaoBlurVertPipeline = std::make_unique<VulkanPipeline>(mDevice, pipeline);
		mPSOs["ssaoBlurVert"] = *ssaoBlurVertPipeline;
		for (auto& shader : shaders) {
			Vulkan::cleanupShaderModule(mDevice, shader.shaderModule);
		}
	}
}


void SkinnedMeshApp::BuildShapeGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
	GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
	UINT quadVertexOffset = cylinderVertexOffset + (UINT)cylinder.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
	UINT quadIndexOffset = cylinderIndexOffset + (UINT)cylinder.Indices32.size();

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

	SubmeshGeometry quadSubmesh;
	quadSubmesh.IndexCount = (UINT)quad.Indices32.size();
	quadSubmesh.StartIndexLocation = quadIndexOffset;
	quadSubmesh.BaseVertexLocation = quadVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size() +
		quad.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TangentU = box.Vertices[i].TangentU;
		vertices[k].TexC = box.Vertices[i].TexC;
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
		vertices[k].TangentU = grid.Vertices[i].TangentU;
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
		vertices[k].TangentU = sphere.Vertices[i].TangentU;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
		vertices[k].TangentU = cylinder.Vertices[i].TangentU;


	}
	for (int i = 0; i < quad.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = quad.Vertices[i].Position;
		vertices[k].Normal = quad.Vertices[i].Normal;
		vertices[k].TexC = quad.Vertices[i].TexC;
		vertices[k].TangentU = quad.Vertices[i].TangentU;
	}

	std::vector<std::uint32_t> indices;
	indices.insert(indices.end(), std::begin(box.Indices32), std::end(box.Indices32));
	indices.insert(indices.end(), std::begin(grid.Indices32), std::end(grid.Indices32));
	indices.insert(indices.end(), std::begin(sphere.Indices32), std::end(sphere.Indices32));
	indices.insert(indices.end(), std::begin(cylinder.Indices32), std::end(cylinder.Indices32));
	indices.insert(indices.end(), std::begin(quad.Indices32), std::end(quad.Indices32));

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

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

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;

	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
	geo->DrawArgs["quad"] = quadSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}


void SkinnedMeshApp::BuildFrameResources() {
	for (int i = 0; i < gNumFrameResources; i++) {
		auto& ub = *uniformBuffer;
		auto& sb = *storageBuffer;
		PassConstants* pc = (PassConstants*)((uint8_t*)ub[1].ptr + ub[1].objectSize * ub[1].objectCount * i);// ((uint8_t*)pPassCB + passSize * i);
		ObjectConstants* oc = (ObjectConstants*)((uint8_t*)ub[0].ptr + ub[0].objectSize * ub[0].objectCount * i);// ((uint8_t*)pObjectCB + objectSize * mAllRitems.size() * i);
		SkinnedConstants* sk = (SkinnedConstants*)((uint8_t*)ub[3].ptr + ub[3].objectSize * ub[3].objectCount * i);
		SsaoConstants* sc = (SsaoConstants*)((uint8_t*)ub[2].ptr + ub[2].objectSize * ub[2].objectCount * i);
		MaterialData* md = (MaterialData*)((uint8_t*)sb[0].ptr + sb[0].objectSize * sb[0].objectCount * i);


		mFrameResources.push_back(std::make_unique<FrameResource>(pc, oc,sk, sc, md));// , pWv));
	}
}

void SkinnedMeshApp::BuildMaterials()
{
	auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	bricks0->NumFramesDirty = gNumFrameResources;
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->NormalSrvHeapIndex = 1;
	bricks0->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = glm::vec3(0.1f, 0.1f, 0.1f);
	bricks0->Roughness = 0.3f;

	auto tile0 = std::make_unique<Material>();
	tile0->NumFramesDirty = gNumFrameResources;
	tile0->Name = "tile0";
	tile0->MatCBIndex = 1;
	tile0->DiffuseSrvHeapIndex = 2;
	tile0->NormalSrvHeapIndex = 3;
	tile0->DiffuseAlbedo = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
	tile0->FresnelR0 = glm::vec3(0.2f, 0.2f, 0.2f);
	tile0->Roughness = 0.1f;

	auto mirror0 = std::make_unique<Material>();
	mirror0->NumFramesDirty = gNumFrameResources;
	mirror0->Name = "mirror0";
	mirror0->MatCBIndex = 2;
	mirror0->DiffuseSrvHeapIndex = 4;
	mirror0->NormalSrvHeapIndex = 5;
	mirror0->DiffuseAlbedo = glm::vec4(0.0f, 0.0f, 0.1f, 1.0f);
	mirror0->FresnelR0 = glm::vec3(0.98f, 0.97f, 0.95f);
	mirror0->Roughness = 0.1f;

	
	auto sky = std::make_unique<Material>();
	sky->NumFramesDirty = gNumFrameResources;
	sky->Name = "sky";
	sky->MatCBIndex = 3;
	sky->DiffuseSrvHeapIndex = 6;
	sky->NormalSrvHeapIndex = 7;
	sky->DiffuseAlbedo = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
	sky->FresnelR0 = glm::vec3(0.1f, 0.1f, 0.1f);
	sky->Roughness = 1.0f;

	mMaterials["bricks0"] = std::move(bricks0);
	mMaterials["tile0"] = std::move(tile0);
	mMaterials["mirror0"] = std::move(mirror0);	
	mMaterials["sky"] = std::move(sky);

	UINT matCBIndex = 4;
	UINT srvHeapIndex = 6;
	for (UINT i = 0; i < mSkinnedMats.size(); ++i)
	{
		auto mat = std::make_unique<Material>();
		mat->NumFramesDirty = gNumFrameResources;
		mat->Name = mSkinnedMats[i].Name;
		mat->MatCBIndex = matCBIndex++;
		mat->DiffuseSrvHeapIndex = srvHeapIndex++;
		mat->NormalSrvHeapIndex = srvHeapIndex++;
		mat->DiffuseAlbedo = mSkinnedMats[i].DiffuseAlbedo;
		mat->FresnelR0 = mSkinnedMats[i].FresnelR0;
		mat->Roughness = mSkinnedMats[i].Roughness;

		mMaterials[mat->Name] = std::move(mat);
	}
}


void SkinnedMeshApp::BuildRenderItems()
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

	auto quadRitem = std::make_unique<RenderItem>();
	quadRitem->World = MathHelper::Identity4x4();
	quadRitem->TexTransform = MathHelper::Identity4x4();
	quadRitem->ObjCBIndex = 1;
	quadRitem->Mat = mMaterials["bricks0"].get();
	quadRitem->Geo = mGeometries["shapeGeo"].get();

	quadRitem->IndexCount = quadRitem->Geo->DrawArgs["quad"].IndexCount;
	quadRitem->StartIndexLocation = quadRitem->Geo->DrawArgs["quad"].StartIndexLocation;
	quadRitem->BaseVertexLocation = quadRitem->Geo->DrawArgs["quad"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Debug].push_back(quadRitem.get());
	mAllRitems.push_back(std::move(quadRitem));


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


	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = MathHelper::Identity4x4();
	gridRitem->TexTransform = glm::scale(glm::mat4(1.0f), glm::vec3(8.0f, 8.0f, 1.0f));
	gridRitem->ObjCBIndex = 3;
	gridRitem->Mat = mMaterials["tile0"].get();
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());
	mAllRitems.push_back(std::move(gridRitem));

	glm::mat4 brickTexTransform = glm::scale(glm::mat4(1.0f), glm::vec3(1.5f, 2.0f, 1.0f));
	UINT objCBIndex = 4;
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
		leftSphereRitem->TexTransform = MathHelper::Identity4x4();
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
		mRitemLayer[(int)RenderLayer::Opaque].push_back(leftSphereRitem.get());
		mRitemLayer[(int)RenderLayer::Opaque].push_back(rightSphereRitem.get());

		mAllRitems.push_back(std::move(leftCylRitem));
		mAllRitems.push_back(std::move(rightCylRitem));
		mAllRitems.push_back(std::move(leftSphereRitem));
		mAllRitems.push_back(std::move(rightSphereRitem));
	}

	for (UINT i = 0; i < mSkinnedMats.size(); ++i)
	{
		std::string submeshName = "sm_" + std::to_string(i);

		auto ritem = std::make_unique<RenderItem>();

		// Reflect to change coordinate system from the RHS the data was exported out as.
		glm::mat4 modelScale = glm::scale(glm::mat4(1.0f),glm::vec3(0.05f, 0.05f, -0.05f));
		glm::mat4 modelRot = glm::rotate(glm::mat4(1.0f),MathHelper::Pi,glm::vec3(0.0,1.0f,0.0f));
		glm::mat4 modelOffset = glm::translate(glm::mat4(1.0f),glm::vec3(0.0f, 0.0f, -5.0f));
		//XMStoreFloat4x4(&ritem->World, modelScale * modelRot * modelOffset);
		ritem->World = modelOffset * modelRot * modelScale;// modelScale* modelRot* modelOffset;

		ritem->TexTransform = MathHelper::Identity4x4();
		ritem->ObjCBIndex = objCBIndex++;
		ritem->Mat = mMaterials[mSkinnedMats[i].Name].get();
		ritem->Geo = mGeometries["Models\\soldier.m3d"].get();
		
		ritem->IndexCount = ritem->Geo->DrawArgs[submeshName].IndexCount;
		ritem->StartIndexLocation = ritem->Geo->DrawArgs[submeshName].StartIndexLocation;
		ritem->BaseVertexLocation = ritem->Geo->DrawArgs[submeshName].BaseVertexLocation;

		// All render items for this solider.m3d instance share
		// the same skinned model instance.
		ritem->SkinnedCBIndex = 0;
		ritem->SkinnedModelInst = mSkinnedModelInst.get();

		mRitemLayer[(int)RenderLayer::SkinnedOpaque].push_back(ritem.get());
		mAllRitems.push_back(std::move(ritem));
	}
}

void SkinnedMeshApp::Update(const GameTimer& gt) {
	VulkApp::Update(gt);
	

	//Cycle through the circular frame resource array
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	OnKeyboardInput(gt);
	//
	// Animate the lights (and hence shadows).
	//

	mLightRotationAngle += 0.1f * gt.DeltaTime();
	glm::mat4 R = glm::rotate(glm::mat4(1.0f), mLightRotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
	for (int i = 0; i < 3; ++i) {
		glm::vec3 lightDir = R * glm::vec4(mBaseLightDirections[i], 0.0f);
		mRotatedLightDirections[i] = lightDir;

	}

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateSkinnedCBs(gt);
	UpdateMaterialsBuffer(gt);
	UpdateShadowTransform(gt);
	UpdateMainPassCB(gt);
	UpdateShadowPassCB(gt);
	UpdateSsaoCB(gt);
	
}

void SkinnedMeshApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();
	if (GetAsyncKeyState('P') & 0x8000)
		saveScreenCap(mDevice, mCommandBuffer, mBackQueue, mSwapchainImages[mIndex], mFormatProperties, mSwapchainFormat.format, mSwapchainExtent, mFrameCount);

	if (GetAsyncKeyState('1') & 0x8000)
		mIsWireframe = true;
	else
		mIsWireframe = false;
	if (GetAsyncKeyState('2') & 0x8000)
		mIsFlatShader = true;
	else
		mIsFlatShader = false;
	mNoSsao = (GetAsyncKeyState('3') & 0x8000) ? true : false;



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

void SkinnedMeshApp::AnimateMaterials(const GameTimer& gt)
{

}


void  SkinnedMeshApp::UpdateObjectCBs(const GameTimer& gt) {

	uint8_t* pObjConsts = (uint8_t*)mCurrFrameResource->pOCs;
	auto& ub = *uniformBuffer;
	VkDeviceSize objSize = ub[0].objectSize;
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

void SkinnedMeshApp::UpdateSkinnedCBs(const GameTimer& gt)
{
	auto currSkinnedCB = mCurrFrameResource->pSCs;
	float delta = gt.DeltaTime();
	lastDelta = delta;
	//std::string deltaStr = std::to_string(delta);
	/*if (deltaMap.find( deltaStr) != deltaMap.end()) {
		deltaMap[deltaStr]++;
	}
	else {
		deltaMap.insert(std::pair<std::string, int>(deltaStr, 1));
	}*/
	// We only have one skinned model being animated.
	mSkinnedModelInst->UpdateSkinnedAnimation(lastDelta);

	//SkinnedConstants skinnedConstants;
	/*std::copy(
		std::begin(mSkinnedModelInst->FinalTransforms),
		std::end(mSkinnedModelInst->FinalTransforms),
		&skinnedConstants.BoneTransforms[0]);*/
	
	
	//memcpy(currSkinnedCB, &skinnedConstants.BoneTransforms, sizeof(glm::mat4) * mSkinnedModelInst->FinalTransforms.size());// sizeof(skinnedConstants.BoneTransforms));
	memcpy(currSkinnedCB, mSkinnedModelInst->FinalTransforms.data(), sizeof(glm::mat4) * mSkinnedModelInst->FinalTransforms.size());
	
}

void SkinnedMeshApp::UpdateShadowTransform(const GameTimer& gt)
{
	// Only the first "main" light casts a shadow.
	glm::vec3 lightDir = mRotatedLightDirections[0];
	glm::vec3 lightPos = -2.0f * mSceneBounds.Radius * lightDir;
	glm::vec3 targetPos = mSceneBounds.Center;
	glm::vec3 lightUp = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::mat4 lightView = glm::lookAtLH(lightPos, targetPos, lightUp);

	mLightPosW = lightPos;

	// Transform bounding sphere to light space.
	glm::vec3 sphereCenterLS = lightView * glm::vec4(targetPos, 1.0f);//needs to be 1.0f?
	//XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	// Ortho frustum in light space encloses scene.
	float l = sphereCenterLS.x - mSceneBounds.Radius;
	float b = sphereCenterLS.y - mSceneBounds.Radius;
	float n = sphereCenterLS.z - mSceneBounds.Radius;
	float r = sphereCenterLS.x + mSceneBounds.Radius;
	float t = sphereCenterLS.y + mSceneBounds.Radius;
	float f = sphereCenterLS.z + mSceneBounds.Radius;

	mLightNearZ = n;
	mLightFarZ = f;
	//XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);
	glm::mat4 lightProj = glm::orthoLH_ZO(l, r, b, t, n, f);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	glm::mat4 T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	glm::mat4 S = T * lightProj * lightView;// lightView* lightProj* T;
	//XMStoreFloat4x4(&mLightView, lightView);
	mLightView = lightView;
	//XMStoreFloat4x4(&mLightProj, lightProj);
	mLightProj = lightProj;
	//XMStoreFloat4x4(&mShadowTransform, S);
	mShadowTransform = S;
}

void SkinnedMeshApp::UpdateMainPassCB(const GameTimer& gt) {
	glm::mat4 view = mCamera.GetView();
	glm::mat4 proj = mCamera.GetProj();
	proj[1][1] *= -1;
	glm::mat4 viewProj = proj * view;//reverse for column major matrices view * proj
	glm::mat4 invView = glm::inverse(view);
	glm::mat4 invProj = glm::inverse(proj);
	glm::mat4 invViewProj = glm::inverse(viewProj);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	glm::mat4 T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);
	glm::mat4 viewProjTex = T * viewProj;//check this!!!!


	PassConstants* pPassConstants = mCurrFrameResource->pPCs;
	mMainPassCB.View = view;
	mMainPassCB.Proj = proj;
	mMainPassCB.ViewProj = viewProj;
	mMainPassCB.InvView = invView;
	mMainPassCB.InvProj = invProj;
	mMainPassCB.InvViewProj = invViewProj;
	mMainPassCB.ViewProjTex = viewProjTex;
	mMainPassCB.ShadowTransform = mShadowTransform;
	mMainPassCB.EyePosW = mCamera.GetPosition();
	mMainPassCB.RenderTargetSize = glm::vec2(mClientWidth, mClientHeight);
	mMainPassCB.InvRenderTargetSize = glm::vec2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mMainPassCB.Lights[0].Direction = mRotatedLightDirections[0];
	mMainPassCB.Lights[0].Strength = { 0.9f, 0.8f, 0.7f };
	mMainPassCB.Lights[1].Direction = mRotatedLightDirections[1];
	mMainPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	mMainPassCB.Lights[2].Direction = mRotatedLightDirections[2];
	mMainPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	memcpy(pPassConstants, &mMainPassCB, sizeof(PassConstants));
}

void SkinnedMeshApp::UpdateShadowPassCB(const GameTimer& gt) {
	glm::mat4 view = mLightView;
	glm::mat4 proj = mLightProj;
	proj[1][1] *= -1;
	glm::mat4 viewProj = proj * view;//reverse for column major matrices view * proj
	glm::mat4 invView = glm::inverse(view);
	glm::mat4 invProj = glm::inverse(proj);
	glm::mat4 invViewProj = glm::inverse(viewProj);

	uint32_t w = mShadowMap->Width();
	uint32_t h = mShadowMap->Height();

	PassConstants* pPassConstants = mCurrFrameResource->pPCs;
	mShadowPassCB.View = view;
	mShadowPassCB.Proj = proj;
	mShadowPassCB.ViewProj = viewProj;
	mShadowPassCB.InvView = invView;
	mShadowPassCB.InvProj = invProj;
	mShadowPassCB.InvViewProj = invViewProj;
	mShadowPassCB.ShadowTransform = mShadowTransform;
	mShadowPassCB.EyePosW = mLightPosW;
	mShadowPassCB.RenderTargetSize = glm::vec2(w, h);
	mShadowPassCB.InvRenderTargetSize = glm::vec2(1.0f / w, 1.0f / h);
	mShadowPassCB.NearZ = mLightNearZ;
	mShadowPassCB.FarZ = mLightFarZ;
	mShadowPassCB.TotalTime = gt.TotalTime();
	mShadowPassCB.DeltaTime = gt.DeltaTime();
	mShadowPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	mShadowPassCB.Lights[0].Direction = mRotatedLightDirections[0];
	mShadowPassCB.Lights[0].Strength = { 0.9f, 0.8f, 0.7f };
	mShadowPassCB.Lights[1].Direction = mRotatedLightDirections[1];
	mShadowPassCB.Lights[1].Strength = { 0.4f, 0.4f, 0.4f };
	mShadowPassCB.Lights[2].Direction = mRotatedLightDirections[2];
	mShadowPassCB.Lights[2].Strength = { 0.2f, 0.2f, 0.2f };

	auto& ub = *uniformBuffer;
	auto size = ub[1].objectSize;
	uint8_t* ptr = (uint8_t*)pPassConstants + size;
	memcpy(ptr, &mShadowPassCB, sizeof(PassConstants));
}

void SkinnedMeshApp::UpdateSsaoCB(const GameTimer& gt)
{
	SsaoConstants ssaoCB;

	glm::mat4 P = mCamera.GetProj();

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	glm::mat4 T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	ssaoCB.Proj = mMainPassCB.Proj;
	ssaoCB.InvProj = mMainPassCB.InvProj;

	ssaoCB.ProjTex = T * P;// glm::transpose(T * P);// , XMMatrixTranspose(P * T));????works when don't transose col major?

	mSsao->GetOffsetVectors(ssaoCB.OffsetVectors);

	auto blurWeights = mSsao->CalcGaussWeights(2.5f);
	ssaoCB.BlurWeights[0] = glm::vec4(blurWeights[0], blurWeights[1], blurWeights[2], blurWeights[3]);
	ssaoCB.BlurWeights[1] = glm::vec4(blurWeights[4], blurWeights[5], blurWeights[6], blurWeights[7]);
	ssaoCB.BlurWeights[2] = glm::vec4(blurWeights[8], blurWeights[9], blurWeights[10], 0.0f);

	ssaoCB.InvRenderTargetSize = glm::vec2(1.0f / mSsao->SsaoMapWidth(), 1.0f / mSsao->SsaoMapHeight());

	// Coordinates given in view space.
	ssaoCB.OcclusionRadius = 0.5f;
	ssaoCB.OcclusionFadeStart = 0.2f;
	ssaoCB.OcclusionFadeEnd = 1.0f;
	ssaoCB.SurfaceEpsilon = 0.05f;

	auto currSsaoCB = mCurrFrameResource->pSsaoCB;
	memcpy(currSsaoCB, &ssaoCB, sizeof(ssaoCB));
	//currSsaoCB->CopyData(0, ssaoCB);
}




void SkinnedMeshApp::UpdateMaterialsBuffer(const GameTimer& gt) {
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
			matData.NormalMapIndex = mat->NormalSrvHeapIndex;
			memcpy((pMatConsts + (objSize * mat->MatCBIndex)), &matData, sizeof(MaterialData));
			mat->NumFramesDirty--;
		}
	}
}


void SkinnedMeshApp::OnResize()
{
	VulkApp::OnResize();

	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);

	if (mSsao != nullptr)
	{
		mSsao->OnResize(mClientWidth, mClientHeight);

		// Resources changed, so need to rebuild descriptors.
		//mSsao->RebuildDescriptors(mDepthStencilBuffer.Get());
	}
}

void SkinnedMeshApp::OnMouseMove(WPARAM btnState, int x, int y) {
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

void SkinnedMeshApp::OnMouseDown(WPARAM btnState, int x, int y) {
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void SkinnedMeshApp::OnMouseUp(WPARAM btnState, int x, int y) {
	ReleaseCapture();
}


void SkinnedMeshApp::Draw(const GameTimer& gt) {
	uint32_t index = 0;
	
	
		auto& ub = *uniformBuffer;
		VkDeviceSize passSize = ub[1].objectSize;
		VkDeviceSize passCount = ub[1].objectCount;
		VkDeviceSize boneSize = ub[3].objectSize;
		VkDeviceSize boneCount = ub[3].objectCount;
		auto& ud = *uniformDescriptors;
		//bind descriptors that don't change during pass
		VkDescriptorSet descriptor0 = ud[0];//pass constant buffer
		uint32_t dynamicOffsets[2] = { mCurrFrame*(uint32_t)boneSize,mCurrFrame * (uint32_t)passSize * (uint32_t)passCount  };

		//bind storage buffer
		auto& sb = *storageBuffer;
		VkDeviceSize matSize = sb[0].objectSize * sb[0].objectCount;
		auto& sd = *storageDescriptors;
		VkDescriptorSet descriptor2 = sd[0];
		auto& td = *textureDescriptors;
		VkDescriptorSet descriptor3 = td[0];
		VkDescriptorSet descriptor4 = td[1];
		VkDescriptorSet descriptor7 = td[2];
		auto& sh = *shadowDescriptors;
		VkDescriptorSet descriptor5 = sh[0];

		VkDescriptorSet descriptors[8];
		descriptors[0] = descriptorSets.boneDescriptorSet;
		descriptors[1] = descriptorSets.passDescriptorSet;
		descriptors[2] = descriptorSets.materialDescriptorSet;
		descriptors[3] = descriptorSets.textureArrayDescriptorSet;
		descriptors[4] = descriptorSets.cubeMapDescriptorSet;
		descriptors[5] = descriptorSets.shadowMapDescriptorSet;
		descriptors[6] = descriptorSets.ssaoMapDescriptorSet;
		VkCommandBuffer cmd = BeginRender(false);//don't want to start main render pass
		{
			dynamicOffsets[1] = { mCurrFrame * (uint32_t)passSize * (uint32_t)passCount + (uint32_t)passSize };
			//shadow pass
			uint32_t w = mShadowMap->Width();
			uint32_t h = mShadowMap->Height();
			VkRenderPassBeginInfo renderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			VkClearValue clearValue[1] = { {1.0f,0.0f } };
			renderPassBeginInfo.clearValueCount = 1;
			renderPassBeginInfo.pClearValues = clearValue;
			renderPassBeginInfo.renderPass = mShadowMap->getRenderPass();
			renderPassBeginInfo.framebuffer = mShadowMap->getFramebuffer();
			renderPassBeginInfo.renderArea = { 0,0,(uint32_t)w,(uint32_t)h };
			VkViewport viewport = mShadowMap->Viewport();
			pvkCmdSetViewport(cmd, 0, 1, &viewport);
			VkRect2D scissor = mShadowMap->ScissorRect();
			pvkCmdSetScissor(cmd, 0, 1, &scissor);
			//vkCmdSetDepthBias(cmd, depthBiasConstant, 0.0f, depthBiasSlope);
			pvkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["shadow_opaque"]);
			//bind all descriptors except first
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *shadowPipelineLayout, 1, 4, &descriptorSets.boneDescriptorSet, 2, dynamicOffsets);
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *shadowPipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *shadowPipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *shadowPipelineLayout, 4, 1, &descriptor4, 0, 0);//bind PC data once
			DrawRenderItems(cmd, *shadowPipelineLayout, mRitemLayer[(int)RenderLayer::Opaque]);
			
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["skinned_shadow_opaque"]);
			DrawRenderItems(cmd, *shadowPipelineLayout, mRitemLayer[(int)RenderLayer::SkinnedOpaque]);
			pvkCmdEndRenderPass(cmd);
			//Vulkan::transitionImage(mDevice,mGraphicsQueue,cmd,mShadowMap->getRenderTargetView(),VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT)
		}
		VkViewport viewport = { 0.0f,0.0f,(float)mClientWidth,(float)mClientHeight,0.0f,1.0f };
		//pvkCmdSetViewport(cmd, 0, 1, &viewport);
		VkRect2D scissor = { {0,0},{(uint32_t)mClientWidth,(uint32_t)mClientHeight} };
		//pvkCmdSetScissor(cmd, 0, 1, &scissor);
		{
			dynamicOffsets[1] = { mCurrFrame * (uint32_t)passSize * (uint32_t)passCount };
			//normal
			uint32_t w = mClientWidth;
			uint32_t h = mClientHeight;
			VkRenderPassBeginInfo renderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			VkClearValue clearValues[2] = { {0.0f,0.0f,1.0f}, {1.0f,0.0f } };

			renderPassBeginInfo.clearValueCount = 2;
			renderPassBeginInfo.pClearValues = clearValues;
			renderPassBeginInfo.renderPass = mSsao->getNormalRenderPass();
			renderPassBeginInfo.framebuffer = mSsao->getNormalFramebuffer();
			renderPassBeginInfo.renderArea = { 0,0,(uint32_t)w,(uint32_t)h };
			pvkCmdSetViewport(cmd, 0, 1, &viewport);
			pvkCmdSetScissor(cmd, 0, 1, &scissor);
			//vkCmdSetDepthBias(cmd, depthBiasConstant, 0.0f, depthBiasSlope);
			pvkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["drawNormals"]);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *drawNormalsPipelineLayout, 1, 4, &descriptorSets.boneDescriptorSet, 2, dynamicOffsets);
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *drawNormalsPipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *drawNormalsPipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *drawNormalsPipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
			DrawRenderItems(cmd, *drawNormalsPipelineLayout, mRitemLayer[(int)RenderLayer::Opaque]);
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["skinned_drawNormals"]);
			DrawRenderItems(cmd, *drawNormalsPipelineLayout, mRitemLayer[(int)RenderLayer::SkinnedOpaque]);
			pvkCmdEndRenderPass(cmd);
		}
		{
			auto ssub = *ssaoUniformDescriptors;
			auto sstx = *ssaoTextureDescriptors;
			VkDescriptorSet descriptor6 = ssub[0];
			VkDescriptorSet descriptor8 = sstx[0];
			VkDescriptorSet descriptor9 = sstx[1];
			VkDescriptorSet descriptor10 = sstx[2];
			//ssao
			VkDeviceSize ssaoSize = ub[2].objectSize;
			VkDeviceSize ssaoCount = ub[2].objectCount;
			dynamicOffsets[0] = { mCurrFrame * (uint32_t)ssaoSize * (uint32_t)ssaoCount };
			//normal
			uint32_t w = mSsao->SsaoMapWidth();
			uint32_t h = mSsao->SsaoMapHeight();
			VkRenderPassBeginInfo renderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			VkClearValue clearValues[1] = { {0.0f,0.0f,0.0f} };

			renderPassBeginInfo.clearValueCount = 1;
			renderPassBeginInfo.pClearValues = clearValues;
			renderPassBeginInfo.renderPass = mSsao->getSsaoRenderPass();
			renderPassBeginInfo.framebuffer = mSsao->getSsaoFramebuffer(0);
			renderPassBeginInfo.renderArea = { 0,0,(uint32_t)w,(uint32_t)h };
			auto ssaoViewport = mSsao->getSsaoViewport();
			auto ssaoScissor = mSsao->getSsaoScissorRect();
			pvkCmdSetViewport(cmd, 0, 1, &ssaoViewport);
			pvkCmdSetScissor(cmd, 0, 1, &ssaoScissor);
			//vkCmdSetDepthBias(cmd, depthBiasConstant, 0.0f, depthBiasSlope);
			Vulkan::transitionImageNoSubmit(cmd, mDepthImage.image, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			pvkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);


			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["ssao"]);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *ssaoPipelineLayout, 0, 1, &descriptor6, 1, dynamicOffsets);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *ssaoPipelineLayout, 1, 1, &descriptor8, 0, 0);//bind PC data once
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *ssaoPipelineLayout, 2, 1, &descriptor9, 0, 0);//bind PC data once
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *ssaoPipelineLayout, 3, 1, &descriptor10, 0, 0);//bind PC data once

			vkCmdDraw(cmd, 6, 1, 0, 0);
			pvkCmdEndRenderPass(cmd);



		}
		{
			//blur
			auto ssub = *ssaoUniformDescriptors;
			auto sstx = *ssaoTextureDescriptors;
			auto tx = *textureDescriptors;
			VkDescriptorSet descriptor6 = ssub[0];
			VkDescriptorSet descriptor7 = tx[2];
			VkDescriptorSet descriptor8 = sstx[0];
			VkDescriptorSet descriptor9 = sstx[1];
			VkDescriptorSet descriptor10 = sstx[2];
			VkDescriptorSet descriptor11 = sstx[3];
			//ssao
			//blur pass
			uint32_t w = mSsao->SsaoMapWidth();
			uint32_t h = mSsao->SsaoMapHeight();
			VkRenderPassBeginInfo renderPassBeginInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			VkClearValue clearValues[1] = { {0.0f,0.0f,0.0f} };

			renderPassBeginInfo.clearValueCount = 1;
			renderPassBeginInfo.pClearValues = clearValues;
			renderPassBeginInfo.renderPass = mSsao->getSsaoRenderPass();
			renderPassBeginInfo.framebuffer = mSsao->getSsaoFramebuffer(1);//draw to second framebuffer (2nd image)
			renderPassBeginInfo.renderArea = { 0,0,(uint32_t)w,(uint32_t)h };
			auto ssaoViewport = mSsao->getSsaoViewport();
			auto ssaoScissor = mSsao->getSsaoScissorRect();
			pvkCmdSetViewport(cmd, 0, 1, &ssaoViewport);
			pvkCmdSetScissor(cmd, 0, 1, &ssaoScissor);
			//Vulkan::transitionImageNoSubmit(cmd, mSsao->getSsaoImage(0), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			Vulkan::transitionImageNoSubmit(cmd, mSsao->getSsaoImage(1), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			pvkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);


			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["ssaoBlurHorz"]);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *ssaoPipelineLayout, 0, 1, &descriptor6, 1, dynamicOffsets);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *ssaoPipelineLayout, 1, 1, &descriptor8, 0, 0);//bind PC data once
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *ssaoPipelineLayout, 2, 1, &descriptor9, 0, 0);//bind PC data once
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *ssaoPipelineLayout, 3, 1, &descriptor7, 0, 0);//bind PC data once

			vkCmdDraw(cmd, 6, 1, 0, 0);
			pvkCmdEndRenderPass(cmd);
			//transition this image to shader read
			Vulkan::transitionImageNoSubmit(cmd, mSsao->getSsaoImage(0), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			//Vulkan::transitionImageNoSubmit(cmd, mSsao->getSsaoImage(1), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
			renderPassBeginInfo.framebuffer = mSsao->getSsaoFramebuffer(0);//draw to first framebuffer (1st image)
			pvkCmdBeginRenderPass(cmd, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);


			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["ssaoBlurVert"]);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *ssaoPipelineLayout, 0, 1, &descriptor6, 1, dynamicOffsets);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *ssaoPipelineLayout, 1, 1, &descriptor8, 0, 0);//bind PC data once
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *ssaoPipelineLayout, 2, 1, &descriptor9, 0, 0);//bind PC data once
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *ssaoPipelineLayout, 3, 1, &descriptor11, 0, 0);//bind PC data once

			vkCmdDraw(cmd, 6, 1, 0, 0);
			pvkCmdEndRenderPass(cmd);

			//Vulkan::transitionImageNoSubmit(cmd, mSsao->getSsaoImage(1), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			//Vulkan::transitionImageNoSubmit(cmd, mSsao->getSsaoImage(0), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );



		}
		Vulkan::transitionImageNoSubmit(cmd, mDepthImage.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		//start main render pass now
		pvkCmdBeginRenderPass(cmd, &mRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		dynamicOffsets[0] = { mCurrFrame * (uint32_t)boneSize };
		dynamicOffsets[1] = { mCurrFrame* (uint32_t)passSize* (uint32_t)passCount};// (uint32_t)(mCurrFrame * passSize * passCount);


		//VkViewport viewport = { 0.0f,0.0f,(float)mClientWidth,(float)mClientHeight,0.0f,1.0f };
		pvkCmdSetViewport(cmd, 0, 1, &viewport);
		//VkRect2D scissor = { {0,0},{(uint32_t)mClientWidth,(uint32_t)mClientHeight} };
		pvkCmdSetScissor(cmd, 0, 1, &scissor);
		if (mIsWireframe) {
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque_wireframe"]);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 1, 7, &descriptorSets.boneDescriptorSet, 2, dynamicOffsets);//bind PC data
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once		
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 4, 1, &descriptor4, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 5, 1, &descriptor5, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 6, 1, &descriptor7, 0, 0);//bind PC data once
			DrawRenderItems(cmd, *pipelineLayout, mRitemLayer[(int)RenderLayer::Opaque]);
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["skinned_opaque_wireframe"]);
			DrawRenderItems(cmd, *pipelineLayout, mRitemLayer[(int)RenderLayer::SkinnedOpaque]);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 6, 1, &descriptor7, 0, 0);//bind PC data once
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["debug"]);
			DrawRenderItems(cmd, *debugPipelineLayout, mRitemLayer[(int)RenderLayer::Debug]);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 1, 7, &descriptorSets.boneDescriptorSet, 2, dynamicOffsets);//bind PC data
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 4, 1, &descriptor4, 0, 0);//bind PC data once
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["sky"]);
			DrawRenderItems(cmd, *cubeMapPipelineLayout, mRitemLayer[(int)RenderLayer::Sky]);
		}
		else if (mIsFlatShader) {
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaqueFlat"]);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 1, 7, &descriptorSets.boneDescriptorSet, 2, dynamicOffsets);//bind PC data
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once		
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 4, 1, &descriptor4, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 5, 1, &descriptor5, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 6, 1, &descriptor7, 0, 0);//bind PC data once

			DrawRenderItems(cmd, *pipelineLayout, mRitemLayer[(int)RenderLayer::Opaque]);
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["skinned_opaqueFlat"]);
			DrawRenderItems(cmd, *pipelineLayout, mRitemLayer[(int)RenderLayer::SkinnedOpaque]);
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["debug"]);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 6, 1, &descriptor7, 0, 0);//bind PC data once
			DrawRenderItems(cmd, *debugPipelineLayout, mRitemLayer[(int)RenderLayer::Debug]);
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 4, 1, &descriptor4, 0, 0);//bind PC data once
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 1, 7, &descriptorSets.boneDescriptorSet, 2, dynamicOffsets);//bind PC data
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["sky"]);
			DrawRenderItems(cmd, *cubeMapPipelineLayout, mRitemLayer[(int)RenderLayer::Sky]);
		}
		else if (mNoSsao) {
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaqueNoSsao"]);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 1, 7, &descriptorSets.boneDescriptorSet, 2, dynamicOffsets);//bind PC data
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once		
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 4, 1, &descriptor4, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 5, 1, &descriptor5, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 6, 1, &descriptor7, 0, 0);//bind PC data once

			DrawRenderItems(cmd, *pipelineLayout, mRitemLayer[(int)RenderLayer::Opaque]);
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["skinned_opaqueNoSsao"]);
			DrawRenderItems(cmd, *pipelineLayout, mRitemLayer[(int)RenderLayer::SkinnedOpaque]);
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["debug"]);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 6, 1, &descriptor7, 0, 0);//bind PC data once
			DrawRenderItems(cmd, *debugPipelineLayout, mRitemLayer[(int)RenderLayer::Debug]);
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 4, 1, &descriptor4, 0, 0);//bind PC data once
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 1, 7, &descriptorSets.boneDescriptorSet, 2, dynamicOffsets);//bind PC data
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["sky"]);
			DrawRenderItems(cmd, *cubeMapPipelineLayout, mRitemLayer[(int)RenderLayer::Sky]);
		}
		else
		{
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["opaque"]);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 1, 7, &descriptorSets.boneDescriptorSet, 2, dynamicOffsets);//bind PC data
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once		
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 4, 1, &descriptor4, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 5, 1, &descriptor5, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 6, 1, &descriptor7, 0, 0);//bind PC data once

			DrawRenderItems(cmd, *pipelineLayout, mRitemLayer[(int)RenderLayer::Opaque]);
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["skinned_opaque"]);
			DrawRenderItems(cmd, *pipelineLayout, mRitemLayer[(int)RenderLayer::SkinnedOpaque]);
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["debug"]);
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 6, 1, &descriptor7, 0, 0);//bind PC data once
			DrawRenderItems(cmd, *debugPipelineLayout, mRitemLayer[(int)RenderLayer::Debug]);
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 0, 1, &descriptor0, 1, dynamicOffsets);//bind PC data
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 2, 1, &descriptor2, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 3, 1, &descriptor3, 0, 0);//bind PC data once
			//pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 4, 1, &descriptor4, 0, 0);//bind PC data once
			pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *pipelineLayout, 1,7, &descriptorSets.boneDescriptorSet, 2, dynamicOffsets);//bind PC data
			pvkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mPSOs["sky"]);
			DrawRenderItems(cmd, *cubeMapPipelineLayout, mRitemLayer[(int)RenderLayer::Sky]);

		}





		EndRender(cmd);
	
}


void SkinnedMeshApp::DrawRenderItems(VkCommandBuffer cmd, VkPipelineLayout layout, const std::vector<RenderItem*>& ritems) {
	auto& ub = *uniformBuffer;
	VkDeviceSize objectSize = ub[0].objectSize;
	//auto& ud = *uniformDescriptors;
	//VkDescriptorSet descriptor1 = ud[1];


	for (size_t i = 0; i < ritems.size(); i++) {
		auto ri = ritems[i];
		uint32_t indexOffset = ri->StartIndexLocation;

		const auto vbv = ri->Geo->vertexBufferGPU;
		const auto ibv = ri->Geo->indexBufferGPU;
		pvkCmdBindVertexBuffers(cmd, 0, 1, &vbv.buffer, mOffsets);

		pvkCmdBindIndexBuffer(cmd, ibv.buffer, indexOffset * sizeof(uint32_t), VK_INDEX_TYPE_UINT32);
		uint32_t cbvIndex = ri->ObjCBIndex;
		uint32_t dyoffsets[1] = { (uint32_t)(cbvIndex * objectSize) };
		pvkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSets.objectDescriptorSet, 1, dyoffsets);
		pvkCmdDrawIndexed(cmd, ri->IndexCount, 1, 0, ri->BaseVertexLocation, 0);
	}
}

int main()
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		SkinnedMeshApp theApp(GetModuleHandle(NULL));
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