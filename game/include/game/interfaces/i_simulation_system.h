#pragma once

#include "engine/ui_manager.h"

namespace game_engine {
class Engine;
}

namespace game {

struct ISimulationSystem {
  virtual ~ISimulationSystem() = default;
  virtual void update(
    game_engine::Engine& engine,
    float deltaTime,
    const UIManager::UIActions& actions) = 0;
};

} // namespace game
