#pragma once
#include <Windows.h>
#include <cstdint>
#include <algorithm>
#include <glm/glm.hpp>
const float pi = 3.14159265358979323846264338327950288f;
class MathHelper
{
public:
	// Returns random float in [0, 1).
	static float RandF()
	{
		return (float)(rand()) / (float)RAND_MAX;
	}

	// Returns random float in [a, b).
	static float RandF(float a, float b)
	{
		return a + RandF() * (b - a);
	}

	static int Rand(int a, int b)
	{
		return a + rand() % ((b - a) + 1);
	}

	template<typename T>
	static T Min(const T& a, const T& b)
	{
		return a < b ? a : b;
	}

	template<typename T>
	static T Max(const T& a, const T& b)
	{
		return a > b ? a : b;
	}

	template<typename T>
	static T Lerp(const T& a, const T& b, float t)
	{
		return a + (b - a) * t;
	}

	template<typename T>
	static T Clamp(const T& x, const T& low, const T& high)
	{
		return x < low ? low : (x > high ? high : x);
	}

	// Returns the polar angle of the point (x,y) in [0, 2*PI).
	static float AngleFromXY(float x, float y);

	static glm::vec4 SphericalToCartesian(float radius, float theta, float phi)
	{
		return glm::vec4(
			radius * sinf(phi) * cosf(theta),
			radius * cosf(phi),
			radius * sinf(phi) * sinf(theta),
			1.0f);
	}

	static glm::mat4 InverseTranspose(glm::mat4 m) {
		glm::mat4 A = m;
		A[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		return glm::transpose(glm::inverse(A));
	}

	static glm::mat4 Identity4x4() {
		return glm::mat4(1.0f);
	}



	static glm::vec3 RandUnitVec3();
	static glm::vec3 RandHemisphereUnitVec3(glm::vec3 n);

	static const float Infinity;
	static const float Pi;



	static glm::vec4 splatX(glm::vec4 v) {
		return glm::vec4(v.x);
	}
	static glm::vec4 splatY(glm::vec4 v) {
		return glm::vec4(v.y);
	}
	static glm::vec4 splatZ(glm::vec4 v) {
		return glm::vec4(v.z);
	}
	static glm::vec4 splatW(glm::vec4 v) {
		return glm::vec4(v.w);
	}
	static glm::vec4 splatOne() {
		return glm::vec4(1.0f);
	}
	static glm::vec4 splatInfinity() {
		return glm::vec4((float)0x78000000);
	}
	static glm::vec4 splatQNaN() {
		return glm::vec4((float)0x7FC00000);
	}
	static glm::vec4 splatEpsilon() {
		return glm::vec4((float)0x34000000);
	}
	static glm::vec4 splatSignMask() {
		return glm::vec4((float)0x80000000);
	}
	/*static float getByIndex(glm::vec4 v, size_t i) {
		return v[i];
	}*/
	static float getX(glm::vec4 v) {
		return v.x;
	}
	static float getY(glm::vec4 v) {
		return v.y;
	}
	static float getZ(glm::vec4 v) {
		return v.y;
	}
	static float getW(glm::vec4 v) {
		return v.w;
	}
	static glm::mat4 shadowMatrix(glm::vec4 lightPos, glm::vec4 plane) {
		glm::mat4 res = glm::dot(plane, lightPos) * glm::mat4(1.0f) - glm::outerProduct(lightPos, plane);
		return res;
	}
	static glm::mat4 reflect(glm::vec4 plane);


	static glm::vec3 vectorMin(glm::vec3& a, glm::vec3& b);
	static glm::vec3 vectorMax(glm::vec3& a, glm::vec3& b);


	/*struct Plane {
		glm::vec3 abc;
		float d{ 0 };
		Plane(glm::vec3& p0, glm::vec3& p1, glm::vec3& p2) {
			glm::vec3 v1 = p1 - p0;
			glm::vec3 v2 = p2 - p0;
			abc = glm::normalize(glm::cross(v1, v2));
			d = -glm::dot(p0, abc);

		}
		float Distance(glm::vec3& point) {
			return glm::dot(point, abc);
		}
	};

	struct BoundingBox {
		glm::vec3 points[8];
	};


	struct Frustrum {
		Plane planes[6];
		glm::vec3 fnear[4];
		glm::vec3 ffar[4];
		enum {
			COORD_BOTTOMLEFT=0,
			COORD_BOTTOMRIGHT,
			COORD_TOPLEFT,
			COORD_TOPRIGHT
		};
		enum {
			PLANE_LEFT=0,
			PLANE_RIGHT,
			PLANE_BOTTOM,
			PLANE_TOP,
			PLANE_NEAR,
			PLANE_FAR
		};
		bool Intersects(glm::vec3& point) {
			for (int i = 0; i < 6; i++) {
				if (planes[i].Distance(point) > 0)
					return false;
			}
			return true;
		}
		bool Intersects(BoundingBox& box) {
			for (int p = 0; p < 6; p++) {
				for (int b = 0; b < 8; b++) {
					if (planes[p].Distance(box.points[b]) < 0) {
						continue;
						return false;
					}

				}
			}
			return true;
		}
	};*/

	


	
};


