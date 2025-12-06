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


// GameApp class
// GameApp:start() -> creates GameEngine -> does everything thats done in the game loop rn
// GameApp class also has networking and persistence, imGui. We inject these as reference to the GameEngine
// persistence:
// Small/local only: write to files (JSON/TOML/INI) for settings; binary or JSON for save data. Use a schema/version field so you can evolve saves. Keep it in a known writable dir (per-platform: %APPDATA%, ~/Library/Application Support/, ~/.local/share/).
// Lightweight DB (SQLite): useful if you need multiple save slots with indexing, leaderboards, or richer queries. It’s a single file and embeddable, but adds a dependency and some overhead compared to just serializing structs.
// Remote/cloud: only if you need cross-device or online profiles; otherwise keep it local.


// Define the surface clearly: e.g., std::function<bool(const GameState&)> saveFn and maybe loadFn. Keep the signature narrow and types stable.
// Ownership/lifetime: store the function objects by value in GameEngine; ensure whatever they capture outlives the engine (or use a weak pointer pattern to detect expired).
// this way I can also create offline mode where networking object is nullptr
// and the functions dont do anything or defauly to local versions
// Error handling: decide how failures are signaled (bool/Status/exception) and what the engine should do if a save fails.
// Threading: if you ever call the callback off the main thread, make sure the persistence side is thread-safe.
// Testing: callbacks make it easy to inject fakes in tests.
// If you find you need more than one or two functions, consider a tiny interface/struct instead (e.g., struct PersistenceAPI { SaveFn save; LoadFn load; };) to avoid callback sprawl.


// sips -z 42 42 data/idle_marie.png --out data/idle_marie_42.png --> resize
// magick data/move_helmet_marie.png -filter point -resize 210x42! data/move_helmet_marie_42.png

namespace GameEngine {

struct SDLState {
  SDL_Window *window;
  SDL_Renderer *renderer;
  int width, height, logW, logH;
  const bool *keys;

  // TODO create a new struct for imgui params (flags, button sizes, images etc.)
  ImGuiWindowFlags ImGuiWindowFlags =
    ImGuiWindowFlags_NoTitleBar |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoSavedSettings;

  SDLState() : keys(SDL_GetKeyboardState(nullptr)) {}
};

enum class GameScreen {
    Playing,
    MainMenu,
    PauseMenu,
    MultiPlayerOptionsMenu, // this menu will show host or client buttons
};

struct GameState {


  GameScreen currentView;

  std::array<std::vector<GameObject>, 2> layers;
  std::vector<GameObject> backgroundTiles;
  std::vector<GameObject> foregroundTiles;
  std::vector<GameObject> bullets;
  bool debugMode;

  int playerIndex;
  SDL_FRect mapViewport; // viewable part of map
  float bg2scroll, bg3scroll, bg4scroll;

  GameState(const SDLState &state): bg2scroll(0), bg3scroll(0), bg4scroll(0), currentView(GameScreen::MainMenu) {
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
    bool running;

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
    void runGameLoop();
    void runEventLoop(GameObject &player);



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

}