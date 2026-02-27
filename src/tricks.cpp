#include "tricks.hpp"

namespace
{

constexpr float FLIP_CUE_THRESHOLD = 0.55f;
constexpr float FLIP_FLICK_THRESHOLD = 0.18f;
constexpr float FLIP_CUE_WINDOW = 0.28f;
constexpr float FLIP_TRICK_MAX_TIME = 0.85f;

} // namespace

void ComboEngine::ensureActive()
{
  if (active_)
  {
    return;
  }
  active_ = true;
  pendingScore_ = 0;
  multiplier_ = 1.0f;
  chainCount_ = 0;
}

void ComboEngine::reset()
{
  pendingScore_ = 0;
  multiplier_ = 1.0f;
  chainCount_ = 0;
  active_ = false;
}

void ComboEngine::onBail()
{
  reset();
}

void ComboEngine::bumpMultiplier(float amount)
{
  if (amount <= 0.0f)
  {
    return;
  }
  ensureActive();
  multiplier_ = std::clamp(multiplier_ + amount, 1.0f, 6.0f);
}

int ComboEngine::addPoints(int basePoints)
{
  if (basePoints <= 0)
  {
    return 0;
  }

  ensureActive();
  ++chainCount_;

  const int gained = static_cast<int>(std::round(static_cast<float>(basePoints) * multiplier_));
  pendingScore_ += gained;
  return gained;
}

int ComboEngine::bank()
{
  const int banked = pendingScore_;
  reset();
  return banked;
}

int ComboEngine::pendingScore() const
{
  return pendingScore_;
}

float ComboEngine::multiplier() const
{
  return multiplier_;
}

bool ComboEngine::active() const
{
  return active_;
}

void TrickFSM::reset(bool startGrounded)
{
  state_ = startGrounded ? TrickState::Grounded : TrickState::Ollie;
  activeFlip_ = FlipTrickType::None;
  flipCue_ = FlipCue::None;
  stateTimer_ = 0.0f;
  cueTimer_ = 0.0f;
  previousFlipPressed_ = false;
}

TrickState TrickFSM::state() const
{
  return state_;
}

FlipTrickType TrickFSM::activeFlip() const
{
  return activeFlip_;
}

float TrickFSM::stateTimer() const
{
  return stateTimer_;
}

void TrickFSM::transitionTo(TrickState nextState)
{
  if (state_ == nextState)
  {
    return;
  }
  state_ = nextState;
  stateTimer_ = 0.0f;
}

void TrickFSM::updateFlipCue(float dt, float turnAxis)
{
  cueTimer_ += dt;

  if (turnAxis <= -FLIP_CUE_THRESHOLD)
  {
    if (flipCue_ != FlipCue::Left)
    {
      flipCue_ = FlipCue::Left;
      cueTimer_ = 0.0f;
    }
  }
  else if (turnAxis >= FLIP_CUE_THRESHOLD)
  {
    if (flipCue_ != FlipCue::Right)
    {
      flipCue_ = FlipCue::Right;
      cueTimer_ = 0.0f;
    }
  }

  if (cueTimer_ > FLIP_CUE_WINDOW)
  {
    flipCue_ = FlipCue::None;
  }
}

bool TrickFSM::tryStartFlip(const InputState& input)
{
  const bool flipPressedEdge = input.flipPressed && !previousFlipPressed_;
  if (!flipPressedEdge)
  {
    return false;
  }

  if (cueTimer_ > FLIP_CUE_WINDOW)
  {
    return false;
  }

  if (flipCue_ == FlipCue::Right && input.turnAxis < -FLIP_FLICK_THRESHOLD)
  {
    activeFlip_ = FlipTrickType::Kickflip;
    return true;
  }

  if (flipCue_ == FlipCue::Left && input.turnAxis > FLIP_FLICK_THRESHOLD)
  {
    activeFlip_ = FlipTrickType::Heelflip;
    return true;
  }

  return false;
}

TrickFrameEvents TrickFSM::update(
  float dt,
  const InputState& input,
  bool grounded,
  bool grinding,
  bool bailRequested)
{
  TrickFrameEvents events;
  stateTimer_ += dt;

  if (bailRequested)
  {
    events.bailed = true;
    activeFlip_ = FlipTrickType::None;
    flipCue_ = FlipCue::None;
    transitionTo(TrickState::Bailing);
    previousFlipPressed_ = input.flipPressed;
    return events;
  }

  switch (state_)
  {
    case TrickState::Grounded:
      activeFlip_ = FlipTrickType::None;
      flipCue_ = FlipCue::None;
      cueTimer_ = 0.0f;
      if (grinding)
      {
        transitionTo(TrickState::Grinding);
      }
      else if (!grounded)
      {
        transitionTo(TrickState::Ollie);
      }
      break;

    case TrickState::Ollie:
      if (grinding)
      {
        transitionTo(TrickState::Grinding);
      }
      else if (grounded)
      {
        events.safeGroundedLanding = true;
        transitionTo(TrickState::Grounded);
      }
      else
      {
        updateFlipCue(dt, input.turnAxis);
        if (tryStartFlip(input))
        {
          transitionTo(TrickState::FlipTrick);
        }
      }
      break;

    case TrickState::FlipTrick:
      if (grinding)
      {
        transitionTo(TrickState::Grinding);
        activeFlip_ = FlipTrickType::None;
      }
      else if (grounded)
      {
        events.flipTrickLanded = true;
        events.landedFlip = activeFlip_;
        events.safeGroundedLanding = true;
        activeFlip_ = FlipTrickType::None;
        transitionTo(TrickState::Grounded);
      }
      else if (stateTimer_ > FLIP_TRICK_MAX_TIME)
      {
        activeFlip_ = FlipTrickType::None;
        transitionTo(TrickState::Ollie);
      }
      break;

    case TrickState::Grinding:
      if (!grinding)
      {
        if (grounded)
        {
          events.safeGroundedLanding = true;
          transitionTo(TrickState::Grounded);
        }
        else
        {
          transitionTo(TrickState::Ollie);
        }
      }
      break;

    case TrickState::Bailing:
      if (grounded)
      {
        transitionTo(TrickState::Grounded);
      }
      break;
  }

  previousFlipPressed_ = input.flipPressed;
  return events;
}
