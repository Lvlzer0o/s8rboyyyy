#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <random>

namespace
{

constexpr float PI = 3.14159265358979323846f;
constexpr float TERRAIN_NORMAL_EPSILON = 0.28f;
constexpr float TERRAIN_Z_FREQ = 0.05f;
constexpr float TERRAIN_X_FREQ = 0.07f;
constexpr float TERRAIN_XZ_FREQ = 0.028f;
constexpr float TERRAIN_XZ_MIX_FREQ = 0.02f;
constexpr float TERRAIN_XZ_AMP = 0.7f;
constexpr float TERRAIN_XZ_MIX_AMP = 0.45f;
constexpr float RAIL_HALF_WIDTH = 0.55f;
constexpr float RAIL_HALF_HEIGHT = 0.45f;
constexpr float RAIL_HALF_LENGTH = 2.6f;
constexpr float CRATE_HALF_SIZE = 1.0f;
std::mt19937 g_worldRng{std::random_device{}()};

float rand01()
{
  std::uniform_real_distribution<float> d(0.0f, 1.0f);
  return d(g_worldRng);
}

float randRange(float min, float max)
{
  std::uniform_real_distribution<float> d(min, max);
  return d(g_worldRng);
}

} // namespace

float terrainHeight(float x, float z)
{
  return std::sin(z * TERRAIN_Z_FREQ) * 1.2f +
    std::cos(x * TERRAIN_X_FREQ) * 1.1f +
    std::sin((x + z) * TERRAIN_XZ_FREQ) * TERRAIN_XZ_AMP +
    std::cos((x - z * 0.9f) * TERRAIN_XZ_MIX_FREQ) * TERRAIN_XZ_MIX_AMP;
}

Vec3 terrainNormal(float x, float z)
{
  const float hX1 = terrainHeight(x - TERRAIN_NORMAL_EPSILON, z);
  const float hX2 = terrainHeight(x + TERRAIN_NORMAL_EPSILON, z);
  const float hZ1 = terrainHeight(x, z - TERRAIN_NORMAL_EPSILON);
  const float hZ2 = terrainHeight(x, z + TERRAIN_NORMAL_EPSILON);
  const float dX = (hX2 - hX1) / (2.0f * TERRAIN_NORMAL_EPSILON);
  const float dZ = (hZ2 - hZ1) / (2.0f * TERRAIN_NORMAL_EPSILON);
  return safeNormalize({-dX, 1.0f, -dZ});
}

namespace
{

void placeObstacleAhead(WorldState& world, std::size_t index, const Vec3& playerPosition)
{
  auto& obstacle = world.obstacles[index];

  const float ahead = (static_cast<float>(index) + 1.0f) * 12.0f;
  const float targetZ = playerPosition.z + (SEGMENT_LENGTH * 0.5f) + randRange(0.0f, OBSTACLE_RECYCLE_OFFSET) + ahead;
  float x = (rand01() * 2.0f - 1.0f) * (BOARD_HALF_WIDTH * 0.78f);
  x = std::clamp(x, -BOARD_HALF_WIDTH + 4.0f, BOARD_HALF_WIDTH - 4.0f);

  obstacle.position = {x, terrainHeight(x, targetZ), targetZ};
  obstacle.active = true;
  obstacle.hitCooldown = 0.0f;
  obstacle.rail = rand01() < 0.2f;
  obstacle.radius = obstacle.rail ? 1.25f : 1.0f;
  obstacle.collisionHalfExtents = obstacle.rail ? Vec3{RAIL_HALF_WIDTH, RAIL_HALF_HEIGHT, RAIL_HALF_LENGTH} :
    Vec3{CRATE_HALF_SIZE, CRATE_HALF_SIZE, CRATE_HALF_SIZE};
}

void placeCoinAhead(WorldState& world, std::size_t index, const Vec3& playerPosition)
{
  auto& coin = world.coins[index];

  const float z = playerPosition.z + randRange(SEGMENT_LENGTH, COIN_RECYCLE_OFFSET) + 12.0f;
  const float x = (rand01() * 2.0f - 1.0f) * (BOARD_HALF_WIDTH * 0.65f);

  coin.position = {x, terrainHeight(x, z) + 1.2f, z};
  coin.active = true;
  coin.spin = 1.2f + randRange(0.0f, 1.2f);
  coin.phase = randRange(0.0f, static_cast<float>(2.0f * PI));
}

} // namespace

void initializeWorld(WorldState& world, const Vec3& playerPosition)
{
  for (size_t i = 0; i < world.obstacles.size(); ++i)
  {
    placeObstacleAhead(world, i, playerPosition);
  }

  for (size_t i = 0; i < world.coins.size(); ++i)
  {
    placeCoinAhead(world, i, playerPosition);
  }
}

void resetWorld(WorldState& world, const Vec3& playerPosition)
{
  initializeWorld(world, playerPosition);
}

void recycleWorld(WorldState& world, const Vec3& playerPosition, float dt)
{
  for (size_t i = 0; i < world.obstacles.size(); ++i)
  {
    auto& obstacle = world.obstacles[i];
    if (obstacle.position.z < playerPosition.z - 40.0f)
    {
      placeObstacleAhead(world, i, playerPosition);
    }

    if (obstacle.hitCooldown > 0.0f)
    {
      obstacle.hitCooldown = std::max(0.0f, obstacle.hitCooldown - dt);
    }
  }

  for (size_t i = 0; i < world.coins.size(); ++i)
  {
    auto& coin = world.coins[i];
    if (!coin.active || coin.position.z < playerPosition.z - 30.0f)
    {
      placeCoinAhead(world, i, playerPosition);
    }
  }
}

bool canGrindOnRail(const Obstacle& rail, const Vec3& playerPos, float playerSpeed)
{
  if (!rail.rail || !rail.active)
  {
    return false;
  }

  const Vec3 localDelta {
    std::fabs(playerPos.x - rail.position.x),
    std::fabs(playerPos.y - rail.position.y),
    std::fabs(playerPos.z - rail.position.z),
  };

  const Vec3 grindGate {
    rail.collisionHalfExtents.x + BOARD_RADIUS + 0.2f,
    std::max(GRIND_Y_TOLERANCE, rail.collisionHalfExtents.y + 0.25f),
    rail.collisionHalfExtents.z + GRIND_CATCH_RADIUS,
  };

  return localDelta.x <= grindGate.x &&
    localDelta.y <= grindGate.y &&
    localDelta.z <= grindGate.z &&
    playerSpeed >= GRIND_ENTRY_SPEED;
}
