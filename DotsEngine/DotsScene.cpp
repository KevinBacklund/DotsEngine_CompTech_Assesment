#include "DotsScene.h"
#include "DebugLines.h"
#include <Time/Time.h>

#include <random>
#include <glm/gtc/random.hpp>
#include <iostream>

Camera* DotsScene::mainCamera;

DotsScene::DotsScene()
{
	mainCamera = new Camera();
	flyingCamera = new FlyingCamera(mainCamera);
	dotVisual = new DotVisual(dotCount);
	octTree = new Octree;
	threadPool = new ThreadPool();

	mainCamera->SetPosition(glm::vec3(0, 0, bounds * 4));
}

void DotsScene::DotsScene_Render()
{
	ZoneScoped;

	for (auto& dot : dots)
	{
		switch (dot.Health)
		{
		case 1:
			mesh1Positions.push_back(dot.position);
			break;
		case 2:
			mesh2Positions.push_back(dot.position);
			break;
		case 3:
			mesh3Positions.push_back(dot.position);
			break;
		default:
			break;
		}
	}
	dotVisual->mesh1->Mesh_Render(dotVisual->shader, mesh1Positions);
	dotVisual->mesh2->Mesh_Render(dotVisual->shader, mesh2Positions);
	dotVisual->mesh3->Mesh_Render(dotVisual->shader, mesh3Positions);

	mesh1Positions.clear();
	mesh2Positions.clear();
	mesh3Positions.clear();

	DotsScene_DrawBounds();
}

void DotsScene::DotsScene_Update()
{
	ZoneScoped;
	flyingCamera->Update();

	//Handle velocity
	{
		ZoneScopedN("Velocity")
		for (auto& dot : dots)
		{
			dot.position += glm::normalize(dot.velocity) * (dotSpeed / dot.radius) * Time::deltaTime;
		}
	}

	{
		ZoneScopedN("RebuildOctree")
		octTree->rebuildOctree(dots, bounds);
		for (int i = 0; i < 8; i++)
		{
			size_t startIndex = (dots.size() / 8) * i;
			size_t endIndex = (dots.size() / 8) * (i + 1);
			if (i == 7) endIndex = dots.size();

			int childIndex = i + 1;
			threadPool->Enqueue([this, startIndex, endIndex, childIndex]()
			{
				ZoneScopedN("RebuildOctreePart");
				for (size_t i = startIndex; i < endIndex; i++)
				{
					octTree->InsertDot(&dots[i], &octTree->octantObjectPool[childIndex], childIndex - 1);
				}
			});
		}
		//octree->DebugDraw(octree->root);
	}

	//Handle bounds
	{
		ZoneScopedN("Bounds")
		for (auto& dot : dots)
		{
			if (dot.position.x > bounds - dot.radius || dot.position.x < -bounds + dot.radius) dot.velocity.x *= -1;
			if (dot.position.y > bounds - dot.radius || dot.position.y < -bounds + dot.radius) dot.velocity.y *= -1;
			if (dot.position.z > bounds - dot.radius || dot.position.z < -bounds + dot.radius) dot.velocity.z *= -1;

			dot.position.x = glm::clamp(dot.position.x, -bounds + dot.radius, bounds - dot.radius);
			dot.position.y = glm::clamp(dot.position.y, -bounds + dot.radius, bounds - dot.radius);
			dot.position.z = glm::clamp(dot.position.z, -bounds + dot.radius, bounds - dot.radius);
		}
	}

	if (dots.empty()) return;

	threadPool->WaitForTasks();

	//Handle collision
	{
		ZoneScopedN("Collision")

		for (int i = 0; i < threadPool->threadCount; i++)
		{
			size_t startIndex = (dots.size() / threadPool->threadCount) * i;
			size_t endIndex = (dots.size() / threadPool->threadCount) * (i + 1);
			if (i == threadPool->threadCount - 1) endIndex = dots.size();

			threadPool->Enqueue([this, startIndex, endIndex]()
			{
				ZoneScopedN("FindCollisions");
				std::vector<Dot*> rangeResults;
				rangeResults.reserve(25);

				for (size_t d1 = startIndex; d1 < endIndex; d1++)
				{
					{
						ZoneScopedN("OctreeQuery")
						octTree->QueryRange(&dots[d1], rangeResults, octTree->root);
					}
					{
						ZoneScopedN("CollisionCheck")
						for (size_t d2 = 0; d2 < rangeResults.size(); d2++)
						{
							if (&dots[d1] == rangeResults[d2]) continue;
							if (glm::distance(dots[d1].position, rangeResults[d2]->position) < dots[d1].radius + rangeResults[d2]->radius)
							{
								collisionMutex.lock();
								dotsToCollide.insert({ &dots[d1], rangeResults[d2] });
								collisionMutex.unlock();
							}
						}
						rangeResults.clear();
					}
				}
			});
		}

		threadPool->WaitForTasks();

		for (int i = 0; i < threadPool->threadCount; i++)
		{
			threadPool->Enqueue([this]()
			{
				ZoneScopedN("ProcessCollisions");

				while (!dotsToCollide.empty())
				{
					collisionMutex.lock();
					if (dotsToCollide.empty())
					{
						collisionMutex.unlock();
						continue;
					}
					CollisionPair dotToCollide = *dotsToCollide.begin();
					dotsToCollide.erase(dotToCollide);
					collisionMutex.unlock();

					glm::vec3 dir1 = glm::normalize(dotToCollide.dot1->velocity);
					glm::vec3 dir2 = glm::normalize(dotToCollide.dot2->velocity);
					glm::vec3 surface = glm::normalize(dotToCollide.dot1->position - dotToCollide.dot2->position);

					dotToCollide.dot2->velocity = glm::reflect(dotToCollide.dot2->velocity, surface);
					dotToCollide.dot1->velocity = glm::reflect(dotToCollide.dot1->velocity, surface);

					dotToCollide.dot2->Health--;
					dotToCollide.dot2->ReCreate(dotVisual);
					dotToCollide.dot1->Health--;
					dotToCollide.dot1->ReCreate(dotVisual);

					dotToCollide.dot2->position += dir2 * 0.9f;
					dotToCollide.dot1->position += dir1 * 0.9f;
				}
			});
		}
	}

	//Look for nan's
	//for (auto& dot : dots)
	//{
	//	if (glm::any(glm::isnan(dot.position)))
	//	{
	//		std::cout << "NAN FOUND IN DOT" << std::endl;
	//	}
	//}

	//Handle death, spawn new dot

	threadPool->WaitForTasks();

	{
		ZoneScopedN("DeathCheck")
		auto it = dots.begin();
		while (it != dots.end())
		{
			if (it->Health <= 0)
			{
				auto lastElement = dots.end() - 1;
				if (it != lastElement) *it = std::move(*lastElement);
				dots.pop_back();
				continue;
			}
			++it;
		}
	}
}

void DotsScene::DotsScene_DrawBounds()
{
	DebugLines::DrawCube(glm::vec3(0, 0, 0), glm::quat(), glm::vec3(bounds, bounds, bounds), glm::vec3(1, 0, 0));
}

void DotsScene::DotsScene_SpawnRandomDot()
{
	glm::vec3 position;
	position.x = RandomFloat(-bounds, bounds);
	position.y = RandomFloat(-bounds, bounds);
	position.z = RandomFloat(-bounds, bounds);

	Dot newDot;
	newDot.Health = 3;
	newDot.ReCreate(dotVisual);

	glm::vec3 velocity = glm::sphericalRand(1.0f);
	newDot.velocity = velocity;
	newDot.position = position;

	dots.push_back(newDot);
}

void DotsScene::DotsScene_SpawnDot(glm::vec3 aPosition, glm::vec3 aVelocity)
{
	Dot newDot;
	newDot.Health = 3;
	newDot.ReCreate(dotVisual);

	newDot.velocity = aVelocity;
	newDot.position = aPosition;

	dots.push_back(newDot);
}

void DotsScene::DotsScene_ClearDots()
{
	dots.clear();
}

float DotsScene::RandomFloat(float min, float max)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist(min, max);
	return dist(gen);
}

void DotsScene::DotsScene_Start()
{
	for (size_t i = 0; i < dotCount; i++)
	{
		DotsScene_SpawnRandomDot();
	}
}

DotsScene::~DotsScene()
{
	 delete mainCamera;
	delete flyingCamera;
	delete dotVisual;
}

void DotsScene::DotsScene_LeaderBoardMode()
{
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
	DotsScene_ClearDots();
	float spacing = 8.0f;
	float volumePerEntity = spacing * spacing * spacing;
	float totalVolume = dotCount * volumePerEntity;
	dotSpeed = 7.0f;
	bounds = cbrt(totalVolume);
	DotsScene_Start();
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
}


void Octree::Subdivide(Octant* aOctant, int octTreeIndex)
{
	aOctant->isLeaf = false;
	aOctant->firstChildIndex = octTreeIndexes[octTreeIndex];
	for (int i = 0; i < 8; i++)
	{
		glm::vec3 newCenter;
		newCenter = aOctant->center;
		float newHalfWidth = aOctant->halfWidth * 0.5f;

		if (i & 1) newCenter.x += newHalfWidth; else newCenter.x -= newHalfWidth;
		if (i & 2) newCenter.y += newHalfWidth; else newCenter.y -= newHalfWidth;  
		if (i & 4) newCenter.z += newHalfWidth; else newCenter.z -= newHalfWidth; 

		octantObjectPool[octTreeIndexes[octTreeIndex]].level = aOctant->level + 1;
		octantObjectPool[octTreeIndexes[octTreeIndex]].center = newCenter;
		octantObjectPool[octTreeIndexes[octTreeIndex]].halfWidth = newHalfWidth;
		octantObjectPool[octTreeIndexes[octTreeIndex]].looseHalfWidth = newHalfWidth * 1.5f;
		octantObjectPool[octTreeIndexes[octTreeIndex]].isLeaf = true;
		octantObjectPool[octTreeIndexes[octTreeIndex]].octantDots.clear();
		octTreeIndexes[octTreeIndex]++;
	}
}
int Octree::FindOctant(const glm::vec3& position, Octant* aOctant)
{
	int index = 0;
	if (position.x > aOctant->center.x) index |= 1;
	if (position.y > aOctant->center.y) index |= 2;
	if (position.z > aOctant->center.z) index |= 4;
	return index;
}

void Octree::InsertDot(Dot* dot, Octant* aOctant, int octTreeIndex)
{
	if (dot->radius > aOctant->halfWidth * 0.5f || aOctant->level >= maxLevel)
	{
		aOctant->octantDots.push_back(dot);
		dot->octant = aOctant;
		return;
	}

	if (aOctant->isLeaf)
	{
		Subdivide(aOctant, octTreeIndex);
	}

	int childOctantIndex = aOctant->firstChildIndex + FindOctant(dot->position, aOctant);
	InsertDot(dot, &octantObjectPool[childOctantIndex], octTreeIndex);
}

bool Octree::InLooseBoundry(const glm::vec3& position, float radius, Octant* aOctant)
{
	if (position.x + radius < aOctant->center.x - aOctant->looseHalfWidth || position.x - radius > aOctant->center.x + aOctant->looseHalfWidth) return false;
	if (position.y + radius < aOctant->center.y - aOctant->looseHalfWidth || position.y - radius > aOctant->center.y + aOctant->looseHalfWidth) return false;
	if (position.z + radius < aOctant->center.z - aOctant->looseHalfWidth || position.z - radius > aOctant->center.z + aOctant->looseHalfWidth) return false;

	return true;
}

void Octree::QueryRange(Dot* aDot, std::vector<Dot*>& results, Octant* aOctant)
{
	if (!InLooseBoundry(aDot->position, aDot->radius, aOctant)) return;

	for (auto& dot : aOctant->octantDots)
	{
		if (dot == aDot) continue;
		results.push_back(dot);
	}

	if (!aOctant->isLeaf)
	{
		for (int i = 0; i < 8; i++)
		{
			QueryRange(aDot, results,&octantObjectPool[aOctant->firstChildIndex + i]);
		}
	}
}

void Octree::DebugDraw(Octant* aOctant)
{
	DebugLines::DrawCube(aOctant->center, glm::quat(), glm::vec3(aOctant->halfWidth, aOctant->halfWidth, aOctant->halfWidth), glm::vec3(0, 1, 1));
	DebugLines::DrawSphere(aOctant->center, 2, glm::vec3(1, 0, 1));
	if (aOctant->isLeaf) return;
	for (int i = 0; i < 8; i++)
	{
		DebugDraw(&octantObjectPool[aOctant->firstChildIndex + i]);
	}
}

void Octree::rebuildOctree(std::vector<Dot>& dots, float bounds)
{
	//octantIndex = 0;
	root = &octantObjectPool[0];
	root->center = glm::vec3(0, 0, 0);
	root->halfWidth = bounds;
	root->looseHalfWidth = bounds * 1.5f;
	root->isLeaf = true;
	root->level = 0;
	//octantIndex++;
	octTreeIndexes[0] = 1;
	Subdivide(root, 0);
	
	for (int i = 1; i < 8; i++)
	{
		octTreeIndexes[i] = (octantObjectPool.size() / 8) * i;
	}
	//for (auto& dot : dots)
	//{
	//	InsertDot(&dot, root);
	//}
};