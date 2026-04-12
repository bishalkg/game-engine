#include "game/game_resources.h"

#include <algorithm>
#include <filesystem>

#include "engine/engine.h"

namespace game {

Level::Level(LevelIndex lvl) : lvlIdx(lvl) {}

SDL_Texture* Level::loadTexture(SDL_Renderer* renderer, const std::string& filepath) {
  SDL_Texture* tex = IMG_LoadTexture(renderer, filepath.c_str());
  if (!tex) {
    SDL_Log("loadTexture failed for '%s': %s", filepath.c_str(), SDL_GetError());
    return nullptr;
  }
  SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
  textures.push_back(tex);
  return tex;
}

GameResources::GameResources(game_engine::SDLState& sdl, TTF_Font* fontIn, MIX_Mixer* mixerIn)
  : mixer(mixerIn), font(fontIn), m_uiManager(sdl, *fontIn) {}

GameResources::~GameResources() {
  unload();
}

std::pair<MIX_Audio*, MIX_Track*> GameResources::loadAudioChunk(
  const std::string& filepath,
  float gain) {
  if (!mixer) {
    return {nullptr, nullptr};
  }

  MIX_Audio* audio = MIX_LoadAudio(mixer, filepath.c_str(), false);
  if (!audio) {
    return {nullptr, nullptr};
  }

  MIX_Track* track = MIX_CreateTrack(mixer);
  if (!track) {
    MIX_DestroyAudio(audio);
    return {nullptr, nullptr};
  }

  MIX_SetTrackAudio(track, audio);
  MIX_SetTrackGain(track, gain);
  return {audio, track};
}

SDL_Texture* GameResources::loadTexture(SDL_Renderer* renderer, const std::string& filepath) {
  SDL_Texture* tex = IMG_LoadTexture(renderer, filepath.c_str());
  if (!tex) {
    SDL_Log("loadTexture failed for '%s': %s", filepath.c_str(), SDL_GetError());
    return nullptr;
  }
  SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
  textures.push_back(tex);
  return tex;
}

bool GameResources::loadLevel(
  const LevelIndex levelId,
  game_engine::SDLState& state,
  game_engine::GameState& gs,
  float masterAudioGain,
  bool headless) {
  unloadLevel();
  gs.setLevelLoadProgress(20);

  m_currLevelIdx = levelId;
  m_currLevel = std::make_unique<Level>(levelId);
  gs.currentLevelId = levelId;
  const LevelAssets& assets = LEVEL_CONFIG.at(levelId);

  m_currLevel->map = tmx::loadMap(assets.mapPath);
  if (!m_currLevel->map) {
    const char* basePathRaw = SDL_GetBasePath();
    if (basePathRaw && *basePathRaw) {
      std::filesystem::path basePath(basePathRaw);
      basePath = basePath.lexically_normal();
      if (basePath.filename().empty()) {
        basePath = basePath.parent_path();
      }
      const std::filesystem::path resourcesPath =
        (basePath.filename() == "MacOS" ? basePath.parent_path() : basePath.parent_path().parent_path())
        / "Resources";
      const std::filesystem::path fallbackMapPath = resourcesPath / assets.mapPath;
      if (std::filesystem::exists(fallbackMapPath)) {
        m_currLevel->map = tmx::loadMap(fallbackMapPath.string());
      }
    }
  }
  if (!m_currLevel->map) {
    std::error_code ec;
    const std::string cwd = std::filesystem::current_path(ec).string();
    SDL_Log("Failed to load map: '%s' (cwd: '%s')", assets.mapPath.c_str(), cwd.c_str());
    return false;
  }
  gs.setLevelLoadProgress(40);

  if (!headless) {
    for (const auto& csa : assets.cutsceneData) {
      m_currLevel->cutscenes.emplace_back(UIManager::Cutscene{
        .tex = m_currLevel->loadTexture(state.renderer, csa.texPath),
        .anim = std::make_shared<Animation>(csa.animSetting.first, csa.animSetting.second, 0, true),
        .scale = csa.scale,
        .numFrameColumns = csa.numFrameColumns,
        .frameH = csa.frameH,
        .frameW = csa.frameW,
        .loopScene = csa.loopScene,
        .dialogue = csa.dialogue,
        .yOffset = csa.yOffset,
        .xOffset = csa.xOffset,
      });
    }
  }

  if (!headless) {
    int i = 0;
    for (tmx::TileSet& tileSet : m_currLevel->map->tileSets) {
      const std::string imagePath =
        "data/tiles/" + std::filesystem::path(tileSet.image.source).filename().string();
      tileSet.texture = m_currLevel->loadTexture(state.renderer, imagePath);
      if (imagePath.find(assets.background4PathName) != std::string::npos) {
        m_currLevel->bg4Idx = i;
      } else if (imagePath.find(assets.background3PathName) != std::string::npos) {
        m_currLevel->bg3Idx = i;
      } else if (imagePath.find(assets.background2PathName) != std::string::npos) {
        m_currLevel->bg2Idx = i;
      } else if (imagePath.find(assets.background1PathName) != std::string::npos) {
        m_currLevel->bg1Idx = i;
      }
      ++i;
    }
  }

  gs.setLevelLoadProgress(50);
  std::sort(
    m_currLevel->map->tileSets.begin(),
    m_currLevel->map->tileSets.end(),
    [](const tmx::TileSet& a, const tmx::TileSet& b) { return a.firstgid < b.firstgid; });

  for (const SpriteType& character : assets.enemyTypes) {
    const SpriteAssets& spriteAssets = ENEMY_CONFIG.at(character);

    if (!headless) {
      m_currLevel->texCharacterMap[character].texIdle =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.idleTex);
      m_currLevel->texCharacterMap[character].texWalk =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.walkTex);
      m_currLevel->texCharacterMap[character].texRun =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.runTex);
      m_currLevel->texCharacterMap[character].texAttack =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.attackTex);
      m_currLevel->texCharacterMap[character].texHit =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.hitTex);
      m_currLevel->texCharacterMap[character].texDie =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.dieTex);
    }

    m_currLevel->texCharacterMap[character].anims.resize(10);
    auto [idleFrames, idleSeconds] = spriteAssets.animSettings.at(ANIM_IDLE);
    m_currLevel->texCharacterMap[character].anims[ANIM_IDLE] =
      Animation(idleFrames, idleSeconds);

    auto [runFrames, runSeconds] = spriteAssets.animSettings.at(ANIM_RUN);
    m_currLevel->texCharacterMap[character].anims[ANIM_RUN] =
      Animation(runFrames, runSeconds);

    auto [hitFrames, hitSeconds] = spriteAssets.animSettings.at(ANIM_HIT);
    m_currLevel->texCharacterMap[character].anims[ANIM_HIT] =
      Animation(hitFrames, hitSeconds);

    auto [dieFrames, dieSeconds] = spriteAssets.animSettings.at(ANIM_DIE);
    m_currLevel->texCharacterMap[character].anims[ANIM_DIE] =
      Animation(dieFrames, dieSeconds);

    auto [attackFrames, attackSeconds] = spriteAssets.animSettings.at(ANIM_SWING);
    m_currLevel->texCharacterMap[character].anims[ANIM_SWING] =
      Animation(attackFrames, attackSeconds);
  }

  gs.setLevelLoadProgress(60);
  for (const auto& [character, spriteAssets] : SPRITE_CONFIG) {
    if (!headless) {
      m_currLevel->texCharacterMap[character].texIdle =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.idleTex);
      m_currLevel->texCharacterMap[character].texWalk =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.walkTex);
      m_currLevel->texCharacterMap[character].texRun =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.runTex);
      m_currLevel->texCharacterMap[character].texAttack =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.attackTex);
      m_currLevel->texCharacterMap[character].texRunAttack =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.runAttackTex);
      m_currLevel->texCharacterMap[character].texAttack2 =
        spriteAssets.paths.attackTex2.empty()
          ? nullptr
          : m_currLevel->loadTexture(state.renderer, spriteAssets.paths.attackTex2);
      m_currLevel->texCharacterMap[character].texHit =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.hitTex);
      m_currLevel->texCharacterMap[character].texDie =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.dieTex);
      m_currLevel->texCharacterMap[character].texShoot =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.shootTex);
      m_currLevel->texCharacterMap[character].texSlide =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.slideTex);
      m_currLevel->texCharacterMap[character].texRunShoot =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.runShootTex);
      m_currLevel->texCharacterMap[character].texSlideShoot =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.slideShootTex);
      m_currLevel->texCharacterMap[character].texJump =
        m_currLevel->loadTexture(state.renderer, spriteAssets.paths.jumpTex);
      m_currLevel->texCharacterMap[character].texUltimate =
        spriteAssets.paths.ultimateTex.empty()
          ? nullptr
          : m_currLevel->loadTexture(state.renderer, spriteAssets.paths.ultimateTex);
    }

    m_currLevel->texCharacterMap[character].anims.resize(13);
    auto [idleFrames, idleSeconds] = spriteAssets.animSettings.at(ANIM_IDLE);
    m_currLevel->texCharacterMap[character].anims[ANIM_IDLE] =
      Animation(idleFrames, idleSeconds);

    auto [runFrames, runSeconds] = spriteAssets.animSettings.at(ANIM_RUN);
    m_currLevel->texCharacterMap[character].anims[ANIM_RUN] =
      Animation(runFrames, runSeconds);

    auto [runAttackFrames, runAttackSeconds] = spriteAssets.animSettings.at(ANIM_RUN_ATTACK);
    m_currLevel->texCharacterMap[character].anims[ANIM_RUN_ATTACK] =
      Animation(runAttackFrames, runAttackSeconds);

    auto [slideFrames, slideSeconds] = spriteAssets.animSettings.at(ANIM_SLIDE);
    m_currLevel->texCharacterMap[character].anims[ANIM_SLIDE] =
      Animation(slideFrames, slideSeconds);

    auto [shootFrames, shootSeconds] = spriteAssets.animSettings.at(ANIM_SHOOT);
    m_currLevel->texCharacterMap[character].anims[ANIM_SHOOT] =
      Animation(shootFrames, shootSeconds, 0, true);

    auto [slideShootFrames, slideShootSeconds] = spriteAssets.animSettings.at(ANIM_SLIDE_SHOOT);
    m_currLevel->texCharacterMap[character].anims[ANIM_SLIDE_SHOOT] =
      Animation(slideShootFrames, slideShootSeconds, 0, true);

    auto [hitFrames, hitSeconds] = spriteAssets.animSettings.at(ANIM_HIT);
    m_currLevel->texCharacterMap[character].anims[ANIM_HIT] =
      Animation(hitFrames, hitSeconds);

    auto [dieFrames, dieSeconds] = spriteAssets.animSettings.at(ANIM_DIE);
    m_currLevel->texCharacterMap[character].anims[ANIM_DIE] =
      Animation(dieFrames, dieSeconds);

    auto [attackFrames, attackSeconds] = spriteAssets.animSettings.at(ANIM_SWING);
    m_currLevel->texCharacterMap[character].anims[ANIM_SWING] =
      Animation(attackFrames, attackSeconds);

    if (spriteAssets.animSettings.contains(ANIM_SWING_2)) {
      auto [attack2Frames, attack2Seconds] = spriteAssets.animSettings.at(ANIM_SWING_2);
      m_currLevel->texCharacterMap[character].anims[ANIM_SWING_2] =
        Animation(attack2Frames, attack2Seconds);
    }

    if (spriteAssets.animSettings.contains(ANIM_ULTIMATE)) {
      auto [ultimateFrames, ultimateSeconds] = spriteAssets.animSettings.at(ANIM_ULTIMATE);
      m_currLevel->texCharacterMap[character].anims[ANIM_ULTIMATE] =
        Animation(ultimateFrames, ultimateSeconds);
    }

    auto [jumpFrames, jumpSeconds] = spriteAssets.animSettings.at(ANIM_JUMP);
    m_currLevel->texCharacterMap[character].anims[ANIM_JUMP] =
      Animation(jumpFrames, jumpSeconds, 2);
  }

  auto [backgroundAudio, backgroundTrack] =
    loadAudioChunk(assets.backgroundAudioPath, masterAudioGain);
  auto [gameOverAudio, gameOverAudioTrack] =
    loadAudioChunk(assets.gameOverAudioPath, masterAudioGain);
  auto [stepAudio, stepTrack] = loadAudioChunk(assets.stepAudioPath, masterAudioGain);

  m_currLevel->backgroundAudio = backgroundAudio;
  m_currLevel->backgroundTrack = backgroundTrack;
  m_currLevel->gameOverAudio = gameOverAudio;
  m_currLevel->gameOverAudioTrack = gameOverAudioTrack;
  m_currLevel->stepTrack = stepTrack;
  m_currLevel->audioStep = stepAudio;

  return true;
}

void GameResources::unloadLevel() {
  if (!m_currLevel) {
    return;
  }

  if (m_currLevel->backgroundTrack && MIX_TrackPlaying(m_currLevel->backgroundTrack)) {
    MIX_StopTrack(m_currLevel->backgroundTrack, 0);
  }
  if (m_currLevel->gameOverAudioTrack && MIX_TrackPlaying(m_currLevel->gameOverAudioTrack)) {
    MIX_StopTrack(m_currLevel->gameOverAudioTrack, 0);
  }

  if (m_currLevel->backgroundTrack) {
    MIX_DestroyTrack(m_currLevel->backgroundTrack);
  }
  if (m_currLevel->backgroundAudio) {
    MIX_DestroyAudio(m_currLevel->backgroundAudio);
  }
  if (m_currLevel->gameOverAudioTrack) {
    MIX_DestroyTrack(m_currLevel->gameOverAudioTrack);
  }
  if (m_currLevel->gameOverAudio) {
    MIX_DestroyAudio(m_currLevel->gameOverAudio);
  }
  if (m_currLevel->stepTrack) {
    MIX_DestroyTrack(m_currLevel->stepTrack);
  }
  if (m_currLevel->audioStep) {
    MIX_DestroyAudio(m_currLevel->audioStep);
  }

  for (SDL_Texture* tex : m_currLevel->textures) {
    if (tex) {
      SDL_DestroyTexture(tex);
    }
  }
  m_currLevel->textures.clear();
  m_currLevel->map.reset();
  m_currLevel.reset();
}

void GameResources::loadAllAssets(
  game_engine::SDLState& state,
  game_engine::GameState& gs,
  const ProgressionProfile& profile,
  bool headless) {

  // TODO this all needs to go in an audioManager
  m_masterAudioGain = mixer ? MIX_GetMasterGain(mixer) : 0.0f;
  const float chunkAudioGain = m_masterAudioGain * 3.0f;

  std::tie(audioShoot, shootTrack) =
    loadAudioChunk("data/audio/fireball_whoosh.mp3", chunkAudioGain);
  std::tie(audioSword1, sword1Track) =
    loadAudioChunk("data/audio/sword/sword_swing_1.mp3", chunkAudioGain);
  std::tie(audioUltimateAttack, ultimateAttackTrack) =
    loadAudioChunk("data/audio/sword/ultimate_attack.wav", chunkAudioGain);
  std::tie(audioShootHit, hitTrack) =
    loadAudioChunk("data/audio/fireball_hit.mp3", chunkAudioGain);
  std::tie(audioBoneImpact, boneImpactHitTrack) =
    loadAudioChunk("data/audio/impact/bone_impact.mp3", chunkAudioGain);
  std::tie(audioProjectileEnemyHit, enemyProjectileHitTrack) =
    loadAudioChunk("data/audio/fireball_hit.mp3", chunkAudioGain);
  std::tie(audioEnemyDie, enemyDieTrack) =
    loadAudioChunk("data/audio/monster_die.wav", chunkAudioGain);
  std::tie(audioJump, jumpTrack) =
    loadAudioChunk("data/audio/movement/jump.wav", chunkAudioGain);

  // TODO loadLevel should be based on the players fileSave
  const bool lvlLoaded = loadLevel(LevelIndex::LEVEL_1, state, gs, m_masterAudioGain, headless);
  if (!lvlLoaded) {
    return;
  }


  // TODO need to move these elsewhere
  bulletAnims.resize(2);
  bulletAnims[ANIM_BULLET_MOVING] = Animation(9, 1.0f);
  bulletAnims[ANIM_BULLET_HIT] = Animation(4, 0.15f);
  if (!headless) {
    texBulletHit = loadTexture(state.renderer, "data/players/Mage/Charge_1.png");
    texBullet = loadTexture(state.renderer, "data/players/Mage/Charge_1.png");
    texMainMenu = loadTexture(state.renderer, "data/maps/title_screen/title_screen.png");

    mainMenuAnim = std::make_shared<Animation>(58, 7.0f);
    std::tie(mainMenuAudio, mainMenuTrack) =
      loadAudioChunk("data/audio/Final_Boss_Battle.wav", chunkAudioGain);
    mainMenuCutscene = {
      UIManager::Cutscene{
        .tex = texMainMenu,
        .anim = mainMenuAnim,
        .scale = 1.2f,
        .numFrameColumns = 8,
        .frameH = 540.0f,
        .frameW = 800.0f,
        .yOffset = -50,
        .loopScene = true,
      },
    };

    texCharSelect = loadTexture(state.renderer, "data/cutscenes/menu/character_select.png");
    charSelectAnim = std::make_shared<Animation>(8, 2.0f);
    characterSelectScene = {
      UIManager::Cutscene{
        .tex = texCharSelect,
        .anim = charSelectAnim,
        .scale = 1.0f,
        .numFrameColumns = 3,
        .frameH = 360.0f,
        .frameW = 640.0f,
        .loopScene = false,
      },
    };

    texPauseMenu = loadTexture(state.renderer, "data/cutscenes/menu/pause_menu.png");
    pauseMenuAnim = std::make_shared<Animation>(1, 1.0f, 0, true);
    pauseMenuScene = {
      UIManager::Cutscene{
        .tex = texPauseMenu,
        .anim = pauseMenuAnim,
        .scale = 1.0f,
        .numFrameColumns = 1,
        .frameH = 360.0f,
        .frameW = 640.0f,
        .loopScene = false,
      },
    };
  }
}

void GameResources::unload() {
  unloadLevel();

  auto destroyTrack = [](MIX_Track*& track) {
    if (track) {
      if (MIX_TrackPlaying(track)) {
        MIX_StopTrack(track, 0);
      }
      MIX_DestroyTrack(track);
      track = nullptr;
    }
  };
  auto destroyAudio = [](MIX_Audio*& audio) {
    if (audio) {
      MIX_DestroyAudio(audio);
      audio = nullptr;
    }
  };

  destroyTrack(mainMenuTrack);
  destroyAudio(mainMenuAudio);
  destroyTrack(shootTrack);
  destroyAudio(audioShoot);
  destroyTrack(sword1Track);
  destroyAudio(audioSword1);
  destroyTrack(ultimateAttackTrack);
  destroyAudio(audioUltimateAttack);
  destroyTrack(hitTrack);
  destroyAudio(audioShootHit);
  destroyTrack(boneImpactHitTrack);
  destroyAudio(audioBoneImpact);
  destroyTrack(enemyProjectileHitTrack);
  destroyAudio(audioProjectileEnemyHit);
  destroyTrack(enemyDieTrack);
  destroyAudio(audioEnemyDie);
  destroyTrack(jumpTrack);
  destroyAudio(audioJump);

  for (SDL_Texture* tex : textures) {
    if (tex) {
      SDL_DestroyTexture(tex);
    }
  }
  textures.clear();
  texBullet = nullptr;
  texBulletHit = nullptr;
  texMainMenu = nullptr;
  texCharSelect = nullptr;
  texPauseMenu = nullptr;
}

} // namespace game
