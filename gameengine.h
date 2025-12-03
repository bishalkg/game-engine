#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <format>
#include <array>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <glm/glm.hpp>

#include "gameobject.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"


// sips -z 42 42 data/idle_marie.png --out data/idle_marie_42.png --> resize
// magick data/move_helmet_marie.png -filter point -resize 210x42! data/move_helmet_marie_42.png

namespace GameEngine {

struct SDLState {
  SDL_Window *window;
  SDL_Renderer *renderer;
  int width, height, logW, logH;
  const bool *keys;

  SDLState() : keys(SDL_GetKeyboardState(nullptr)) {}
};

struct GameState {

  // state for menu or game or network options?


  std::array<std::vector<GameObject>, 2> layers;
  std::vector<GameObject> backgroundTiles;
  std::vector<GameObject> foregroundTiles;
  std::vector<GameObject> bullets;
  bool debugMode;

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
    debugMode = false;
  }

  // get current player
  GameObject &player(size_t layer_idx_chars) { return layers[layer_idx_chars][playerIndex]; }
};

struct Resources {
  const int ANIM_PLAYER_IDLE = 0;
  const int ANIM_PLAYER_RUN = 1;
  const int ANIM_PLAYER_SLIDE = 2;
  const int ANIM_PLAYER_SHOOT = 3;
  const int ANIM_PLAYER_SLIDE_SHOOT = 4;
  std::vector<Animation> playerAnims;

  const int ANIM_BULLET_MOVING = 0;
  const int ANIM_BULLET_HIT = 1;
  std::vector<Animation> bulletAnims;

  const int ANIM_ENEMY = 0;
  const int ANIM_ENEMY_HIT = 1;
  const int ANIM_ENEMY_DIE = 2;
  std::vector<Animation> enemyAnims;

  std::vector<SDL_Texture*> textures;
  SDL_Texture *texIdle, *texRun, *texSlide, *texBrick,
    *texGrass, *texGround, *texPanel,
    *texBg1, *texBg2, *texBg3, *texBg4, *texBullet, *texBulletHit,
    *texShoot, *texRunShoot, *texSlideShoot,
    *texEnemy, *texEnemyHit, *texEnemyDie;

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
    playerAnims[ANIM_PLAYER_SLIDE] = Animation(1, 1.0f);
    playerAnims[ANIM_PLAYER_SHOOT] = Animation(4, 0.5f);
    playerAnims[ANIM_PLAYER_SLIDE_SHOOT] = Animation(4, 0.5f);

    texIdle = loadTexture(state.renderer,  "data/idle.png");
    // texIdle = loadTexture(state.renderer,  "data/move_helmet_marie_42.png");
    texRun = loadTexture(state.renderer, "data/run.png");
    texSlide = loadTexture(state.renderer, "data/slide.png");
    // texRun = loadTexture(state.renderer, "data/move_helmet_marie_42.png");
    texShoot = loadTexture(state.renderer, "data/shoot.png");
    texRunShoot = loadTexture(state.renderer, "data/shoot_run.png");
    texSlideShoot = loadTexture(state.renderer, "data/slide_shoot.png");


    texBrick = loadTexture(state.renderer, "data/tiles/brick.png");
    texGrass = loadTexture(state.renderer, "data/tiles/grass.png");
    texGround = loadTexture(state.renderer, "data/tiles/ground.png");
    texPanel = loadTexture(state.renderer, "data/tiles/panel.png");
    texBg1 = loadTexture(state.renderer, "data/bg/bg_layer1.png");
    texBg2 = loadTexture(state.renderer, "data/bg/bg_layer2.png");
    texBg3 = loadTexture(state.renderer, "data/bg/bg_layer3.png");
    texBg4 = loadTexture(state.renderer, "data/bg/bg_layer4.png");

    bulletAnims.resize(2);
    bulletAnims[ANIM_BULLET_MOVING] = Animation(4, 0.05f);
    bulletAnims[ANIM_BULLET_HIT] = Animation(4, 0.15f);
    texBullet = loadTexture(state.renderer, "data/bullet.png");
    texBulletHit = loadTexture(state.renderer, "data/bullet_hit.png");

    enemyAnims.resize(3);
    enemyAnims[ANIM_ENEMY] = Animation(8, 2.0f);
    enemyAnims[ANIM_ENEMY_HIT] = Animation(8, 1.0f);
    enemyAnims[ANIM_ENEMY_DIE] = Animation(18, 2.0f);
    texEnemy = loadTexture(state.renderer, "data/enemy.png");
    texEnemyHit = loadTexture(state.renderer, "data/enemy_hit.png");
    texEnemyDie = loadTexture(state.renderer, "data/enemy_die.png");
  };

  void unload() {
    for (SDL_Texture *tex : textures) {
      SDL_DestroyTexture(tex);
    }
  };

};



/*
GameEngine is the main class that provides all the functionality to run our game
*/
class GameEngine
{
  private:
    SDLState state;
    GameState gs;
    Resources res;

  public:
    GameEngine() : state{}, gs(state), res{} {}
    GameEngine(SDLState& state, GameState& gs, Resources& res)
        : state(state), gs(gs), res(res) {}


    inline static constexpr glm::vec2 GRAVITY = glm::vec2(0, 500);
    inline static constexpr size_t LAYER_IDX_LEVEL = 0;
    inline static constexpr size_t LAYER_IDX_CHARACTERS = 1;
    inline static constexpr int MAP_ROWS = 5;
    inline static constexpr int MAP_COLS = 50;
    inline static constexpr int TILE_SIZE = 32; // TODO all tiles are 32 right now
    inline static constexpr float JUMP_FORCE = -200.f;


    bool init(int width, int height, int logW, int logH);
    bool initWindowAndRenderer(int width, int height, int logW, int logH);
    void cleanupTextures();
    void cleanup(); // can be ref or pointer; if using pointer, need to use -> instead of .
    void drawObject(GameObject &obj, float height, float width, float deltaTime);
    void updateGameObject(GameObject &obj, float deltaTime);
    bool initAllTiles();
    void handleCollision(GameObject &a, GameObject &b, float deltaTime);
    void collisionResponse(const SDL_FRect &rectA, const SDL_FRect &rectB, const SDL_FRect &rectC, GameObject &objA, GameObject &objB, float deltaTime);
    void handleKeyInput(GameObject &obj, SDL_Scancode key, bool keyDown);
    void drawParalaxBackground(SDL_Texture *texture, float xVelocity, float &scrollPos, float scrollFactor, float deltaTime);

    // getters
    GameObject &getPlayer();
    SDLState &getSDLState();
    GameState &getGameState();
    Resources &getResources();

    // setters
    void setWindowSize(int height, int width);
};

GameObject &GameEngine::getPlayer() {
 return gs.player(LAYER_IDX_CHARACTERS);
};

SDLState &GameEngine::getSDLState() {
  return state;
};

GameState &GameEngine::getGameState() {
  return gs;
};

Resources &GameEngine::getResources() {
  return res;
};

void GameEngine::setWindowSize(int height, int width) {
  state.width = width;
  state.height = height;
}

bool GameEngine::init(int width, int height, int logW, int logH) {

  // init window and renderer
  if (!this->initWindowAndRenderer(width, height, logW, logH)) {
    return false;
  };

  // load game assets
  res.load(state);
  if (!res.texIdle) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Idle Texture load failed", "Failed to load idle image", nullptr);
    this->cleanup();
    return false;
  }

  // setup game data
  gs = GameState(state);
  return this->initAllTiles();
}

bool GameEngine::initWindowAndRenderer(int width, int height, int logW, int logH) {

  state.width = width;
  state.height = height;
  state.logW = logW;
  state.logH = logH;

  if (!SDL_Init(SDL_INIT_VIDEO)) { // later to add audio we'll also need SDL_INIT_AUDIO
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL Init Error", "Failed to initialize SDL.", nullptr);
    return false;
  };

  // SDL_CreateWindow("SDL Game Engine",width, height, fullscreen ? SDL_WINDOW_FULLSCREEN : 0),
  // SDL_CreateRenderer(sdlstate.window, nullptr)

  if (!SDL_CreateWindowAndRenderer("SDL Game Engine", state.width, state.height, SDL_WINDOW_RESIZABLE, &state.window, &state.renderer)) {

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL init error", SDL_GetError(), nullptr);

    this->cleanup();
    return false;
  }
  SDL_SetRenderVSync(state.renderer, 1);

  // configure presentation
  SDL_SetRenderLogicalPresentation(state.renderer, state.logW , state.logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);

  // Setup ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // optional

  // Setup ImGui style
  ImGui::StyleColorsDark();

  // Setup ImGui platform/renderer backends
  ImGui_ImplSDL3_InitForSDLRenderer(state.window, state.renderer);
  ImGui_ImplSDLRenderer3_Init(state.renderer);

  return true;

}

void GameEngine::cleanupTextures() {
  this->res.unload();
}

void GameEngine::cleanup() {
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyRenderer(state.renderer); // destroy renderer before window
  SDL_DestroyWindow(state.window);
  SDL_Quit();
}

void GameEngine::drawObject(GameObject &obj, float height, float width, float deltaTime) {

    // pull out specific sprite frame from sprite sheet
    float srcX = obj.currentAnimation != -1 ? obj.animations[obj.currentAnimation].currentFrame() * width : 0.0f;

    SDL_FRect src = {
      .x = srcX, // different starting x position in sprite sheet
      .y = 0,
      .w = width,
      .h = height
    };

    SDL_FRect dst = {
      .x = obj.position.x - gs.mapViewport.x, // move objects according to updated viewport position
      .y = obj.position.y,
      .w = width,
      .h = height,
    };

    SDL_FlipMode flipMode = obj.direction == -1 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    if (obj.shouldFlash) {
      SDL_SetTextureColorModFloat(obj.texture, 2.5f, 1.0f, 1.0f);
    }
    SDL_RenderTextureRotated(state.renderer, obj.texture, &src, &dst, 0, nullptr, flipMode);
    if (obj.shouldFlash) {
      SDL_SetTextureColorModFloat(obj.texture, 1.0f, 1.0f, 1.0f);
      if (obj.flashTimer.step(deltaTime)) {
        obj.shouldFlash = false;
      }
    }

    if (gs.debugMode) {
      // display each objects collision hitbox
      SDL_FRect rectA{
        .x = obj.position.x + obj.collider.x - gs.mapViewport.x,
        .y = obj.position.y + obj.collider.y,
        .w = obj.collider.w,
        .h = obj.collider.h
      };
      SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 100);
      SDL_RenderFillRect(state.renderer, &rectA);
      SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_NONE);

      SDL_FRect sensor{
        .x = obj.position.x + obj.collider.x - gs.mapViewport.x,
        .y = obj.position.y + obj.collider.y + obj.collider.h,
        .w = obj.collider.w, .h = 1
      };
      SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(state.renderer, 0, 0, 255, 255);
      SDL_RenderFillRect(state.renderer, &sensor);
      SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_NONE);
    }
}

// update updates the state of the passed in game object every render loop
void GameEngine::updateGameObject(GameObject &obj, float deltaTime) {

  if (obj.currentAnimation != -1) {
    obj.animations[obj.currentAnimation].step(deltaTime);
  }

  // gravity applied globally; downward y force when not grounded
  if (obj.dynamic && !obj.grounded) {
    // increase downward velocity = acc*deltaTime every frame
    obj.velocity += GRAVITY * deltaTime;
  }

  float currDirection = 0;
  if (obj.type == ObjectType::player) {

    // update direction
    if (state.keys[SDL_SCANCODE_LEFT]) {
      currDirection += -1;
    }
    if (state.keys[SDL_SCANCODE_RIGHT]) {
      currDirection += 1;
    }

    Timer &weaponTimer = obj.data.player.weaponTimer;
    weaponTimer.step(deltaTime);

    const auto handleShooting = [this, &obj, &weaponTimer, &currDirection](
      SDL_Texture *tex, SDL_Texture *shootTex, int animIndex, int shootAnimIndex){
    // TODO use similar condition to prevent double jump
      if (state.keys[SDL_SCANCODE_A]) {

        // set player texture during shooting anims
        obj.texture = shootTex;
        obj.currentAnimation = shootAnimIndex;
        if (weaponTimer.isTimedOut()) {
          weaponTimer.reset();
          // create bullets
          GameObject bullet(4, 4);
          bullet.data.bullet = BulletData();
          bullet.type = ObjectType::bullet;
          bullet.direction = obj.direction;
          bullet.texture = res.texBullet;
          bullet.currentAnimation = res.ANIM_BULLET_MOVING;
          bullet.collider = SDL_FRect{
            .x = 0, .y = 0,
            .w = static_cast<float>(res.texBullet->h),
            .h = static_cast<float>(res.texBullet->h),
          };
          const int yJitter = 50;
          const float yVelocity = SDL_rand(yJitter) - yJitter / 2.0f;
          bullet.velocity = glm::vec2(
            obj.velocity.x + 600.0f,
            yVelocity
          ) * obj.direction;
          bullet.maxSpeedX = 1000.0f;
          bullet.animations = res.bulletAnims;

          // adjust depending on direction faced; lerp
          const float left = 4;
          const float right = 24;
          const float t = (obj.direction + 1) / 2.0f; // 0 or 1 taking into account neg sign
          const float xOffset = left + right * t;
          bullet.position = glm::vec2(
            obj.position.x + xOffset,
            obj.position.y + obj.spritePixelH / 2 + 1
          );

          bool foundInactive = false;
          for (int i = 0; i < gs.bullets.size() && !foundInactive; i++) {
            if (gs.bullets[i].data.bullet.state == BulletState::inactive) {
              foundInactive = true;
              gs.bullets[i] = bullet;
            }
          }

          // only add new if no inactive found
          if (!foundInactive) {
            this->gs.bullets.push_back(bullet); // push bullets so we can draw them
          }
        }
      } else {
          obj.texture = tex;
          obj.currentAnimation = animIndex;
      }
    };

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

        handleShooting(res.texIdle, res.texShoot, res.ANIM_PLAYER_IDLE, res.ANIM_PLAYER_SHOOT);

        break;
      }
      case PlayerState::running:
      {
        if (currDirection == 0) {
          obj.data.player.state = PlayerState::idle;
        }

        // move in opposite dir of velocity, sliding
        if (obj.velocity.x * obj.direction < 0 && obj.grounded) {
          handleShooting(res.texSlide, res.texSlideShoot, res.ANIM_PLAYER_SLIDE, res.ANIM_PLAYER_SLIDE_SHOOT);
        } else {
          handleShooting(res.texRun, res.texRunShoot, res.ANIM_PLAYER_RUN, res.ANIM_PLAYER_RUN);
          // sprite sheets have same frames so we can seamlessly swap between the two sheets
        }

        break;
      }
      case PlayerState::jumping:
      {
        handleShooting(res.texRun, res.texRunShoot, res.ANIM_PLAYER_RUN, res.ANIM_PLAYER_RUN);
        // obj.texture = res.texRun;
        // obj.currentAnimation = res.ANIM_PLAYER_RUN;
        break;
      }
    }
  }
  else if (obj.type == ObjectType::bullet) {

    switch (obj.data.bullet.state) {
      case BulletState::moving:
      {
        if (obj.position.x - gs.mapViewport.x < 0 || obj.position.x - gs.mapViewport.x > state.logW ||
        obj.position.y - gs.mapViewport.y < 0 ||
        obj.position.y - gs.mapViewport.y > state.logH) {
        obj.data.bullet.state = BulletState::inactive;
        }
        break;
      }
      case BulletState::colliding:
      {
        if (obj.animations[obj.currentAnimation].isDone()) {
          obj.data.bullet.state = BulletState::inactive;
        }
        break;
      }
    }
  }
  else if (obj.type == ObjectType::enemy) {
    switch (obj.data.enemy.state) {
      case EnemyState::dying:
      {
        if (obj.data.enemy.damageTimer.step(deltaTime)) {
          obj.data.enemy.state = EnemyState::idle;
          obj.texture = res.texEnemy;
          obj.currentAnimation = res.ANIM_ENEMY;
          obj.data.enemy.damageTimer.reset();
        }
        break;
      }
    }
  }

  if (currDirection) {
    obj.direction = currDirection;
  }
  // update velocity based on currDirection (which way we're facing),
  // acceleration and deltaTime
  obj.velocity += currDirection * obj.acceleration * deltaTime;
  if (std::abs(obj.velocity.x) > obj.maxSpeedX) { // cap the max velocity
    obj.velocity.x = currDirection * obj.maxSpeedX;
  }
  // update position based on velocity
  obj.position += obj.velocity * deltaTime;

  // handle collision detection
  bool foundGround = false;
  for (auto &layer : gs.layers) {
    for (GameObject &objB: layer){
      if (&obj != &objB && objB.collider.h != 0 && objB.collider.w != 0) {
        this->handleCollision(obj, objB, deltaTime);

        // update ground sensor only when landing on level tiles
        if (objB.type == ObjectType::level) {
          SDL_FRect sensor{
            .x = obj.position.x + obj.collider.x,
            .y = obj.position.y + obj.collider.y + obj.collider.h,
            .w = obj.collider.w, .h = 1
          };

          SDL_FRect rectB{
            .x = objB.position.x + objB.collider.x,
            .y = objB.position.y + objB.collider.y,
            .w = objB.collider.w, .h = objB.collider.h
          };

          SDL_FRect dummyRectC{0};

          if (SDL_GetRectIntersectionFloat(&sensor, &rectB, &dummyRectC)) {
            foundGround = true;
          }
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

void GameEngine::collisionResponse(const SDL_FRect &rectA, const SDL_FRect &rectB, const SDL_FRect &rectC, GameObject &objA, GameObject &objB, float deltaTime) {

  // logRectEvery(rectC, 100000);
  const auto defaultResponse = [&]() {
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
  };

  if (objA.type == ObjectType::player) {
    switch (objB.type) {
      case ObjectType::level:
      {
        defaultResponse();
        break;
      }
      case ObjectType::enemy:
      {
        break;
      }
      case ObjectType::player:
      {
        break;
      }
    }
  } else if (objA.type == ObjectType::bullet) {

    switch (objA.data.bullet.state) {
      case BulletState::moving:
      {
        switch (objB.type) {
          case ObjectType::level:
          {
            break;
          }
          case ObjectType::enemy:
          {
            objB.direction = -1 * objA.direction;
            objB.shouldFlash = true;
            objB.flashTimer.reset();
            objB.texture = res.texEnemyHit;
            objB.currentAnimation = res.ANIM_ENEMY_HIT;
            objB.data.enemy.state = EnemyState::dying;
            break;
          }
        }
        defaultResponse();
        objA.velocity *= 0;
        objA.data.bullet.state = BulletState::colliding;
        objA.texture = res.texBulletHit;
        objA.currentAnimation = res.ANIM_BULLET_HIT;
        break;
      }
    }

  }

}

void GameEngine::handleCollision(GameObject &a, GameObject &b, float deltaTime) {

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
    this->collisionResponse(rectA, rectB, rectC, a, b, deltaTime);
  };
};

bool GameEngine::initAllTiles() {

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
    0, 0, 0, 0, 2, 0, 0, 0, 0, 3,2, 2, 2, 2, 2,2, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 2, 2, 0, 0, 3, 2, 2,2, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
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
  const auto loadMap = [this](short maplayer[MAP_ROWS][MAP_COLS]) {
    const auto createObject = [this](int r, int c, SDL_Texture *tex, ObjectType type, float spriteH, float spriteW) {
      GameObject o(spriteH, spriteW);
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
            GameObject ground = createObject(r, c, res.texGround, ObjectType::level, TILE_SIZE, TILE_SIZE);
            ground.data.level = LevelData();
            gs.layers[LAYER_IDX_LEVEL].push_back(ground); // we do this so we can update each object and destroy the objects with easy access
            break;
          }
          case 2: // Panel
          {
            GameObject panel = createObject(r, c, res.texPanel, ObjectType::level, TILE_SIZE, TILE_SIZE);
            panel.data.level = LevelData();
            gs.layers[LAYER_IDX_LEVEL].push_back(panel);
            break;
          }
          case 3: // Enemy
          {
            GameObject enemy = createObject(r, c, res.texEnemy, ObjectType::enemy, TILE_SIZE, TILE_SIZE);
            enemy.data.enemy = EnemyData();
            enemy.currentAnimation = res.ANIM_ENEMY;
            enemy.animations = res.enemyAnims;
            enemy.dynamic = true;
            gs.layers[LAYER_IDX_CHARACTERS].push_back(enemy);
            break;
          }
          case 4: // player
          {
            GameObject player = createObject(r, c, res.texIdle, ObjectType::player, 32, 32); // TODO update with new dimensions
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
            GameObject grass = createObject(r, c, res.texGrass, ObjectType::level, TILE_SIZE, TILE_SIZE);
            grass.data.level = LevelData();
            // gs.layers[LAYER_IDX_LEVEL].push_back(grass);
            gs.foregroundTiles.push_back(grass);
            break;
          }
          case 6:
          {
            GameObject brick = createObject(r, c, res.texBrick, ObjectType::level, TILE_SIZE, TILE_SIZE);
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

  // assert(gs.playerIndex != -1); // player index must be set
  return gs.playerIndex != -1;
};

void GameEngine::handleKeyInput(GameObject &obj, SDL_Scancode key, bool keyDown) {

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

void GameEngine::drawParalaxBackground(SDL_Texture *texture, float xVelocity, float &scrollPos, float scrollFactor, float deltaTime) {
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

  SDL_RenderTextureTiled(state.renderer, texture, nullptr, 1, &dst);
}

}