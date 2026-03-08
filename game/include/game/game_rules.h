#pragma once

#include "engine/igame_rules.h"
#include "engine/net/game_net_common.h"
#include "engine/ui_manager.h"
#include "game/default_systems.h"
#include "game/ui_controller.h"

namespace game {

class GameRules : public eng::IGameRules {
public:
  GameRules(
    std::unique_ptr<IBootstrap> bootstrap = nullptr,
    std::unique_ptr<IInputSystem> inputSystem = nullptr,
    std::unique_ptr<IUIFlow> uiFlow = nullptr,
    std::unique_ptr<ISimulationSystem> simulationSystem = nullptr,
    std::unique_ptr<IRenderSystem> renderSystem = nullptr);

  bool onInit(game_engine::Engine& engine) override;
  void onEvent(game_engine::Engine& engine, const SDL_Event& event) override;
  void onUpdate(game_engine::Engine& engine, float deltaTime) override;
  void onRender(game_engine::Engine& engine, float deltaTime) override;
  void onShutdown(game_engine::Engine& engine) override;

private:
  std::unique_ptr<IBootstrap> bootstrap_;
  std::unique_ptr<IInputSystem> inputSystem_;
  std::unique_ptr<IUIFlow> uiFlow_;
  std::unique_ptr<ISimulationSystem> simulationSystem_;
  std::unique_ptr<IRenderSystem> renderSystem_;

  game_engine::NetGameInput input_{};
  UIManager::UISnapshots snaps_{};
  UIManager::UIActions lastEngineActions_{};
  UIController uiController_{};
};

} // namespace game
