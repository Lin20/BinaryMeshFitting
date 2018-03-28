#pragma once

class Frustum
{
public:
	// Call this every time the camera moves to update the frustum
	void CalculateFrustum(float* proj, float* modl);

	// This takes a 3D point and returns TRUE if it's inside of the frustum
	bool PointInFrustum(float x, float y, float z);

	// This takes a 3D point and a radius and returns TRUE if the sphere is inside of the frustum
	bool SphereInFrustum(float x, float y, float z, float radius);

	// This takes the center and half the length of the cube.
	bool CubeInFrustum(float x, float y, float z, float size);

private:
	// This holds the A B C and D values for each side of our frustum.
	float m_Frustum[6][4];
};