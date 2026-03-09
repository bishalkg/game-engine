#include "game/default_systems.h"

#include <algorithm>

#include "engine/engine.h"

namespace {

class DefaultUIFlow final : public game::IUIFlow {
public:
  UIManager::UIActions update(
    game_engine::Engine& engine,
    float deltaTime,
    UIManager::UISnapshots& snaps) override {
    auto& gameState = engine.getGameState();
    auto& sdlState = engine.getSDLState();
    auto& resources = engine.getResources();
    auto& uiManager = resources.m_uiManager;

    auto& player = engine.getPlayer();
    snaps.playerHP = player.data.player.healthPoints;
    snaps.playerMana = player.data.player.manaPoints;
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

    return uiManager.renderView(
      gameState.currentView,
      snaps,
      sdlState.ImGuiWindowFlags,
      sdlState);
  }

  void apply(game_engine::Engine& engine, const UIManager::UIActions& actions) override {
    auto& gameState = engine.getGameState();
    auto& resources = engine.getResources();

    if (actions.stopBackgroundTrack) {
      engine.stopBackgroundSoundtrack();
    }
    if (actions.stopGameOverSoundTrack) {
      engine.stopGameOverSoundtrack();
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
    if (actions.nextView) {
      gameState.currentView = *actions.nextView;
    }
    if (actions.adjustVolume && resources.mixer) {
      resources.m_masterAudioGain = actions.newVolume;
      if (!MIX_SetMasterGain(resources.mixer, actions.newVolume)) {
        SDL_Log("MIX_SetMasterGain failed: %s", SDL_GetError());
      }
    }
    if (actions.restartLevel && resources.m_currLevel) {
      (void)game::switchToLevel(engine, resources.m_currLevelIdx);
    }
  }
};

} // namespace

namespace game {

std::unique_ptr<IUIFlow> createDefaultUIFlow() {
  return std::make_unique<DefaultUIFlow>();
}

} // namespace game
