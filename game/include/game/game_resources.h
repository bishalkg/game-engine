#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "engine/gameobject.h"
#include "engine/level_types.h"
#include "engine/tmx.h"
#include "engine/ui_manager.h"

namespace game_engine {
struct SDLState;
struct GameState;
}

namespace game {

  struct ProgressionProfile;

struct EntityResources {
  SDL_Texture *texIdle{}, *texWalk{}, *texRun{}, *texSlide{}, *texAttack{}, *texJump{}, *texHit{},
    *texDie{}, *texShoot{}, *texRunShoot{}, *texSlideShoot{}, *texRunAttack{}, *texAttack2{},
    *texUltimate{};
  std::vector<Animation> anims;
};

struct Level {
  LevelIndex lvlIdx;
  std::unique_ptr<tmx::Map> map;
  std::unordered_map<SpriteType, EntityResources> texCharacterMap;
  int bg1Idx{};
  int bg2Idx{};
  int bg3Idx{};
  int bg4Idx{};
  std::vector<SDL_Texture*> textures;
  std::vector<UIManager::Cutscene> cutscenes;
  MIX_Audio* backgroundAudio{nullptr};
  MIX_Track* backgroundTrack{nullptr};
  MIX_Audio* gameOverAudio{nullptr};
  MIX_Track* gameOverAudioTrack{nullptr};
  MIX_Audio* audioStep{nullptr};
  MIX_Track* stepTrack{nullptr};

  explicit Level(LevelIndex lvl);
  SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& filepath);
};

struct GameResources {
  GameResources(game_engine::SDLState& sdl, TTF_Font* font, MIX_Mixer* mixer);
  ~GameResources();

  GameResources(const GameResources&) = delete;
  GameResources& operator=(const GameResources&) = delete;
  GameResources(GameResources&&) = delete;
  GameResources& operator=(GameResources&&) = delete;

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
  const int ANIM_SWING_2 = 11;
  const int ANIM_ULTIMATE = 12;

  const int ANIM_MAIN_MENU = 0;

  const int ANIM_BULLET_MOVING = 0;
  const int ANIM_BULLET_HIT = 1;
  std::vector<Animation> bulletAnims;

  std::vector<SDL_Texture*> textures;
  SDL_Texture* texBullet{};
  SDL_Texture* texBulletHit{};

  SDL_Texture* texMainMenu{};
  std::shared_ptr<Animation> mainMenuAnim;
  MIX_Audio* mainMenuAudio{nullptr};
  MIX_Track* mainMenuTrack{nullptr};
  std::vector<UIManager::Cutscene> mainMenuCutscene;

  SDL_Texture* texCharSelect{};
  std::shared_ptr<Animation> charSelectAnim;
  std::vector<UIManager::Cutscene> characterSelectScene;

  MIX_Audio *audioShoot{}, *audioSword1{}, *audioUltimateAttack{}, *audioShootHit{}, *audioBoneImpact{},
    *audioProjectileEnemyHit{}, *audioEnemyDie{};
  MIX_Track *shootTrack{}, *sword1Track{}, *ultimateAttackTrack{}, *hitTrack{}, *boneImpactHitTrack{},
    *enemyProjectileHitTrack{}, *enemyDieTrack{};

  MIX_Audio* audioJump{};
  MIX_Track* jumpTrack{};

  float m_masterAudioGain = 0.0f;
  MIX_Mixer* mixer = nullptr;
  size_t projectileTrackIdx = 0;
  Timer whooshCooldown{0.25f};
  Timer stepAudioCooldown{0.25f};

  TTF_Font* font = nullptr;
  UIManager::UI_Manager m_uiManager;

  std::unique_ptr<Level> m_currLevel;
  LevelIndex m_currLevelIdx{LevelIndex::LEVEL_1};

  std::vector<UIManager::Cutscene> testCutscene;
  SDL_Texture* texTestCutscene{};
  Animation testCusceneAnim;
  SDL_Texture* texTestCutscene2{};
  Animation testCusceneAnim2;

  std::vector<UIManager::Cutscene> pauseMenuScene;
  SDL_Texture* texPauseMenu{};
  std::shared_ptr<Animation> pauseMenuAnim;

  std::pair<MIX_Audio*, MIX_Track*> loadAudioChunk(const std::string& filepath, float gain = 1.0f);

  SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& filepath);

  bool loadLevel(
    LevelIndex levelId,
    game_engine::SDLState& state,
    game_engine::GameState& gs,
    float masterAudioGain,
    bool headless);

  void unloadLevel();

  void loadAllAssets(game_engine::SDLState& state, game_engine::GameState& gs, const ProgressionProfile& profile, bool headless);

  void unload();
};

} // namespace game
