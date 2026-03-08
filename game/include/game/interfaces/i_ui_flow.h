#pragma once

#include "engine/ui_manager.h"

namespace game_engine {
class Engine;
}

namespace game {

struct IUIFlow {
  virtual ~IUIFlow() = default;

  virtual UIManager::UIActions update(
    game_engine::Engine& engine,
    float deltaTime,
    UIManager::UISnapshots& snaps) = 0;

  virtual void apply(
    game_engine::Engine& engine,
    const UIManager::UIActions& actions) = 0;
};

} // namespace game
