#include "engine/engine.h"

void game_engine::Engine::drawObject(GameObject &obj, float height, float width, float deltaTime) {

  float frameW = obj.spritePixelW;
  float frameH = obj.spritePixelH;

  // select frame from sprite sheet
  float srcX = (obj.currentAnimation != -1)
                 ? obj.animations[obj.currentAnimation].currentFrame() * frameW
                 : (obj.spriteFrame - 1) * frameW;

  SDL_FRect src{srcX, 0, frameW, frameH};

  // scale sprites up or down
  float drawW = frameW / obj.drawScale;
  float drawH = frameH / obj.drawScale;

  SDL_FRect dst{
    obj.position.x - m_gameState.mapViewport.x,
    obj.position.y - m_gameState.mapViewport.y,
    drawW, drawH
  };

  SDL_FlipMode flipMode = obj.direction == -1 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

  // set flash animations on enemies
  if (obj.shouldFlash) {
    SDL_SetTextureColorModFloat(obj.texture, 2.5f, 1.0f, 1.0f);
  }

  SDL_RenderTextureRotated(m_sdlState.renderer, obj.texture, &src, &dst, 0, nullptr, flipMode);

  if (obj.shouldFlash) {
    SDL_SetTextureColorModFloat(obj.texture, 1.0f, 1.0f, 1.0f);
    if (obj.flashTimer.step(deltaTime)) {
      obj.shouldFlash = false;
    }
  }

  if (m_gameState.debugMode) {

    SDL_FRect spriteBox{
      .x = obj.position.x - m_gameState.mapViewport.x,
      .y = obj.position.y - m_gameState.mapViewport.y,
      .w = obj.collider.w,
      .h = obj.collider.h
    };
    SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_sdlState.renderer, 200, 100, 0, 100);
    SDL_RenderFillRect(m_sdlState.renderer, &spriteBox);
    SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_NONE);


    // display each objects collision hitbox
    SDL_FRect rectA{
      .x = obj.position.x + obj.collider.x - m_gameState.mapViewport.x,
      .y = obj.position.y + obj.collider.y - m_gameState.mapViewport.y,
      .w = obj.collider.w,
      .h = obj.collider.h
    };
    SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_sdlState.renderer, 255, 0, 0, 100);
    SDL_RenderFillRect(m_sdlState.renderer, &rectA);
    SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_NONE);

    SDL_FRect sensor{
      .x = obj.position.x + obj.collider.x - m_gameState.mapViewport.x,
      .y = obj.position.y + obj.collider.y + obj.collider.h - m_gameState.mapViewport.y,
      .w = obj.collider.w, .h = 1
    };
    SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_sdlState.renderer, 0, 0, 255, 255);
    SDL_RenderFillRect(m_sdlState.renderer, &sensor);
    SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_NONE);
  }
}


void game_engine::Engine::drawAllObjects(float deltaTime, UIManager::UIActions& actions) {
  // draw all interactable objects
  for (auto &layer : m_gameState.layers) {
    for (GameObject &obj : layer) {

      if (obj.objClass == ObjectClass::Background) {
        // draw background images;
        // TODO needs to be sorted by background layers; comes from a map of uncertain order
        drawParalaxBackground(obj.texture, getPlayer().velocity.x, obj.bgscroll, obj.scrollFactor, deltaTime, -80.0f);

      } else if (obj.objClass == ObjectClass::Level) {
        // if level tile, let src and dst override so that
        // src points to a specfic 32x32 tile texture from the whole png; dst is where on our window we want to place it
        SDL_FRect dst = obj.data.level.dst;
        dst.x -= m_gameState.mapViewport.x;  // if you scroll horizontally
        dst.y -= m_gameState.mapViewport.y;  // if vertical scrolling
        // dst.w = static_cast<float>(obj.texture->w);
        // dst.h = static_cast<float>(obj.texture->h);
        SDL_RenderTexture(m_sdlState.renderer, obj.texture, &obj.data.level.src, &dst);

        if (m_gameState.debugMode) {
          // display each objects collision hitbox
          SDL_FRect rectA{
            .x = obj.position.x + obj.collider.x - m_gameState.mapViewport.x,
            .y = obj.position.y + obj.collider.y - m_gameState.mapViewport.y,
            .w = obj.collider.w,
            .h = obj.collider.h
          };
          SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_BLEND);
          SDL_SetRenderDrawColor(m_sdlState.renderer, 255, 0, 0, 100);
          SDL_RenderFillRect(m_sdlState.renderer, &rectA);
          SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_NONE);

          SDL_FRect sensor{
            .x = obj.position.x + obj.collider.x - m_gameState.mapViewport.x,
            .y = obj.position.y + obj.collider.y + obj.collider.h - m_gameState.mapViewport.y,
            .w = obj.collider.w, .h = 1
          };
          SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_BLEND);
          SDL_SetRenderDrawColor(m_sdlState.renderer, 0, 0, 255, 255);
          SDL_RenderFillRect(m_sdlState.renderer, &sensor);
          SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_NONE);
        }
      } else {
        drawObject(obj, obj.spritePixelH, obj.spritePixelW, deltaTime);
      }
    }
  }

  // draw bullets
  for (GameObject &bullet: m_gameState.bullets) {
    if (bullet.data.bullet.state != BulletState::inactive) {
      drawObject(bullet, bullet.collider.h, bullet.collider.w, deltaTime);
    }
  }

  if (actions.drawSceneOverlay) {
    m_resources.m_uiManager.draw(m_sdlState, deltaTime, actions.dimBackground, actions.drawText, 0);
  }

}

void game_engine::Engine::drawParalaxBackground(SDL_Texture* tex,
                                   float camVelX,
                                   float& scrollPos,
                                   float scrollFactor,
                                   float dt,
                                   float baseY = -175.0f) { // horoz/vert offsets should be from tilesheet

    scrollPos -= camVelX * scrollFactor * dt;
    auto scrollY = 0 * scrollFactor * dt;   // factorY ≈ 0 for sky

    float w = static_cast<float>(tex->w);
    scrollPos = std::fmod(scrollPos, w);
    if (scrollPos > 0) scrollPos -= w;

    SDL_FRect dst1{ scrollPos,        baseY + scrollY, w, (float)tex->h };
    SDL_FRect dst2{ scrollPos + w,    baseY + scrollY, w, (float)tex->h };
    // SDL_FRect dst1{ scrollPos,        baseY*1.5f + scrollY, w*1.5f, (float)tex->h*1.5f };
    // SDL_FRect dst2{ scrollPos + w,    baseY*1.5f + scrollY, w*1.5f, (float)tex->h*1.5f };
    SDL_RenderTexture(m_sdlState.renderer, tex, nullptr, &dst1);
    SDL_RenderTexture(m_sdlState.renderer, tex, nullptr, &dst2);
    // // advance
    // scrollPos -= cameraVelX * scrollFactor * dt;

    // // keep in [-w, 0)
    // float w = static_cast<float>(tex->w); // or query via SDL_GetTextureSize
    // scrollPos = std::fmod(scrollPos, w);
    // if (scrollPos > 0) scrollPos -= w;

    // SDL_FRect dst1{ scrollPos,       y, w, static_cast<float>(tex->h) };
    // SDL_FRect dst2{ scrollPos + w,   y, w, static_cast<float>(tex->h) };

    // SDL_RenderTexture(m_sdlState.renderer, tex, nullptr, &dst1);
    // SDL_RenderTexture(m_sdlState.renderer, tex, nullptr, &dst2);
}
