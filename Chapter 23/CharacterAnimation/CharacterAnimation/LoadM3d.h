#pragma once

#include "SkinnedData.h"



class M3DLoader
{
public:
    struct Vertex
    {
        glm::vec3 Pos;
        glm::vec3 Normal;
        glm::vec2 TexC;
        glm::vec4 TangentU;
    };

    struct SkinnedVertex
    {
        glm::vec3 Pos;
        glm::vec3 Normal;
        glm::vec2 TexC;
        glm::vec3 TangentU;
        glm::vec3 BoneWeights;
        glm::ivec4 BoneIndices;
    };

    struct Subset
    {
        uint32_t Id = -1;
        uint32_t VertexStart = 0;
        uint32_t VertexCount = 0;
        uint32_t FaceStart = 0;
        uint32_t FaceCount = 0;
    };

    struct M3dMaterial
    {
        std::string Name;

        glm::vec4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
        glm::vec3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
        float Roughness = 0.8f;
        bool AlphaClip = false;

        std::string MaterialTypeName;
        std::string DiffuseMapName;
        std::string NormalMapName;
    };

    bool LoadM3d(const std::string& filename,
        std::vector<Vertex>& vertices,
        std::vector<uint32_t>& indices,
        std::vector<Subset>& subsets,
        std::vector<M3dMaterial>& mats);
    bool LoadM3d(const std::string& filename,
        std::vector<SkinnedVertex>& vertices,
        std::vector<uint32_t>& indices,
        std::vector<Subset>& subsets,
        std::vector<M3dMaterial>& mats,
        SkinnedData& skinInfo);

private:
    void ReadMaterials(std::ifstream& fin, uint32_t numMaterials, std::vector<M3dMaterial>& mats);
    void ReadSubsetTable(std::ifstream& fin, uint32_t numSubsets, std::vector<Subset>& subsets);
    void ReadVertices(std::ifstream& fin, uint32_t numVertices, std::vector<Vertex>& vertices);
    void ReadSkinnedVertices(std::ifstream& fin, uint32_t numVertices, std::vector<SkinnedVertex>& vertices);
    void ReadTriangles(std::ifstream& fin, uint32_t numTriangles, std::vector<uint32_t>& indices);
    void ReadBoneOffsets(std::ifstream& fin, uint32_t numBones, std::vector<glm::mat4>& boneOffsets);
    void ReadBoneHierarchy(std::ifstream& fin, uint32_t numBones, std::vector<int>& boneIndexToParentIndex);
    void ReadAnimationClips(std::ifstream& fin, uint32_t numBones, uint32_t numAnimationClips, std::unordered_map<std::string, AnimationClip>& animations);
    void ReadBoneKeyframes(std::ifstream& fin, uint32_t numBones, BoneAnimation& boneAnimation);
};


