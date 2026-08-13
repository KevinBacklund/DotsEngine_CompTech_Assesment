#pragma once
#include "Mesh_Sphere.h"
#include "Shader.h"
#include <vector>
#include <Camera/FlyingCamera.h>
#include "DebugLines.h"
#include "tracy/Tracy.hpp"


struct DotVisual
{
	Shader* shader = new Shader("Assets/Shaders/VertexShader.glsl", "Assets/Shaders/FragmentShader.glsl");;
	Mesh_Sphere* mesh1;
	Mesh_Sphere* mesh2;
	Mesh_Sphere* mesh3;
	DotVisual(size_t maxInstances)
	{
		mesh1 = new Mesh_Sphere(1, glm::vec3(1, 0, 0), maxInstances);
		mesh2 = new Mesh_Sphere(2, glm::vec3(0, 1, 0), maxInstances);
		mesh3 = new Mesh_Sphere(3, glm::vec3(0, 0, 1), maxInstances);
	}

};

class Octant;

struct Dot
{
	Mesh_Sphere* mesh;

	Octant* octant = nullptr;

	glm::vec3 position;
	glm::vec3 velocity;
	float radius;
	int Health;

	//void Dot_Render(DotVisual* visual)
	//{
	//	mesh->Mesh_Render(visual->shader, position);
	//}

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

	std::vector<glm::vec3> mesh1Positions;
	std::vector<glm::vec3> mesh2Positions;
	std::vector<glm::vec3> mesh3Positions;

private:

	std::vector<Dot> dots;

	FlyingCamera* flyingCamera;
	DotVisual* dotVisual;
	Octant* octreeRoot = nullptr;
};

class Octant
{
	glm::vec3 center;
	float halfWidth;
	float looseHalfWidth;

	std::vector<Dot*> dots;

	Octant* parent = nullptr;
	Octant* children[8] = { nullptr };

	bool isLeaf = true;
	const int maxLevel = 4;
	int level = 0;

public:

	Octant(glm::vec3 aCenter, float aHalfWidth)
	{
		center = aCenter;
		halfWidth = aHalfWidth;
		looseHalfWidth = halfWidth * 1.5f;
	};

	~Octant()
	{
		RemoveChildren();
	}	

	void RemoveChildren()
	{
		if (isLeaf) return;
		for (int i = 0; i < 8; i++)
		{
			if (children[i] == nullptr) continue;
			children[i]->RemoveChildren();
			delete children[i];
			children[i] = nullptr;
		}
	}

	void DebugDraw()
	{
		DebugLines::DrawCube(center, glm::quat(), glm::vec3(halfWidth, halfWidth, halfWidth), glm::vec3(0, 1, 1));
		DebugLines::DrawSphere(center, 2, glm::vec3(1, 0, 1));
		if (isLeaf) return;
		for (int i = 0; i < 8; i++)
		{
			if (children[i] == nullptr) continue;
			children[i]->DebugDraw();
		}
	}



	void Subdivide(int childIndex)
	{
		isLeaf = false;
		glm::vec3 newCenter;
		newCenter = center;
		float newHalfWidth = halfWidth * 0.5f;

		if (childIndex & 1) newCenter.x += newHalfWidth; else newCenter.x -= newHalfWidth; // binary 1 = 0001
		if (childIndex & 2) newCenter.y += newHalfWidth; else newCenter.y -= newHalfWidth; // binary 2 = 0010 
		if (childIndex & 4) newCenter.z += newHalfWidth; else newCenter.z -= newHalfWidth; // binary 4 = 0100
		children[childIndex] = new Octant(newCenter, newHalfWidth);
		children[childIndex]->level = level + 1;
		children[childIndex]->parent = this;
	}

	int FindOctant(const glm::vec3& position)
	{
		int index = 0;
		if (position.x > center.x) index |= 1; 
		if (position.y > center.y) index |= 2; 
		if (position.z > center.z) index |= 4; 
		return index;
	}

	void InsertDot(Dot* dot)
	{
		if (dot->radius > halfWidth || level >= maxLevel)
		{
			dots.push_back(dot);
			dot->octant = this;
			return;
		}

		int octantIndex = FindOctant(dot->position);

		if (isLeaf || children[octantIndex] == nullptr)
		{
			Subdivide(octantIndex);
		}

		children[octantIndex]->InsertDot(dot);
	}

	void RemoveDot(Dot* dot)
	{
		dots.erase(std::remove(dots.begin(), dots.end(), dot), dots.end());
	}

	bool InLooseBoundry(const glm::vec3& position, float radius)
	{
		if (position.x + radius < center.x - looseHalfWidth || position.x - radius > center.x + looseHalfWidth) return false;
		if (position.y + radius < center.y - looseHalfWidth || position.y - radius > center.y + looseHalfWidth) return false;
		if (position.z + radius < center.z - looseHalfWidth || position.z - radius > center.z + looseHalfWidth) return false;

		return true;
	}

	void QueryRange(Dot* aDot, std::vector<Dot*>& results)	
	{
		if(!InLooseBoundry(aDot->position, aDot->radius)) return;

		for (auto& dot : dots)
		{
			if (dot == aDot) continue;
			results.push_back(dot);
		}

		if(!isLeaf)
		{
			for (int i = 0; i < 8; i++)
			{
				if (children[i] == nullptr) continue;
				children[i]->QueryRange(aDot, results);
			}
		}
	}
};
