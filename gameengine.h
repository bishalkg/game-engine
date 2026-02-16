#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <format>
#include <array>
#include <filesystem>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <glm/glm.hpp>

#include "gameobject.h"
#include "game_server.h" // needs complete type for unique_ptr destructor
#include "game_client.h"
#include "level_manifest.cpp"
#include "ui_manager.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

#include <SDL3_mixer/SDL_mixer.h>

#include "tmx.h"
#include <algorithm>

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

namespace game_engine {

  // forward declare
  class GameClient;

  struct SDLState {
    SDL_Window *window;
    SDL_Renderer *renderer;
    int width, height, logW, logH;
    const bool *keys;
    bool fullscreen;

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

  // enum class GameView {
  //     Playing,
  //     MainMenu,
  //     PauseMenu,
  //     LevelLoading,
  //     InventoryMenu,
  //     GameOver,
  //     MultiPlayerOptionsMenu, // this menu will show host or client buttons
  // };

  struct GameState {

    GameState() = default;
    GameState(const GameState&) = delete;
    GameState& operator=(const GameState&) = delete;

    // std::atomic is neither copy- nor move-assignable, so provide a custom move
    GameState(GameState&& other) noexcept
      : m_loadProgress(other.m_loadProgress.load()),
        currentView(other.currentView),
        m_stateLastUpdatedAt(other.m_stateLastUpdatedAt),
        layers(std::move(other.layers)),
        bullets(std::move(other.bullets)),
        debugMode(other.debugMode),
        playerLayer(other.playerLayer),
        playerIndex(other.playerIndex),
        mapViewport(other.mapViewport),
        bg2scroll(other.bg2scroll),
        bg3scroll(other.bg3scroll),
        bg4scroll(other.bg4scroll)
    {}

    GameState& operator=(GameState&& other) noexcept {
      if (this != &other) {
        m_loadProgress.store(other.m_loadProgress.load());
        currentView         = other.currentView;
        m_stateLastUpdatedAt= other.m_stateLastUpdatedAt;
        layers              = std::move(other.layers);
        bullets             = std::move(other.bullets);
        debugMode           = other.debugMode;
        playerLayer         = other.playerLayer;
        playerIndex         = other.playerIndex;
        mapViewport         = other.mapViewport;
        bg2scroll           = other.bg2scroll;
        bg3scroll           = other.bg3scroll;
        bg4scroll           = other.bg4scroll;
      }
      return *this;
    }

      std::atomic<uint8_t> m_loadProgress = 100;

      UIManager::GameView currentView;

      uint64_t m_stateLastUpdatedAt; // when the gameState was last updated, by local or by server msg

      std::vector<std::vector<GameObject>> layers;
      // std::vector<GameObject> backgroundTiles;
      // std::vector<GameObject> foregroundTiles;
      std::vector<GameObject> bullets;
      bool debugMode;

      int playerLayer, playerIndex;
      SDL_FRect mapViewport; // viewable part of map
      float bg2scroll, bg3scroll, bg4scroll;

      GameState(const SDLState &state): bg2scroll(0), bg3scroll(0), bg4scroll(0), currentView(UIManager::GameView::MainMenu) {
        playerIndex = -1;
        mapViewport = SDL_FRect{
          .x = 0, .y = 0,
          .w = static_cast<float>(state.logW),
          .h = static_cast<float>(state.logH)
        };
        debugMode = false;
      }

      // get current player
      GameObject &player(size_t layer_idx_chars) { return layers[playerLayer][playerIndex]; }

      void setLevelLoadProgress(uint8_t progress) {m_loadProgress.store(progress); }
      uint8_t getLevelLoadProgress() { return m_loadProgress.load(); }

      bool evaluateGameOver() {

        GameObject& p = player(playerIndex);

        if (!p.grounded && p.position.y > 1500) {
          currentView = UIManager::GameView::GameOver;
          return true;
        }
        return false;

      };

      game_engine::NetGameStateSnapshot extractNetSnapshot() const {
        NetGameStateSnapshot snapshot;
        snapshot.m_stateLastUpdatedAt = m_stateLastUpdatedAt;

        for (size_t layerIdx = 0; layerIdx < layers.size(); ++layerIdx) {
            for (const auto& obj : layers[layerIdx]) {
              if (obj.dynamic) {
                NetGameObjectSnapshot s{};
                s.id = obj.id;
                s.layer = static_cast<uint32_t>(layerIdx);
                s.type = obj.objClass;
                s.position = obj.position;
                s.velocity = obj.velocity;
                s.acceleration = obj.acceleration;
                s.direction = obj.direction;
                s.maxSpeedX = obj.maxSpeedX;
                s.currentAnimation = static_cast<uint32_t>(obj.currentAnimation);
                s.grounded = obj.grounded;
                s.shouldFlash = obj.shouldFlash;
                s.spriteFrame = static_cast<uint32_t>(obj.spriteFrame);
                s.data = obj.data; // union to be handled in encodeNetGameStateSnapshot
                snapshot.m_gameObjects[{obj.objClass, obj.id}] = s;
              };
            };
        };

        for (const auto& obj : bullets) {
          // if (obj.dynamic) {
          NetGameObjectSnapshot s{};
          s.id = obj.id;
          s.layer = static_cast<uint32_t>(playerLayer);
          s.type = obj.objClass;
          s.position = obj.position;
          s.velocity = obj.velocity;
          s.acceleration = obj.acceleration;
          s.direction = obj.direction;
          s.maxSpeedX = obj.maxSpeedX;
          s.currentAnimation = static_cast<uint32_t>(obj.currentAnimation);
          s.grounded = obj.grounded;
          s.shouldFlash = obj.shouldFlash;
          s.spriteFrame = static_cast<uint32_t>(obj.spriteFrame);
          snapshot.m_gameObjects[{obj.objClass, obj.id}] = s;
          // }
        };

          return snapshot;
        };

  };

  // struct TileSetTextures {
  //   int firstGid;
  //   std::vector<SDL_Texture *> textures;
  // };

  struct EntityResources {
    SDL_Texture *texIdle, *texWalk, *texRun, *texSlide, *texAttack, *texJump, *texHit, *texDie,
          *texShoot, *texRunShoot, *texSlideShoot, *texRunAttack;
    std::vector<Animation> anims;
  };

  struct Level {
    LevelIndex lvlIdx;
    // std::string name;
    // std::string mapPath;
    // std::string backgroundMusicPath;
    std::unique_ptr<tmx::Map> map;
    std::unordered_map<SpriteType, EntityResources> texCharacterMap;
    int bg1Idx, bg2Idx, bg3Idx, bg4Idx; // indexes of where in the map are the bg tilsets
    std::vector<SDL_Texture*> textures; // store textures to cleanup later
    MIX_Audio *backgroundAudio{nullptr};
    MIX_Track *backgroundTrack{nullptr};
    MIX_Audio *gameOverAudio{nullptr};
    MIX_Track *gameOverAudioTrack{nullptr};
    MIX_Audio *audioStep;
    MIX_Track *stepTrack;

    Level(LevelIndex lvl): lvlIdx(lvl) {};

    SDL_Texture *loadTexture(SDL_Renderer *renderer, const std::string &filepath){
      SDL_Texture *tex = IMG_LoadTexture(renderer, filepath.c_str()); // textures on gpu, surface in cpu memory (we can access)
      if (!tex) {
        SDL_Log("loadTexture failed for '%s': %s", filepath.c_str(), SDL_GetError());
        return nullptr;
    }
      SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST); // scale so pixels aren't blended;
      textures.push_back(tex);
      return tex;
    }
  };

  struct Resources {


    Resources(game_engine::SDLState& sdl) : m_uiManager(sdl) {};

    // const int ANIM_PLAYER_IDLE = 0;
    // const int ANIM_PLAYER_RUN = 1;
    // const int ANIM_PLAYER_SLIDE = 2;
    // const int ANIM_PLAYER_SHOOT = 3;
    // const int ANIM_PLAYER_SLIDE_SHOOT = 4;
    // const int ANIM_PLAYER_SWING = 5;
    // const int ANIM_PLAYER_JUMP = 6;
    // std::vector<Animation> playerAnims;
    const int ANIM_IDLE = 0;
    const int ANIM_RUN = 1;
    const int ANIM_SLIDE = 2;
    const int ANIM_SHOOT = 3;
    const int ANIM_SLIDE_SHOOT = 4;
    const int ANIM_SWING = 5;
    const int ANIM_JUMP = 6;
    const int ANIM_WALK = 7;
    const int ANIM_HIT = 8;
    const int ANIM_DIE = 9;
    const int ANIM_RUN_ATTACK = 10;

    const int ANIM_MAIN_MENU = 0;

    const int ANIM_BULLET_MOVING = 0;
    const int ANIM_BULLET_HIT = 1;
    std::vector<Animation> bulletAnims;

    std::vector<SDL_Texture*> textures; // store vector of pointers so we can delete later

    SDL_Texture  *texBullet, *texBulletHit; // tex of bullets

    SDL_Texture *texMainMenu;
    Animation mainMenuAnim;
    MIX_Track *mainMenuTrack;

    // ----- AudioManager
    // TODO track is for long running music. Audio is for one time sound effects.
    // but track can be set to not replay. audio is fire and forget, no state kept.
    // Create an AudioManager to handle all the below...
    MIX_Audio *audioShoot, *audioSword1, *audioShootHit, *audioBoneImpact, *audioProjectileEnemyHit, *audioEnemyDie;
    MIX_Track *shootTrack, *sword1Track, *hitTrack, *boneImpactHitTrack, *enemyProjectileHitTrack, *enemyDieTrack;

    // MIX_Audio *audioStepGrass;
    // MIX_Track *stepGrassTrack;
    MIX_Audio *audioJump;
    MIX_Track *jumpTrack;

    // std::vector<MIX_Track*> audioTracks;
    float m_masterAudioGain = 0;
    MIX_Mixer* mixer;
    size_t projectileTrackIdx = 0; // initialize once
    Timer whooshCooldown{0.25f};  // 100 ms. needs to be here and not on each projectile entity
    Timer stepAudioCooldown{0.25f};  // 100 ms. needs to be here and not on each projectile entity

    // ------- AudioManager^


    UIManager::UI_Manager m_uiManager;

    std::unique_ptr<Level> m_currLevel;
    std::vector<UIManager::Scene> mainMenuCutscene;
    LevelIndex m_currLevelIdx;

    // cutscenes
    std::vector<UIManager::Scene> testCutscene;
    SDL_Texture *texTestCutscene;
    Animation testCusceneAnim;

    // cutscenes
    std::vector<UIManager::Scene> pauseMenuScene;
    SDL_Texture *texPauseMenu;
    Animation pauseMenuAnim;

    // Resources() {
    //   m_uiManager = std::make_unique<UIManager::UI_Manager>();
    // };

    std::pair<MIX_Audio*, MIX_Track*> loadAudioChunk(const std::string& filepath, float gain = 1.0f) {

      if (!mixer) return {nullptr, nullptr};

      // then in this func load the audio
      MIX_Audio* audio = MIX_LoadAudio(mixer, filepath.c_str(), false);
      if (!audio) return {nullptr, nullptr};
      // audioBuff.push_back(audio);

      // need one for EACH sound that will be played; TODO might not need track for one time sounds?
      MIX_Track* track = MIX_CreateTrack(mixer);
      if (!track) return {nullptr, nullptr};
      // audioTracks.push_back(track);

      MIX_SetTrackAudio(track, audio);

      MIX_SetTrackGain(track, gain);

      // return the audio, and the track to set
      return {audio, track};

    };


    bool loadLevel(const LevelIndex levelId, SDLState &state, GameState &gs, float masterAudioGain, bool headless) {

      unloadLevel();
      gs.setLevelLoadProgress(20);

      m_currLevelIdx = levelId;
      m_currLevel = std::make_unique<Level>(levelId);
      LevelAssets& assets = LEVEL_CONFIG.at(levelId);

      m_currLevel->map = tmx::loadMap(assets.mapPath); // only the resource struct instance can hold this pointer and it will be automatically deleted when not used (eg. when we swap out maps)
      gs.setLevelLoadProgress(40);
      if (!headless) {
        int i = 0;
        for (tmx::TileSet &tileSet: m_currLevel->map->tileSets)
        {
          // each tileSet already loads in each texture, including the background textures we need; need to set onto *texBg1, *texBg2, *texBg3, *texBg4
          const std::string imagePath = "data/tiles/" + std::filesystem::path(tileSet.image.source).filename().string();
          tileSet.texture = m_currLevel->loadTexture(state.renderer, imagePath);
          // tilesetTextures.push_back(&tileSet); // need to fix for shutdown
          // Skyx32 (3) , Flora1x32 (2), Flora2x32 (1)
          if (imagePath.find(assets.background4PathName) != std::string::npos) {
            m_currLevel->bg4Idx = i;
          } else if (imagePath.find(assets.background3PathName) != std::string::npos) {
            m_currLevel->bg3Idx = i;
          } else if (imagePath.find(assets.background2PathName) != std::string::npos) {
            m_currLevel->bg2Idx = i;
          } else if (imagePath.find(assets.background1PathName) != std::string::npos) {
            m_currLevel->bg1Idx = i;
          }
          i++;
        }
      }

      gs.setLevelLoadProgress(50);
      std::sort(m_currLevel->map->tileSets.begin(), m_currLevel->map->tileSets.end(),
      [](const tmx::TileSet& a, const tmx::TileSet& b) {
          return a.firstgid < b.firstgid;
      });

      // load enemies for this level
      if (!headless) {

        for (const SpriteType& character: assets.enemyTypes) {

          SpriteAssets& spriteAssets = ENEMY_CONFIG.at(character);

          m_currLevel->texCharacterMap[character].texIdle = m_currLevel->loadTexture(state.renderer,  spriteAssets.paths.idleTex);
          m_currLevel->texCharacterMap[character].texWalk = m_currLevel->loadTexture(state.renderer, spriteAssets.paths.walkTex);
          m_currLevel->texCharacterMap[character].texRun = m_currLevel->loadTexture(state.renderer, spriteAssets.paths.runTex);
          m_currLevel->texCharacterMap[character].texAttack = m_currLevel->loadTexture(state.renderer,  spriteAssets.paths.attackTex);
          m_currLevel->texCharacterMap[character].texHit = m_currLevel->loadTexture(state.renderer,  spriteAssets.paths.hitTex);
          m_currLevel->texCharacterMap[character].texDie = m_currLevel->loadTexture(state.renderer,  spriteAssets.paths.dieTex);

          m_currLevel->texCharacterMap[character].anims.resize(10);
          auto [idleFrames, idleSeconds] = spriteAssets.animSettings.at(ANIM_IDLE);
          m_currLevel->texCharacterMap[character].anims[ANIM_IDLE] = Animation(idleFrames, idleSeconds);

          auto [runFrames, runSeconds] = spriteAssets.animSettings.at(ANIM_RUN);
          m_currLevel->texCharacterMap[character].anims[ANIM_RUN] = Animation(runFrames, runSeconds);

          auto [hitFrames, hitSeconds] = spriteAssets.animSettings.at(ANIM_HIT);
          m_currLevel->texCharacterMap[character].anims[ANIM_HIT] = Animation(hitFrames, hitSeconds);

          auto [dieFrames, dieSeconds] = spriteAssets.animSettings.at(ANIM_DIE);
          m_currLevel->texCharacterMap[character].anims[ANIM_DIE] = Animation(dieFrames, dieSeconds);

          auto [attackFrames, attackSeconds] = spriteAssets.animSettings.at(ANIM_SWING);
          m_currLevel->texCharacterMap[character].anims[ANIM_SWING] = Animation(attackFrames, attackSeconds);
          // texCharacterMap[SpriteType::Minotaur_1].anims[ANIM_JUMP] = Animation(6, 0.5f);

        }
      }

      gs.setLevelLoadProgress(60);
      // load player assets. NEED TO MOVE THIS TO BE GLOBAL SO WE DONT RE CREATE EACH LEVEL
      if (!headless) {

        for (auto const [character, spriteAssets]: PLAYER_CONFIG) {

          m_currLevel->texCharacterMap[character].texIdle = m_currLevel->loadTexture(state.renderer,  spriteAssets.paths.idleTex);
          m_currLevel->texCharacterMap[character].texWalk = m_currLevel->loadTexture(state.renderer, spriteAssets.paths.walkTex);
          m_currLevel->texCharacterMap[character].texRun = m_currLevel->loadTexture(state.renderer, spriteAssets.paths.runTex);
          m_currLevel->texCharacterMap[character].texAttack = m_currLevel->loadTexture(state.renderer,  spriteAssets.paths.attackTex);
          m_currLevel->texCharacterMap[character].texRunAttack = m_currLevel->loadTexture(state.renderer,  spriteAssets.paths.runAttackTex);
          m_currLevel->texCharacterMap[character].texHit = m_currLevel->loadTexture(state.renderer,  spriteAssets.paths.hitTex);
          m_currLevel->texCharacterMap[character].texDie = m_currLevel->loadTexture(state.renderer,  spriteAssets.paths.dieTex);
          m_currLevel->texCharacterMap[character].texShoot = m_currLevel->loadTexture(state.renderer,  spriteAssets.paths.shootTex);
          m_currLevel->texCharacterMap[character].texSlide = m_currLevel->loadTexture(state.renderer,  spriteAssets.paths.slideTex);
          m_currLevel->texCharacterMap[character].texRunShoot = m_currLevel->loadTexture(state.renderer,  spriteAssets.paths.runShootTex);
          m_currLevel->texCharacterMap[character].texSlideShoot = m_currLevel->loadTexture(state.renderer,  spriteAssets.paths.slideShootTex);
          m_currLevel->texCharacterMap[character].texJump = m_currLevel->loadTexture(state.renderer,  spriteAssets.paths.jumpTex);

          // TODO THIS IS SIZE OF ANIMS
          m_currLevel->texCharacterMap[character].anims.resize(11);
          auto [idleFrames, idleSeconds] = spriteAssets.animSettings.at(ANIM_IDLE);
          m_currLevel->texCharacterMap[character].anims[ANIM_IDLE] = Animation(idleFrames, idleSeconds);

          auto [runFrames, runSeconds] = spriteAssets.animSettings.at(ANIM_RUN);
          m_currLevel->texCharacterMap[character].anims[ANIM_RUN] = Animation(runFrames, runSeconds);

          auto [runAttackFrames, runAttackSeconds] = spriteAssets.animSettings.at(ANIM_RUN_ATTACK);
          m_currLevel->texCharacterMap[character].anims[ANIM_RUN_ATTACK] = Animation(runAttackFrames, runAttackSeconds);

          auto [slideFrames, slideSeconds] = spriteAssets.animSettings.at(ANIM_SLIDE);
          m_currLevel->texCharacterMap[character].anims[ANIM_SLIDE] = Animation(slideFrames, slideSeconds);

          auto [shootFrames, shootSeconds] = spriteAssets.animSettings.at(ANIM_SHOOT);
          m_currLevel->texCharacterMap[character].anims[ANIM_SHOOT] = Animation(shootFrames, shootSeconds, 0, true);

          auto [slideShootFrames, slideShootSeconds] = spriteAssets.animSettings.at(ANIM_SLIDE_SHOOT);
          m_currLevel->texCharacterMap[character].anims[ANIM_SLIDE_SHOOT] = Animation(slideShootFrames, slideShootSeconds, 0, true);

          auto [hitFrames, hitSeconds] = spriteAssets.animSettings.at(ANIM_HIT);
          m_currLevel->texCharacterMap[character].anims[ANIM_HIT] = Animation(hitFrames, hitSeconds);

          auto [dieFrames, dieSeconds] = spriteAssets.animSettings.at(ANIM_DIE);
          m_currLevel->texCharacterMap[character].anims[ANIM_DIE] = Animation(dieFrames, dieSeconds);

          auto [attackFrames, attackSeconds] = spriteAssets.animSettings.at(ANIM_SWING);
          m_currLevel->texCharacterMap[character].anims[ANIM_SWING] = Animation(attackFrames, attackSeconds);

          auto [jumpFrames, jumpSeconds] = spriteAssets.animSettings.at(ANIM_JUMP);
          m_currLevel->texCharacterMap[character].anims[ANIM_JUMP] = Animation(jumpFrames, jumpSeconds, 2);

        }
      }

      auto [backgroundAudio, backgroundTrack] = loadAudioChunk(assets.backgroundAudioPath, masterAudioGain);

      auto [gameOverAudio, gameOverAudioTrack] = loadAudioChunk(assets.gameOverAudioPath, masterAudioGain);

      auto [stepAudio, stepTrack] = loadAudioChunk(assets.stepAudioPath, masterAudioGain);

      m_currLevel->backgroundAudio = backgroundAudio;
      m_currLevel->backgroundTrack = backgroundTrack;
      m_currLevel->gameOverAudio = gameOverAudio;
      m_currLevel->gameOverAudioTrack = gameOverAudioTrack;
      m_currLevel->stepTrack = stepTrack;
      m_currLevel->audioStep = stepAudio;

      // this->lvl = std::move(lvl);
      return true;
    };

    void unloadLevel() {

      if (!m_currLevel) {
        return;
      }

      if (m_currLevel->backgroundTrack && MIX_TrackPlaying(m_currLevel->backgroundTrack)) {
        MIX_StopTrack(m_currLevel->backgroundTrack, 0);
      }

      if (m_currLevel->backgroundTrack) { MIX_DestroyTrack(m_currLevel->backgroundTrack); }
      if (m_currLevel->backgroundAudio) { MIX_DestroyAudio(m_currLevel->backgroundAudio); }

      for (SDL_Texture *tex : textures) {
        if (tex) {
          SDL_DestroyTexture(tex);
        }
      }
      m_currLevel->textures.clear();

      // deletes the owned obj and sets to nullptr
      m_currLevel->map.reset();
      m_currLevel.reset();


    };

    // headless is for server resources state
    void loadAllAssets(SDLState &state, GameState &gs, bool headless){

      m_masterAudioGain = MIX_GetMasterGain(mixer);
      float chunkAudioGain = m_masterAudioGain * 3;

      std::tie(audioShoot, shootTrack) = loadAudioChunk("data/audio/fireball_whoosh.mp3", chunkAudioGain);
      std::tie(audioSword1, sword1Track) = loadAudioChunk("data/audio/sword/sword_swing_1.mp3", chunkAudioGain);
      std::tie(audioShootHit, hitTrack) = loadAudioChunk("data/audio/fireball_hit.mp3", chunkAudioGain);
      std::tie(audioBoneImpact, boneImpactHitTrack) = loadAudioChunk("data/audio/impact/bone_impact.mp3", chunkAudioGain);
      std::tie(audioProjectileEnemyHit, enemyProjectileHitTrack) = loadAudioChunk("data/audio/fireball_hit.mp3", chunkAudioGain);
      std::tie(audioEnemyDie, enemyDieTrack) = loadAudioChunk("data/audio/monster_die.wav", chunkAudioGain);
      std::tie(audioJump, jumpTrack) = loadAudioChunk("data/audio/movement/jump.wav", chunkAudioGain);



      // load level specific assets
      bool lvlLoaded = loadLevel(LevelIndex::LEVEL_1, state, gs, m_masterAudioGain, headless);

      if (!lvlLoaded) {
        static_assert("level failed to load");
        return;
      }

      // load global assets, need to more players as globalResources
      bulletAnims.resize(2);
      bulletAnims[ANIM_BULLET_MOVING] = Animation(9, 1.0f);
      bulletAnims[ANIM_BULLET_HIT] = Animation(4, 0.15f);
      if (!headless) {
        // texBullet = loadTexture(state.renderer, "data/bullet.png");
        // texBulletHit = loadTexture(state.renderer, "data/bullet_hit.png");
        texBulletHit = m_currLevel->loadTexture(state.renderer, "data/players/Mage/Charge_1.png");
        texBullet = m_currLevel->loadTexture(state.renderer, "data/players/Mage/Charge_1.png");
        texMainMenu = m_currLevel->loadTexture(state.renderer, "data/maps/title_screen/title_screen_sized.png");
        mainMenuAnim = Animation(58, 7.0f);
        auto [mainMenuAudio, mainMenuTrack] = loadAudioChunk("data/audio/22. Banners in the Wind.wav", chunkAudioGain);
        this->mainMenuTrack = mainMenuTrack;
        // TODO make scene from level manifest
        mainMenuCutscene = {
          UIManager::Scene{
          .tex = texMainMenu,
          .anim = &mainMenuAnim,
          .scale = 1.2,
          .numFrameColumns = 10,
          .frameH = 540.0f,
          .frameW = 800.0f,
          .yOffset = -50,
          .loopScene = true,
          }
        };


        texTestCutscene = m_currLevel->loadTexture(state.renderer, "data/cutscenes/text_test.png");
        testCusceneAnim = Animation(6, 1.0f, 0, true);
        testCutscene = {
          UIManager::Scene{
          .tex = texTestCutscene,
          .anim = &testCusceneAnim,
          .scale = 1.0,
          .numFrameColumns = 3,
          .frameH = 360.0f,
          .frameW = 640.0f,
          .loopScene = false,
          }
        };

        texPauseMenu = m_currLevel->loadTexture(state.renderer, "data/cutscenes/menu/pause_menu.png");
        pauseMenuAnim = Animation(1, 1.0f, 0, true);
        pauseMenuScene = {
          UIManager::Scene{
          .tex = texPauseMenu,
          .anim = &pauseMenuAnim,
          .scale = 1.0,
          .numFrameColumns = 1,
          .frameH = 360.0f,
          .frameW = 640.0f,
          .loopScene = false,
          }
        };
      }



    }

    // TODO: unload global resources and currLevel resources
    void unload() {
      // for (SDL_Texture *tex : textures) {
      //   SDL_DestroyTexture(tex);
      // }


      unloadLevel(); // todo need to fix
      // // cleanup audio
      // for (MIX_Audio* chunk: audioBuff) {

      // // destroy chunks
      //   MIX_DestroyAudio(chunk);
      // };

      // for (MIX_Track* track: audioTracks) {
      //   MIX_DestroyTrack(track);
      // }

    };

  };


  /*
  Engine is the main class that provides all the functionality to run our game
  */
  class Engine
  {
    enum GameRunMode {
      SinglePlayer, // default to single player
      Host,
      Client,
    };

    private:
      SDLState m_sdlState;
      GameState m_gameState;
      Resources m_resources;
      std::atomic<bool> m_gameRunning{false};
      std::mutex m_levelMutex;
      GameRunMode m_gameType;

      std::unique_ptr<GameServer> m_gameServer = nullptr; // only create if gameType==Host
      bool isConnectedToServer = false;
      std::unique_ptr<GameClient> m_gameClient = nullptr; // only create if gameType!=SinglePlayer
      std::thread m_serverLoopThd;
      std::thread m_levelLoadThd;


    public:
      Engine() : m_sdlState{}, m_gameState(m_sdlState), m_resources{m_sdlState}, m_gameType(SinglePlayer) {
      }
      ~Engine();

      inline static constexpr glm::vec2 GRAVITY = glm::vec2(0, 600);
      inline static constexpr size_t LAYER_IDX_LEVEL = 0;
      inline static constexpr size_t LAYER_IDX_CHARACTERS = 1;
      inline static constexpr int TILE_SIZE = 32;
      inline static constexpr float JUMP_FORCE = -400.0f;


      bool init(int width, int height, int logW, int logH);
      void runGameLoop();
      void runEventLoop(GameObject &player, game_engine::NetGameInput &input, UIManager::UISnapshots &snaps);

      bool initWindowAndRenderer(int width, int height, int logW, int logH);
      void cleanupTextures();
      void cleanup(); // can be ref or pointer; if using pointer, need to use -> instead of .
      void drawObject(GameObject &obj, float height, float width, float deltaTime);
      void updateGameObject(GameObject &obj, float deltaTime);
      bool initAllTiles(GameState &gameState);
      // const tmx::TileSet* pickTileset(uint32_t gid);

      void handleCollision(GameObject &a, GameObject &b, float deltaTime);
      void collisionResponse(const SDL_FRect &rectA, const SDL_FRect &rectB, const SDL_FRect &rectC, GameObject &objA, GameObject &objB, float deltaTime);

      void handleKeyInput(GameObject &obj, SDL_Scancode key, bool keyDown, game_engine::NetGameInput &input);

      void setBackgroundSoundtrack();
      void stopBackgroundSoundtrack();
      void setGameOverSoundtrack();
      void stopGameOverSoundtrack();
      void setAudioSoundtrack(MIX_Track* track); // generics
      void stopAudioSoundtrack(MIX_Track* track);

      void updateGameplayState(float deltaTime, GameObject& player, UIManager::UIActions& actions);
      void updateAllObjects(float deltaTime);
      void updateMapViewport(GameObject& player);
      void drawAllObjects(float deltaTime, UIManager::UIActions& actions);
      void drawParalaxBackground(SDL_Texture *texture, float xVelocity, float &scrollPos, float scrollFactor, float deltaTime, float y);
      UIManager::UIActions updateUI(UIManager::UI_Manager& uiManager, float deltaTime, UIManager::UISnapshots &snaps);
      void applyUIActions(const UIManager::UIActions& a);
      // void clearRenderer();
      // void renderUpdates();

      void buildAuthoritativeStateForServer();
      bool handleMultiplayerConnections();
      void runGameServerLoopThread();
      void initNextLevel(LevelIndex lvl);
      void asyncSwitchToLevel(LevelIndex lvl);
      // MIX_PauseTrack(track) / MIX_ResumeTrack(track)

      // getters
      GameObject &getPlayer();
      SDLState &getSDLState();
      GameState &getGameState();
      Resources &getResources();

      // setters
      void setWindowSize(int height, int width);

  };

}
