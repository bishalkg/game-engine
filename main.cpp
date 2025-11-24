#include <iostream>
#include <vector>
#include <string>
#include <format>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>

using namespace std;

struct SDLState {
  SDL_Window *window;
  SDL_Renderer *renderer;
};

void cleanup(SDLState &state); // can be ref or pointer; if using pointer, need to use -> instead of .

void cleanup(SDLState &state) {
  SDL_DestroyRenderer(state.renderer); // destroy renderer before window
  SDL_DestroyWindow(state.window);
  SDL_Quit();
}

int main(int argc, char *argv[]) {

  if (!SDL_Init(SDL_INIT_VIDEO)) { // later to add audio we'll also need SDL_INIT_AUDIO
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL Init Error", "Failed to initialize SDL.", nullptr);
    return 1;
  };

  // create window
  int width = 800;
  int height = 600;
  bool fullscreen = false;

  // create SDL state
  SDLState sdlstate;

  // SDL_CreateWindow("SDL Game Engine",width, height, fullscreen ? SDL_WINDOW_FULLSCREEN : 0),
  // SDL_CreateRenderer(sdlstate.window, nullptr)

  if (!SDL_CreateWindowAndRenderer("SDL Game Engine", width, height, fullscreen ? SDL_WINDOW_FULLSCREEN : SDL_WINDOW_RESIZABLE, &sdlstate.window, &sdlstate.renderer)) {

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL init error", SDL_GetError(), nullptr);

    cleanup(sdlstate);
  }

  // configure presentation
  int logW = 640;
  int logH = 320;
  SDL_SetRenderLogicalPresentation(sdlstate.renderer, logW, logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);

  // load game assets
  SDL_Texture *idleTex = IMG_LoadTexture(sdlstate.renderer, "data/idle.png"); // textures on gpu, surface in cpu memory (we can access)
  SDL_SetTextureScaleMode(idleTex, SDL_SCALEMODE_NEAREST); // scale so pixels aren't blended

  // start the game loop
  bool running = true;
  while (running){
    SDL_Event event{0};
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_EVENT_QUIT:
        {
          running = false;
          break;
        }
      }
    }

    // perform drawing commands
    SDL_SetRenderDrawColor(sdlstate.renderer, 20, 10, 30, 255);
    SDL_RenderClear(sdlstate.renderer);

    SDL_FRect src{
      .x = 0,
      .y = 0,
      .w = 32,
      .h = 32
    };

    SDL_FRect dst{
      .x = 0,
      .y = 0,
      .w = 32,
      .h = 32,
    };

    SDL_RenderTexture(sdlstate.renderer, idleTex, &src, &dst);

    // swap buffer
    SDL_RenderPresent(sdlstate.renderer);

  };

  SDL_DestroyTexture(idleTex);
  cleanup(sdlstate); // Clean up SDL

  std::cout << "Exited cleanly." << std::endl;
  return 0;
}