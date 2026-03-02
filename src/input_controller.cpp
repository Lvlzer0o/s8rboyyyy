#include "input_controller.hpp"

InputController::InputController()
  : keys(SDL_NUM_SCANCODES, false)
{
}

bool InputController::isValidScancode(SDL_Scancode scancode) const
{
  return scancode >= 0 && static_cast<size_t>(scancode) < keys.size();
}

InputCommand InputController::translateSdlEvent(const SDL_Event& event) const
{
  InputCommand command;
  command.timestamp = event.common.timestamp;

  switch (event.type)
  {
    case SDL_QUIT:
      command.type = InputCommandType::Quit;
      break;
    case SDL_WINDOWEVENT:
      if (event.window.event == SDL_WINDOWEVENT_RESIZED)
      {
        command.type = InputCommandType::WindowResized;
        command.windowW = event.window.data1;
        command.windowH = event.window.data2;
      }
      break;
    case SDL_KEYDOWN:
    {
      const SDL_KeyboardEvent& e = event.key;
      command.repeated = e.repeat != 0;
      if (e.repeat)
      {
        command.repeated = true;
        return command; // None
      }

      if (e.keysym.sym == SDLK_ESCAPE)
      {
        command.type = InputCommandType::Escape;
        command.key = e.keysym.sym;
        command.scancode = e.keysym.scancode;
        command.pressed = true;
      }
      else
      {
        command.type = InputCommandType::KeyDown;
        command.scancode = e.keysym.scancode;
        command.key = e.keysym.sym;
        command.pressed = true;
      }
      break;
    }
    case SDL_KEYUP:
    {
      const SDL_KeyboardEvent& e = event.key;
      command.repeated = e.repeat != 0;
      command.type = InputCommandType::KeyUp;
      command.scancode = e.keysym.scancode;
      command.key = e.keysym.sym;
      command.pressed = false;
      break;
    }
    default:
      break;
  }
  return command;
}

void InputController::dispatchCommand(
  const InputCommand& command,
  const std::function<void()>& onQuit,
  const std::function<void(int, int)>& onWindowResized,
  const std::function<void(SDL_Scancode, SDL_Keycode)>& onKeyDown,
  const std::function<void(SDL_Scancode, SDL_Keycode)>& onKeyUp,
  const std::function<void()>& onEscapePressed) const
{
  switch (command.type)
  {
    case InputCommandType::Quit:
      onQuit();
      break;
    case InputCommandType::WindowResized:
      onWindowResized(command.windowW, command.windowH);
      break;
    case InputCommandType::Escape:
      onEscapePressed();
      break;
    case InputCommandType::KeyDown:
      onKeyDown(command.scancode, command.key);
      break;
    case InputCommandType::KeyUp:
      onKeyUp(command.scancode, command.key);
      break;
    case InputCommandType::Mouse:
    case InputCommandType::Other:
    case InputCommandType::None:
    default:
      break;
  }
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
    const InputCommand command = translateSdlEvent(event);
    if (command.type != InputCommandType::None)
    {
      dispatchCommand(command, onQuit, onWindowResized, onKeyDown, onKeyUp, onEscapePressed);
    }
  }
}

bool InputController::isDown(SDL_Scancode scancode) const
{
  if (isValidScancode(scancode))
  {
    return keys[scancode];
  }
  return false;
}

void InputController::setDown(SDL_Scancode scancode, bool isPressed)
{
  if (isValidScancode(scancode))
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
