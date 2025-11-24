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

  if (!SDL_CreateWindowAndRenderer("SDL Game Engine", width, height, fullscreen ? SDL_WINDOW_FULLSCREEN : 0, &sdlstate.window, &sdlstate.renderer)) {

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL init error", SDL_GetError(), nullptr);

    cleanup(sdlstate);
  }



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
    SDL_SetRenderDrawColor(sdlstate.renderer, 255, 255, 255, 255);
    SDL_RenderClear(sdlstate.renderer);

    // swap buffer
    SDL_RenderPresent(sdlstate.renderer);

  };

  cleanup(sdlstate); // Clean up SDL

  std::cout << "Exited cleanly." << std::endl;
  return 0;
}