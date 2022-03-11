#include "VulkUtil.h"

//check if object inside frustum in clip space
bool AABB::InsideFrustum(glm::mat4& MVP) {//
	glm::vec4 corners[8] = {
		{min,1.0f},
		{max.x,min.y,min.z,1.0f},
		{min.x,max.y,min.z,1.0f},
		{max.x,max.y,min.z,1.0f},

		{min.x,min.y,max.z,1.0f},
		{max.x,min.y,max.z,1.0f},
		{min.x,max.y,max.z,1.0f},
		{max,1.0f}
	};
	bool inside = false;
	for (size_t cornerIdx = 0; cornerIdx < (sizeof(corners) / sizeof(corners[0])); cornerIdx++) {
		glm::vec4 corner = MVP * corners[cornerIdx];
		bool t1 = within(-corner.w, corner.x, corner.w);
		bool t2 = within(-corner.w, corner.y, corner.w);
		bool t3 = within(0.0f, corner.z, corner.w);
		inside = inside || (t1 && t2 && t3);
		/*within(corner.w, corner.x, corner.w) &&
		within(-corner.w, corner.y, corner.w) &&
		within(0.0f, corner.z, corner.w);*/
	}
	return inside;
}

bool AABB::Intersects(glm::vec3 Origin, glm::vec3 Direction, float& dist)const {

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
	dist = glm::length(vec);
	return true;
}



template <typename T> float Ray::IntersectMesh(std::vector<T>& vertices, std::vector<uint32_t>& indices) {
	bool hit = false;
	float dist = INFINITY;
	uint32_t hitTri = UINT32_MAX;
	uint32_t currTri = 0;
	for (size_t f = 0; f < indices.size(); f += 3) {
		uint32_t i0 = indices[f + 0];
		uint32_t i1 = indices[f + 1];
		uint32_t i2 = indices[f + 2];
		glm::vec3 v0 = vertices[i0];
		glm::vec3 v1 = vertices[i1];
		glm::vec3 v2 = vertices[i2];
		glm::vec2 bary;
		float currDist = 0.0f;
		if (glm::intersectRayTriangle(orig, dir, v0, v1, v2, bary, currDist)) {
			hit = true;
			hitTri = currTri;
			dist = std::min(dist, currDist);
		}
		currTri++;
	}
	return hit ? dist : -1.0f;
}

//void BoundingFrustum::Transform(BoundingFrustum& out, glm::mat4& m)const {
//	glm::vec3 vOrigin = Origin;
//	glm::quat vOrientation = Orientation;
//	
//	//Composite the frustum rotation and the transform rotation
//	glm::mat4 nM;
//	nM = m;
//	nM[3][0] = nM[3][1] = nM[3][2] = 0;
//	nM[3][3] = 1.0f;
//
//	glm::quat Rotation = glm::quat(nM);
//	vOrientation = vOrientation * Rotation;
//
//	//transform the center.
//	vOrigin = m * glm::vec4(vOrigin,0.0f);
//
//	//store the frustum
//	out.Origin = vOrigin;
//	out.Orientation = vOrientation;
//
//	//scale the near and far distances (the slopes remain the same).
//	auto dX = glm::dot(glm::vec3(m[0]), glm::vec3(m[0]));
//	auto dY = glm::dot(glm::vec3(m[1]), glm::vec3(m[1]));
//	auto dZ = glm::dot(glm::vec3(m[2]), glm::vec3(m[2]));
//
//	auto d = std::max(dX, std::max(dY, dZ));
//	float Scale = sqrtf(d);
//
//	out.Near = Near * Scale;
//	out.Far = Far * Scale;
//
//	//Copy the slopes.
//	out.RightSlope = RightSlope;
//	out.LeftSlope = LeftSlope;
//	out.TopSlope = TopSlope;
//	out.BottomSlope = BottomSlope;
//
//
//
//}
//
