#include "MathHelper.h"

const float MathHelper::Infinity = FLT_MAX;
const float MathHelper::Pi = 3.1415926535f;

float MathHelper::AngleFromXY(float x, float y)
{
	float theta = 0.0f;

	// Quadrant I or IV
	if (x >= 0.0f)
	{
		// If x = 0, then atanf(y/x) = +pi/2 if y > 0
		//                atanf(y/x) = -pi/2 if y < 0
		theta = atanf(y / x); // in [-pi/2, +pi/2]

		if (theta < 0.0f)
			theta += 2.0f * Pi; // in [0, 2*pi).
	}

	// Quadrant II or III
	else
		theta = atanf(y / x) + Pi; // in [0, 2*pi).

	return theta;
}

glm::vec3 MathHelper::RandUnitVec3()
{


	// Keep trying until we get a point on/in the hemisphere.
	while (true)
	{
		// Generate random point in the cube [-1,1]^3.
		glm::vec4 v = glm::vec4(MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), 0.0f);

		// Ignore points outside the unit sphere in order to get an even distribution 
		// over the unit sphere.  Otherwise points will clump more on the sphere near 
		// the corners of the cube.
		float dot = glm::dot(v, v);
		if (dot > 1.0f)
			continue;

		return glm::normalize(v);
	}
}

glm::vec3 MathHelper::RandHemisphereUnitVec3(glm::vec3 n)
{

	// Keep trying until we get a point on/in the hemisphere.
	while (true)
	{
		// Generate random point in the cube [-1,1]^3.
		glm::vec3 v = glm::vec3(MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f), MathHelper::RandF(-1.0f, 1.0f));

		// Ignore points outside the unit sphere in order to get an even distribution 
		// over the unit sphere.  Otherwise points will clump more on the sphere near 
		// the corners of the cube.
		float dot = glm::dot(v, v);
		if (dot > 1.0f)

			continue;

		// Ignore points in the bottom hemisphere.
		if (dot < 0.0f)

			continue;

		return glm::normalize(v);
	}

}

glm::mat4 MathHelper::reflect(glm::vec4 plane) {
	glm::vec4 Negative2 = glm::vec4(-2.0f, -2.0f, -2.0f, 0.0f);
	glm::vec4 P = glm::normalize(plane);
	glm::vec4 S = P * Negative2;
	glm::mat4 m(1.0f);
	m[0][0] += P[0] * S[0];
	m[1][1] += P[1] * S[1];
	m[2][2] += P[2] * S[2];
	m[3][3] += P[3] * S[3];
	return m;
}

glm::vec3 MathHelper::vectorMin(glm::vec3& a, glm::vec3& b) {
	glm::vec3 res = {

		{std::min(a.x,b.x)},
		{std::min(a.y,b.y)},
		{std::min(a.z,b.z)}
	};
	return res;
}

glm::vec3 MathHelper::vectorMax(glm::vec3& a, glm::vec3& b) {
	glm::vec3 res = {

		{std::max(a.x,b.x)},
		{std::max(a.y,b.y)},
		{std::max(a.z,b.z)}
	};
	return res;
}