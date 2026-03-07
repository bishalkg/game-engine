#pragma once

#include <SDL3/SDL.h>

namespace game_engine {
class Engine;
}

namespace eng {

class IGameRules {
public:
  virtual ~IGameRules() = default;

  virtual bool onInit(game_engine::Engine&) { return true; }
  virtual void onEvent(game_engine::Engine&, const SDL_Event&) {}
  virtual void onUpdate(game_engine::Engine&, float) {}
  virtual void onRender(game_engine::Engine&, float) {}
  virtual void onShutdown(game_engine::Engine&) {}
};

} // namespace eng
