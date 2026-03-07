#pragma once

#include "engine/igame_rules.h"
#include "engine/net/game_net_common.h"
#include "engine/ui_manager.h"
#include "game/ui_controller.h"

namespace game {

class GameRules : public eng::IGameRules {
public:
  bool onInit(game_engine::Engine& engine) override;
  void onEvent(game_engine::Engine& engine, const SDL_Event& event) override;
  void onUpdate(game_engine::Engine& engine, float deltaTime) override;
  void onRender(game_engine::Engine& engine, float deltaTime) override;
  void onShutdown(game_engine::Engine& engine) override;

private:
  game_engine::NetGameInput input_{};
  UIManager::UISnapshots snaps_{};
  UIManager::UIActions lastEngineActions_{};
  UIController uiController_{};
};

} // namespace game
