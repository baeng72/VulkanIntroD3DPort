#pragma once
#include <Windows.h>
#include <cstdint>
#include <glm/glm.hpp>
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
		glm::mat4 res = glm::dot(plane,lightPos) *glm::mat4(1.0f)- glm::outerProduct(lightPos, plane);
		return res;
	}
	static glm::mat4 reflect(glm::vec4 plane);

};


