#include "game/default_systems.h"

#include <algorithm>

#include "engine/engine.h"
#include "game/player_command.h"

namespace {

void playOneShotUiAudio(game::GameResources& resources, MIX_Audio* audio) {
  if (resources.mixer && audio) {
    MIX_PlayAudio(resources.mixer, audio);
  }
}

void playAudioCue(game::GameResources& resources, game::PlayerCommandAudioCue cue) {
  switch (cue) {
    case game::PlayerCommandAudioCue::CoinPurchase:
      playOneShotUiAudio(resources, resources.audioCoinPurchase);
      break;
    case game::PlayerCommandAudioCue::GemPurchase:
      playOneShotUiAudio(resources, resources.audioGemPurchase);
      break;
    case game::PlayerCommandAudioCue::ConsumableUse:
      playOneShotUiAudio(resources, resources.audioDrinkSlurp);
      break;
    case game::PlayerCommandAudioCue::None:
      break;
  }
}

void persistInventoryProgress(
  game_engine::Engine& engine,
  game::ProgressionService& progService,
  const Inventory& inventory) {
  progService.updatePlayerInventory(inventory);
  engine.writeToSlotPath("slot_1", progService.serealizeSaveState());
}

GameObject* resolveLocalPlayer(game_engine::Engine& engine) {
  auto& gameState = engine.getGameState();
  if (gameState.playerLayer < 0 ||
      gameState.playerLayer >= static_cast<int>(gameState.layers.size()) ||
      gameState.playerIndex < 0 ||
      gameState.playerIndex >= static_cast<int>(gameState.layers[gameState.playerLayer].size())) {
    return nullptr;
  }

  GameObject& player = gameState.layers[gameState.playerLayer][gameState.playerIndex];
  return player.objClass == ObjectClass::Player ? &player : nullptr;
}

std::optional<game::PlayerCommand> shopPurchaseToCommand(UIManager::ShopPurchase purchase) {
  switch (purchase) {
    case UIManager::ShopPurchase::HealthPotion:
      return game::PlayerCommand{
        .type = game::PlayerCommandType::ShopPurchase,
        .value = static_cast<std::uint8_t>(game::ShopPurchaseCommand::HealthPotion),
      };
    case UIManager::ShopPurchase::ManaPotion:
      return game::PlayerCommand{
        .type = game::PlayerCommandType::ShopPurchase,
        .value = static_cast<std::uint8_t>(game::ShopPurchaseCommand::ManaPotion),
      };
    case UIManager::ShopPurchase::AttackUp:
      return game::PlayerCommand{
        .type = game::PlayerCommandType::ShopPurchase,
        .value = static_cast<std::uint8_t>(game::ShopPurchaseCommand::AttackUp),
      };
    case UIManager::ShopPurchase::DefenceUp:
      return game::PlayerCommand{
        .type = game::PlayerCommandType::ShopPurchase,
        .value = static_cast<std::uint8_t>(game::ShopPurchaseCommand::DefenceUp),
      };
  }
  return std::nullopt;
}

std::optional<game::PlayerCommand> inventoryUseToCommand(UIManager::InventoryUse use) {
  switch (use) {
    case UIManager::InventoryUse::HealthPotion:
      return game::PlayerCommand{
        .type = game::PlayerCommandType::InventoryUse,
        .value = static_cast<std::uint8_t>(game::InventoryUseCommand::HealthPotion),
      };
    case UIManager::InventoryUse::ManaPotion:
      return game::PlayerCommand{
        .type = game::PlayerCommandType::InventoryUse,
        .value = static_cast<std::uint8_t>(game::InventoryUseCommand::ManaPotion),
      };
  }
  return std::nullopt;
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
  auto* player = resolveLocalPlayer(engine);
  if (!player) {
    return false;
  }

  const auto command = shopPurchaseToCommand(purchase);
  if (!command) {
    return false;
  }

  const auto result = game::applyPlayerCommand(player->data.player, *command);
  if (!result.applied) {
    return false;
  }

  persistInventoryProgress(engine, progService, player->data.player.inventory);
  playAudioCue(resources, result.audioCue);
  return true;
}

bool applyInventoryUse(
  game_engine::Engine& engine,
  game::GameResources& resources,
  game::ProgressionService& progService,
  UIManager::InventoryUse use) {
  auto* player = resolveLocalPlayer(engine);
  if (!player) {
    return false;
  }

  const auto command = inventoryUseToCommand(use);
  if (!command) {
    return false;
  }

  const auto result = game::applyPlayerCommand(player->data.player, *command);
  if (!result.applied) {
    return false;
  }

  persistInventoryProgress(engine, progService, player->data.player.inventory);
  playAudioCue(resources, result.audioCue);
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
    snaps.currBossHP = 0;
    snaps.maxBossHP = 0;

    // set player values for UI view -> TODO helpers
    if (gameState.playerLayer >= 0 &&
        gameState.playerLayer < static_cast<int>(gameState.layers.size()) &&
        gameState.playerIndex >= 0 &&
        gameState.playerIndex < static_cast<int>(gameState.layers[gameState.playerLayer].size())) {
      auto& player = engine.getPlayer();
      snaps.playerHP = player.data.player.healthPoints;
      snaps.maxPlayerHP = player.data.player.maxHealthPoints;
      snaps.maxPlayerMana = player.data.player.maxManaPoints;
      snaps.maxUltimatePoints = player.data.player.maxUltimatePoints;
      snaps.playerMana = player.data.player.manaPoints;
      snaps.playerUltimate = player.data.player.ultimatePoints;
      snaps.playerUltimateUnlocked = player.data.player.unlockedUltimateOne;
      snaps.playerUltimateReady =
        player.data.player.ultimatePoints >= player.data.player.maxUltimatePoints;
      snaps.gameplayHud.playerCoins = player.data.player.inventory.coins.count;
      snaps.gameplayHud.playerGems = player.data.player.inventory.gems.count;
      snaps.gameplayHud.playerHealthPotions = player.data.player.inventory.healthPotions.count;
      snaps.gameplayHud.playerManaPotions = player.data.player.inventory.manaPotions.count;
      snaps.gameplayHud.playerAttackUps = player.data.player.inventory.attackUps.count;
      snaps.gameplayHud.playerDefenceUps = player.data.player.inventory.defenceUps.count;

      GameObject* boss = engine.getActiveCurrBoss();
      if (boss) {
        snaps.currBossHP = boss->data.enemy.healthPoints;
        snaps.maxBossHP = boss->data.enemy.maxHealthPoints;
      }

    } else {
      snaps.playerHP = 0;
      snaps.playerMana = 0;
      snaps.playerUltimate = 0;
      snaps.playerUltimateReady = false;
      snaps.playerUltimateUnlocked = false;
      snaps.maxPlayerHP = 0;
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
      if (resources.m_currLevel) {
        engine.stopAudioSoundtrack(resources.m_currLevel->backgroundTrack);
        engine.stopAudioSoundtrack(resources.m_currLevel->bossTrack);
      }
      engine.stopAudioSoundtrack(resources.floatingStoneTrack);
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
      if (engine.isMultiplayerActive()) {
        const auto command = shopPurchaseToCommand(*actions.shopPurchase);
        if (command) {
          engine.submitLocalPlayerCommand(command->serialize());
        }
      } else {
        (void)applyShopPurchase(engine, resources, progService, *actions.shopPurchase);
      }
    }
    if (actions.inventoryUse) {
      if (engine.isMultiplayerActive()) {
        const auto command = inventoryUseToCommand(*actions.inventoryUse);
        if (command) {
          engine.submitLocalPlayerCommand(command->serialize());
        }
      } else {
        (void)applyInventoryUse(engine, resources, progService, *actions.inventoryUse);
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
