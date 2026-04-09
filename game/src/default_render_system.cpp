#include "game/default_systems.h"

#include <cmath>

#include "engine/engine.h"

namespace {

class DefaultRenderSystem final : public game::IRenderSystem {
public:
  void render(
    game_engine::Engine& engine,
    game::GameResources& resources,
    float deltaTime,
    const UIManager::UIActions& actions) override {
    auto& gameState = engine.getGameState();
    auto& renderer = engine.getSDLState().renderer;

    if (gameState.currentView == UIManager::GameView::LevelLoading) {
      return;
    }

    if (gameState.currentView == UIManager::GameView::Playing ||
        gameState.currentView == UIManager::GameView::PauseMenu) {
      drawAllObjects(engine, resources, deltaTime, actions);

      auto& player = engine.getPlayer();
      if (gameState.debugMode) {
        char debugText[256];
        SDL_snprintf(
          debugText,
          sizeof(debugText),
          "State: %d  Direction: %.2f B: %zu, G: %d, Px: %.2f, Py: %.2f, VPx: %.2f",
          static_cast<int>(player.data.player.state),
          player.direction,
          gameState.bullets.size(),
          player.grounded ? 1 : 0,
          player.position.x,
          player.position.y,
          gameState.mapViewport.x);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(renderer, 5, 5, debugText);
      }
    }
  }

private:
  void drawObject(game_engine::Engine& engine, GameObject& obj, float deltaTime) {
    auto& gameState = engine.getGameState();
    auto& renderer = engine.getSDLState().renderer;

    bool shouldInterpolate = false;
    if (engine.isMultiplayerActive() && obj.dynamic) {
      const auto* client = engine.getGameClient();
      const bool isLocalPlayer =
        client && obj.objClass == ObjectClass::Player && obj.id == client->GetPlayerID();
      shouldInterpolate = !isLocalPlayer;
    }

    if (engine.isMultiplayerActive() && obj.dynamic &&
        obj.currentAnimation >= 0 &&
        obj.currentAnimation < static_cast<int>(obj.animations.size())) {
      obj.animations[obj.currentAnimation].step(deltaTime);
    }

    if (!obj.renderPositionInitialized) {
      obj.renderPosition = obj.position;
      obj.renderPositionInitialized = true;
    } else if (shouldInterpolate) {
      const float blend = std::clamp(deltaTime * 15.0f, 0.0f, 1.0f);
      obj.renderPosition += (obj.position - obj.renderPosition) * blend;
    } else {
      obj.renderPosition = obj.position;
    }

    float frameW = obj.spritePixelW;
    float frameH = obj.spritePixelH;
    float srcX = (obj.currentAnimation != -1)
                   ? obj.animations[obj.currentAnimation].currentFrame() * frameW
                   : (obj.spriteFrame - 1) * frameW;

    SDL_FRect src{srcX, 0, frameW, frameH};

    float drawW = frameW / obj.drawScale;
    float drawH = frameH / obj.drawScale;

    SDL_FRect dst{
      obj.renderPosition.x - gameState.mapViewport.x,
      obj.renderPosition.y - gameState.mapViewport.y,
      drawW,
      drawH,
    };

    SDL_FlipMode flipMode = obj.direction == -1 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    if (obj.shouldFlash) {
      SDL_SetTextureColorModFloat(obj.texture, 2.5f, 1.0f, 1.0f);
    }

    SDL_RenderTextureRotated(renderer, obj.texture, &src, &dst, 0, nullptr, flipMode);

    if (obj.shouldFlash) {
      SDL_SetTextureColorModFloat(obj.texture, 1.0f, 1.0f, 1.0f);
    }

    if (!gameState.debugMode) {
      return;
    }

    SDL_FRect spriteBox{
      .x = obj.position.x - gameState.mapViewport.x,
      .y = obj.position.y - gameState.mapViewport.y,
      .w = obj.collider.w,
      .h = obj.collider.h,
    };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 200, 100, 0, 100);
    SDL_RenderFillRect(renderer, &spriteBox);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    SDL_FRect rectA{
      .x = obj.position.x + obj.collider.x - gameState.mapViewport.x,
      .y = obj.position.y + obj.collider.y - gameState.mapViewport.y,
      .w = obj.collider.w,
      .h = obj.collider.h,
    };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 100);
    SDL_RenderFillRect(renderer, &rectA);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    SDL_FRect sensor{
      .x = obj.position.x + obj.collider.x - gameState.mapViewport.x,
      .y = obj.position.y + obj.collider.y + obj.collider.h - gameState.mapViewport.y,
      .w = obj.collider.w,
      .h = 1,
    };
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
    SDL_RenderFillRect(renderer, &sensor);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  }

  void drawParallaxBackground(
    game_engine::Engine& engine,
    SDL_Texture* tex,
    float camVelX,
    float& scrollPos,
    float scrollFactor,
    float dt,
    float baseY) {
    auto& renderer = engine.getSDLState().renderer;

    scrollPos -= camVelX * scrollFactor * dt;
    float scrollY = 0.0f * scrollFactor * dt;

    float w = static_cast<float>(tex->w);
    scrollPos = std::fmod(scrollPos, w);
    if (scrollPos > 0) {
      scrollPos -= w;
    }

    SDL_FRect dst1{scrollPos, baseY + scrollY, w, static_cast<float>(tex->h)};
    SDL_FRect dst2{scrollPos + w, baseY + scrollY, w, static_cast<float>(tex->h)};
    SDL_RenderTexture(renderer, tex, nullptr, &dst1);
    SDL_RenderTexture(renderer, tex, nullptr, &dst2);
  }

  void drawAllObjects(
    game_engine::Engine& engine,
    game::GameResources& resources,
    float deltaTime,
    const UIManager::UIActions& actions) {
    auto& gameState = engine.getGameState();
    auto& sdlState = engine.getSDLState();

    for (auto& layer : gameState.layers) {
      for (GameObject& obj : layer) {
        if (obj.objClass == ObjectClass::Background) {
          drawParallaxBackground(
            engine,
            obj.texture,
            engine.getPlayer().velocity.x,
            obj.bgscroll,
            obj.scrollFactor,
            deltaTime,
            -80.0f);
        } else if (obj.objClass == ObjectClass::Level) {
          SDL_FRect dst = obj.data.level.dst;
          dst.x -= gameState.mapViewport.x;
          dst.y -= gameState.mapViewport.y;
          SDL_RenderTexture(sdlState.renderer, obj.texture, &obj.data.level.src, &dst);

          if (gameState.debugMode) {
            SDL_FRect rectA{
              .x = obj.position.x + obj.collider.x - gameState.mapViewport.x,
              .y = obj.position.y + obj.collider.y - gameState.mapViewport.y,
              .w = obj.collider.w,
              .h = obj.collider.h,
            };
            SDL_SetRenderDrawBlendMode(sdlState.renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(sdlState.renderer, 255, 0, 0, 100);
            SDL_RenderFillRect(sdlState.renderer, &rectA);
            SDL_SetRenderDrawBlendMode(sdlState.renderer, SDL_BLENDMODE_NONE);

            SDL_FRect sensor{
              .x = obj.position.x + obj.collider.x - gameState.mapViewport.x,
              .y = obj.position.y + obj.collider.y + obj.collider.h - gameState.mapViewport.y,
              .w = obj.collider.w,
              .h = 1,
            };
            SDL_SetRenderDrawBlendMode(sdlState.renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(sdlState.renderer, 0, 0, 255, 255);
            SDL_RenderFillRect(sdlState.renderer, &sensor);
            SDL_SetRenderDrawBlendMode(sdlState.renderer, SDL_BLENDMODE_NONE);
          }
        } else {
          drawObject(engine, obj, deltaTime);
        }
      }
    }

    for (GameObject& bullet : gameState.bullets) {
      if (bullet.data.bullet.state != BulletState::inactive) {
        drawObject(engine, bullet, deltaTime);
      }
    }

    if (actions.drawSceneOverlay) {
      resources.m_uiManager.draw(
        sdlState,
        deltaTime,
        actions.dimBackground,
        actions.drawText,
        0);
    }
  }
};

} // namespace

namespace game {

std::unique_ptr<IRenderSystem> createDefaultRenderSystem() {
  return std::make_unique<DefaultRenderSystem>();
}

} // namespace game
