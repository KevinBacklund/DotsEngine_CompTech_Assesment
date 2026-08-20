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
	//octreeRoot = new Octant(glm::vec3(0,0,0), bounds, dotCount * 0.01f);
	octree = new Octree;
	//octree->root = new Octant(glm::vec3(0, 0, 0), bounds, dotCount * 0.01f);
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
		octree->rebuildOctree(dots, bounds);
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

	//Handle collision
	{
		ZoneScopedN("Collision")

		for (int i = 0; i < threadPool->threadCount; i++)
		{
			int startIndex = (dots.size() / threadPool->threadCount) * i;
			int endIndex = (dots.size() / threadPool->threadCount) * (i + 1);
			if (i == threadPool->threadCount - 1) endIndex = dots.size();

			threadPool->Enqueue([this, startIndex, endIndex]()
			{
				ZoneScopedN("FindCollisions");
				std::vector<Dot*> rangeResults;
				rangeResults.reserve(25);

				for (int d1 = startIndex; d1 < endIndex; d1++)
				{
					{
						ZoneScopedN("OctreeQuery")
						octree->QueryRange(&dots[d1], rangeResults, octree->root);
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

					glm::vec3 n1 = glm::normalize(dotToCollide.dot1->velocity);
					glm::vec3 n2 = glm::normalize(dotToCollide.dot2->velocity);

					dotToCollide.dot2->velocity = glm::reflect(dotToCollide.dot2->velocity, n1);
					dotToCollide.dot1->velocity = glm::reflect(dotToCollide.dot1->velocity, n2);

					dotToCollide.dot2->Health--;
					dotToCollide.dot2->ReCreate(dotVisual);
					dotToCollide.dot1->Health--;
					dotToCollide.dot1->ReCreate(dotVisual);

					dotToCollide.dot2->position += n2 * 0.9f;
					dotToCollide.dot1->position += n1 * 0.9f;
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

		for (size_t i = 0; i < dots.size(); i++)
		{
			if (dots[i].Health <= 0)
			{
				toRemove.push_back(i);
				DotsScene_SpawnRandomDot();
			}
		}

		//Handle removal from vector and spawning new dot
		for (int i = toRemove.size() - 1; i >= 0; i--)
		{
			dots.erase(dots.begin() + toRemove[i]);
		}
		toRemove.clear();
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


void Octree::Subdivide(Octant* aOctant)
{
	aOctant->isLeaf = false;

	for (int i = 0; i < 8; i++)
	{
		glm::vec3 newCenter;
		newCenter = aOctant->center;
		float newHalfWidth = aOctant->halfWidth * 0.5f;

		if (i & 1) newCenter.x += newHalfWidth; else newCenter.x -= newHalfWidth; // binary 1 = 0001
		if (i & 2) newCenter.y += newHalfWidth; else newCenter.y -= newHalfWidth; // binary 2 = 0010 
		if (i & 4) newCenter.z += newHalfWidth; else newCenter.z -= newHalfWidth; // binary 4 = 0100

		aOctant->children[i] = octantIndex;
		octantObjectPool[octantIndex].level = aOctant->level + 1;
		octantObjectPool[octantIndex].center = newCenter;
		octantObjectPool[octantIndex].halfWidth = newHalfWidth;
		octantObjectPool[octantIndex].looseHalfWidth = newHalfWidth * 1.5f;
		octantObjectPool[octantIndex].isLeaf = true;
		octantObjectPool[octantIndex].octantDots.clear();
		octantIndex++;
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

void Octree::InsertDot(Dot* dot, Octant* aOctant)
{
	if (dot->radius > aOctant->halfWidth * 0.5f || aOctant->level >= maxLevel)
	{
		aOctant->octantDots.push_back(dot);
		dot->octant = aOctant;
		return;
	}

	if (aOctant->isLeaf)
	{
		Subdivide(aOctant);
	}

	int octantIndex = FindOctant(dot->position, aOctant);
	InsertDot(dot, &octantObjectPool[aOctant->children[octantIndex]]);
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
			QueryRange(aDot, results,&octantObjectPool[aOctant->children[i]]);
		}
	}
}

//void Octant::RemoveChildren()
//{
//	if (isLeaf) return;
//	for (int i = 0; i < 8; i++)
//	{
//		if (children[i] == nullptr) continue;
//		children[i]->RemoveChildren();
//		delete children[i];
//		children[i] = nullptr;
//	}
//}

void Octree::DebugDraw(Octant* aOctant)
{
	DebugLines::DrawCube(aOctant->center, glm::quat(), glm::vec3(aOctant->halfWidth, aOctant->halfWidth, aOctant->halfWidth), glm::vec3(0, 1, 1));
	DebugLines::DrawSphere(aOctant->center, 2, glm::vec3(1, 0, 1));
	if (aOctant->isLeaf) return;
	for (int i = 0; i < 8; i++)
	{
		DebugDraw(&octantObjectPool[aOctant->children[i]]);
	}
}