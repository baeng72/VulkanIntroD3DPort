#include "AnimationHelper.h"

Keyframe::Keyframe():TimePos(0.0f),Translation(0.0f),Scale(1.0f),RotationQuat(0.0f,0.0f,0.0f,1.0f) {
	
}

Keyframe::~Keyframe(){}

void BoneAnimation::Interpolate(float t,glm::mat4& M)const {
	if (t <= Keyframes.front().TimePos) {
		glm::mat4 rot = glm::mat4(Keyframes.front().RotationQuat);
		glm::mat4 scale = glm::scale(glm::mat4(1.0f),Keyframes.front().Scale);
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
				float lerpPercent = (t - Keyframes[i].TimePos) / (Keyframes[i+1].TimePos - Keyframes[i].TimePos);
				glm::vec3 transVec = glm::mix(Keyframes[i].Translation, Keyframes[i+1].Translation, lerpPercent);
				glm::vec3 scaleVec = glm::mix(Keyframes[i].Scale, Keyframes[i+1].Scale, lerpPercent);
				glm::quat quat = glm::lerp(Keyframes[i].RotationQuat, Keyframes[i+1].RotationQuat, lerpPercent);
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