#pragma once

#include <SDL2/SDL.h>

#include <cstdint>
#include <functional>
#include <vector>

enum class InputCommandType
{
  None,
  Quit,
  WindowResized,
  KeyDown,
  KeyUp,
  Escape,
  Mouse,
  Other,
};

struct InputCommand
{
  InputCommandType type = InputCommandType::None;
  SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
  SDL_Keycode key = SDLK_UNKNOWN;
  bool pressed = false;
  bool repeated = false;
  int windowW = 0;
  int windowH = 0;
  Uint32 timestamp = 0;
};

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
  void setDown(SDL_Scancode scancode, bool isPressed);

  bool jumpQueued() const;
  void setJumpQueued(bool queued);

private:
  bool isValidScancode(SDL_Scancode scancode) const;
  InputCommand translateSdlEvent(const SDL_Event& event) const;
  void dispatchCommand(const InputCommand& command,
                      const std::function<void()>& onQuit,
                      const std::function<void(int, int)>& onWindowResized,
                      const std::function<void(SDL_Scancode, SDL_Keycode)>& onKeyDown,
                      const std::function<void(SDL_Scancode, SDL_Keycode)>& onKeyUp,
                      const std::function<void()>& onEscapePressed) const;

  std::vector<bool> keys;
  bool jumpQueuedState = false;
};
