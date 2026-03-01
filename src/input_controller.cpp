#include "input_controller.hpp"

InputController::InputController()
  : keys(SDL_NUM_SCANCODES, false)
{
}

void InputController::handleEvents(const std::function<void()>& onQuit,
                                   const std::function<void(int, int)>& onWindowResized,
                                   const std::function<void(SDL_Scancode, SDL_Keycode)>& onKeyDown,
                                   const std::function<void(SDL_Scancode, SDL_Keycode)>& onKeyUp,
                                   const std::function<void()>& onEscapePressed)
{
  SDL_Event event;

  while (SDL_PollEvent(&event))
  {
    switch (event.type)
    {
      case SDL_QUIT:
        onQuit();
        break;
      case SDL_WINDOWEVENT:
        if (event.window.event == SDL_WINDOWEVENT_RESIZED)
        {
          onWindowResized(event.window.data1, event.window.data2);
        }
        break;
      case SDL_KEYDOWN:
      {
        const SDL_KeyboardEvent& e = event.key;
        if (e.repeat)
        {
          break;
        }

        if (e.keysym.sym == SDLK_ESCAPE)
        {
          onEscapePressed();
          break;
        }

        onKeyDown(e.keysym.scancode, e.keysym.sym);
        break;
      }
      case SDL_KEYUP:
      {
        const SDL_KeyboardEvent& e = event.key;
        onKeyUp(e.keysym.scancode, e.keysym.sym);
        break;
      }
      default:
        break;
    }
  }
}

bool InputController::isDown(SDL_Scancode scancode) const
{
  if (scancode < keys.size())
  {
    return keys[scancode];
  }
  return false;
}

void InputController::setDown(SDL_Scancode scancode, bool isPressed)
{
  if (scancode < keys.size())
  {
    keys[scancode] = isPressed;
  }
}

bool InputController::jumpQueued() const
{
  return jumpQueuedState;
}

void InputController::setJumpQueued(bool queued)
{
  jumpQueuedState = queued;
}
