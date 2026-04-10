#pragma once

#include <memory>

#include <SDL3_ttf/SDL_ttf.h>

#include "engine/igame_rules.h"
#include "engine/net/game_net_common.h"
#include "game/default_systems.h"
#include "game/game_resources.h"
#include "game/ui_controller.h"

namespace game {

// IGameRules is defined in the engine and requires us to implement its methods (onInit, onEvent, onUpdate, onRender, onShutdown) which are called at appropriate places in the game engines game loop
class GameRules : public eng::IGameRules {
public:
  // With explicit, only direct construction is allowed:

  // GameRules rules(font);      // OK
  // GameRules rules{font};      // OK

  // GameRules rules = font;     // not allowed
  // f(font);                    // not allowed
  explicit GameRules(
    TTF_Font* font,
    std::unique_ptr<IBootstrap> bootstrap = nullptr,
    std::unique_ptr<IInputSystem> inputSystem = nullptr,
    std::unique_ptr<IUIFlow> uiFlow = nullptr,
    std::unique_ptr<ISimulationSystem> simulationSystem = nullptr,
    std::unique_ptr<IRenderSystem> renderSystem = nullptr);

  // “there is no valid GameRules constructor with exactly these 5 arguments.” = delete is a C++ way to explicitly ban a function or overload.
  GameRules(
    std::unique_ptr<IBootstrap>,
    std::unique_ptr<IInputSystem>,
    std::unique_ptr<IUIFlow>,
    std::unique_ptr<ISimulationSystem>,
    std::unique_ptr<IRenderSystem>) = delete;

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
  std::unique_ptr<GameResources> resources_;
  TTF_Font* font_;

  game_engine::NetGameInput input_{};
  UIManager::UISnapshots snaps_{};
  UIManager::UIActions lastEngineActions_{};
  UIController uiController_{};
};

} // namespace game
