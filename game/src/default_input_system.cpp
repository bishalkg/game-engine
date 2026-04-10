#include "game/default_systems.h"

#include <algorithm>

#include "engine/engine.h"

namespace {

class DefaultInputSystem final : public game::IInputSystem {
public:
  void onEvent(
    game_engine::Engine& engine,
    game::GameResources& resources,
    const SDL_Event& event,
    game_engine::NetGameInput& input,
    UIManager::UISnapshots& snaps) override {
    auto& player = engine.getPlayer();
    auto& gameState = engine.getGameState();
    auto& sdlState = engine.getSDLState();

    switch (event.type) {
      case SDL_EVENT_QUIT:
        engine.requestQuit();
        break;
      case SDL_EVENT_WINDOW_RESIZED:
        engine.setWindowSize(event.window.data2, event.window.data1);
        break;
      case SDL_EVENT_KEY_DOWN:
        if (!event.key.repeat && event.key.scancode == SDL_SCANCODE_UP) {
          input.jumpPressed = true;
          input.shouldSendMessage = true;
        } else if (!event.key.repeat && event.key.scancode == SDL_SCANCODE_S) {
          input.meleePressed = true;
          input.shouldSendMessage = true;
        } else if (!event.key.repeat && event.key.scancode == SDL_SCANCODE_Z) {
          input.ultimatePressed = true;
          input.shouldSendMessage = true;
        }
        break;
      case SDL_EVENT_KEY_UP:
        if (event.key.scancode == SDL_SCANCODE_Q) {
          gameState.debugMode = !gameState.debugMode;
        } else if (event.key.scancode == SDL_SCANCODE_F11) {
          sdlState.fullscreen = !sdlState.fullscreen;
          SDL_SetWindowFullscreen(sdlState.window, sdlState.fullscreen);
        } else if (event.key.scancode == SDL_SCANCODE_TAB) {
          gameState.currentView = UIManager::GameView::InventoryMenu;
        } else if (gameState.currentView == UIManager::GameView::CutScene &&
                   event.key.scancode == SDL_SCANCODE_RETURN) {
          snaps.advanceToNextScene = true;
        } else if ((gameState.currentView == UIManager::GameView::Playing ||
                    gameState.currentView == UIManager::GameView::PauseMenu) &&
                   event.key.scancode == SDL_SCANCODE_P) {
          snaps.togglePauseGameplay = true;
        }
        break;
      default:
        break;
    }
  }
};

} // namespace

namespace game {

std::unique_ptr<IInputSystem> createDefaultInputSystem() {
  return std::make_unique<DefaultInputSystem>();
}

} // namespace game
