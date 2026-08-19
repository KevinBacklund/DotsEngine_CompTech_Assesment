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
	octreeRoot = new Octant(glm::vec3(0,0,0), bounds, dotCount * 0.01f);
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
		delete octreeRoot;
		octreeRoot = new Octant(glm::vec3(0, 0, 0), bounds);
		for (auto& dot : dots)
		{
			octreeRoot->InsertDot(&dot);
		}
		//octreeRoot->DebugDraw();
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
				rangeResults.reserve(dotCount * 0.002f);

				for (int d1 = startIndex; d1 < endIndex; d1++)
				{
					{
						ZoneScopedN("OctreeQuery")
						octreeRoot->QueryRange(&dots[d1], rangeResults);
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
	for (auto& dot : dots)
	{
		if (glm::any(glm::isnan(dot.position)))
		{
			std::cout << "NAN FOUND IN DOT" << std::endl;
		}
	}

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


void Octant::Subdivide(int childIndex)
{
	isLeaf = false;
	glm::vec3 newCenter;
	newCenter = center;
	float newHalfWidth = halfWidth * 0.5f;

	if (childIndex & 1) newCenter.x += newHalfWidth; else newCenter.x -= newHalfWidth; // binary 1 = 0001
	if (childIndex & 2) newCenter.y += newHalfWidth; else newCenter.y -= newHalfWidth; // binary 2 = 0010 
	if (childIndex & 4) newCenter.z += newHalfWidth; else newCenter.z -= newHalfWidth; // binary 4 = 0100
	children[childIndex] = new Octant(newCenter, newHalfWidth, dotReserveSize);
	children[childIndex]->level = level + 1;
}

int Octant::FindOctant(const glm::vec3& position)
{
	int index = 0;
	if (position.x > center.x) index |= 1;
	if (position.y > center.y) index |= 2;
	if (position.z > center.z) index |= 4;
	return index;
}

void Octant::InsertDot(Dot* dot)
{
	if (dot->radius > halfWidth * 0.5f || level >= maxLevel)
	{
		octantDots.push_back(dot);
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

bool Octant::InLooseBoundry(const glm::vec3& position, float radius)
{
	if (position.x + radius < center.x - looseHalfWidth || position.x - radius > center.x + looseHalfWidth) return false;
	if (position.y + radius < center.y - looseHalfWidth || position.y - radius > center.y + looseHalfWidth) return false;
	if (position.z + radius < center.z - looseHalfWidth || position.z - radius > center.z + looseHalfWidth) return false;

	return true;
}

void Octant::QueryRange(Dot* aDot, std::vector<Dot*>& results)
{
	if (!InLooseBoundry(aDot->position, aDot->radius)) return;

	for (auto& dot : octantDots)
	{
		if (dot == aDot) continue;
		results.push_back(dot);
	}


	if (!isLeaf)
	{
		for (int i = 0; i < 8; i++)
		{
			if (children[i] == nullptr) continue;
			children[i]->QueryRange(aDot, results);
		}
	}
}

void Octant::RemoveChildren()
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

void Octant::DebugDraw()
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