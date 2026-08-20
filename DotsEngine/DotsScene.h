#pragma once
#include "Mesh_Sphere.h"
#include "Shader.h"
#include <vector>
#include <Camera/FlyingCamera.h>
#include "DebugLines.h"
#include "tracy/Tracy.hpp"
#include <thread>
#include <mutex>
#include <unordered_set>
#include <semaphore>
#include <cmath>

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

class ThreadPool
{
public:

	int threadCount = 0;

	ThreadPool(size_t numThreads = std::thread::hardware_concurrency())
	{
		threadCount = numThreads;
		for (size_t i = 0; i < numThreads; i++)
		{
			threads.emplace_back([this, i]()
			{
				std::string threadName = "Thread " + std::to_string(i);
				tracy::SetThreadName(threadName.c_str());
				while (true)
				{
					std::function<void()> task;
					{
						std::unique_lock<std::mutex> lock(queueMutex);
						conditionAddTask.wait(lock, [this]() { return stop || !tasks.empty(); });
						if (stop && tasks.empty()) return;

						task = std::move(tasks.back());
						tasks.pop_back();
					}
					waitMutex.lock();
					busyThreads++;
					waitMutex.unlock();

					task();

					waitMutex.lock();
					busyThreads--;
					waitMutex.unlock();

					conditionTaskWait.notify_one();
				}
			});
		}
	}

	~ThreadPool()
	{
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			stop = true;
		}
		conditionAddTask.notify_all();
		for (std::thread& thread : threads)
		{
			thread.join();
		}
	}

	void WaitForTasks()
	{
		std::unique_lock<std::mutex> lock(waitMutex);
		conditionTaskWait.wait(lock, [this](){ return tasks.empty() && (busyThreads == 0); });
	}

	void Enqueue(std::function<void()> task)
	{
		
		std::unique_lock<std::mutex> lock(queueMutex);
		tasks.push_back(std::move(task));
		conditionAddTask.notify_one();
	}

private:
	std::vector<std::thread> threads;
	std::vector<std::function<void()>> tasks;
	std::mutex queueMutex;
	std::mutex waitMutex;
	std::condition_variable conditionAddTask;
	std::condition_variable conditionTaskWait;
	bool stop = false;
	int busyThreads = 0;
};

class Octant;
class Octree;

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

struct CollisionPair
{
	Dot* dot1;
	Dot* dot2;

	bool operator==(const CollisionPair& other) const
	{
		return (dot1 == other.dot1 && dot2 == other.dot2) || (dot1 == other.dot2 && dot2 == other.dot1);
	}
};

namespace std
{
	template<> struct hash<CollisionPair>
	{
		std::size_t operator()(const CollisionPair& pair) const
		{
			return std::hash<Dot*>()(pair.dot1) ^ std::hash<Dot*>()(pair.dot2);
		}
	};
}

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

	std::mutex collisionMutex;
	std::counting_semaphore<64> dotPairSemaphore{0};


private:

	std::vector<Dot> dots;
	std::vector<size_t> toRemove;


	FlyingCamera* flyingCamera;
	DotVisual* dotVisual;
	Octant* octreeRoot = nullptr;
	Octree* octree;
	ThreadPool* threadPool;
	std::unordered_set<CollisionPair> dotsToCollide;

};

class Octant
{


public:
	Octant(glm::vec3 aCenter, float aHalfWidth, size_t adotReserveSize = 10)
	{
		center = aCenter;
		halfWidth = aHalfWidth;
		looseHalfWidth = halfWidth * 1.5f;
		dotReserveSize = adotReserveSize;
		octantDots.reserve(dotReserveSize);
	};

	~Octant()
	{
	}	

	std::vector<Dot*> octantDots;

	glm::vec3 center;
	float halfWidth;
	float looseHalfWidth;
	int level = 0;



	int children[8];

	bool isLeaf = true;
	int dotReserveSize;

};

class Octree
{
public:

	Octree() 
	{
		int maxOctants = pow(8, maxLevel);
		for (int i = 0; i < maxOctants; i++)
		{
			Octant newOctant = Octant(glm::vec3(0, 0, 0), 0);
			octantObjectPool.push_back(newOctant);
		}
	};

	std::vector<Octant> octantObjectPool;
	Octant* root = nullptr;
	const int maxLevel = 5;
	int octantIndex = 0;

	void rebuildOctree(std::vector<Dot> &dots, float bounds)
	{
		octantIndex = 0;
		root = &octantObjectPool[octantIndex];
		root->center = glm::vec3(0, 0, 0);
		root->halfWidth = bounds;
		root->looseHalfWidth = bounds * 1.5f;
		root->isLeaf = true;
		root->level = 0;
		octantIndex++;
		for (auto& dot : dots)
		{
			InsertDot(&dot, root);
		}
	};

	void DebugDraw(Octant* aOctant);

	void Subdivide(Octant* aOctant);

	int FindOctant(const glm::vec3& position, Octant* aOctant);

	void InsertDot(Dot* dot, Octant* aOctant);

	void RemoveDot(Dot* dot, Octant* aOctant)
	{
		aOctant->octantDots.erase(std::remove(aOctant->octantDots.begin(), aOctant->octantDots.end(), dot), aOctant->octantDots.end());
	}

	bool InLooseBoundry(const glm::vec3& position, float radius, Octant* aOctant);

	void QueryRange(Dot* aDot, std::vector<Dot*>& results, Octant* aOctant);
};