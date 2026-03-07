#pragma once

#include <SDL2/SDL.h>
#include <string>

#include "input_controller.hpp"
#include "player.hpp"
#include "tricks.hpp"
#include "world.hpp"

namespace Gameplay
{

enum class GameState
{
  Menu,
  Play,
  Pause,
  Over,
  Win,
};

constexpr float WORLD_TIME = 180.0f;

void setMessage(std::string& message, float& messageTimer, const std::string& text, float seconds = 1.5f);
void showOverlay(std::string& overlayTitle, std::string& overlayHint, const std::string& title, const std::string& hint);
void makeWorld(WorldState& world, const Vec3& playerPosition);

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
  std::string& overlayHint);

void loseLife(
  Player& player,
  GameState& gameState,
  std::string& overlayTitle,
  std::string& overlayHint,
  std::string& message,
  float& messageTimer,
  float& flash);

void winRun(
  Player& player,
  GameState& gameState,
  std::string& overlayTitle,
  std::string& overlayHint);

bool forwardPressed(const InputController& inputController);
bool backwardPressed(const InputController& inputController);
bool leftPressed(const InputController& inputController);
bool rightPressed(const InputController& inputController);
InputState buildInputState(InputController& inputController, bool consumeQueuedJump = false);

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
  std::string& overlayHint);

void updateTimers(float dt, Player& player, std::string& message, float& messageTimer, float& flash);

}  // namespace Gameplay
