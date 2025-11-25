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


using namespace std;

struct SDLState {
  SDL_Window *window;
  SDL_Renderer *renderer;
  int width, height, logW, logH;
};

// sips -z 42 42 data/idle_marie.png --out data/idle_marie_42.png --> resize
// magick data/move_helmet_marie.png -filter point -resize 210x42! data/move_helmet_marie_42.png

const size_t LAYER_IDX_LEVEL = 0;
const size_t LAYER_IDX_CHARACTERS = 1;

struct GameState {
  std::array<std::vector<GameObject>, 2> layers;
  int playerIndex;

  GameState() {
    playerIndex = 0;
  }
};

struct Resources {
  const int ANIM_PLAYER_IDLE = 0;
  std::vector<Animation> playerAnims;
  std::vector<SDL_Texture*> textures;
  SDL_Texture *texIdle;

  SDL_Texture *loadTexture(SDL_Renderer *renderer, const std::string &filepath){

    SDL_Texture *tex = IMG_LoadTexture(renderer, filepath.c_str()); // textures on gpu, surface in cpu memory (we can access)
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST); // scale so pixels aren't blended;
    textures.push_back(tex);
    return tex;

  }

  void load(SDLState &state){
    playerAnims.resize(5); // not reserve
    playerAnims[ANIM_PLAYER_IDLE] = Animation(8, 1.6f); // 8 frames in 1.6 sec

    texIdle = loadTexture(state.renderer,  "data/idle.png");
  };

  void unload() {
    for (SDL_Texture *tex : textures) {
      SDL_DestroyTexture(tex);
    }
  };


};


bool initalize(SDLState &state);
void cleanup(SDLState &state); // can be ref or pointer; if using pointer, need to use -> instead of .
void drawObject(const SDLState &state, GameState &gs, GameObject &obj, float deltaTime);
int main(int argc, char *argv[]) {


  // create SDL state
  SDLState sdlstate;
  sdlstate.width = 1600;
  sdlstate.height = 900;
  sdlstate.logW = 640;
  sdlstate.logH = 320;

  if (!initalize(sdlstate)) {
    return 1;
  }


  // load game assets
  Resources res;
  res.load(sdlstate);
  if (!res.texIdle) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Idle Texture load failed", "Failed to load idle image", nullptr);
    cleanup(sdlstate);
    return 1;
  }

  // setup game data
  GameState gs;

  // create the player
  GameObject player;
  player.type = ObjectType::player;
  player.texture = res.texIdle;
  player.animations = res.playerAnims; // copies via std::vector copy assignment
  player.currentAnimation = res.ANIM_PLAYER_IDLE;
  gs.layers[LAYER_IDX_CHARACTERS].push_back(player);

  const bool *keys = SDL_GetKeyboardState(nullptr);

  uint64_t prevTime = SDL_GetTicks();

  // start the game loop
  bool running = true;
  while (running){

    uint64_t nowTime = SDL_GetTicks();
    float deltaTime = (nowTime - prevTime) / 1000.0f; // convert to seconds; time bw frames

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
          sdlstate.width = event.window.data1;
          sdlstate.height = event.window.data2;
          break;
        }
      }
    }


    // update all objects
    for (auto &layer : gs.layers) {
      for (GameObject &obj : layer) { // for each obj in layer
        if (obj.currentAnimation != -1) {
          obj.animations[obj.currentAnimation].step(deltaTime);
        }
      }
    }

    // perform drawing commands
    SDL_SetRenderDrawColor(sdlstate.renderer, 20, 10, 30, 255);
    SDL_RenderClear(sdlstate.renderer);

    // draw all objects
    for (auto &layer : gs.layers) {
      for (GameObject &obj : layer) {
        drawObject(sdlstate, gs, obj, deltaTime);
      }
    }

    // swap buffer
    SDL_RenderPresent(sdlstate.renderer);

    prevTime = nowTime;
  };

  res.unload();
  cleanup(sdlstate); // Clean up SDL

  std::cout << "Exited cleanly." << std::endl;
  return 0;
}

bool initalize(SDLState &sdlstate) {

  if (!SDL_Init(SDL_INIT_VIDEO)) { // later to add audio we'll also need SDL_INIT_AUDIO
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL Init Error", "Failed to initialize SDL.", nullptr);
    return false;
  };

  // SDL_CreateWindow("SDL Game Engine",width, height, fullscreen ? SDL_WINDOW_FULLSCREEN : 0),
  // SDL_CreateRenderer(sdlstate.window, nullptr)

  if (!SDL_CreateWindowAndRenderer("SDL Game Engine", sdlstate.width, sdlstate.height, SDL_WINDOW_RESIZABLE, &sdlstate.window, &sdlstate.renderer)) {

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL init error", SDL_GetError(), nullptr);

    cleanup(sdlstate);
    return false;
  }

  // configure presentation
  SDL_SetRenderLogicalPresentation(sdlstate.renderer, sdlstate.logW , sdlstate.logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);

  return true;

}

void cleanup(SDLState &state) {
  SDL_DestroyRenderer(state.renderer); // destroy renderer before window
  SDL_DestroyWindow(state.window);
  SDL_Quit();
}

void drawObject(const SDLState &state, GameState &gs, GameObject &obj, float deltaTime) {
    const float spriteSize = 32; // TODO need size of images


    // pull out specific sprite frame from sprite sheet
    float srcX = obj.currentAnimation != -1 ? obj.animations[obj.currentAnimation].currentFrame() * spriteSize : 0.0f;

    SDL_FRect src = {
      .x = srcX, // different starting x position in sprite sheet
      .y = 0,
      .w = spriteSize,
      .h = spriteSize
    };

    SDL_FRect dst = {
      .x = obj.position.x,
      .y = obj.position.y,
      .w = spriteSize,
      .h = spriteSize,
    };

    SDL_FlipMode flipMode = obj.direction == -1 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    SDL_RenderTextureRotated(state.renderer, obj.texture, &src, &dst, 0, nullptr, flipMode);
}