#include <iostream>
#include <vector>
#include <string>
#include <format>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

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

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL Init Error", "Failed to initialize SDL.", nullptr);
    return 1;
  };

  // create window
  int width = 800;
  int height = 600;
  bool fullscreen = false;

  // create SDL state
  SDLState sdlstate{
    SDL_CreateWindow("SDL Game Engine",width, height, fullscreen ? SDL_WINDOW_FULLSCREEN : 0),
    SDL_CreateRenderer(sdlstate.window, nullptr)
  };

  if (!sdlstate.window) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Window Creation Error", "Failed to create SDL window.", nullptr);
    cleanup(sdlstate);
    return 1;
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
  }


  cleanup(sdlstate); // Clean up SDL

  std::cout << "Exited cleanly." << std::endl;
  return 0;
}