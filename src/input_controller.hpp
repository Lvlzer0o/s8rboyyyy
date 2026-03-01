#pragma once

#include <SDL2/SDL.h>

#include <functional>
#include <vector>

class InputController
{
public:
  InputController();

  void handleEvents(const std::function<void()>& onQuit,
                    const std::function<void(int, int)>& onWindowResized,
                    const std::function<void(SDL_Scancode, SDL_Keycode)>& onKeyDown,
                    const std::function<void(SDL_Scancode, SDL_Keycode)>& onKeyUp,
                    const std::function<void()>& onEscapePressed);

  bool isDown(SDL_Scancode scancode) const;
  void setDown(SDL_Scancode scancode, bool isDown);

  bool jumpQueued() const;
  void setJumpQueued(bool queued);

private:
  std::vector<bool> keys;
  bool jumpQueuedState = false;
};
