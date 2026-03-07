#include "gameplay_state.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace Gameplay
{

namespace
{

constexpr float GRIND_ENTRY_BONUS = 150.0f;
constexpr float GRIND_EXIT_BONUS = 85.0f;
constexpr float GRIND_SCORE_RATE = 140.0f;
constexpr float GRIND_SPEED_MULT = 1.012f;

Vec3 g_previousPlayerPos;
bool g_previousPlayerPosReady = false;

void syncPreviousPlayerPosition(const Vec3& playerPosition)
{
  g_previousPlayerPos = playerPosition;
  g_previousPlayerPosReady = true;
}

bool intersectsAabb(
  const Vec3& centerA,
  const Vec3& halfExtentsA,
  const Vec3& centerB,
  const Vec3& halfExtentsB)
{
  return std::fabs(centerA.x - centerB.x) <= halfExtentsA.x + halfExtentsB.x &&
    std::fabs(centerA.y - centerB.y) <= halfExtentsA.y + halfExtentsB.y &&
    std::fabs(centerA.z - centerB.z) <= halfExtentsA.z + halfExtentsB.z;
}

bool sweptAabbHit(
  const Vec3& start,
  const Vec3& end,
  const Vec3& playerHalfExtents,
  const Vec3& obstacleCenter,
  const Vec3& obstacleHalfExtents)
{
  const Vec3 expandedHalf {
    obstacleHalfExtents.x + playerHalfExtents.x,
    obstacleHalfExtents.y + playerHalfExtents.y,
    obstacleHalfExtents.z + playerHalfExtents.z,
  };

  const Vec3 delta {end.x - start.x, end.y - start.y, end.z - start.z};

  float entry = 0.0f;
  float exit = 1.0f;

  auto sweepAxis = [&](float startAxis, float deltaAxis, float minAxis, float maxAxis)
  {
    if (std::fabs(deltaAxis) < 0.00001f)
    {
      return startAxis >= minAxis && startAxis <= maxAxis;
    }

    const float inv = 1.0f / deltaAxis;
    float t0 = (minAxis - startAxis) * inv;
    float t1 = (maxAxis - startAxis) * inv;

    if (t0 > t1)
    {
      std::swap(t0, t1);
    }

    entry = std::max(entry, t0);
    exit = std::min(exit, t1);
    return entry <= exit;
  };

  const float minX = obstacleCenter.x - expandedHalf.x;
  const float maxX = obstacleCenter.x + expandedHalf.x;
  const float minY = obstacleCenter.y - expandedHalf.y;
  const float maxY = obstacleCenter.y + expandedHalf.y;
  const float minZ = obstacleCenter.z - expandedHalf.z;
  const float maxZ = obstacleCenter.z + expandedHalf.z;

  if (!sweepAxis(start.x, delta.x, minX, maxX))
  {
    return false;
  }
  if (!sweepAxis(start.y, delta.y, minY, maxY))
  {
    return false;
  }
  if (!sweepAxis(start.z, delta.z, minZ, maxZ))
  {
    return false;
  }

  return exit >= 0.0f && entry <= 1.0f;
}

}  // namespace

void setMessage(std::string& message, float& messageTimer, const std::string& text, float seconds)
{
  message = text;
  messageTimer = std::max(messageTimer, seconds);
}

void showOverlay(std::string& overlayTitle, std::string& overlayHint, const std::string& title, const std::string& hint)
{
  overlayTitle = title;
  overlayHint = hint;
}

void makeWorld(WorldState& world, const Vec3& playerPosition)
{
  initializeWorld(world, playerPosition);
}

void resetRun(
  Player& player,
  WorldState& world,
  GameState& gameState,
  bool& paused,
  float& timeLeft,
  float& menuYaw,
  float& flash,
  std::string& message,
  float& messageTimer,
  std::string& overlayTitle,
  std::string& overlayHint)
{
  player.position = {0.0f, 4.0f, 0.0f};
  player.velocity = {0.0f, 0.0f, 6.0f};
  player.yaw = 0.0f;
  player.grounded = false;
  player.grinding = false;
  player.grindCombo = 0;
  player.grindTime = 0.0f;
  player.grindHintTimer = 0.0f;
  player.currentGrindIndex = -1;
  player.deckFlex = 0.0f;
  player.airTime = 0.0f;
  player.ollieAnim = 0.0f;
  player.manualBalance = 0.0f;
  player.balanceVelocity = 0.0f;
  player.comboEngine.reset();
  player.trickFsm.reset(true);
  player.trickState = TrickState::Grounded;
  player.bailedThisFrame = false;
  player.invuln = 0.0f;
  player.lives = 3;
  player.score = 0;
  player.distance = 0.0f;
  timeLeft = WORLD_TIME;
  flash = 0.0f;
  gameState = GameState::Play;
  paused = false;
  showOverlay(overlayTitle, overlayHint, "", "");
  menuYaw = 0.0f;

  resetWorld(world, player.position);
  syncPreviousPlayerPosition(player.position);

  message = "Ride the line";
  messageTimer = 0.75f;
}

void loseLife(
  Player& player,
  GameState& gameState,
  std::string& overlayTitle,
  std::string& overlayHint,
  std::string& message,
  float& messageTimer,
  float& flash)
{
  player.bailedThisFrame = true;
  player.comboEngine.onBail();
  player.grinding = false;
  player.grindCombo = 0;
  player.grindTime = 0.0f;
  player.currentGrindIndex = -1;
  player.deckFlex = 0.0f;
  player.ollieAnim = 0.0f;
  player.manualBalance = 0.0f;
  player.balanceVelocity = 0.0f;
  --player.lives;
  player.invuln = 1.2f;
  flash = 0.18f;
  player.velocity.x *= 0.33f;
  player.velocity.z *= 0.33f;
  player.velocity.y *= 0.25f;
  setMessage(message, messageTimer, "Clean Hit! -1 Life", 1.1f);

  if (player.lives <= 0)
  {
    gameState = GameState::Over;
    showOverlay(
      overlayTitle,
      overlayHint,
      "GAME OVER",
      "Score " + std::to_string(player.score) + " - Press Enter or R to Replay");
  }
}

void winRun(
  Player& player,
  GameState& gameState,
  std::string& overlayTitle,
  std::string& overlayHint)
{
  gameState = GameState::Win;
  showOverlay(
    overlayTitle,
    overlayHint,
    "TIME FINISHED - GREAT RIDE!",
    "Score " + std::to_string(player.score) + " - Press Enter or R to Replay");
}

bool forwardPressed(const InputController& inputController)
{
  return inputController.isDown(SDL_SCANCODE_W) || inputController.isDown(SDL_SCANCODE_UP);
}

bool backwardPressed(const InputController& inputController)
{
  return inputController.isDown(SDL_SCANCODE_S) || inputController.isDown(SDL_SCANCODE_DOWN);
}

bool leftPressed(const InputController& inputController)
{
  return inputController.isDown(SDL_SCANCODE_A) || inputController.isDown(SDL_SCANCODE_LEFT);
}

bool rightPressed(const InputController& inputController)
{
  return inputController.isDown(SDL_SCANCODE_D) || inputController.isDown(SDL_SCANCODE_RIGHT);
}

InputState buildInputState(InputController& inputController, bool consumeQueuedJump)
{
  InputState input;
  input.forwardPressed = forwardPressed(inputController);
  input.backwardPressed = backwardPressed(inputController);
  input.turnAxis = (rightPressed(inputController) ? 1.0f : 0.0f) - (leftPressed(inputController) ? 1.0f : 0.0f);
  input.jumpPressed = inputController.jumpQueued();
  if (consumeQueuedJump && input.jumpPressed)
  {
    inputController.setJumpQueued(false);
  }
  input.flipPressed = inputController.isDown(SDL_SCANCODE_LSHIFT) || inputController.isDown(SDL_SCANCODE_RSHIFT);
  return input;
}

void updateObstaclesAndCoins(
  float dt,
  float currentTime,
  WorldState& world,
  Player& player,
  GameState& gameState,
  float& flash,
  std::string& message,
  float& messageTimer,
  std::string& overlayTitle,
  std::string& overlayHint)
{
  if (!g_previousPlayerPosReady)
  {
    syncPreviousPlayerPosition(player.position);
  }

  const Vec3 previousPlayerPos = g_previousPlayerPos;
  const Vec3 playerPos = player.position;
  g_previousPlayerPos = playerPos;
  const Vec3 playerHalfExtents {BOARD_RADIUS, BOARD_RADIUS, BOARD_RADIUS};
  const float playerSpeed = length2D(player.velocity);
  int bestRailIndex = -1;
  float bestRailScore = 1.0e9f;

  for (size_t i = 0; i < world.obstacles.size(); ++i)
  {
    auto& obstacle = world.obstacles[i];
    if (!obstacle.active)
    {
      continue;
    }

    const float testY = terrainHeight(obstacle.position.x, obstacle.position.z) + obstacle.radius;
    obstacle.position.y = std::fma(0.2f, (testY - obstacle.position.y), obstacle.position.y);

    const Vec3 d {playerPos.x - obstacle.position.x, playerPos.y - obstacle.position.y, playerPos.z - obstacle.position.z};
    const bool collidesNow = intersectsAabb(playerPos, playerHalfExtents, obstacle.position, obstacle.collisionHalfExtents);
    const bool sweptCollision = sweptAabbHit(
      previousPlayerPos,
      playerPos,
      playerHalfExtents,
      obstacle.position,
      obstacle.collisionHalfExtents);

    if (!obstacle.rail && (collidesNow || sweptCollision) && player.invuln <= 0.0f && obstacle.hitCooldown <= 0.0f)
    {
      obstacle.hitCooldown = 1.0f;
      loseLife(player, gameState, overlayTitle, overlayHint, message, messageTimer, flash);
      break;
    }

    if (obstacle.rail && (collidesNow || sweptCollision) && player.invuln <= 0.0f && obstacle.hitCooldown <= 0.0f &&
      !canGrindOnRail(obstacle, playerPos, playerSpeed))
    {
      obstacle.hitCooldown = 1.0f;
      loseLife(player, gameState, overlayTitle, overlayHint, message, messageTimer, flash);
      break;
    }

    if (obstacle.rail && canGrindOnRail(obstacle, playerPos, playerSpeed))
    {
      const float score = d.x * d.x + d.z * d.z;
      if (score < bestRailScore)
      {
        bestRailScore = score;
        bestRailIndex = static_cast<int>(i);
      }
    }
  }

  const bool canGrind = player.invuln <= 0.0f && bestRailIndex >= 0 && player.grounded;
  if (canGrind)
  {
    const bool continuingRail = player.grinding && player.currentGrindIndex == bestRailIndex;

    if (!continuingRail)
    {
      const int combo = std::max(1, player.grindCombo + 1);
      player.grindCombo = combo;
      const int entryScore = static_cast<int>(GRIND_ENTRY_BONUS) + combo * 20;
      player.comboEngine.bumpMultiplier(0.22f);
      const int entryAward = player.comboEngine.addPoints(entryScore);
      player.grindHintTimer = 0.35f;
      setMessage(message, messageTimer, "GRIND +" + std::to_string(entryAward) + "x" + std::to_string(combo), 0.9f);
      player.grindTime = 0.0f;
      player.grindScoreCarry = 0.0f;
    }

    player.grinding = true;
    player.currentGrindIndex = bestRailIndex;
    player.grindTime += dt;
    player.grindHintTimer = std::max(0.0f, player.grindHintTimer - dt);
    player.grindScoreCarry += GRIND_SCORE_RATE * dt;

    const int grindScore = static_cast<int>(std::floor(player.grindScoreCarry));
    if (grindScore > 0)
    {
      player.comboEngine.addPoints(grindScore);
      player.grindScoreCarry -= static_cast<float>(grindScore);
    }

    player.velocity.z *= GRIND_SPEED_MULT;
    player.velocity.x *= 0.88f;
    if (player.velocity.y > 0.0f)
    {
      player.velocity.y = 0.0f;
    }
    player.airTime = 0.0f;
    player.grounded = true;
    if (player.grindHintTimer <= 0.0f)
    {
      setMessage(message, messageTimer, "GRINDING", 0.35f);
      player.grindHintTimer = 0.42f;
    }
  }
  else
  {
    if (player.grinding)
    {
      const int exitScore = static_cast<int>(GRIND_EXIT_BONUS) + player.grindCombo * 30;
      player.comboEngine.bumpMultiplier(0.12f);
      const int exitAward = player.comboEngine.addPoints(exitScore);
      setMessage(message, messageTimer, "GRIND END +" + std::to_string(exitAward), 0.55f);
    }

    player.grinding = false;
    player.currentGrindIndex = -1;
    player.grindTime = 0.0f;
    player.grindHintTimer = 0.0f;
    player.grindScoreCarry = 0.0f;
    player.grindCombo = 0;

    if (player.invuln <= 0.0f && player.velocity.y <= 0.0f)
    {
      for (const auto& obstacle : world.obstacles)
      {
        if (!obstacle.active || !obstacle.rail)
        {
          continue;
        }

        const bool collidesNow = intersectsAabb(playerPos, playerHalfExtents, obstacle.position, obstacle.collisionHalfExtents);
        const bool sweptCollision = sweptAabbHit(
          previousPlayerPos,
          playerPos,
          playerHalfExtents,
          obstacle.position,
          obstacle.collisionHalfExtents);
        if ((collidesNow || sweptCollision) && obstacle.hitCooldown <= 0.0f)
        {
          loseLife(player, gameState, overlayTitle, overlayHint, message, messageTimer, flash);
          break;
        }
      }
    }
  }

  for (auto& coin : world.coins)
  {
    if (!coin.active)
    {
      continue;
    }

    coin.spin += dt * 1.1f;
    coin.position.y = terrainHeight(coin.position.x, coin.position.z) + 1.15f + std::sin(currentTime * 2.0f + coin.phase) * 0.12f;

    const Vec3 d {playerPos.x - coin.position.x, playerPos.y - coin.position.y, playerPos.z - coin.position.z};
    const float dist = length3D(d);

    if (dist < coin.radius + BOARD_RADIUS)
    {
      player.score += 175;
      coin.active = false;
      setMessage(message, messageTimer, "+175 Neon Coin", 1.0f);
    }
  }
}

void updateTimers(float dt, Player& player, std::string& message, float& messageTimer, float& flash)
{
  if (player.invuln > 0.0f)
  {
    player.invuln = std::max(0.0f, player.invuln - dt);
  }

  if (messageTimer > 0.0f)
  {
    messageTimer -= dt;
    if (messageTimer <= 0.0f)
    {
      message.clear();
    }
  }

  if (flash > 0.0f)
  {
    flash = std::max(0.0f, flash - dt);
  }
}

}  // namespace Gameplay
