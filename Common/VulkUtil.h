#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/intersect.hpp>
#include "Vulkan.h"
struct AABB {
	glm::vec3 min = {};
	glm::vec3 max = {};
	AABB(glm::vec3& min_, glm::vec3& max_) {
		min = min_;
		max = max_;
	}
	AABB() {

	}

	bool within(float l, float a, float r) {
		return l < a&& a < r;
	}
	//test if within clip space
	bool InsideFrustum(glm::mat4& MVP);// {
	//	glm::vec4 corners[8] = {
	//		{min,1.0f},
	//		{max.x,min.y,min.z,1.0f},
	//		{min.x,max.y,min.z,1.0f},
	//		{max.x,max.y,min.z,1.0f},

	//		{min.x,min.y,max.z,1.0f},
	//		{max.x,min.y,max.z,1.0f},
	//		{min.x,max.y,max.z,1.0f},
	//		{max,1.0f}
	//	};
	//	bool inside = false;
	//	for (size_t cornerIdx = 0; cornerIdx < (sizeof(corners) / sizeof(corners[0])); cornerIdx++) {
	//		glm::vec4 corner = MVP * corners[cornerIdx];
	//		bool t1 = within(-corner.w, corner.x, corner.w);
	//		bool t2 = within(-corner.w, corner.y, corner.w);
	//		bool t3 = within(0.0f, corner.z, corner.w);
	//		inside = inside || (t1 && t2 && t3);
	//			/*within(corner.w, corner.x, corner.w) &&
	//			within(-corner.w, corner.y, corner.w) &&
	//			within(0.0f, corner.z, corner.w);*/
	//	}
	//	return inside;
	//}
	bool Intersects(glm::vec3 Origin, glm::vec3 Direction, float& dist)const;/* {
		
		dist = -1.0f;
		float div, tmin, tmax, tymin, tymax, tzmin, tzmax;
		div = 1.0f / Direction.x;
		if (div >= 0.0f) {
			tmin = (min.x - Origin.x) * div;
			tmax = (max.x - Origin.x) * div;
		}
		else {
			tmin = (max.x - Origin.x) * div;
			tmax = (min.x - Origin.x) * div;
		}
		if (tmax < 0.0f) 
			return false;

		div = 1.0f / Direction.y;
		if (div >= 0.0f) {
			tymin = (min.y - Origin.y) * div;
			tymax = (max.y - Origin.y) * div;

		}
		else {
			tymin = (max.y - Origin.y) * div;
			tymax = (min.y - Origin.y) * div;
		}
		if ((tymax < 0.0f) || (tmin > tymax) || (tymin > tmax))return false;

		if (tymin > tmin)tmin = tymin;
		if (tymax < tmax) tmax = tymax;

		div = 1.0f / Direction.z;
		if (div >= 0.0f) {
			tzmin = (min.z - Origin.z) * div;
			tzmax = (max.z - Origin.z) * div;
		}
		else {
			tzmin = (max.z - Origin.z) * div;
			tzmax = (min.z - Origin.z) * div;
		}
		if ((tzmax < 0.0f) || (tmin > tzmax) || (tzmin > tmax))return false;
		glm::vec3 vec = (((min + max) / 2.0f) - Origin);
		dist= glm::length(vec);
		return true;
	}*/
	
};

class Ray {
	glm::vec3 orig = {};
	glm::vec3 dir = {};
	Ray() = delete;

public:
	Ray(glm::vec3& orig_, glm::vec3& dir_) :orig(orig_), dir(dir_) {}
	template <typename T> float IntersectMesh(std::vector<T>& vertices, std::vector<uint32_t>& indices);
	template<typename T> bool IntersectMesh(T* vertices, uint32_t* indices, size_t indexCount,uint32_t&tri,float &dist)
	{

		bool hit = false;
		dist = INFINITY;
		uint32_t hitTri = UINT32_MAX;
		uint32_t currTri = 0;
		for (size_t f = 0; f < indexCount; f += 3) {
			uint32_t i0 = indices[f + 0];
			uint32_t i1 = indices[f + 1];
			uint32_t i2 = indices[f + 2];
			glm::vec3 *p = (glm::vec3*)&vertices[i0];
			glm::vec3 v0 = *p;
			p = (glm::vec3*)&vertices[i1];
			glm::vec3 v1 = *p;
			p = (glm::vec3*)&vertices[i2];
			glm::vec3 v2 = *p;
			
			glm::vec2 bary;
			float currDist = 0.0f;
			if (glm::intersectRayTriangle(orig, dir, v0, v1, v2, bary, currDist)) {
				hit = true;
				if (currDist < dist) {
					hitTri = currTri;
					dist = currDist;// = std::min(dist, currDist);
				}
			}
			currTri++;
		}
		tri = hitTri;
		return hit;
	}
};

struct Sphere {
	glm::vec3 Center{};
	float Radius{ 0 };
};

struct SubmeshGeometry {
	uint32_t IndexCount{ 0 };
	uint32_t StartIndexLocation{ 0 };
	uint32_t BaseVertexLocation{ 0 };
	AABB Bounds;
};

struct MeshGeometry {
	std::string Name;
	Vulkan::Buffer vertexBufferGPU;
	Vulkan::Buffer indexBufferGPU;
	void* vertexBufferCPU{ nullptr };
	void* indexBufferCPU{ nullptr };
	
	uint32_t VertexByteStride{ 0 };
	uint32_t VertexBufferByteSize{ 0 };
	uint32_t IndexBufferByteSize{ 0 };
	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	~MeshGeometry() {
		/*if (vertexBufferCPU != nullptr) {
			free(vertexBufferCPU);
			vertexBufferCPU = nullptr;
		}
		if (indexBufferCPU != nullptr) {
			free(indexBufferCPU);
			indexBufferCPU = nullptr;
		}*/
	}
};

struct Light {
	glm::vec3 Strength = glm::vec3(0.5f);
	float FalloffStart = 1.0f;								//point/spot light only
	glm::vec3 Direction = glm::vec3(0.0f, -1.0f, 0.0f);		//directional/spot light only
	float FalloffEnd = 10.0f;								//point/spot light only
	glm::vec3 Position = glm::vec3(0.0f);					//point/spot light only
	float SpotPower = 64.0f;								//spot light only
};

#define MaxLights 16

struct MaterialConstants {
	glm::vec4 DiffuseAlbedo = glm::vec4(1.0f);
	glm::vec3 FresnelR0 = glm::vec3(0.1f);
	float Roughness = 0.25f;
	//used in texture mapping
	glm::mat4 MatTransform = glm::mat4(1.0f);
};

struct Material {
	std::string Name;

	//Index into constant buffer corresponding to this material
	int MatCBIndex{ -1 };

	////Index into heap for diffuse texture?
	int DiffuseSrvHeapIndex = -1;

	int NormalSrvHeapIndex = -1;

	////Index into SRV heap for normal texture
	//int NormalSrvHeapIndex = -1;

	int NumFramesDirty{ 0 };

	glm::vec4 DiffuseAlbedo = glm::vec4(1.0f);
	glm::vec3 FresnelR0 = glm::vec3(1.0f);
	float Roughness = 0.25f;
	glm::mat4 MatTransform = glm::mat4(1.0f);
};

struct Texture : Vulkan::Texture {
	std::string	Name;
	std::string FileName;
	std::vector<const char*> FileNames;
	
};

////"Borrowed" from DirectXCollisions
//struct BoundingBox {
//	glm::vec3 Center;
//	glm::vec3 Extents;
//	BoundingBox()noexcept :Center(0, 0, 0), Extents(1.0f, 1.0f, 1.0f) {}
//	BoundingBox(const BoundingBox&) = default;
//	BoundingBox& operator=(const BoundingBox&) = default;
//	BoundingBox(BoundingBox&&) = default;
//	BoundingBox& operator=(BoundingBox&&) = default;
//	constexpr BoundingBox(const glm::vec3& center, const glm::vec3& extents) :Center(center), Extents(extents) {}
//	
//
//};
//enum ContainmentType
//{
//	DISJOINT = 0,
//	INTERSECTS = 1,
//	CONTAINS = 2
//};
//struct BoundingFrustum {
//	glm::vec3 Origin;//origin of frustrum (and projection)
//	glm::quat Orientation;//quaternion representing rotation
//
//	float RightSlope;	//Position X(X/Z)
//	float LeftSlope;	//Negative X
//	float TopSlope;		//Positive Y (Y/Z)
//	float BottomSlope;	//Negative Y
//	float Near, Far;	//Z of the near and far plane
//
//	//Creators
//	BoundingFrustum() noexcept:Origin(0.0f), Orientation(0, 0, 0, 1.0f), RightSlope(1.0f), LeftSlope(-1.0f), TopSlope(1.0f), BottomSlope(-1.0f), Near(0), Far(1.0f) {}
//
//	void Transform(BoundingFrustum& out, glm::mat4& m)const;
//
//	ContainmentType Contains(const BoundingBox& box);
//};