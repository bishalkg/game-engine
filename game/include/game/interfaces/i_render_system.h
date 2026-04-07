#pragma once

#include "engine/ui_manager.h"

namespace game_engine {
class Engine;
}

namespace game {

struct GameResources;

struct IRenderSystem {
  virtual ~IRenderSystem() = default;
  virtual void render(
    game_engine::Engine& engine,
    GameResources& resources,
    float deltaTime,
    const UIManager::UIActions& actions) = 0;
};

} // namespace game
