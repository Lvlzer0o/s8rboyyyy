#pragma once

#include "player.hpp"

#include <array>

constexpr int OBSTACLE_COUNT = 34;
constexpr int COIN_COUNT = 22;

constexpr float GRIND_ENTRY_SPEED = 6.5f;
constexpr float GRIND_CATCH_RADIUS = 1.42f;
constexpr float GRIND_Y_TOLERANCE = 0.72f;

constexpr float SEGMENT_LENGTH = 180.0f;
constexpr float OBSTACLE_RECYCLE_OFFSET = 260.0f;
constexpr float COIN_RECYCLE_OFFSET = 180.0f;

struct Obstacle
{
  Vec3 position;
  float radius = 1.0f;
  bool rail = false;
  bool active = true;
  float hitCooldown = 0.0f;
};

struct Coin
{
  Vec3 position;
  bool active = true;
  float spin = 1.0f;
  float phase = 0.0f;
  float radius = 0.7f;
};

struct WorldState
{
  std::array<Obstacle, OBSTACLE_COUNT> obstacles;
  std::array<Coin, COIN_COUNT> coins;
};

float terrainHeight(float x, float z);
Vec3 terrainNormal(float x, float z);

void initializeWorld(WorldState& world, const Vec3& playerPosition);
void resetWorld(WorldState& world, const Vec3& playerPosition);
void recycleWorld(WorldState& world, const Vec3& playerPosition, float dt);
bool canGrindOnRail(const Obstacle& rail, const Vec3& playerPos, float playerSpeed);
