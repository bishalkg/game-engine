#include "game/game_rules.h"

#include "engine/engine.h"
#include "game/game_resources.h"

namespace game {

bool GameRules::onInit(game_engine::Engine& engine) {
  if (!initializeGameResources(engine)) {
    return false;
  }

  return engine.initAllTiles(engine.getGameState());
}

void GameRules::onEvent(game_engine::Engine& engine, const SDL_Event& event) {
  auto& player = engine.getPlayer();
  auto& gameState = engine.getGameState();
  auto& sdlState = engine.getSDLState();

  switch (event.type) {
    case SDL_EVENT_WINDOW_RESIZED:
      engine.setWindowSize(event.window.data2, event.window.data1);
      break;
    case SDL_EVENT_KEY_DOWN:
      engine.handleKeyInput(player, event.key.scancode, true, input_);
      break;
    case SDL_EVENT_KEY_UP:
      engine.handleKeyInput(player, event.key.scancode, false, input_);
      if (event.key.scancode == SDL_SCANCODE_Q) {
        gameState.debugMode = !gameState.debugMode;
      } else if (event.key.scancode == SDL_SCANCODE_F11) {
        sdlState.fullscreen = !sdlState.fullscreen;
        SDL_SetWindowFullscreen(sdlState.window, sdlState.fullscreen);
      } else if (event.key.scancode == SDL_SCANCODE_TAB) {
        gameState.currentView = UIManager::GameView::InventoryMenu;
      } else if (gameState.currentView == UIManager::GameView::CutScene &&
                 event.key.scancode == SDL_SCANCODE_RETURN) {
        snaps_.advanceToNextScene = true;
      } else if ((gameState.currentView == UIManager::GameView::Playing ||
                  gameState.currentView == UIManager::GameView::PauseMenu) &&
                 event.key.scancode == SDL_SCANCODE_P) {
        snaps_.togglePauseGameplay = true;
      }
      break;
    default:
      break;
  }
}

void GameRules::onUpdate(game_engine::Engine& engine, float deltaTime) {
  auto& resources = engine.getResources();
  auto& sdlState = engine.getSDLState();
  auto& uiManager = resources.m_uiManager;
  auto& player = engine.getPlayer();

  const UIManager::UIActions engineActions = engine.updateUI(uiManager, deltaTime, snaps_);
  const auto gameActions = uiController_.fromEngineActions(engineActions);
  lastEngineActions_ = uiController_.toEngineActions(gameActions);
  engine.applyUIActions(lastEngineActions_);

  if (!lastEngineActions_.blockMainGameDraw) {
    uiManager.clearRenderer(sdlState);

    if (!engine.handleMultiplayerConnections()) {
      UIManager::UIActions quitActions{};
      quitActions.quitGame = true;
      engine.applyUIActions(quitActions);
      return;
    }

    engine.updateGameplayState(deltaTime, player, lastEngineActions_);
  }

  snaps_.advanceToNextScene = false;
  snaps_.togglePauseGameplay = false;
}

void GameRules::onRender(game_engine::Engine& engine, float) {
  engine.getResources().m_uiManager.renderPresent(engine.getSDLState());
}

void GameRules::onShutdown(game_engine::Engine&) {}

} // namespace game
