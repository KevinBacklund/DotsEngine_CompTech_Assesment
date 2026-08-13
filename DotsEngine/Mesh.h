#pragma once
#include <vector>
#include <glm/glm.hpp>

class Shader;

class Mesh
{
public:

	void Mesh_Setup(size_t maxInstances);
	void Mesh_Render(Shader* aShader, std::vector<glm::vec3> positions);

protected:
	virtual void Mesh_CreateMesh();
	std::vector<float> vertices;
	std::vector<unsigned int> indices;
	glm::vec3 color;

private:
	unsigned int VAO, meshVBO, instanceVBO, EBO;
};

