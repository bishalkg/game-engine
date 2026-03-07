#include "engine/engine.h"

void game_engine::Engine::applyUIActions(const UIManager::UIActions& a) {
  if (a.stopBackgroundTrack) { stopBackgroundSoundtrack(); }
  if (a.startSinglePlayer) { m_gameType = SinglePlayer; }
  if (a.quitGame) { m_gameRunning.store(false); }
  if (a.finishLoading) { if (m_levelLoadThd.joinable()) m_levelLoadThd.join(); }
  if (a.nextView) m_gameState.currentView = *a.nextView;
  if (a.stopBackgroundTrack) { stopBackgroundSoundtrack(); }
  if (a.stopGameOverSoundTrack ) { stopGameOverSoundtrack(); }
  if (a.restartLevel) { asyncSwitchToLevel(m_resources.m_currLevelIdx); }
  if (a.startMultiPlayerClient) { m_gameType = Client; }
  if (a.startMultiPlayerHost) { m_gameType = Host; }
  if (a.adjustVolume && m_resources.mixer) {
    m_resources.m_masterAudioGain = a.newVolume;
    if (!MIX_SetMasterGain(m_resources.mixer, a.newVolume)) {
        SDL_Log("MIX_SetMasterGain failed: %s", SDL_GetError());
    }
  }
}

UIManager::UIActions game_engine::Engine::updateUI(UIManager::UI_Manager& uiManager, float deltaTime, UIManager::UISnapshots &snaps) {

  auto player = getPlayer();
  snaps.playerHP = player.data.player.healthPoints;
  snaps.playerMana = player.data.player.manaPoints;
  snaps.winDims = ImVec2(m_sdlState.logW, m_sdlState.logH);

  switch (m_gameState.currentView) {
    case UIManager::GameView::LevelLoading:
    {
      std::lock_guard<std::mutex> lock(m_levelMutex);
      const uint8_t p = m_gameState.getLevelLoadProgress();
      snaps.loading.progress01 = std::clamp(p * 0.01f, 0.0f, 1.0f);
      snaps.loading.done = (p >= 100);
      break;
    }
    case UIManager::GameView::MainMenu:
    {
      snaps.deltaTime = deltaTime;
      snaps.currVolume = m_resources.m_masterAudioGain; // 0-1.0f
      setAudioSoundtrack(m_resources.mainMenuTrack);
      snaps.cutscene = &m_resources.mainMenuCutscene;
      snaps.cutSceneID = -3;
      break;
    }
    case UIManager::GameView::PauseMenu: {
      snaps.deltaTime = deltaTime;
      snaps.cutscene = &m_resources.pauseMenuScene;
      snaps.cutSceneID = -2;
      break;
    }
   case UIManager::GameView::CutScene:
    {
      if (m_resources.m_currLevel && !m_resources.m_currLevel->cutscenes.empty()) {
        snaps.deltaTime = deltaTime;
        // pull out the cutscene from the current level
        stopAudioSoundtrack(m_resources.mainMenuTrack);
        setAudioSoundtrack(m_resources.m_currLevel->backgroundTrack);
        snaps.cutscene = &m_resources.m_currLevel->cutscenes;
        snaps.cutSceneID = static_cast<int>(m_resources.m_currLevel->lvlIdx);
      }
      break;
    }
  default:
    {
      stopAudioSoundtrack(m_resources.mainMenuTrack);
    }
  }

  UIManager::UIActions actions = uiManager.renderView(m_gameState.currentView, snaps, m_sdlState.ImGuiWindowFlags, m_sdlState);

  applyUIActions(actions);

  return actions;
}
