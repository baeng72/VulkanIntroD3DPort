#pragma once
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "MathHelper.h"

class Camera {
	glm::vec3 mPosition = { 0.0f,0.0f,0.0f };
	glm::vec3 mRight = { 1.0f,0.0f,0.0f };
	glm::vec3 mUp = { 0.0f,1.0f,0.0f };
	glm::vec3 mLook = { 0.0f,0.0f,1.0f };
	//cache frustrum properties
	float mNearZ = 0.0f;
	float mFarZ = 0.0f;
	float mAspect = 0.0f;
	float mFovY = 0.0f;
	float mNearWindowHeight = 0.0f;
	float mFarWindowHeight = 0.0f;
	bool mViewDirty = true;

	//cache view/proj matrices
	glm::mat4 mView = glm::mat4(1.0f);
	glm::mat4 mProj = glm::mat4(1.0f);
public:
	Camera();
	~Camera();

	//get/set world camera position
	glm::vec3 GetPosition()const {return mPosition;	};
	void SetPosition(float x, float y, float z);
	void SetPosition(const glm::vec3& v);

	//get camera basis vectors
	glm::vec3 GetRight()const { return mRight; }
	glm::vec3 GetUp()const { return mUp; }
	glm::vec3 GetLook()const { return mLook; }

	//get frustrum properties
	float GetNearZ()const { return mNearZ; }
	float GetFarZ()const { return mFarZ; }
	float GetAspect()const { return mAspect; }
	float GetFovY()const { return mFovY; }
	float GetFovX()const;

	//get near and far plane dimensions in view space coordinates.
	float GetNearWindowWidth()const { return mAspect * mNearWindowHeight; }
	float GetNearWindowHeight()const { return mNearWindowHeight; }
	float GetFarWindowWidth()const { return mAspect * mFarWindowHeight; }
	float GetFarWindowHeight()const { return mFarWindowHeight; }

	//set frustum
	void SetLens(float fovY, float aspect, float zn, float zf);

	//Define camera space via LookAt parameters.
	void LookAt(glm::vec3 pos, glm::vec3 target, glm::vec3 worldUp);

	//get view/proj matrices
	glm::mat4 GetView()const { return mView; }
	glm::mat4 GetProj()const { return mProj; }

	//strafe/walk the camera a distance d
	void Strafe(float angle);
	void Walk(float d);

	//Rotate the camera
	void Pitch(float angle);
	void RotateY(float angle);

	//after modifying camera position/orientation, call to rebuild the view matrix.
	void UpdateViewMatrix();
	


};