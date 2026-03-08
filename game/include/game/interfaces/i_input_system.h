#pragma once

#include <SDL3/SDL.h>

#include "engine/net/game_net_common.h"
#include "engine/ui_manager.h"

namespace game_engine {
class Engine;
}

namespace game {

struct IInputSystem {
  virtual ~IInputSystem() = default;
  virtual void onEvent(
    game_engine::Engine& engine,
    const SDL_Event& event,
    game_engine::NetGameInput& input,
    UIManager::UISnapshots& snaps) = 0;
};

} // namespace game
