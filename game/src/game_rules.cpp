#include "game/game_rules.h"

#include "engine/engine.h"

namespace game {

// defaults when GameRules gamerules; (without passing in any components)
GameRules::GameRules(
  TTF_Font* font,
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
    renderSystem_(renderSystem ? std::move(renderSystem) : createDefaultRenderSystem()),
    font_(font) {}

bool GameRules::onInit(game_engine::Engine& engine) {
  resources_ = std::make_unique<GameResources>(engine.getSDLState(), font_, engine.getMixer());

  if (!bootstrap_->initialize(engine, *resources_, false)) {
    resources_.reset();
    return false;
  }

  return true;
}

void GameRules::onEvent(game_engine::Engine& engine, const SDL_Event& event) {
  inputSystem_->onEvent(engine, *resources_, event, input_, snaps_);
}

void GameRules::onUpdate(game_engine::Engine& engine, float deltaTime) {
  auto& sdlState = engine.getSDLState();

  auto& uiManager = resources_->m_uiManager;

  const UIManager::UIActions engineActions =
    uiFlow_->update(engine, *resources_, deltaTime, snaps_);

  const auto gameActions = uiController_.fromEngineActions(engineActions);
  // TODO is from and toEngineActions even required?

  lastEngineActions_ = uiController_.toEngineActions(gameActions);

  uiFlow_->apply(engine, *resources_, lastEngineActions_);

  if (!lastEngineActions_.blockMainGameDraw) {
    uiManager.clearRenderer(sdlState);

    if (!engine.handleMultiplayerConnections()) {
      engine.requestQuit();
      return;
    }

    simulationSystem_->update(engine, *resources_, deltaTime, lastEngineActions_);
  }

  snaps_.advanceToNextScene = false;
  snaps_.togglePauseGameplay = false;
}

void GameRules::onRender(game_engine::Engine& engine, float deltaTime) {
  renderSystem_->render(engine, *resources_, deltaTime, lastEngineActions_);
  resources_->m_uiManager.renderPresent(engine.getSDLState());
}

void GameRules::onShutdown(game_engine::Engine&) {
  resources_.reset();
}

} // namespace game
