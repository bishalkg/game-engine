#include "game/default_systems.h"

#include <algorithm>

#include "engine/engine.h"

namespace {

void playOneShotUiAudio(game::GameResources& resources, MIX_Audio* audio) {
  if (resources.mixer && audio) {
    MIX_PlayAudio(resources.mixer, audio);
  }
}

void submitMultiplayerUiInput(game_engine::Engine& engine, game_engine::NetGameInput input) {
  engine.submitLocalInput(input);
  engine.flushLocalInput(1.0f);
}

const char* levelName(LevelIndex levelId) {
  switch (levelId) {
    case LevelIndex::LEVEL_1:
      return "Level 1";
    case LevelIndex::LEVEL_2:
      return "Level 2";
    case LevelIndex::LEVEL_3:
      return "Level 3";
    case LevelIndex::LEVEL_4:
      return "Level 4";
    case LevelIndex::LEVEL_5:
      return "Level 5";
  }
  return "Unknown";
}

bool applyShopPurchase(
  game_engine::Engine& engine,
  game::GameResources& resources,
  game::ProgressionService& progService,
  UIManager::ShopPurchase purchase) {
  auto& gameState = engine.getGameState();
  if (gameState.playerLayer < 0 ||
      gameState.playerLayer >= static_cast<int>(gameState.layers.size()) ||
      gameState.playerIndex < 0 ||
      gameState.playerIndex >= static_cast<int>(gameState.layers[gameState.playerLayer].size())) {
    return false;
  }

  GameObject& player = gameState.layers[gameState.playerLayer][gameState.playerIndex];
  if (player.objClass != ObjectClass::Player) {
    return false;
  }

  auto& inventory = player.data.player.inventory;
  MaterialData* currency = nullptr;
  MaterialData* item = nullptr;
  MIX_Audio* purchaseAudio = nullptr;
  uint32_t cost = 0;

  switch (purchase) {
    case UIManager::ShopPurchase::HealthPotion:
      currency = &inventory.coins;
      item = &inventory.healthPotions;
      purchaseAudio = resources.audioCoinPurchase;
      cost = 15;
      break;
    case UIManager::ShopPurchase::ManaPotion:
      currency = &inventory.coins;
      item = &inventory.manaPotions;
      purchaseAudio = resources.audioCoinPurchase;
      cost = 15;
      break;
    case UIManager::ShopPurchase::AttackUp:
      currency = &inventory.gems;
      item = &inventory.attackUps;
      purchaseAudio = resources.audioGemPurchase;
      cost = 10;
      break;
    case UIManager::ShopPurchase::DefenceUp:
      currency = &inventory.gems;
      item = &inventory.defenceUps;
      purchaseAudio = resources.audioGemPurchase;
      cost = 10;
      break;
  }

  if (!currency || !item || currency->count < cost) {
    return false;
  }

  currency->count -= cost;
  ++item->count;
  progService.updatePlayerInventory(inventory);
  engine.writeToSlotPath("slot_1", progService.serealizeSaveState());
  playOneShotUiAudio(resources, purchaseAudio);
  return true;
}

bool applyInventoryUse(
  game_engine::Engine& engine,
  game::GameResources& resources,
  game::ProgressionService& progService,
  UIManager::InventoryUse use) {
  auto& gameState = engine.getGameState();
  if (gameState.playerLayer < 0 ||
      gameState.playerLayer >= static_cast<int>(gameState.layers.size()) ||
      gameState.playerIndex < 0 ||
      gameState.playerIndex >= static_cast<int>(gameState.layers[gameState.playerLayer].size())) {
    return false;
  }

  GameObject& player = gameState.layers[gameState.playerLayer][gameState.playerIndex];
  if (player.objClass != ObjectClass::Player) {
    return false;
  }

  auto& playerData = player.data.player;
  auto& inventory = playerData.inventory;
  MaterialData* item = nullptr;
  int* statValue = nullptr;
  int maxStatValue = 0;

  switch (use) {
    case UIManager::InventoryUse::HealthPotion:
      item = &inventory.healthPotions;
      statValue = &playerData.healthPoints;
      maxStatValue = playerData.maxHealthPoints;
      break;
    case UIManager::InventoryUse::ManaPotion:
      item = &inventory.manaPotions;
      statValue = &playerData.manaPoints;
      maxStatValue = playerData.maxManaPoints;
      break;
  }

  if (!item || !statValue || item->count == 0 || *statValue >= maxStatValue) {
    return false;
  }

  *statValue = std::clamp(*statValue + 20, 0, maxStatValue);
  --item->count;
  progService.updatePlayerInventory(inventory);
  engine.writeToSlotPath("slot_1", progService.serealizeSaveState());
  playOneShotUiAudio(resources, resources.audioDrinkSlurp);
  return true;
}

class DefaultUIFlow final : public game::IUIFlow {
public:
  UIManager::UIActions update(
    game_engine::Engine& engine,
    game::GameResources& resources,
    game::ProgressionService& progService,
    float deltaTime,
    UIManager::UISnapshots& snaps) override {
    auto& gameState = engine.getGameState();
    auto& sdlState = engine.getSDLState();
    snaps.multiplayerSessions.clear();
    snaps.multiplayerStatus.clear();
    snaps.showGameplayHud = false;
    snaps.gameplayHud = UIManager::GameplayHudSnapshot{};

    // set player values for UI view -> TODO helpers
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
      snaps.gameplayHud.playerCoins = player.data.player.inventory.coins.count;
      snaps.gameplayHud.playerGems = player.data.player.inventory.gems.count;
      snaps.gameplayHud.playerHealthPotions = player.data.player.inventory.healthPotions.count;
      snaps.gameplayHud.playerManaPotions = player.data.player.inventory.manaPotions.count;
      snaps.gameplayHud.playerAttackUps = player.data.player.inventory.attackUps.count;
      snaps.gameplayHud.playerDefenceUps = player.data.player.inventory.defenceUps.count;
    } else {
      snaps.playerHP = 0;
      snaps.playerMana = 0;
      snaps.playerUltimate = 0;
      snaps.playerUltimateReady = false;
    }
    snaps.gameplayHud.coinCountHudAnim = resources.coinCountUIAnim.get();
    snaps.gameplayHud.gemCountHudAnim = resources.gemCountUIAnim.get();
    snaps.gameplayHud.numbersHudTex = resources.texHudNumbers;
    snaps.gameplayHud.coinCountHudTex = resources.texCoinCountUI;
    snaps.gameplayHud.gemCountHudTex = resources.texGemCountUI;
    snaps.winDims = ImVec2(static_cast<float>(sdlState.logW), static_cast<float>(sdlState.logH));
    snaps.debugMode = gameState.debugMode;

    switch (gameState.currentView) {
      case UIManager::GameView::LevelLoading: {
        const uint8_t progress = gameState.getLevelLoadProgress();
        snaps.loading.progress01 = std::clamp(progress * 0.01f, 0.0f, 1.0f);
        snaps.loading.done = (progress >= 100);
        // only set cutscene if available -> TODO helper
        if (resources.m_currLevel && !resources.m_currLevel->cutscenes.empty()) {
          snaps.cutscene = &resources.m_currLevel->cutscenes;
          snaps.cutSceneID = static_cast<int>(resources.m_currLevel->lvlIdx);
        }
        break;
      }
      case UIManager::GameView::Playing: {
        snaps.deltaTime = deltaTime;
        snaps.showGameplayHud = true;
        engine.stopAudioSoundtrack(resources.mainMenuTrack);
        break;
      }
      case UIManager::GameView::PauseMenu: {
        snaps.deltaTime = deltaTime;
        snaps.cutscene = &resources.pauseMenuScene;
        snaps.cutSceneID = -2;
        break;
      }
      case UIManager::GameView::ShopMenu: {
        snaps.deltaTime = deltaTime;
        snaps.cutscene = &resources.shopScene;
        snaps.cutSceneID = -6;
        break;
      }
      case UIManager::GameView::InventoryMenu: {
        snaps.deltaTime = deltaTime;
        snaps.cutscene = &resources.inventoryScene;
        snaps.cutSceneID = -7;
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
        engine.setAudioSoundtrack(resources.mainMenuTrack);
        snaps.cutSceneID = -4;
        break;
      }
      case UIManager::GameView::LevelSelection: {
        snaps.deltaTime = deltaTime;
        snaps.levelProgressionIdx = progService.getLastCompletedLevel();
        snaps.cutscene = &resources.levelSelectCutscene;
        snaps.cutSceneID = -5;
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
      case UIManager::GameView::MultiplayerHostWaiting: {
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

    return resources.m_uiManager.getRenderViewActions(
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
    if (actions.shopPurchase) {
      if (engine.isClientMode()) {
        game_engine::NetGameInput input{};
        input.shopPurchaseCode = static_cast<std::uint8_t>(*actions.shopPurchase);
        submitMultiplayerUiInput(engine, input);
      } else {
        const bool purchased =
          applyShopPurchase(engine, resources, progService, *actions.shopPurchase);
        if (purchased && engine.isHostMode()) {
          engine.synchronizeHostAuthoritativeState();
          engine.broadcastHostSnapshot();
        }
      }
    }
    if (actions.inventoryUse) {
      if (engine.isClientMode()) {
        game_engine::NetGameInput input{};
        input.inventoryUseCode = static_cast<std::uint8_t>(*actions.inventoryUse);
        submitMultiplayerUiInput(engine, input);
      } else {
        const bool used =
          applyInventoryUse(engine, resources, progService, *actions.inventoryUse);
        if (used && engine.isHostMode()) {
          engine.synchronizeHostAuthoritativeState();
          engine.broadcastHostSnapshot();
        }
      }
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
    if (actions.selectedLevel.has_value() && resources.m_currLevel) {
      if (engine.isHostMode() && game::switchToLevel(engine, resources, progService, actions.selectedLevel.value())) {
        // gameState.currentView = UIManager::GameView::MultiplayerHostWaiting;
        engine.restartMultiplayerSession();
      } else if (engine.isClientMode()) {
        engine.restartMultiplayerSession();
        // gameState.currentView = UIManager::GameView::Playing;
      } else {
         (void)game::switchToLevel(engine, resources, progService, actions.selectedLevel.value());
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

    // TODO: IF HOST DIES BOTH PLAYERS RESPAWN FROM START; IF CLIENT DIES ONLY CLIENT STARTS FROM START. CHANGE THIS.
    if (actions.restartLevel && resources.m_currLevel) {
      if (engine.isHostMode()) {
        if (game::switchToLevel(
              engine,
              resources,
              progService,
              resources.m_currLevelIdx,
              true)) {
          engine.restartMultiplayerSession();
        }
      } else if (engine.isClientMode()) {
        engine.restartMultiplayerSession();
      } else {
        (void)game::switchToLevel(
          engine,
          resources,
          progService,
          resources.m_currLevelIdx,
          true);
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
