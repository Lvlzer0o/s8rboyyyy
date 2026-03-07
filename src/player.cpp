#include "player.hpp"
#include "world.hpp"

#include <algorithm>
#include <cmath>

namespace
{

int flipBasePoints(FlipTrickType trick)
{
  switch (trick)
  {
    case FlipTrickType::Kickflip:
      return 320;
    case FlipTrickType::Heelflip:
      return 360;
    case FlipTrickType::None:
    default:
      return 0;
  }
}

void handleMovement(
  float dt,
  const InputState& input)
{
  const float accelInput = (input.forwardPressed ? 1.0f : 0.0f) - (input.backwardPressed ? 0.75f : 0.0f);
  const Vec3 forward(std::sin(g_player.yaw), 0.0f, std::cos(g_player.yaw));

  if (g_player.grounded)
  {
    if (accelInput > 0.0f)
    {
      g_player.velocity.x += forward.x * PLAYER_ACCEL * accelInput * dt;
      g_player.velocity.z += forward.z * PLAYER_ACCEL * accelInput * dt;
    }
    else if (accelInput < 0.0f)
    {
      g_player.velocity.x *= BRAKE;
      g_player.velocity.z *= BRAKE;
    }

    g_player.velocity.x *= DRAG_GROUND;
    g_player.velocity.z *= DRAG_GROUND;
  }
  else
  {
    g_player.velocity.x += forward.x * PLAYER_ACCEL * accelInput * dt * 0.58f;
    g_player.velocity.z += forward.z * PLAYER_ACCEL * accelInput * dt * 0.58f;
    g_player.velocity.x *= DRAG_AIR;
    g_player.velocity.z *= DRAG_AIR;
  }

  const float groundSpeed = length2D(g_player.velocity);
  if (groundSpeed > MAX_SPEED)
  {
    const float scale = MAX_SPEED / groundSpeed;
    g_player.velocity.x *= scale;
    g_player.velocity.z *= scale;
  }

  const float turnSign = std::clamp(input.turnAxis, -1.0f, 1.0f);
  if (std::abs(turnSign) > 0.01f)
  {
    float turn = 0.0f;
    if (groundSpeed > 0.8f)
    {
      turn = (turnSign * TURN_RATE * dt * (0.35f + std::min(0.8f, groundSpeed / 14.0f))) *
        (g_player.velocity.z >= 0.0f ? 1.0f : -1.0f);
    }
    else if (g_player.grounded)
    {
      turn = turnSign * TURN_RATE * dt * 0.45f;
    }
    else
    {
      turn = turnSign * TURN_RATE * dt * 0.2f;
    }
    g_player.yaw += turn;
  }

  if (input.jumpPressed && g_player.grounded)
  {
    g_player.velocity.y = JUMP_SPEED;
    g_player.grounded = false;
    g_player.airTime = 0.0f;
    g_player.ollieAnim = OLLIE_POP_TIME;
  }
}

} // namespace

Player g_player;
GroundState g_playerGround;

void refreshPlayerGroundState()
{
  g_playerGround.height = terrainHeight(g_player.position.x, g_player.position.z);
  g_playerGround.normal = terrainNormal(g_player.position.x, g_player.position.z);
}

bool updateManualBalance(float dt, float turnInput)
{
  const float speed = length2D(g_player.velocity);
  const float speedFactor = std::clamp(speed / MAX_SPEED, 0.0f, 1.0f);
  const float airScale = g_player.grounded ? 1.0f : BALANCE_AIR_DAMPEN;
  const float inputForce = BALANCE_INPUT_FORCE * turnInput * (0.3f + 0.7f * speedFactor) * airScale;
  const float slopeLean = -g_playerGround.normal.x;
  const float terrainForce = slopeLean * BALANCE_TERRAIN_INFLUENCE * (airScale * 0.55f);
  const float gravityMagnitude = BALANCE_GRAVITY * (std::abs(g_player.manualBalance) + BALANCE_GRAVITY_OFFSET) *
    std::abs(g_player.manualBalance);
  const float gravityForce = g_player.manualBalance >= 0.0f ? gravityMagnitude : -gravityMagnitude;
  const float dampingForce = -BALANCE_DAMPING * g_player.balanceVelocity;
  const float balanceAcceleration = gravityForce + terrainForce + inputForce + dampingForce;

  g_player.balanceVelocity += balanceAcceleration * dt;
  g_player.balanceVelocity = std::clamp(g_player.balanceVelocity, -BALANCE_MAX_VELOCITY, BALANCE_MAX_VELOCITY);
  g_player.manualBalance += g_player.balanceVelocity * dt;

  if (std::abs(g_player.manualBalance) >= 1.0f)
  {
    return true;
  }

  if (std::abs(g_player.manualBalance) < 0.16f)
  {
    g_player.balanceVelocity *= std::clamp(1.0f - dt * 2.8f, 0.0f, 1.0f);
    g_player.manualBalance *= std::clamp(1.0f - dt * 0.95f, 0.0f, 1.0f);
  }

  g_player.manualBalance = std::clamp(g_player.manualBalance, -1.0f, 1.0f);
  return false;
}

bool updatePlayer(
  float dt,
  const InputState& input)
{
  const float turnSign = std::clamp(input.turnAxis, -1.0f, 1.0f);

  handleMovement(dt, input);

  if (updateManualBalance(dt, turnSign))
  {
    return true;
  }

  if (g_player.ollieAnim > 0.0f)
  {
    g_player.ollieAnim = std::max(0.0f, g_player.ollieAnim - dt);
  }

  if (g_player.position.x > BOARD_HALF_WIDTH - 2.0f)
  {
    g_player.position.x = BOARD_HALF_WIDTH - 2.0f;
    g_player.velocity.x *= -0.3f;
  }

  if (g_player.position.x < -(BOARD_HALF_WIDTH - 2.0f))
  {
    g_player.position.x = -(BOARD_HALF_WIDTH - 2.0f);
    g_player.velocity.x *= -0.3f;
  }

  g_player.velocity.y -= GRAVITY * dt;
  g_player.position += g_player.velocity * dt;

  const float groundY = g_playerGround.height + BOARD_RADIUS;
  const bool onGroundNow = g_player.position.y <= groundY;

  if (onGroundNow && g_player.velocity.y <= 0.0f)
  {
    g_player.position.y = groundY;

    if (!g_player.grounded)
    {
      const float landingSpeed = std::max(0.0f, -g_player.velocity.y);
      const float impactFlex = std::min(0.0f, -landingSpeed * BOARD_LANDING_FLEX_SCALE);
      g_player.deckFlex = std::min(g_player.deckFlex, std::max(-BOARD_FLEX_LIMIT, impactFlex));
      g_player.airTime = 0.0f;
    }

    g_player.grounded = true;
    g_player.velocity.y = 0.0f;

    if (input.forwardPressed)
    {
      g_player.velocity.x *= 1.005f;
      g_player.velocity.z *= 1.005f;
    }
  }
  else
  {
    g_player.deckFlex = g_player.deckFlex + (BOARD_AIR_FLEX - g_player.deckFlex) * std::clamp(22.0f * dt, 0.0f, 1.0f);
    g_player.airTime += dt;
    g_player.grounded = false;
  }

  if (g_player.grounded)
  {
    g_player.deckFlex = g_player.deckFlex + (0.0f - g_player.deckFlex) * std::clamp(BOARD_FLEX_RECOVERY * dt, 0.0f, 1.0f);
  }

  return false;
}

void updateTrickState(float dt, const InputState& input)
{
  const TrickFrameEvents events = g_player.trickFsm.update(
    dt,
    input,
    g_player.grounded,
    g_player.grinding,
    g_player.bailedThisFrame);

  g_player.trickState = g_player.trickFsm.state();

  if (events.bailed)
  {
    g_player.comboEngine.onBail();
  }

  if (events.flipTrickLanded)
  {
    g_player.comboEngine.bumpMultiplier(0.45f);
    g_player.comboEngine.addPoints(flipBasePoints(events.landedFlip));
  }

  if (events.safeGroundedLanding)
  {
    g_player.score += g_player.comboEngine.bank();
  }

  g_player.bailedThisFrame = false;
}
