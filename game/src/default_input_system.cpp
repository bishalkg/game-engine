#include "game/default_systems.h"

#include <algorithm>

#include "engine/engine.h"

namespace {

void handleKeyInput(
  game_engine::Engine& engine,
  game::GameResources& resources,
  GameObject& obj,
  SDL_Scancode key,
  bool keyDown,
  game_engine::NetGameInput& input) {
  (void)keyDown;

  if (obj.objClass != ObjectClass::Player) {
    return;
  }

  switch (obj.data.player.state) {
    case PlayerState::idle: {
      if (key == SDL_SCANCODE_UP && obj.grounded) {
        obj.data.player.state = PlayerState::jumping;
        input.move = game_engine::PlayerInput::Jump;
        input.shouldSendMessage = true;
        obj.data.player.jumpWindupTimer.reset();
        obj.data.player.jumpImpulseApplied = false;
      }
      break;
    }
    case PlayerState::jumping: {
      if (!obj.grounded && obj.currentAnimation == resources.ANIM_JUMP) {
        int n = obj.animations[resources.ANIM_JUMP].getFrameCount();
        int cap = std::max(0, n - 2);
        if (obj.spriteFrame >= cap) {
          obj.spriteFrame = cap;
          obj.data.player.playLandingFrame = true;
        }
      }

      if (obj.grounded) {
        if (obj.data.player.playLandingFrame) {
          int n = obj.animations[resources.ANIM_JUMP].getFrameCount();
          obj.currentAnimation = resources.ANIM_JUMP;
          obj.spriteFrame = n - 1;
          obj.data.player.playLandingFrame = false;
        } else {
          obj.velocity.y = 0;
          obj.data.player.state = PlayerState::idle;
          obj.animations[resources.ANIM_JUMP].reset();
        }
      }
      break;
    }
    case PlayerState::running: {
      if (key == SDL_SCANCODE_UP && obj.grounded) {
        obj.data.player.state = PlayerState::jumping;
        obj.data.player.jumpWindupTimer.reset();
        obj.data.player.jumpImpulseApplied = false;

        input.move = game_engine::PlayerInput::Jump;
        input.shouldSendMessage = true;
      }
      break;
    }
    default:
      break;
  }
}

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
        if (event.key.scancode == SDL_SCANCODE_S && !event.key.repeat) {
          player.data.player.meleePressedThisFrame = true;
        }
        handleKeyInput(engine, resources, player, event.key.scancode, true, input);
        break;
      case SDL_EVENT_KEY_UP:
        handleKeyInput(engine, resources, player, event.key.scancode, false, input);
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
