#pragma once
#include "Mesh_Sphere.h"
#include "Shader.h"
#include <vector>
#include <Camera/FlyingCamera.h>
#include "DebugLines.h"


struct DotVisual
{
	Shader* shader = new Shader("Assets/Shaders/VertexShader.glsl", "Assets/Shaders/FragmentShader.glsl");;
	Mesh_Sphere* mesh1 = new Mesh_Sphere(1, glm::vec3(1, 0, 0));;
	Mesh_Sphere* mesh2 = new Mesh_Sphere(2, glm::vec3(0, 1, 0));;
	Mesh_Sphere* mesh3 = new Mesh_Sphere(3, glm::vec3(0, 0, 1));;
};

class Octant;

struct Dot
{
	Mesh_Sphere* mesh;

	Octant* octant = nullptr;

	glm::vec3 position;
	glm::vec3 velocity;
	//float timeSinceVelocityChange
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
	Octant* octreeRoot = nullptr;
};

class Octant
{
	glm::vec3 topRightFront;
	glm::vec3 bottomLeftBack;
	glm::vec3 center;

	std::vector<Dot*> dots;

	Octant* parent = nullptr;
	Octant* children[8] = { nullptr };

	const int MaxLevel = 4;
	int level = 0;

public:
	float bounds = 50.0f;

	Octant(const glm::vec3& aTopRightFront, const glm::vec3& aBottomLeftBack)
	{
		topRightFront = aTopRightFront;
		bottomLeftBack = aBottomLeftBack;
		center = (topRightFront + bottomLeftBack) / 2.0f;
		bounds = topRightFront.x;
	};

	~Octant()
	{
		if (children[0] == nullptr) return;
		for (int i = 0; i < 8; i++)
		{
			delete children[i];
		}
	}	

	void DebugDraw()
	{
		DebugLines::DrawCube(center, glm::quat(), (topRightFront - bottomLeftBack)/2.0f, glm::vec3(0, 1, 1));
		//DebugLines::DrawSphere(center, 2, glm::vec3(1, 0, 1));
		if (children[0] == nullptr) return;
		for (int i = 0; i < 8; i++)
		{
			children[i]->DebugDraw();
		}
	}

	void Subdivide()
	{
		glm::vec3 newTopRightFront, newBottomLeftBack;
		for (int i = 0; i < 8; i++)
		{
			newTopRightFront = topRightFront;
			newBottomLeftBack = bottomLeftBack;

			if (i & 1) newBottomLeftBack.x = center.x; else newTopRightFront.x = center.x; // binary 1 = 0001
			if (i & 2) newBottomLeftBack.y = center.y; else newTopRightFront.y = center.y; // binary 2 = 0010 
			if (i & 4) newBottomLeftBack.z = center.z; else newTopRightFront.z = center.z; // binary 4 = 0100

			children[i] = new Octant(newTopRightFront, newBottomLeftBack);
			children[i]->level = level + 1;
			children[i]->parent = this;
			for (auto& dot : dots)
			{
				if (children[i]->InBoundry(dot->position, dot->radius))
				{
					children[i]->InsertDot(dot);
					dots.erase(std::remove(dots.begin(), dots.end(), dot), dots.end());
				}
			}
		}
	}

	void InsertDot(Dot* dot){
		if (level >= MaxLevel || dots.empty())
		{
			dots.push_back(dot);
			dot->octant = this;
			return;
		}
		if (children[0] == nullptr)
		{
			Subdivide();
		}
		for (int i = 0; i < 8; i++)
		{
			if (children[i]->InBoundry(dot->position, dot->radius))
			{
				children[i]->InsertDot(dot);
				return;
			}
		}
		dots.push_back(dot);
		dot->octant = this;
	}

	void RemoveDot(Dot* dot)
	{
		dots.erase(std::remove(dots.begin(), dots.end(), dot), dots.end());
	}

	bool InBoundry(const glm::vec3& position, float radius)
	{
		if (position.x + radius < bottomLeftBack.x || position.x - radius > topRightFront.x) return false;
		if (position.y + radius < bottomLeftBack.y || position.y - radius > topRightFront.y) return false;
		if (position.z + radius < bottomLeftBack.z || position.z - radius > topRightFront.z) return false;

		return true;
	}

	void QueryRange(Dot* aDot, std::vector<Dot*>& results)	
	{
		std::vector<Dot*> tempResults;
		for (auto& dot : dots)
		{
			if (dot == aDot) continue;
			tempResults.push_back(dot);
		}
		results = tempResults;
	}
};
