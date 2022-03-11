#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

///<summary>
/// A Keyframe defines the bone transformation at an instant in time.
///</summary>
struct Keyframe {
	Keyframe();
	~Keyframe();
	float TimePos;
	glm::vec3 Translation;
	glm::vec3 Scale;
	glm::quat RotationQuat;
};


///<summary>
/// A BoneAnimation is defined by a list of keyframes.  For time
/// values inbetween two keyframes, we interpolate between the
/// two nearest keyframes that bound the time.  
///
/// We assume an animation always has two keyframes.
///</summary>

struct BoneAnimation {
	float GetStartTime()const { return Keyframes.front().TimePos; }
	float GetEndTime()const { return Keyframes.back().TimePos; }
	void Interpolate(float t, glm::mat4& m)const;
	std::vector<Keyframe> Keyframes;
};