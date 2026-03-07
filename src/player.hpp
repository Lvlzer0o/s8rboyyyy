#pragma once

#include <algorithm>
#include <cmath>

#include "tricks.hpp"

struct Vec3
{
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;

  Vec3() = default;
  Vec3(float px, float py, float pz) : x(px), y(py), z(pz) {}

  Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3& operator+=(const Vec3& o)
  {
    x += o.x;
    y += o.y;
    z += o.z;
    return *this;
  }
};

inline float length2D(const Vec3& v)
{
  return std::sqrt(v.x * v.x + v.z * v.z);
}

inline float length3D(const Vec3& v)
{
  return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

inline Vec3 cross(const Vec3& a, const Vec3& b)
{
  return {
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x,
  };
}

inline Vec3 safeNormalize(const Vec3& v)
{
  const float len = length3D(v);
  if (len <= 0.00001f)
  {
    return {0.0f, 1.0f, 0.0f};
  }
  return {v.x / len, v.y / len, v.z / len};
}

struct GroundState
{
  float height = 0.0f;
  Vec3 normal {0.0f, 1.0f, 0.0f};
};

struct Player
{
  Vec3 position {0.0f, 4.0f, 0.0f};
  Vec3 velocity {0.0f, 0.0f, 6.0f};
  float yaw = 0.0f;
  bool grounded = false;
  float airTime = 0.0f;
  bool grinding = false;
  int grindCombo = 0;
  float grindTime = 0.0f;
  float grindHintTimer = 0.0f;
  float grindScoreCarry = 0.0f;
  int currentGrindIndex = -1;
  float deckFlex = 0.0f;
  float ollieAnim = 0.0f;
  float manualBalance = 0.0f;
  float balanceVelocity = 0.0f;
  TrickFSM trickFsm;
  ComboEngine comboEngine;
  TrickState trickState = TrickState::Grounded;
  bool bailedThisFrame = false;

  int lives = 3;
  int score = 0;
  float distance = 0.0f;
  float invuln = 0.0f;
};

constexpr float GRAVITY = 33.0f;
constexpr float PLAYER_ACCEL = 34.0f;
constexpr float BRAKE = 0.88f;
constexpr float DRAG_GROUND = 0.985f;
constexpr float DRAG_AIR = 0.996f;
constexpr float MAX_SPEED = 30.0f;
constexpr float TURN_RATE = 1.9f;
constexpr float JUMP_SPEED = 16.0f;
constexpr float OLLIE_POP_TIME = 0.35f;
constexpr float OLLIE_POP_LIFT = 0.20f;
constexpr float OLLIE_BOARD_PITCH = 18.0f;
constexpr float OLLIE_RIDER_RISE = 0.08f;
constexpr float OLLIE_RIDER_TUCK = 18.0f;
constexpr float BALANCE_INPUT_FORCE = 2.4f;
constexpr float BALANCE_TERRAIN_INFLUENCE = 0.42f;
constexpr float BALANCE_AIR_DAMPEN = 0.6f;
constexpr float BALANCE_WARNING = 0.75f;
constexpr float BALANCE_DAMPING = 2.6f;
constexpr float BALANCE_GRAVITY = 3.2f;
constexpr float BALANCE_GRAVITY_OFFSET = 0.08f;
constexpr float BALANCE_MAX_VELOCITY = 4.8f;
constexpr float BOARD_FLEX_LIMIT = 0.056f;
constexpr float BOARD_FLEX_RECOVERY = 7.0f;
constexpr float BOARD_AIR_FLEX = -0.028f;
constexpr float BOARD_LANDING_FLEX_SCALE = 0.0068f;
constexpr float BOARD_RADIUS = 0.85f;
constexpr float BOARD_HALF_WIDTH = 44.0f;

extern Player g_player;
extern GroundState g_playerGround;

bool updatePlayer(
  float dt,
  const InputState& input);
bool updateManualBalance(float dt, float turnInput);
void updateTrickState(float dt, const InputState& input);
void refreshPlayerGroundState();
