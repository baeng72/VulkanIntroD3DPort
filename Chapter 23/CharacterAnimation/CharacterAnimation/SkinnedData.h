#pragma once
#include <vector>
#include <string>
#include <unordered_map>
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

///<summary>
/// Examples of AnimationClips are "Walk", "Run", "Attack", "Defend".
/// An AnimationClip requires a BoneAnimation for every bone to form
/// the animation clip.    
///</summary>
struct AnimationClip
{
	float GetClipStartTime()const;
	float GetClipEndTime()const;

	void Interpolate(float t, std::vector<glm::mat4>& boneTransforms)const;

	std::vector<BoneAnimation> BoneAnimations;
};


class SkinnedData
{
public:

	uint32_t BoneCount()const;

	float GetClipStartTime(const std::string& clipName)const;
	float GetClipEndTime(const std::string& clipName)const;

	void Set(
		std::vector<int>& boneHierarchy,
		std::vector<glm::mat4>& boneOffsets,
		std::unordered_map<std::string, AnimationClip>& animations);

	// In a real project, you'd want to cache the result if there was a chance
	// that you were calling this several times with the same clipName at 
	// the same timePos.
	void GetFinalTransforms(const std::string& clipName, float timePos,
		std::vector<glm::mat4>& finalTransforms)const;

private:
	// Gives parentIndex of ith bone.
	std::vector<int> mBoneHierarchy;

	std::vector<glm::mat4> mBoneOffsets;

	std::unordered_map<std::string, AnimationClip> mAnimations;
};