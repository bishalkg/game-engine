#pragma once

#include "engine/ui_manager.h"

namespace game_engine {
class Engine;
}

namespace game {

struct GameResources;

struct IUIFlow {
  virtual ~IUIFlow() = default;

  virtual UIManager::UIActions update(
    game_engine::Engine& engine,
    GameResources& resources,
    float deltaTime,
    UIManager::UISnapshots& snaps) = 0;

  virtual void apply(
    game_engine::Engine& engine,
    GameResources& resources,
    ProgressionService& progService,
    const UIManager::UIActions& actions) = 0;
};

} // namespace game
