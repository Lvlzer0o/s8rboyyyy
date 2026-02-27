#pragma once

#include <algorithm>
#include <cmath>

enum class TrickState
{
  Grounded,
  Ollie,
  FlipTrick,
  Grinding,
  Bailing,
};

enum class FlipTrickType
{
  None,
  Kickflip,
  Heelflip,
};

struct InputState
{
  bool forwardPressed = false;
  bool backwardPressed = false;
  bool jumpPressed = false;
  bool flipPressed = false;
  float turnAxis = 0.0f;
};

struct TrickFrameEvents
{
  bool bailed = false;
  bool safeGroundedLanding = false;
  bool flipTrickLanded = false;
  FlipTrickType landedFlip = FlipTrickType::None;
};

class ComboEngine
{
public:
  void reset();
  void onBail();
  void bumpMultiplier(float amount);
  int addPoints(int basePoints);
  int bank();
  int pendingScore() const;
  float multiplier() const;
  bool active() const;

private:
  void ensureActive();

  int pendingScore_ = 0;
  float multiplier_ = 1.0f;
  int chainCount_ = 0;
  bool active_ = false;
};

class TrickFSM
{
public:
  void reset(bool startGrounded);
  TrickFrameEvents update(
    float dt,
    const InputState& input,
    bool grounded,
    bool grinding,
    bool bailRequested);
  TrickState state() const;
  FlipTrickType activeFlip() const;
  float stateTimer() const;

private:
  enum class FlipCue
  {
    None,
    Left,
    Right,
  };

  void transitionTo(TrickState nextState);
  void updateFlipCue(float dt, float turnAxis);
  bool tryStartFlip(const InputState& input);

  TrickState state_ = TrickState::Grounded;
  FlipTrickType activeFlip_ = FlipTrickType::None;
  FlipCue flipCue_ = FlipCue::None;
  float stateTimer_ = 0.0f;
  float cueTimer_ = 0.0f;
  bool previousFlipPressed_ = false;
};
