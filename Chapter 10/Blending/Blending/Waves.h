#pragma once
#include <vector>
#include <glm/glm.hpp>

class Waves {
	int		mNumRows{ 0 };
	int		mNumCols{ 0 };

	int		mVertexCount{ 0 };
	int		mTriangleCount{ 0 };

	// Simulation constants we can precompute.
	float mK1 = 0.0f;
	float mK2 = 0.0f;
	float mK3 = 0.0f;

	float mTimeStep = 0.0f;
	float mSpatialStep = 0.0f;

	std::vector<glm::vec3>	mPrevSolution;
	std::vector<glm::vec3>	mCurrSolution;
	std::vector<glm::vec3>	mNormals;
	std::vector<glm::vec3>	mTangentX;

public:
	Waves(int m, int n, float dx, float dt, float speed, float damping);
	Waves(const Waves& rhs) = delete;
	Waves& operator=(const Waves& rhs) = delete;
	~Waves();

	int RowCount()const;
	int ColumnCount()const;
	int VertexCount()const;
	int TriangleCount()const;
	float Width()const;
	float Depth()const;

	//returns the solution at the ith grid point
	const glm::vec3& Position(int i)const {
		return mCurrSolution[i];
	}

	//returns the solution normal at the ith grid point.
	const glm::vec3& Normal(int i)const { return mNormals[i]; }

	const glm::vec3& TangentX(int i)const { return mTangentX[i]; }

	void Update(float dt);
	void Disturb(int i, int j, float magnitude);

};