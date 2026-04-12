#include "game/default_systems.h"

#include <algorithm>

#include "engine/engine.h"

namespace {

const char* levelName(LevelIndex levelId) {
  switch (levelId) {
    case LevelIndex::LEVEL_1:
      return "Level 1";
    case LevelIndex::LEVEL_2:
      return "Level 2";
    case LevelIndex::LEVEL_3:
      return "Level 3";
  }
  return "Unknown";
}

class DefaultUIFlow final : public game::IUIFlow {
public:
  UIManager::UIActions update(
    game_engine::Engine& engine,
    game::GameResources& resources,
    float deltaTime,
    UIManager::UISnapshots& snaps) override {
    auto& gameState = engine.getGameState();
    auto& sdlState = engine.getSDLState();
    auto& uiManager = resources.m_uiManager;
    snaps.multiplayerSessions.clear();
    snaps.multiplayerStatus.clear();

    if (gameState.playerLayer >= 0 &&
        gameState.playerLayer < static_cast<int>(gameState.layers.size()) &&
        gameState.playerIndex >= 0 &&
        gameState.playerIndex < static_cast<int>(gameState.layers[gameState.playerLayer].size())) {
      auto& player = engine.getPlayer();
      snaps.playerHP = player.data.player.healthPoints;
      snaps.playerMana = player.data.player.manaPoints;
      snaps.playerUltimate = player.data.player.ultimatePoints;
      snaps.playerUltimateReady =
        player.data.player.ultimatePoints >= player.data.player.maxUltimatePoints;
    } else {
      snaps.playerHP = 0;
      snaps.playerMana = 0;
      snaps.playerUltimate = 0;
      snaps.playerUltimateReady = false;
    }
    snaps.winDims = ImVec2(static_cast<float>(sdlState.logW), static_cast<float>(sdlState.logH));
    snaps.debugMode = gameState.debugMode;

    switch (gameState.currentView) {
      case UIManager::GameView::LevelLoading: {
        const uint8_t progress = gameState.getLevelLoadProgress();
        snaps.loading.progress01 = std::clamp(progress * 0.01f, 0.0f, 1.0f);
        snaps.loading.done = (progress >= 100);
        break;
      }
      case UIManager::GameView::MainMenu: {
        snaps.deltaTime = deltaTime;
        snaps.currVolume = resources.m_masterAudioGain;
        engine.setAudioSoundtrack(resources.mainMenuTrack);
        snaps.cutscene = &resources.mainMenuCutscene;
        snaps.cutSceneID = -3;
        break;
      }
      case UIManager::GameView::CharacterSelect: {
        snaps.deltaTime = deltaTime;
        snaps.cutscene = &resources.characterSelectScene;
        snaps.cutSceneID = -4;
        break;
      }
      case UIManager::GameView::PauseMenu: {
        snaps.deltaTime = deltaTime;
        snaps.cutscene = &resources.pauseMenuScene;
        snaps.cutSceneID = -2;
        break;
      }
      case UIManager::GameView::MultiplayerBrowse: {
        snaps.deltaTime = deltaTime;
        snaps.multiplayerStatus = engine.getMultiplayerStatus();
        for (const auto& session : engine.copyDiscoveredSessions()) {
          snaps.multiplayerSessions.push_back(UIManager::MultiplayerSessionDisplay{
            .hostName = session.hostName,
            .hostAddress = session.hostAddress,
            .levelName = levelName(session.levelId),
            .playerCount = session.playerCount,
          });
        }
        break;
      }
      case UIManager::GameView::MultiplayerHostWaiting:
      case UIManager::GameView::MultiplayerRespawnWait: {
        snaps.deltaTime = deltaTime;
        snaps.multiplayerStatus = engine.getMultiplayerStatus();
        break;
      }
      case UIManager::GameView::CutScene: {
        if (resources.m_currLevel && !resources.m_currLevel->cutscenes.empty()) {
          snaps.deltaTime = deltaTime;
          engine.stopAudioSoundtrack(resources.mainMenuTrack);
          engine.setAudioSoundtrack(resources.m_currLevel->backgroundTrack);
          snaps.cutscene = &resources.m_currLevel->cutscenes;
          snaps.cutSceneID = static_cast<int>(resources.m_currLevel->lvlIdx);
        }
        break;
      }
      default:
        engine.stopAudioSoundtrack(resources.mainMenuTrack);
        break;
    }

    return uiManager.getRenderViewActions(
      gameState.currentView,
      snaps,
      sdlState.ImGuiWindowFlags,
      sdlState);
  }

  void apply(
    game_engine::Engine& engine,
    game::GameResources& resources,
    game::ProgressionService& progService,
    const UIManager::UIActions& actions) override {
    auto& gameState = engine.getGameState();

    if (actions.stopBackgroundTrack) {
      engine.stopAudioSoundtrack(
        resources.m_currLevel ? resources.m_currLevel->backgroundTrack : nullptr);
    }
    if (actions.stopGameOverSoundTrack) {
      engine.stopAudioSoundtrack(
        resources.m_currLevel ? resources.m_currLevel->gameOverAudioTrack : nullptr);
    }
    if (actions.startSinglePlayer) {
      engine.setRunModeSinglePlayer();
    }
    if (actions.startMultiPlayerHost) {
      engine.setRunModeHost();
    }
    if (actions.startMultiPlayerClient) {
      engine.setRunModeClient();
    }
    if (actions.quitGame) {
      engine.requestQuit();
    }
    if (actions.selectedSessionIndex) {
      (void)engine.selectDiscoveredSession(*actions.selectedSessionIndex);
    }
    if (actions.selectedPlayerSprite) {
      gameState.selectedPlayerSprite = *actions.selectedPlayerSprite;
      if (resources.m_currLevel) {
        (void)game::switchToLevel(engine, resources, progService, resources.m_currLevelIdx);
        if (engine.isHostMode()) {
          gameState.currentView = UIManager::GameView::MultiplayerHostWaiting;
        } else if (engine.isClientMode()) {
          gameState.currentView = UIManager::GameView::Playing;
        }
      }
    }
    if (actions.nextView == UIManager::GameView::MainMenu && engine.isMultiplayerActive()) {
      engine.setRunModeSinglePlayer();
    }
    if (actions.nextView && !(actions.selectedPlayerSprite && engine.isMultiplayerActive())) {
      gameState.currentView = *actions.nextView;
    }
    if (actions.adjustVolume && resources.mixer) {
      resources.m_masterAudioGain = actions.newVolume;
      if (!MIX_SetMasterGain(resources.mixer, actions.newVolume)) {
        SDL_Log("MIX_SetMasterGain failed: %s", SDL_GetError());
      }
    }
    if (actions.restartLevel && resources.m_currLevel) {
      if (engine.isHostMode()) {
        if (game::switchToLevel(engine, resources, progService, resources.m_currLevelIdx)) {
          engine.restartMultiplayerSession();
        }
      } else if (engine.isClientMode()) {
        engine.restartMultiplayerSession();
      } else {
        (void)game::switchToLevel(engine, resources, progService, resources.m_currLevelIdx);
      }
    }
  }
};

} // namespace

namespace game {

std::unique_ptr<IUIFlow> createDefaultUIFlow() {
  return std::make_unique<DefaultUIFlow>();
}

} // namespace game
