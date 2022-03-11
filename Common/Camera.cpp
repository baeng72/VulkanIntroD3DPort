#include "Camera.h"

Camera::Camera() {
	SetLens(0.25f * MathHelper::Pi, 1.0f, 1.0f, 1000.0f);
}

Camera::~Camera() {
}

void Camera::SetPosition(float x, float y, float z) {
	mPosition = glm::vec3(x, y, z);
	mViewDirty = true;
}

void Camera::SetPosition(const glm::vec3& v) {
	mPosition = v;
	mViewDirty = true;
}

float Camera::GetFovX()const {
	float halfWidth = 0.5f * GetNearWindowWidth();
	return 2.0f * atan(halfWidth / mNearZ);
}

void Camera::SetLens(float fovY, float aspect, float zn, float zf) {
	//cache properties
	mFovY = fovY;
	mAspect = aspect;
	mNearZ = zn;
	mFarZ = zf;

	mNearWindowHeight = 2.0f * mNearZ * tanf(0.5f * mFovY);
	mFarWindowHeight = 2.0f * mFarZ * tanf(0.5f * mFovY);

	mProj = glm::perspectiveLH_ZO(mFovY, mAspect, mNearZ, mFarZ);//check this, might not be correct
}


void Camera::LookAt(glm::vec3 pos, glm::vec3 target, glm::vec3 worldUp) {
	glm::vec3 L = glm::normalize(target - pos);
	glm::vec3 R = glm::normalize(glm::cross(worldUp, L));
	glm::vec3 U = glm::cross(L, R);

	mPosition = pos;
	mLook = L;
	mRight = R;
	mUp = U;
	mViewDirty = true;
}

void Camera::Strafe(float d) {
	mPosition += d * mRight;
	mViewDirty = true;
}

void Camera::Walk(float d) {
	mPosition += d * mLook;
	mViewDirty = true;
}

void Camera::Pitch(float angle) {
	//Rotate up and look vector about the right vector
	glm::mat4 R = glm::rotate(glm::mat4(1.0f), angle, mRight);
	mUp = R * glm::vec4(mUp,0.0f);//not points, vectors, so 4th element is 0.0f
	mLook = R * glm::vec4(mLook,0.0f);
	mViewDirty = true;
}

void Camera::RotateY(float angle) {
	//Rotate the basic vectors about the world y-axis
	glm::mat4 R = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 1.0f, 0));

	mRight = R * glm::vec4(mRight, 0.0f);
	mUp = R * glm::vec4(mUp, 0.0f);
	mLook = R * glm::vec4(mLook, 0.0f);

	mViewDirty = true;
}

void Camera::UpdateViewMatrix() {
	if (mViewDirty) {
		//keep camera's axes orthogonal to each other and of unit length
		glm::vec3 L = glm::normalize(mLook);
		glm::vec3 U = glm::normalize(glm::cross(L,mRight));

		//U, L alrady ortho-normal, so no need to normalize cross product
		glm::vec3 R = glm::cross(U, L);

		//Fill in the view matrix entries
		float x = -glm::dot(mPosition, R);
		float y = -glm::dot(mPosition, U);
		float z = -glm::dot(mPosition, L);

		mRight = R;
		mUp = U;
		mLook = L;
//#define __TEST__
#ifdef __TEST__
		glm::mat4 view = glm::lookAtLH(mPosition, mLook, mUp);
		mView = view;
#else
		mView[0][0] = mRight.x;
		mView[1][0] = mRight.y;
		mView[2][0] = mRight.z;
		mView[3][0] = x;

		mView[0][1] = mUp.x;
		mView[1][1] = mUp.y;
		mView[2][1] = mUp.z;
		mView[3][1] = y;

		mView[0][2] = mLook.x;
		mView[1][2] = mLook.y;
		mView[2][2] = mLook.z;
		mView[3][2] = z;

		mView[0][3] = 0.0f;
		mView[1][3] = 0.0f;
		mView[2][3] = 0.0f;
		mView[3][3] = 1.0f;

		
#endif
		mViewDirty = false;

	}
}