#include <iostream>
#include <vector>
#include <string>
#include <format>
#include <array>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <glm/glm.hpp>

// #include "animation.h"
#include "gameobject.h"
#include "gameengine.h"


using namespace std;

int main(int argc, char *argv[]) {


  GameEngine game;
  if (!game.init(1600, 900, 640, 320)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game Init Failed", "Failed to init Game", nullptr);
        return 0;
  }

  // start the game loop
  uint64_t prevTime = SDL_GetTicks();
  bool running = true;

  GameState &gs = game.getGameState();
  SDLState &sdl = game.getSDLState();
  Resources &res = game.getResources();
  while (running){
    GameObject &player = game.getPlayer();  // fetch each frame in case index changes

    uint64_t nowTime = SDL_GetTicks();
    float deltaTime = (nowTime - prevTime) / 1000.0f; // convert to seconds; time bw frames

    // event loop
    SDL_Event event{0};
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_EVENT_QUIT:
        {
          running = false;
          break;
        }
        case SDL_EVENT_WINDOW_RESIZED:
        {
          game.state.width = event.window.data1;
          game.state.height = event.window.data2;
          break;
        }
        case SDL_EVENT_KEY_DOWN: // non-continuous presses
        {
          game.handleKeyInput(player, event.key.scancode, true);
          break;
        }
        case SDL_EVENT_KEY_UP:
        {
          game.handleKeyInput(player, event.key.scancode, false);
          if (event.key.scancode == SDL_SCANCODE_Q) {
            gs.debugMode = !gs.debugMode;
          }
          break;
        }
      }
    }

    // TODO make into helper: UpdateAllObjects()
    // update all objects;
    for (auto &layer : gs.layers) {
      for (GameObject &obj : layer) { // for each obj in layer
        if (obj.dynamic) {
          game.updateGameObject(obj, deltaTime);
        }
      }
    }

    // update bullet physics
    for (GameObject &bullet : gs.bullets) {
      game.updateGameObject(bullet, deltaTime);
    }

    // TODO wrap all below in Render() function
    // calculate viewport position based on player updated position
    gs.mapViewport.x = (player.position.x + player.spritePixelW / 2) - gs.mapViewport.w / 2;

    SDL_SetRenderDrawColor(sdl.renderer, 20, 10, 30, 255);

    // clear the backbuffer before drawing onto it with black from draw color above
    SDL_RenderClear(sdl.renderer);

    // Perform drawing commands:

    // draw background images
    SDL_RenderTexture(sdl.renderer, res.texBg1, nullptr, nullptr);
    game.drawParalaxBackground(res.texBg4, player.velocity.x, gs.bg4scroll, 0.075f, deltaTime);
    game.drawParalaxBackground(res.texBg3, player.velocity.x, gs.bg3scroll, 0.15f, deltaTime);
    game.drawParalaxBackground(res.texBg2, player.velocity.x, gs.bg2scroll, 0.3f, deltaTime);

    // draw all background objects
    for (auto &tile : gs.backgroundTiles) {
      SDL_FRect dst{
        .x = tile.position.x - gs.mapViewport.x,
        .y = tile.position.y,
        .w = static_cast<float>(tile.texture->w),
        .h = static_cast<float>(tile.texture->h),
      };
      SDL_RenderTexture(sdl.renderer, tile.texture, nullptr, &dst);
    }

    // draw all interactable objects
    for (auto &layer : gs.layers) {
      for (GameObject &obj : layer) {
        game.drawObject(obj, obj.spritePixelH, obj.spritePixelW, deltaTime);
      }
    }

    // draw bullets
    for (GameObject &bullet: gs.bullets) {
      game.drawObject(bullet, bullet.collider.h, bullet.collider.w, deltaTime);
    }

    // draw all foreground objects
    for (auto &tile : gs.foregroundTiles) {
      SDL_FRect dst{
        .x = tile.position.x - gs.mapViewport.x,
        .y = tile.position.y,
        .w = static_cast<float>(tile.texture->w),
        .h = static_cast<float>(tile.texture->h),
      };
      SDL_RenderTexture(sdl.renderer, tile.texture, nullptr, &dst);
    }

    // debugging
    if (gs.debugMode) {
      SDL_SetRenderDrawColor(sdl.renderer, 255, 255, 255, 255);
      SDL_RenderDebugText(
          sdl.renderer,
          5,
          5,
          std::format("State3: {}  Direction: {} B: {}, G: {}", static_cast<int>(player.data.player.state), player.direction, gs.bullets.size(), player.grounded).c_str());
    }
    // swap backbuffer to display new state
    // Textures live in GPU memory; the renderer batches copies/draws and flushes them on present.
    SDL_RenderPresent(sdl.renderer);

    prevTime = nowTime;
  };

  game.cleanupTextures();
  game.cleanup(); // Clean up SDL

  std::cout << "Exited cleanly." << std::endl;
  return 0;
}