#include "game/game_rules.h"

#include "engine/engine.h"

namespace game {

GameRules::GameRules(
  std::unique_ptr<IBootstrap> bootstrap,
  std::unique_ptr<IInputSystem> inputSystem,
  std::unique_ptr<IUIFlow> uiFlow,
  std::unique_ptr<ISimulationSystem> simulationSystem,
  std::unique_ptr<IRenderSystem> renderSystem)
  : bootstrap_(bootstrap ? std::move(bootstrap) : createDefaultBootstrap()),
    inputSystem_(inputSystem ? std::move(inputSystem) : createDefaultInputSystem()),
    uiFlow_(uiFlow ? std::move(uiFlow) : createDefaultUIFlow()),
    simulationSystem_(
      simulationSystem ? std::move(simulationSystem) : createDefaultSimulationSystem()),
    renderSystem_(renderSystem ? std::move(renderSystem) : createDefaultRenderSystem()) {}

bool GameRules::onInit(game_engine::Engine& engine) {
  if (!bootstrap_->initialize(engine, false)) {
    return false;
  }
  return true;
}

void GameRules::onEvent(game_engine::Engine& engine, const SDL_Event& event) {
  inputSystem_->onEvent(engine, event, input_, snaps_);
}

void GameRules::onUpdate(game_engine::Engine& engine, float deltaTime) {
  auto& resources = engine.getResources();
  auto& sdlState = engine.getSDLState();
  auto& uiManager = resources.m_uiManager;

  const UIManager::UIActions engineActions = uiFlow_->update(engine, deltaTime, snaps_);
  const auto gameActions = uiController_.fromEngineActions(engineActions);
  lastEngineActions_ = uiController_.toEngineActions(gameActions);
  uiFlow_->apply(engine, lastEngineActions_);

  if (!lastEngineActions_.blockMainGameDraw) {
    uiManager.clearRenderer(sdlState);

    if (!engine.handleMultiplayerConnections()) {
      engine.requestQuit();
      return;
    }

    simulationSystem_->update(engine, deltaTime, lastEngineActions_);
  }

  snaps_.advanceToNextScene = false;
  snaps_.togglePauseGameplay = false;
}

void GameRules::onRender(game_engine::Engine& engine, float deltaTime) {
  renderSystem_->render(engine, deltaTime, lastEngineActions_);
  engine.getResources().m_uiManager.renderPresent(engine.getSDLState());
}

void GameRules::onShutdown(game_engine::Engine&) {}

} // namespace game
