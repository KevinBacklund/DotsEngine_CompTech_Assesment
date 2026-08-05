#include "DotsScene.h"
#include "DebugLines.h"
#include <Time/Time.h>

#include <random>
#include <glm/gtc/random.hpp>

#include <iostream>

#include "tracy/Tracy.hpp"

Camera* DotsScene::mainCamera;

DotsScene::DotsScene()
{
	mainCamera = new Camera();
	flyingCamera = new FlyingCamera(mainCamera);
	dotVisual = new DotVisual();

	mainCamera->SetPosition(glm::vec3(0, 0, bounds * 4));
}

void DotsScene::DotsScene_Render()
{
	for (auto& dot : dots)
	{
		dot.Dot_Render(dotVisual);
	}

	DotsScene_DrawBounds();
}

void DotsScene::DotsScene_Update()
{
	ZoneScoped;
	flyingCamera->Update();

	//Handle velocity
	for (auto& dot : dots)
	{
		dot.position += glm::normalize(dot.velocity) * (dotSpeed / dot.radius) * Time::deltaTime;
	}

	//Handle bounds
	for (auto& dot : dots)
	{
		if (dot.position.x > bounds - dot.radius || dot.position.x < -bounds + dot.radius) dot.velocity.x *= -1;
		if (dot.position.y > bounds - dot.radius || dot.position.y < -bounds + dot.radius) dot.velocity.y *= -1;
		if (dot.position.z > bounds - dot.radius || dot.position.z < -bounds + dot.radius) dot.velocity.z *= -1;

		dot.position.x = glm::clamp(dot.position.x, -bounds + dot.radius, bounds - dot.radius);
		dot.position.y = glm::clamp(dot.position.y, -bounds + dot.radius, bounds - dot.radius);
		dot.position.z = glm::clamp(dot.position.z, -bounds + dot.radius, bounds - dot.radius);
	}

	//Handle collision
	for (size_t d1 = 0; d1 < dots.size(); d1++)
	{
		for (size_t d2 = d1; d2 < dots.size(); d2++)
		{
			if (d1 == d2) continue;
			if (glm::distance(dots[d1].position, dots[d2].position) < dots[d1].radius + dots[d2].radius)
			{
				glm::vec3 n1 = glm::normalize(dots[d1].velocity);
				glm::vec3 n2 = glm::normalize(dots[d2].velocity);

				dots[d2].velocity = glm::reflect(dots[d2].velocity, n1);
				dots[d1].velocity = glm::reflect(dots[d1].velocity, n2);

				dots[d2].Health--;
				dots[d2].ReCreate(dotVisual);
				dots[d1].Health--;
				dots[d1].ReCreate(dotVisual);
			}
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
	std::vector<size_t> toRemove;

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
