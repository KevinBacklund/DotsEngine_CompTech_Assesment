#pragma once
#include "Mesh_Sphere.h"
#include "Shader.h"
#include <vector>
#include <Camera/FlyingCamera.h>

struct DotVisual
{
	Shader* shader = new Shader("Assets/Shaders/VertexShader.glsl", "Assets/Shaders/FragmentShader.glsl");;
	Mesh_Sphere* mesh1 = new Mesh_Sphere(1, glm::vec3(1, 0, 0));;
	Mesh_Sphere* mesh2 = new Mesh_Sphere(2, glm::vec3(0, 1, 0));;
	Mesh_Sphere* mesh3 = new Mesh_Sphere(3, glm::vec3(0, 0, 1));;
};


struct Dot
{
	Mesh_Sphere* mesh;

	glm::vec3 position;
	glm::vec3 velocity;
	float radius;
	int Health;

	void Dot_Render(DotVisual* visual)
	{
		mesh->Mesh_Render(visual->shader, position);
	}

	void ReCreate(DotVisual* visual)
	{
		Mesh_Sphere* newMesh = visual->mesh3;
		switch (Health)
		{
		case 1:
			radius = 1;
			newMesh = visual->mesh1;
			break;
		case 2:
			radius = 2;
			newMesh = visual->mesh2;
			break;
		case 3:
			radius = 3;
			newMesh = visual->mesh3;
			break;
		default:
			break;
		}

		mesh = newMesh;
	}
};

class DotsScene
{
public:

	DotsScene();
	~DotsScene();

	void DotsScene_Start();

	void DotsScene_Render();
	void DotsScene_Update();

	void DotsScene_DrawBounds();

	void DotsScene_SpawnRandomDot();
	void DotsScene_SpawnDot(glm::vec3 aPosition, glm::vec3 aVelocity);

	void DotsScene_ClearDots();
	void DotsScene_LeaderBoardMode();

	float RandomFloat(float min, float max);

	static Camera* mainCamera;

	int dotCount = 200;
	float bounds = 50.0f;
	float dotSpeed = 5.0f;

private:

	std::vector<Dot> dots;

	FlyingCamera* flyingCamera;
	DotVisual* dotVisual;
};



