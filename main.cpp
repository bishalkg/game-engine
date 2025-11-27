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
  const bool *keys;

  SDLState() : keys(SDL_GetKeyboardState(nullptr)) {}
};

// sips -z 42 42 data/idle_marie.png --out data/idle_marie_42.png --> resize
// magick data/move_helmet_marie.png -filter point -resize 210x42! data/move_helmet_marie_42.png

const size_t LAYER_IDX_LEVEL = 0;
const size_t LAYER_IDX_CHARACTERS = 1;
const int MAP_ROWS = 5;
const int MAP_COLS = 50;
const int TILE_SIZE = 32; // TODO all tiles are 32 right now

struct GameState {
  std::array<std::vector<GameObject>, 2> layers;
  std::vector<GameObject> backgroundTiles;
  std::vector<GameObject> foregroundTiles;
  int playerIndex;
  SDL_FRect mapViewport; // viewable part of map
  float bg2scroll, bg3scroll, bg4scroll;

  GameState(const SDLState &state): bg2scroll(0), bg3scroll(0), bg4scroll(0) {
    playerIndex = -1;
    mapViewport = SDL_FRect{
      .x = 0, .y = 0,
      .w = static_cast<float>(state.logW),
      .h = static_cast<float>(state.logH)
    };
  }

  // get current player
  GameObject &player() { return layers[LAYER_IDX_CHARACTERS][playerIndex]; }
};

struct Resources {
  const int ANIM_PLAYER_IDLE = 0;
  const int ANIM_PLAYER_RUN = 1;
  const int ANIM_PLAYER_SLIDE = 2;
  std::vector<Animation> playerAnims;
  std::vector<SDL_Texture*> textures;
  SDL_Texture *texIdle, *texRun, *texSlide, *texBrick,
    *texGrass, *texGround, *texPanel,
    *texBg1, *texBg2, *texBg3, *texBg4;

  SDL_Texture *loadTexture(SDL_Renderer *renderer, const std::string &filepath){
    SDL_Texture *tex = IMG_LoadTexture(renderer, filepath.c_str()); // textures on gpu, surface in cpu memory (we can access)
    SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST); // scale so pixels aren't blended;
    textures.push_back(tex);
    return tex;
  }

  void load(SDLState &state){
    playerAnims.resize(5); // not reserve
    playerAnims[ANIM_PLAYER_IDLE] = Animation(8, 1.6f); // 8 frames in 1.6 sec
    playerAnims[ANIM_PLAYER_RUN] = Animation(4, 0.5f); //4
    playerAnims[ANIM_PLAYER_SLIDE] = Animation(1, 1.0f); //1

    texIdle = loadTexture(state.renderer,  "data/idle.png");
    // texIdle = loadTexture(state.renderer,  "data/move_helmet_marie_42.png");
    texRun = loadTexture(state.renderer, "data/run.png");
    texSlide = loadTexture(state.renderer, "data/slide.png");
    // texRun = loadTexture(state.renderer, "data/move_helmet_marie_42.png");
    texBrick = loadTexture(state.renderer, "data/tiles/brick.png");
    texGrass = loadTexture(state.renderer, "data/tiles/grass.png");
    texGround = loadTexture(state.renderer, "data/tiles/ground.png");
    texPanel = loadTexture(state.renderer, "data/tiles/panel.png");
    texBg1 = loadTexture(state.renderer, "data/bg/bg_layer1.png");
    texBg2 = loadTexture(state.renderer, "data/bg/bg_layer2.png");
    texBg3 = loadTexture(state.renderer, "data/bg/bg_layer3.png");
    texBg4 = loadTexture(state.renderer, "data/bg/bg_layer4.png");
  };

  void unload() {
    for (SDL_Texture *tex : textures) {
      SDL_DestroyTexture(tex);
    }
  };


};


bool initWindowAndRenderer(SDLState &state);
void cleanup(SDLState &state); // can be ref or pointer; if using pointer, need to use -> instead of .
void drawObject(const SDLState &state, GameState &gs, GameObject &obj, float deltaTime);
void updateGameObject(const SDLState &state, GameState &gs, Resources &res, GameObject &obj, float deltaTime);
void initAllTiles(const SDLState &state, GameState &gs, const Resources &res);
void handleCollision(const SDLState &state, GameState &gs, Resources &res, GameObject &a, GameObject &b, float deltaTime);
void collisionResponse(const SDLState &state, GameState &gs, Resources &res, const SDL_FRect &rectA, const SDL_FRect &rectB, const SDL_FRect &rectC, GameObject &objA, GameObject &objB, float deltaTime);
void handleKeyInput(const SDLState &state, GameState &gs, GameObject &obj, SDL_Scancode key, bool keyDown);
void drawParalaxBackground(SDL_Renderer *renderer, SDL_Texture *texture, float xVelocity, float &scrollPos, float scrollFactor, float deltaTime);

int main(int argc, char *argv[]) {

  // create SDL state
  SDLState sdlstate;
  sdlstate.width = 1600;
  sdlstate.height = 900;
  sdlstate.logW = 640;
  sdlstate.logH = 320;

  if (!initWindowAndRenderer(sdlstate)) {
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
  GameState gs(sdlstate);
  initAllTiles(sdlstate, gs, res);

  // start the game loop
  uint64_t prevTime = SDL_GetTicks();
  bool running = true;
  while (running){

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
          sdlstate.width = event.window.data1;
          sdlstate.height = event.window.data2;
          break;
        }
        case SDL_EVENT_KEY_DOWN: // non-continuous presses
        {
          handleKeyInput(sdlstate, gs, gs.player(), event.key.scancode, true);
          break;
        }
        case SDL_EVENT_KEY_UP:
        {
          handleKeyInput(sdlstate, gs, gs.player(), event.key.scancode, false);
          break;
        }
      }
    }

    // update all objects
    for (auto &layer : gs.layers) {
      for (GameObject &obj : layer) { // for each obj in layer
        updateGameObject(sdlstate, gs, res, obj, deltaTime);
        if (obj.currentAnimation != -1) {
          obj.animations[obj.currentAnimation].step(deltaTime);
        }
      }
    }

    // calculate viewport position based on player updated position
    gs.mapViewport.x = (gs.player().position.x + gs.player().spritePixelSize / 2) - gs.mapViewport.w / 2;

    // perform drawing commands
    SDL_SetRenderDrawColor(sdlstate.renderer, 20, 10, 30, 255);
    SDL_RenderClear(sdlstate.renderer);

    // draw background images
    SDL_RenderTexture(sdlstate.renderer, res.texBg1, nullptr, nullptr);
    drawParalaxBackground(sdlstate.renderer, res.texBg4, gs.player().velocity.x, gs.bg4scroll, 0.075f, deltaTime);
    drawParalaxBackground(sdlstate.renderer, res.texBg3, gs.player().velocity.x, gs.bg3scroll, 0.15f, deltaTime);
    drawParalaxBackground(sdlstate.renderer, res.texBg2, gs.player().velocity.x, gs.bg2scroll, 0.3f, deltaTime);

    // draw all background objects
    for (auto &tile : gs.backgroundTiles) {
      SDL_FRect dst{
        .x = tile.position.x - gs.mapViewport.x,
        .y = tile.position.y,
        .w = static_cast<float>(tile.texture->w),
        .h = static_cast<float>(tile.texture->h),
      };
      SDL_RenderTexture(sdlstate.renderer, tile.texture, nullptr, &dst);
    }

    // draw all objects
    for (auto &layer : gs.layers) {
      for (GameObject &obj : layer) {
        drawObject(sdlstate, gs, obj, deltaTime);
      }
    }

    // draw all foreground objects
    for (auto &tile : gs.foregroundTiles) {
      SDL_FRect dst{
        .x = tile.position.x - gs.mapViewport.x,
        .y = tile.position.y,
        .w = static_cast<float>(tile.texture->w),
        .h = static_cast<float>(tile.texture->h),
      };
      SDL_RenderTexture(sdlstate.renderer, tile.texture, nullptr, &dst);
    }

    // debugging
    SDL_SetRenderDrawColor(sdlstate.renderer, 255, 255, 255, 255);
    SDL_RenderDebugText(sdlstate.renderer, 5, 5, std::format("State: {}", static_cast<int>(gs.player().data.player.state)).c_str());


    // swap buffer
    SDL_RenderPresent(sdlstate.renderer);

    prevTime = nowTime;
  };

  res.unload();
  cleanup(sdlstate); // Clean up SDL

  std::cout << "Exited cleanly." << std::endl;
  return 0;
}

bool initWindowAndRenderer(SDLState &sdlstate) {

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

    // pull out specific sprite frame from sprite sheet
    float srcX = obj.currentAnimation != -1 ? obj.animations[obj.currentAnimation].currentFrame() * obj.spritePixelSize : 0.0f;

    SDL_FRect src = {
      .x = srcX, // different starting x position in sprite sheet
      .y = 0,
      .w = obj.spritePixelSize,
      .h = obj.spritePixelSize
    };

    SDL_FRect dst = {
      .x = obj.position.x - gs.mapViewport.x, // move objects according to updated viewport position
      .y = obj.position.y,
      .w = obj.spritePixelSize,
      .h = obj.spritePixelSize,
    };

    // // pick a debug color
    // SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 255);

    // // outline only
    // SDL_FRect box{obj.position.x, obj.position.y, TILE_SIZE, TILE_SIZE};
    // SDL_RenderRect(state.renderer, &box);   // SDL3: takes SDL_FRect

    SDL_FlipMode flipMode = obj.direction == -1 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    SDL_RenderTextureRotated(state.renderer, obj.texture, &src, &dst, 0, nullptr, flipMode);
}

// update updates the state of the passed in game object every render loop
void updateGameObject(const SDLState &state, GameState &gs, Resources &res, GameObject &obj, float deltaTime) {

  // gravity applied globally; downward y force
  if (obj.dynamic) {
    // increase downward velocity = acc*deltaTime every frame
    obj.velocity += glm::vec2(0, 500) * deltaTime;
  }

  if (obj.type == ObjectType::player) {

    // update direction
    float currDirection = 0;
    if (state.keys[SDL_SCANCODE_LEFT]) {
      currDirection += -1;
    }
    if (state.keys[SDL_SCANCODE_RIGHT]) {
      currDirection += 1;
    }
    if (currDirection) {
      obj.direction = currDirection;
    }


    // update animation state
    switch (obj.data.player.state) {
      case PlayerState::idle:
      {
        if (currDirection != 0) {
          obj.data.player.state = PlayerState::running;
          obj.texture = res.texRun;
          obj.currentAnimation = res.ANIM_PLAYER_RUN;
        } else {
          // decelerate faster than we speed up
          if (obj.velocity.x) {
            const float factor = obj.velocity.x > 0 ? -1.5f : 1.5f;
            float amount = factor * obj.acceleration.x * deltaTime;
            if (std::abs(obj.velocity.x) < std::abs(amount)) {
              obj.velocity.x = 0;
            } else {
              obj.velocity.x += amount;
            }
          }
        }
        obj.texture = res.texIdle;
        obj.currentAnimation = res.ANIM_PLAYER_IDLE;
        break;
      }
      case PlayerState::running:
      {
        if (currDirection == 0) {
          obj.data.player.state = PlayerState::idle;
        }

        // move in opposite dir of velocity, sliding
        if (obj.velocity.x * obj.direction < 0 && obj.grounded) {
          obj.texture = res.texSlide;
          obj.currentAnimation = res.ANIM_PLAYER_SLIDE;
        } else {
          obj.texture = res.texRun;
          obj.currentAnimation = res.ANIM_PLAYER_RUN;
        }
        break;
      }
      case PlayerState::jumping:
      {
        obj.texture = res.texRun;
        obj.currentAnimation = res.ANIM_PLAYER_RUN;
        break;
      }
    }

    // update velocity based on currDirection (which way we're facing),
    // acceleration and deltaTime
    obj.velocity += currDirection * obj.acceleration * deltaTime;
    if (std::abs(obj.velocity.x) > obj.maxSpeedX) { // cap the max velocity
      obj.velocity.x = currDirection * obj.maxSpeedX;
    }


  }

  // update position based on velocity
  obj.position += obj.velocity * deltaTime;

  // handle collision detection
  bool foundGround = false;
  for (auto &layer : gs.layers) {
    for (GameObject &objB: layer){
      if (&obj != &objB) {
        handleCollision(state, gs, res, obj, objB, deltaTime);

        //ground sensor
        SDL_FRect sensor{
          .x = obj.position.x + obj.collider.x,
          .y = obj.position.y + obj.collider.y + obj.collider.h,
          .w = obj.collider.w, .h = 1
        };

        SDL_FRect rectB{
          .x = objB.position.x + objB.collider.x,
          .y = objB.position.y + objB.collider.y,
          .w = objB.collider.w, .h = objB.collider.w
        };

        if (SDL_HasRectIntersectionFloat(&sensor, &rectB)) {
          foundGround = true;
        }


      }
    }
  }

  if (obj.grounded != foundGround) {
    // switching grounded state
    obj.grounded = foundGround;
    if (foundGround && obj.type == ObjectType::player) {
      obj.data.player.state = PlayerState::running;
    }
  }
}

// debug
void logRectEvery(const SDL_FRect& rectC, int frames);
void logRectEvery(const SDL_FRect& rectC, int frames) {
    static int frameCount = 0;
    if (++frameCount % frames == 0) {
        SDL_Log("rect: x=%.3f y=%.3f w=%.3f h=%.3f",
                rectC.x, rectC.y, rectC.w, rectC.h);
    }
}

void collisionResponse(const SDLState &state, GameState &gs, Resources &res, const SDL_FRect &rectA, const SDL_FRect &rectB, const SDL_FRect &rectC, GameObject &objA, GameObject &objB, float deltaTime) {

  // logRectEvery(rectC, 100000);

  if (objA.type == ObjectType::player) {
    switch (objB.type) {
      case ObjectType::level:
      {
        if (rectC.w < rectC.h) {
          // horizontal collision
          if (objA.velocity.x > 0) {
            // traveling to right, colliding object must be to the right, so sub .w; need extra 0.1 to escape collision for next frame
            objA.position.x -= rectC.w+0.1;
          } else if (objA.velocity.x < 0) {
            objA.position.x += rectC.w+0.1;
          }
          objA.velocity.x = 0; // reset velocity to 0 so object stops
        } else {
          //vertical collison
          if (objA.velocity.y > 0) {
            objA.position.y -= rectC.h; // down
          } else if (objA.velocity.y < 0) {
            objA.position.y += rectC.h; // up
          }
          objA.velocity.y = 0;
        }
        break;
      }
      case ObjectType::enemy:
      {

      }
      case ObjectType::player:
      {

      }
    }
  }

}

void handleCollision(const SDLState &state, GameState &gs, Resources &res, GameObject &a, GameObject &b, float deltaTime) {

  SDL_FRect rectA{
    .x = a.position.x + a.collider.x,
    .y = a.position.y + a.collider.y,
    .w = a.collider.w,
    .h = a.collider.h
  };
  SDL_FRect rectB{
    .x = b.position.x + b.collider.x,
    .y = b.position.y + b.collider.y,
    .w = b.collider.w,
    .h = b.collider.h
  };
  SDL_FRect rectC{ 0 };

  if (SDL_GetRectIntersectionFloat(&rectA, &rectB, &rectC)) {
    // found interection
    collisionResponse(state, gs, res, rectA, rectB, rectC, a, b, deltaTime);
  };
};

void initAllTiles(const SDLState &state, GameState &gs, const Resources &res) {

  /*
    1 - Ground
    2 - Panel
    3 - Enemy
    4 - Player
    5 - Grass
    6 - Brick
  */

  short map[MAP_ROWS][MAP_COLS] = {
    0, 0, 0, 0, 4, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 2, 0, 0, 0, 0, 0,2, 2, 2, 2, 2,2, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 2, 2, 0, 0, 0, 2, 2,2, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,1, 1, 1, 1, 1,1, 1, 1, 1, 1,1, 1, 1, 1, 1,1, 1, 1, 1, 1, 1, 1, 1, 1, 1,1, 1, 1, 1, 1,1, 1, 1, 1, 1,1, 1, 1, 1, 1,
  };

  short foregroundMap[MAP_ROWS][MAP_COLS] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    5, 5, 5, 0, 0, 5, 5, 5, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
  };

  short backgroundMap[MAP_ROWS][MAP_COLS] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 6, 6, 6, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
  };

  // short maplayer[MAP_ROWS][MAP_COLS] really means short (*maplayer)[MAP_COLS]: a pointer to the first row (each row is an array of MAP_COLS shorts)
  // const short (&maplayer)[MAP_ROWS][MAP_COLS] if want by reference
  const auto loadMap = [&state, &gs, &res](short maplayer[MAP_ROWS][MAP_COLS]) {
    const auto createObject = [&state](int r, int c, SDL_Texture *tex, ObjectType type, int spriteSize) {
      GameObject o(spriteSize);
      o.type = type;
      o.position = glm::vec2(c * TILE_SIZE, state.logH - (MAP_ROWS - r) * TILE_SIZE);
      o.texture = tex;
      o.collider = {
        .x = 0,
        .y = 0,
        .w = TILE_SIZE,
        .h = TILE_SIZE
      };
      return o;
    };

    for (int r = 0; r < MAP_ROWS; r++) {
      for (int c = 0; c < MAP_COLS; c++) {
        switch (maplayer[r][c]) {
          case 1: // Ground
          {
            GameObject ground = createObject(r, c, res.texGround, ObjectType::level, 32);
            ground.data.level = LevelData();
            gs.layers[LAYER_IDX_LEVEL].push_back(ground); // we do this so we can update each object and destroy the objects with easy access
            break;
          }
          case 2: // Panel
          {
            GameObject panel = createObject(r, c, res.texPanel, ObjectType::level, 32);
            panel.data.level = LevelData();
            gs.layers[LAYER_IDX_LEVEL].push_back(panel);
            break;
          }
          case 4: // player
          {
            GameObject player = createObject(r, c, res.texIdle, ObjectType::player, 32);
            player.data.player = PlayerData();
            player.animations = res.playerAnims; // copies via std::vector copy assignment
            player.currentAnimation = res.ANIM_PLAYER_IDLE;
            player.acceleration = glm::vec2(300, 0);
            player.maxSpeedX = 100;
            player.dynamic = true;
            player.collider = {
              .x = 11,
              .y = 6,
              .w = 10,
              .h = 26
            };
            gs.layers[LAYER_IDX_CHARACTERS].push_back(player);
            gs.playerIndex = gs.layers[LAYER_IDX_CHARACTERS].size() - 1;
            break;
          }
          case 5:
          {
            GameObject grass = createObject(r, c, res.texGrass, ObjectType::level, 32);
            grass.data.level = LevelData();
            // gs.layers[LAYER_IDX_LEVEL].push_back(grass);
            gs.foregroundTiles.push_back(grass);
            break;
          }
          case 6:
          {
            GameObject brick = createObject(r, c, res.texBrick, ObjectType::level, 32);
            brick.data.level = LevelData();
            // gs.layers[LAYER_IDX_LEVEL].push_back(brick);
            gs.backgroundTiles.push_back(brick);
            break;
          }
        }
      }
    }
  };

  loadMap(map);
  loadMap(backgroundMap);
  loadMap(foregroundMap);

  assert(gs.playerIndex != -1); // player index must be set
};

void handleKeyInput(const SDLState &state, GameState &gs, GameObject &obj, SDL_Scancode key, bool keyDown) {

  const float JUMP_FORCE = -200.f;
  if (obj.type == ObjectType::player) {
    switch (obj.data.player.state) {
      case PlayerState::idle:
      {
        if (key == SDL_SCANCODE_UP && keyDown) {
          obj.velocity.y += JUMP_FORCE;
          obj.data.player.state = PlayerState::jumping;
        }
        break;
      }
      case PlayerState::jumping:
      {
        if (!keyDown) {
          obj.velocity.y = 0;
          obj.data.player.state = PlayerState::idle;
        }
        break;
      }
      case PlayerState::running:
      {
        if (key == SDL_SCANCODE_UP && keyDown) {
          obj.velocity.y += JUMP_FORCE;
          obj.data.player.state = PlayerState::jumping;
        }
        break;
      }
    }

  }



};

void drawParalaxBackground(SDL_Renderer *renderer, SDL_Texture *texture, float xVelocity, float &scrollPos, float scrollFactor, float deltaTime) {
  scrollPos -= xVelocity * scrollFactor * deltaTime; // scroll position passed by reference, is updated every loop
  if (scrollPos <= -texture->w) {
    scrollPos = 0;
  }

  SDL_FRect dst{
    .x = scrollPos,
    .y = 40,
    .w = texture->w * 2.0f,
    .h = static_cast<float>(texture->h)
  };

  SDL_RenderTextureTiled(renderer, texture, nullptr, 1, &dst);
}