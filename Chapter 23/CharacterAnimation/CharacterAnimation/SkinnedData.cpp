#include "SkinnedData.h"
#include "../../Common/MathHelper.h"

Keyframe::Keyframe()
	: TimePos(0.0f),
	Translation(0.0f, 0.0f, 0.0f),
	Scale(1.0f, 1.0f, 1.0f),
	RotationQuat(0.0f, 0.0f, 0.0f, 1.0f)
{
}

Keyframe::~Keyframe()
{
}


void BoneAnimation::Interpolate(float t, glm::mat4& M)const {
	if (t <= Keyframes.front().TimePos) {
		glm::mat4 rot = glm::mat4(Keyframes.front().RotationQuat);
		glm::mat4 scale = glm::scale(glm::mat4(1.0f), Keyframes.front().Scale);
		glm::mat4 trans = glm::translate(glm::mat4(1.0f), Keyframes.front().Translation);
		M = scale * rot * trans;
	}
	else if (t >= Keyframes.back().TimePos) {
		glm::mat4 rot = glm::mat4(Keyframes.back().RotationQuat);
		glm::mat4 scale = glm::scale(glm::mat4(1.0f), Keyframes.back().Scale);
		glm::mat4 trans = glm::translate(glm::mat4(1.0f), Keyframes.back().Translation);
		M = scale * rot * trans;
	}
	else {
		for (size_t i = 0; i < Keyframes.size() - 1; ++i) {
			if (t >= Keyframes[i].TimePos && t <= Keyframes[i + 1].TimePos) {
				float lerpPercent = (t - Keyframes[i].TimePos) / (Keyframes[i + 1].TimePos - Keyframes[i].TimePos);
				glm::vec3 transVec = glm::mix(Keyframes[i].Translation, Keyframes[i + 1].Translation, lerpPercent);
				glm::vec3 scaleVec = glm::mix(Keyframes[i].Scale, Keyframes[i + 1].Scale, lerpPercent);
				glm::quat quat = glm::lerp(Keyframes[i].RotationQuat, Keyframes[i + 1].RotationQuat, lerpPercent);
				glm::mat4 rot = glm::mat4(quat);
				glm::mat4 scale = glm::scale(glm::mat4(1.0f), scaleVec);
				glm::mat4 trans = glm::translate(glm::mat4(1.0f), transVec);
				//M =  scale* rot* trans;
				M = trans * rot * scale;
				break;
			}
		}
	}
}

float AnimationClip::GetClipStartTime()const
{
	// Find smallest start time over all bones in this clip.
	float t = MathHelper::Infinity;
	for (UINT i = 0; i < BoneAnimations.size(); ++i)
	{
		t = MathHelper::Min(t, BoneAnimations[i].GetStartTime());
	}

	return t;
}

float AnimationClip::GetClipEndTime()const
{
	// Find largest end time over all bones in this clip.
	float t = 0.0f;
	for (UINT i = 0; i < BoneAnimations.size(); ++i)
	{
		t = MathHelper::Max(t, BoneAnimations[i].GetEndTime());
	}

	return t;
}

void AnimationClip::Interpolate(float t, std::vector<glm::mat4>& boneTransforms)const
{
	for (UINT i = 0; i < BoneAnimations.size(); ++i)
	{
		BoneAnimations[i].Interpolate(t, boneTransforms[i]);
	}
}

float SkinnedData::GetClipStartTime(const std::string& clipName)const
{
	auto clip = mAnimations.find(clipName);
	return clip->second.GetClipStartTime();
}

float SkinnedData::GetClipEndTime(const std::string& clipName)const
{
	auto clip = mAnimations.find(clipName);
	return clip->second.GetClipEndTime();
}

uint32_t SkinnedData::BoneCount()const
{
	return (uint32_t)mBoneHierarchy.size();
}

void SkinnedData::Set(std::vector<int>& boneHierarchy,
	std::vector<glm::mat4>& boneOffsets,
	std::unordered_map<std::string, AnimationClip>& animations)
{
	mBoneHierarchy = boneHierarchy;
	mBoneOffsets = boneOffsets;
	mAnimations = animations;
}

void SkinnedData::GetFinalTransforms(const std::string& clipName, float timePos, std::vector<glm::mat4>& finalTransforms)const
{
	uint32_t numBones = (uint32_t)mBoneOffsets.size();

	std::vector<glm::mat4> toParentTransforms(numBones);

	// Interpolate all the bones of this clip at the given time instance.
	auto clip = mAnimations.find(clipName);
	clip->second.Interpolate(timePos, toParentTransforms);

	//
	// Traverse the hierarchy and transform all the bones to the root space.
	//

	std::vector<glm::mat4> toRootTransforms(numBones);

	// The root bone has index 0.  The root bone has no parent, so its toRootTransform
	// is just its local bone transform.
	toRootTransforms[0] = toParentTransforms[0];

	// Now find the toRootTransform of the children.
	for (uint32_t i = 1; i < numBones; ++i)
	{
		glm::mat4 toParent = toParentTransforms[i];

		int parentIndex = mBoneHierarchy[i];
		glm::mat4 parentToRoot = toRootTransforms[parentIndex];

		glm::mat4 toRoot = parentToRoot * toParent;// XMMatrixMultiply(toParent, parentToRoot);

		toRootTransforms[i] = toRoot;
		//XMStoreFloat4x4(&toRootTransforms[i], toRoot);
	}

	// Premultiply by the bone offset transform to get the final transform.
	for (UINT i = 0; i < numBones; ++i)
	{
		glm::mat4 offset = mBoneOffsets[i];
		glm::mat4 toRoot = toRootTransforms[i];
		glm::mat4 finalTransform = toRoot * offset;
		//XMMATRIX finalTransform = XMMatrixMultiply(offset, toRoot);
		finalTransforms[i] = finalTransform;
		//XMStoreFloat4x4(&finalTransforms[i], XMMatrixTranspose(finalTransform));
	}
}