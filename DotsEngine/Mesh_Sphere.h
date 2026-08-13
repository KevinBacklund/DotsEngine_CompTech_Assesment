#pragma once
#include "Mesh.h"
class Mesh_Sphere : public Mesh
{
public:
	Mesh_Sphere(float aRadius, glm::vec3 aColor, size_t maxInstances);
protected:
	void Mesh_CreateMesh() override;
	float radius;
};

