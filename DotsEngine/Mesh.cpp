#include "Mesh.h"
#include <glad/glad.h>
#include "Shader.h"
#include "DotsScene.h"
#include <Camera/Camera.h>
#include "tracy/Tracy.hpp"


void Mesh::Mesh_CreateMesh()
{
	// nothing burger
}

void Mesh::Mesh_Setup(size_t maxInstances)
{
	Mesh_CreateMesh();

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &meshVBO);
	glGenBuffers(1, &instanceVBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, meshVBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(glm::vec3), vertices.data(), GL_DYNAMIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
	glVertexAttribDivisor(1, 1);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
}

void Mesh::Mesh_Render(Shader* aShader, std::vector<glm::vec3> positions)
{
	if(positions.empty()) return;



	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
	glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), positions.data(), GL_DYNAMIC_DRAW);

	aShader->Bind();

	//glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
	aShader->SetUniformMat4("ViewProjection", DotsScene::mainCamera->GetViewProjection());
	aShader->SetUniformVec3("Color", color);

	glBindVertexArray(VAO);

	glDrawElementsInstanced(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, (void*)0, positions.size());
	//glDisableVertexAttribArray(1);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	aShader->Unbind();
}
