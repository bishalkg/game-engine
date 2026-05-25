#include "game/default_systems.h"

#include <algorithm>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "engine/engine.h"
#include "engine/gameplay_simulation.h"
#include "game/player_command.h"

namespace {

using game::EntityResources;

struct SimContext {
  game_engine::Engine& engine;
  game_engine::GameState& gameState;
  game::GameResources& resources;
  game::ProgressionService& progService;
};

struct AudioObjectState {
  ObjectClass objClass = ObjectClass::Level;
  int currentAnimation = -1;
  PlayerState playerState = PlayerState::idle;
  EnemyState enemyState = EnemyState::idle;
  BulletState bulletState = BulletState::inactive;
  int healthPoints = 0;
  int manaPoints = 0;
  uint32_t coinPickupCueCount = 0;
  uint32_t gemPickupCueCount = 0;
  uint32_t coinPurchaseCueCount = 0;
  uint32_t gemPurchaseCueCount = 0;
  uint32_t consumableUseCueCount = 0;
  bool jumpImpulseApplied = false;
};

struct InventorySnapshot {
  uint32_t coins = 0;
  uint32_t gems = 0;
  uint32_t healthPotions = 0;
  uint32_t manaPotions = 0;
  uint32_t attackUps = 0;
  uint32_t defenceUps = 0;

  bool operator==(const InventorySnapshot& other) const {
    return coins == other.coins &&
           gems == other.gems &&
           healthPotions == other.healthPotions &&
           manaPotions == other.manaPotions &&
           attackUps == other.attackUps &&
           defenceUps == other.defenceUps;
  }
};

using AudioStateMap =
  std::unordered_map<game_engine::GameObjectKey, AudioObjectState, game_engine::GameObjectKeyHash>;

using game_engine::GameObjectKey;
using game_engine::LocalHitStopTarget;
using game_engine::NetHitStopEvent;

struct LayeredDynamicKey {
  uint32_t layer = 0;
  ObjectClass objClass = ObjectClass::Level;
  uint32_t id = 0;

  bool operator==(const LayeredDynamicKey& other) const {
    return layer == other.layer && objClass == other.objClass && id == other.id;
  }
};

struct LayeredDynamicKeyHash {
  size_t operator()(const LayeredDynamicKey& key) const noexcept {
    const size_t h1 = std::hash<uint32_t>{}(key.layer);
    const size_t h2 =
      std::hash<std::underlying_type_t<ObjectClass>>{}(static_cast<std::underlying_type_t<ObjectClass>>(key.objClass));
    const size_t h3 = std::hash<uint32_t>{}(key.id);
    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2)) ^
           (h3 + 0x9e3779b9 + (h2 << 6) + (h2 >> 2));
  }
};

SDL_Texture* pickEntityTexture(
  const EntityResources& entityRes,
  ObjectClass objClass,
  PresentationVariant presentation) {
  if (objClass == ObjectClass::Projectile) {
    return nullptr;
  }

  switch (presentation) {
    case PresentationVariant::Run:
      return entityRes.texRun ? entityRes.texRun : entityRes.texWalk;
    case PresentationVariant::Shoot:
      return entityRes.texShoot ? entityRes.texShoot : entityRes.texIdle;
    case PresentationVariant::RunShoot:
      return entityRes.texRunShoot ? entityRes.texRunShoot : entityRes.texShoot;
    case PresentationVariant::Slide:
      return entityRes.texSlide ? entityRes.texSlide : entityRes.texRun;
    case PresentationVariant::SlideShoot:
      return entityRes.texSlideShoot ? entityRes.texSlideShoot : entityRes.texSlide;
    case PresentationVariant::Swing:
      return entityRes.texAttack ? entityRes.texAttack : entityRes.texIdle;
    case PresentationVariant::Jump:
      return entityRes.texJump ? entityRes.texJump : entityRes.texRun;
    case PresentationVariant::JumpShoot:
      return entityRes.texRunShoot ? entityRes.texRunShoot : entityRes.texShoot;
    case PresentationVariant::Hit:
      return entityRes.texHit ? entityRes.texHit : entityRes.texIdle;
    case PresentationVariant::Die:
      return entityRes.texDie ? entityRes.texDie : entityRes.texIdle;
    case PresentationVariant::RunAttack:
      return entityRes.texRunAttack ? entityRes.texRunAttack : entityRes.texRun;
    case PresentationVariant::Swing2:
      return entityRes.texAttack2 ? entityRes.texAttack2 : entityRes.texAttack;
    case PresentationVariant::Collapsing:
      if (objClass == ObjectClass::Material && entityRes.texDie) {
        return entityRes.texDie;
      }
    case PresentationVariant::Ultimate:
      if (entityRes.texUltimate) {
        return entityRes.texUltimate;
      }
      return entityRes.texAttack2 ? entityRes.texAttack2 : entityRes.texAttack;
    case PresentationVariant::Powerup:
      return entityRes.texPowerup ? entityRes.texPowerup : entityRes.texIdle;
    case PresentationVariant::Idle:
    default:
      break;
  }

  return entityRes.texIdle;
}

const EntityResources* findEntityResources(
  const game::GameResources& resources,
  SpriteType spriteType) {
  if (!resources.m_currLevel) {
    return nullptr;
  }

  const auto it = resources.m_currLevel->texCharacterMap.find(spriteType);
  return it == resources.m_currLevel->texCharacterMap.end() ? nullptr : &it->second;
}

SDL_FRect replicatedBaseFacing(const GameObject& obj) {
  SDL_FRect c = obj.baseCollider;
  if (obj.direction < 0.0f) {
    const float drawW = obj.spritePixelW / obj.drawScale;
    c.x = drawW - (c.x + c.w);
  }
  return c;
}

void widenReplicatedColliderForSwing(GameObject& obj) {
  const float drawW = obj.spritePixelW / obj.drawScale;
  const float extra = 0.2f * drawW;

  SDL_FRect c = replicatedBaseFacing(obj);
  c.w += extra;
  if (obj.direction < 0.0f) {
    c.x -= extra;
  }
  obj.collider = c;

}

void widenReplicatedColliderForUltimate(GameObject& obj) {
  const float drawW = obj.spritePixelW / obj.drawScale;
  const float drawH = obj.spritePixelH / obj.drawScale;
  obj.collider = SDL_FRect{
    -0.15f * drawW,
    -0.15f * drawH,
    drawW * 1.3f,
    drawH * 1.3f,
  };
}

void syncReplicatedCollider(GameObject& obj) {
  if (obj.objClass == ObjectClass::Player) {
    if (obj.data.player.state == PlayerState::ultimate) {
      widenReplicatedColliderForUltimate(obj);
    } else if (obj.data.player.state == PlayerState::swingWeapon) {
      widenReplicatedColliderForSwing(obj);
    } else {
      obj.collider = replicatedBaseFacing(obj);
    }
    return;
  }

  if (obj.objClass == ObjectClass::Enemy) {
    if (obj.data.enemy.state == EnemyState::attack) {
      widenReplicatedColliderForSwing(obj);
    } else {
      obj.collider = replicatedBaseFacing(obj);
    }
  }
}

void applyReplicatedAnimation(GameObject& obj, const game_engine::NetGameObjectSnapshot& snap) {
  if (snap.currentAnimation == UINT32_MAX) {
    obj.currentAnimation = -1;
    obj.spriteFrame = static_cast<int>(snap.spriteFrame);
    return;
  }

  const int animIndex = static_cast<int>(snap.currentAnimation);
  if (animIndex < 0 || animIndex >= static_cast<int>(obj.animations.size()) ||
      obj.animations[animIndex].getFrameCount() <= 0) {
    obj.currentAnimation = -1;
    obj.spriteFrame = static_cast<int>(snap.spriteFrame);
    return;
  }

  obj.currentAnimation = animIndex;
  obj.spriteFrame = static_cast<int>(snap.spriteFrame);
  obj.animations[obj.currentAnimation].setElapsed(snap.animElapsed, snap.animTimedOut);
}

void applyPresentation(game::GameResources& resources, GameObject& obj) {
  if (obj.objClass == ObjectClass::Projectile) {
    obj.texture = obj.presentationVariant == PresentationVariant::ProjectileHit
                    ? resources.texBulletHit
                    : resources.texBullet;
    return;
  }

  const EntityResources* entityRes = findEntityResources(resources, obj.spriteType);
  if (!entityRes) {
    obj.texture = nullptr;
    return;
  }

  obj.texture = pickEntityTexture(
    *entityRes,
    obj.objClass,
    obj.presentationVariant);
}

GameObject buildReplicatedObject(SimContext& ctx, const game_engine::NetGameObjectSnapshot& snap) {
  GameObject obj(128, 128);
  obj.id = snap.id;
  obj.objClass = snap.type;
  obj.spriteType = snap.spriteType;
  obj.presentationVariant = snap.presentationVariant;
  obj.position = snap.position;
  obj.renderPosition = snap.position;
  obj.renderPositionInitialized = true;
  obj.velocity = snap.velocity;
  obj.acceleration = snap.acceleration;
  obj.direction = snap.direction;
  obj.maxSpeedX = snap.maxSpeedX;
  obj.grounded = snap.grounded;
  obj.shouldFlash = snap.shouldFlash;
  obj.dynamic = true;

  if (snap.type == ObjectClass::Material) {
    if (snap.spriteType == SpriteType::FlyingStone) {
      obj.spritePixelW = 160;
      obj.spritePixelH = 128;
    }
    if (const EntityResources* entityRes = findEntityResources(ctx.resources, snap.spriteType)) {
      obj.animations = entityRes->anims;
    }
    obj.drawScale = snap.spriteType == SpriteType::FlyingStone ? 4.0f : 1.0f;
    if (snap.spriteType == SpriteType::FlyingStone) {
      obj.colliderNorm = {.x = 0.1f, .y = 0.0f, .w = 0.8f, .h = 1.0f};
      obj.applyScale();
    } else {
      obj.collider = {.x = 0, .y = 0, .w = obj.spritePixelW, .h = obj.spritePixelH};
      obj.baseCollider = obj.collider;
    }
    obj.data.material = snap.data.material;
  } else if (snap.type == ObjectClass::Projectile) {
    obj.drawScale = 2.0f;
    obj.colliderNorm = {.x = 0.0f, .y = 0.40f, .w = 0.5f, .h = 0.1f};
    obj.applyScale();
    obj.animations = ctx.resources.bulletAnims;
    obj.data.bullet = snap.data.bullet;
  } else {
    if (const EntityResources* entityRes = findEntityResources(ctx.resources, snap.spriteType)) {
      obj.animations = entityRes->anims;
    }

    if (snap.type == ObjectClass::Player) {
      obj.data.player = snap.data.player;
      obj.drawScale = 1.5f;
      float wFrac = 0.30f;
      float hFrac = 0.40f;
      obj.colliderNorm = {.x = 0.10f, .y = 0.9f - hFrac, .w = wFrac, .h = hFrac};
      switch (snap.spriteType) {
        case SpriteType::Player_Knight:
          obj.colliderNorm = {.x = 0.1f, .y = 0.5f, .w = wFrac, .h = 0.5f};
          break;
        case SpriteType::Player_Mage:
          obj.colliderNorm = {.x = 0.30f, .y = 0.5f, .w = wFrac, .h = 0.5f};
          break;
        case SpriteType::Player_Marie:
        case SpriteType::Player_Bonkfather:
          obj.colliderNorm = {.x = 0.30f, .y = 0.5f, .w = wFrac, .h = 0.5f};
          obj.drawScale = 2.0f;
          break;
        default:
          break;
      }
    } else {
      obj.data.enemy = snap.data.enemy;
      switch (snap.spriteType) {
        case SpriteType::Minotaur_1:
          obj.drawScale = 2.0f;
          break;
        case SpriteType::Skeleton_Warrior:
        case SpriteType::Red_Werewolf:
        case SpriteType::Skeleton_Pikeman:
          obj.drawScale = 1.5f;
          break;
        default:
          break;
      }
      obj.colliderNorm = {.x = 0.35f, .y = 0.4f, .w = 0.30f, .h = 0.6f};
    }
    obj.applyScale();
  }

  applyReplicatedAnimation(obj, snap);
  syncReplicatedCollider(obj);
  applyPresentation(ctx.resources, obj);
  return obj;
}

void updateReplicatedObject(
  SimContext& ctx,
  GameObject& obj,
  const game_engine::NetGameObjectSnapshot& snap) {
  const bool wasDead =
    obj.objClass == ObjectClass::Player ? obj.data.player.state == PlayerState::dead
                                        : obj.objClass == ObjectClass::Enemy
                                            ? obj.data.enemy.state == EnemyState::dead
                                            : false;
  const glm::vec2 previousPosition = obj.position;
  obj.spriteType = snap.spriteType;
  obj.presentationVariant = snap.presentationVariant;
  obj.position = snap.position;
  obj.velocity = snap.velocity;
  obj.acceleration = snap.acceleration;
  obj.direction = snap.direction;
  obj.maxSpeedX = snap.maxSpeedX;
  obj.grounded = snap.grounded;
  obj.shouldFlash = snap.shouldFlash;
  if (!obj.renderPositionInitialized) {
    obj.renderPosition = obj.position;
    obj.renderPositionInitialized = true;
  }

  if (obj.objClass == ObjectClass::Projectile) {
    obj.data.bullet = snap.data.bullet;
  } else if (obj.objClass == ObjectClass::Player) {
    obj.data.player = snap.data.player;
  } else if (obj.objClass == ObjectClass::Enemy) {
    obj.data.enemy = snap.data.enemy;
  } else if (obj.objClass == ObjectClass::Material) {
    obj.data.material = snap.data.material;
  }

  const bool isRespawn =
    obj.objClass == ObjectClass::Player &&
    wasDead &&
    snap.data.player.state != PlayerState::dead;
  const bool teleported = glm::length(obj.position - previousPosition) > 128.0f;
  if (!obj.renderPositionInitialized || isRespawn || teleported) {
    obj.renderPosition = obj.position;
    obj.renderPositionInitialized = true;
  }

  applyReplicatedAnimation(obj, snap);
  syncReplicatedCollider(obj);
  applyPresentation(ctx.resources, obj);
}

void purgeReplicatedActors(game_engine::GameState& gameState) {
  for (auto& layer : gameState.layers) {
    layer.erase(
      std::remove_if(
        layer.begin(),
        layer.end(),
        [](const GameObject& obj) {
          return obj.dynamic &&
                 (obj.objClass == ObjectClass::Player || obj.objClass == ObjectClass::Enemy);
        }),
      layer.end());
  }
  gameState.bullets.clear();
}

void reconcileReplicatedActors(
  SimContext& ctx,
  const game_engine::NetGameStateSnapshot& snapshot) {
  std::unordered_map<LayeredDynamicKey, std::size_t, LayeredDynamicKeyHash> existing;
  for (std::size_t layerIdx = 0; layerIdx < ctx.gameState.layers.size(); ++layerIdx) {
    auto& layer = ctx.gameState.layers[layerIdx];
    for (std::size_t objIdx = 0; objIdx < layer.size(); ++objIdx) {
      const auto& obj = layer[objIdx];
      if (obj.dynamic && (obj.objClass == ObjectClass::Player || obj.objClass == ObjectClass::Enemy || obj.objClass == ObjectClass::Material)) {
        existing[{static_cast<uint32_t>(layerIdx), obj.objClass, obj.id}] = objIdx;
      }
    }
  }

  std::unordered_set<LayeredDynamicKey, LayeredDynamicKeyHash> seen;
  for (const auto& [_, snap] : snapshot.m_gameObjects) {
    if (snap.type != ObjectClass::Player && snap.type != ObjectClass::Enemy && snap.type !=   ObjectClass::Material) {
      continue;
    }
    if (snap.layer >= ctx.gameState.layers.size()) {
      ctx.gameState.layers.resize(snap.layer + 1);
    }

    LayeredDynamicKey key{snap.layer, snap.type, snap.id};
    seen.insert(key);
    auto& targetLayer = ctx.gameState.layers[snap.layer];
    const auto it = existing.find(key);
    if (it == existing.end()) {
      targetLayer.push_back(buildReplicatedObject(ctx, snap));
      existing[key] = targetLayer.size() - 1;
    } else {
      updateReplicatedObject(ctx, targetLayer[it->second], snap);
    }
  }

  // remove objects
  for (std::size_t layerIdx = 0; layerIdx < ctx.gameState.layers.size(); ++layerIdx) {
    auto& layer = ctx.gameState.layers[layerIdx];
    layer.erase(
      std::remove_if(
        layer.begin(),
        layer.end(),
        [&seen, layerIdx](const GameObject& obj) {
          return obj.dynamic &&
                 (obj.objClass == ObjectClass::Player || obj.objClass == ObjectClass::Enemy || obj.objClass == ObjectClass::Material) &&
                 !seen.contains({static_cast<uint32_t>(layerIdx), obj.objClass, obj.id});
        }),
      layer.end());
  }
}

void reconcileReplicatedBullets(
  SimContext& ctx,
  const game_engine::NetGameStateSnapshot& snapshot) {
  std::unordered_map<uint32_t, std::size_t> existing;
  for (std::size_t idx = 0; idx < ctx.gameState.bullets.size(); ++idx) {
    existing[ctx.gameState.bullets[idx].id] = idx;
  }

  std::unordered_set<uint32_t> seen;
  for (const auto& [key, snap] : snapshot.m_gameObjects) {
    if (snap.type != ObjectClass::Projectile) {
      continue;
    }
    seen.insert(key.second);
    const auto it = existing.find(key.second);
    if (it == existing.end()) {
      ctx.gameState.bullets.push_back(buildReplicatedObject(ctx, snap));
      existing[key.second] = ctx.gameState.bullets.size() - 1;
    } else {
      updateReplicatedObject(ctx, ctx.gameState.bullets[it->second], snap);
    }
  }

  ctx.gameState.bullets.erase(
    std::remove_if(
      ctx.gameState.bullets.begin(),
      ctx.gameState.bullets.end(),
      [&seen](const GameObject& obj) { return !seen.contains(obj.id); }),
    ctx.gameState.bullets.end());
}

bool syncAuthoritativeLevel(
  SimContext& ctx,
  const game_engine::NetGameStateSnapshot& snapshot,
  bool& levelChanged) {
  levelChanged = false;
  if (ctx.gameState.currentLevelId == snapshot.levelId &&
      ctx.resources.m_currLevel &&
      ctx.resources.m_currLevelIdx == snapshot.levelId) {
    return true;
  }

  if (!game::switchToLevel(ctx.engine, ctx.resources, ctx.progService, snapshot.levelId)) {
    SDL_Log("Failed to synchronize client to authoritative level %u", static_cast<unsigned>(snapshot.levelId));
    return false;
  }

  levelChanged = true;
  ctx.gameState.currentView = UIManager::GameView::Playing;
  return true;
}

void applyAuthoritativeSnapshot(
  SimContext& ctx,
  const game_engine::NetGameStateSnapshot& snapshot,
  uint32_t localPlayerID,
  bool forceFullRebuild) {
  if (!ctx.resources.m_currLevel) {
    return;
  }

  ctx.gameState.m_stateLastUpdatedAt = snapshot.m_stateLastUpdatedAt;
  ctx.gameState.currentLevelId = snapshot.levelId;

  if (forceFullRebuild) {
    purgeReplicatedActors(ctx.gameState);
  }

  reconcileReplicatedActors(ctx, snapshot);
  reconcileReplicatedBullets(ctx, snapshot);

  ctx.gameState.playerIndex = -1;
  for (int layerIdx = 0; layerIdx < static_cast<int>(ctx.gameState.layers.size()); ++layerIdx) {
    for (int idx = 0; idx < static_cast<int>(ctx.gameState.layers[layerIdx].size()); ++idx) {
      const auto& obj = ctx.gameState.layers[layerIdx][idx];
      if (obj.objClass == ObjectClass::Player && obj.id == localPlayerID) {
        ctx.gameState.playerLayer = layerIdx;
        ctx.gameState.playerIndex = idx;
        return;
      }
    }
  }

  for (int layerIdx = 0; layerIdx < static_cast<int>(ctx.gameState.layers.size()); ++layerIdx) {
    for (int idx = 0; idx < static_cast<int>(ctx.gameState.layers[layerIdx].size()); ++idx) {
      if (ctx.gameState.layers[layerIdx][idx].objClass == ObjectClass::Player) {
        ctx.gameState.playerLayer = layerIdx;
        ctx.gameState.playerIndex = idx;
        return;
      }
    }
  }
}

void updateMapViewport(SimContext& ctx, GameObject& player) {
  if (!ctx.resources.m_currLevel || !ctx.resources.m_currLevel->map) {
    return;
  }

  const int mapWpx = ctx.resources.m_currLevel->map->mapWidth * ctx.resources.m_currLevel->map->tileWidth;
  const int mapHpx = ctx.resources.m_currLevel->map->mapHeight * ctx.resources.m_currLevel->map->tileHeight;

  ctx.gameState.mapViewport.x = std::clamp(
    (player.position.x + player.spritePixelW * 0.5f) - ctx.gameState.mapViewport.w * 0.5f,
    0.0f,
    std::max(0.0f, float(mapWpx - ctx.gameState.mapViewport.w)));

  ctx.gameState.mapViewport.y = std::clamp(
    (player.position.y + player.spritePixelH * 0.5f) - ctx.gameState.mapViewport.h * 0.5f,
    0.0f,
    std::max(0.0f, float(mapHpx - ctx.gameState.mapViewport.h)));
}

AudioStateMap captureAudioState(const game_engine::GameState& gameState) {
  AudioStateMap states;
  for (const auto& layer : gameState.layers) {
    for (const auto& obj : layer) {
      if (!obj.dynamic) {
        continue;
      }

      AudioObjectState state;
      state.objClass = obj.objClass;
      state.currentAnimation = obj.currentAnimation;
      if (obj.objClass == ObjectClass::Player) {
        state.playerState = obj.data.player.state;
        state.healthPoints = obj.data.player.healthPoints;
        state.manaPoints = obj.data.player.manaPoints;
        state.coinPickupCueCount = obj.data.player.coinPickupCueCount;
        state.gemPickupCueCount = obj.data.player.gemPickupCueCount;
        state.coinPurchaseCueCount = obj.data.player.coinPurchaseCueCount;
        state.gemPurchaseCueCount = obj.data.player.gemPurchaseCueCount;
        state.consumableUseCueCount = obj.data.player.consumableUseCueCount;
        state.jumpImpulseApplied = obj.data.player.jumpImpulseApplied;
      } else if (obj.objClass == ObjectClass::Enemy) {
        state.enemyState = obj.data.enemy.state;
        state.healthPoints = obj.data.enemy.healthPoints;
      }
      states[{obj.objClass, obj.id}] = state;
    }
  }

  for (const auto& bullet : gameState.bullets) {
    AudioObjectState state;
    state.objClass = ObjectClass::Projectile;
    state.currentAnimation = bullet.currentAnimation;
    state.bulletState = bullet.data.bullet.state;
    states[{bullet.objClass, bullet.id}] = state;
  }
  return states;
}

GameObject* findPlayerById(game_engine::GameState& gameState, uint32_t playerID) {
  for (auto& layer : gameState.layers) {
    for (auto& obj : layer) {
      if (obj.objClass == ObjectClass::Player && obj.id == playerID) {
        return &obj;
      }
    }
  }
  return nullptr;
}

InventorySnapshot captureInventorySnapshot(
  game_engine::GameState& gameState,
  uint32_t playerID) {
  if (GameObject* player = findPlayerById(gameState, playerID)) {
    return InventorySnapshot{
      .coins = player->data.player.inventory.coins.count,
      .gems = player->data.player.inventory.gems.count,
      .healthPotions = player->data.player.inventory.healthPotions.count,
      .manaPotions = player->data.player.inventory.manaPotions.count,
      .attackUps = player->data.player.inventory.attackUps.count,
      .defenceUps = player->data.player.inventory.defenceUps.count,
    };
  }

  return {};
}

void persistInventorySnapshot(
  game_engine::Engine& engine,
  game::ProgressionService& progService,
  const Inventory& inventory) {
  progService.updatePlayerInventory(inventory);
  engine.writeToSlotPath("slot_1", progService.serealizeSaveState());
}

bool localUltimateOneUnlocked(game_engine::GameState& gameState, uint32_t playerID) {
  if (GameObject* player = findPlayerById(gameState, playerID)) {
    return player->data.player.unlockedUltimateOne;
  }
  return false;
}

void applyLevelUnlockForFlyingStone(
  LevelIndex levelId,
  game_engine::GameState& gameState,
  game::ProgressionService& progService) {
  switch (levelId) {
    case LevelIndex::LEVEL_1:
      for (auto& layer : gameState.layers) {
        for (auto& obj : layer) {
          if (obj.objClass != ObjectClass::Player) {
            continue;
          }
          obj.data.player.unlockedUltimateOne = true;
          progService.unlockUltimateForChar(obj.spriteType, 1);
        }
      }
      break;
    default:
      break;
  }
}

void persistLocalUltimateUnlockIfNew(
  game_engine::Engine& engine,
  game_engine::GameState& gameState,
  game::ProgressionService& progService,
  uint32_t playerID,
  bool wasUnlocked) {
  if (wasUnlocked) {
    return;
  }

  GameObject* player = findPlayerById(gameState, playerID);
  if (!player || !player->data.player.unlockedUltimateOne) {
    return;
  }

  progService.unlockUltimateForChar(player->spriteType, 1);
  engine.writeToSlotPath("slot_1", progService.serealizeSaveState());
}

bool processPendingHostPlayerCommands(
  SimContext& ctx,
  uint32_t localPlayerID) {
  bool anyApplied = false;
  for (auto& netCommand : ctx.engine.consumePendingHostPlayerCommands()) {
    auto command = game::PlayerCommand::deserialize(netCommand.payload);
    if (!command) {
      continue;
    }

    PlayerData playerData;
    if (!ctx.engine.copyHostAuthoritativePlayerData(netCommand.playerID, playerData)) {
      continue;
    }

    const auto result = game::applyPlayerCommand(playerData, *command);
    if (!result.applied) {
      continue;
    }

    if (!ctx.engine.updateHostAuthoritativePlayerData(netCommand.playerID, playerData)) {
      continue;
    }

    anyApplied = true;
    if (netCommand.playerID == localPlayerID) {
      persistInventorySnapshot(ctx.engine, ctx.progService, playerData.inventory);
    }
  }

  if (anyApplied) {
    ctx.engine.broadcastHostSnapshot();
  }

  return anyApplied;
}

GameObject* findObjectByKey(game_engine::GameState& gameState, GameObjectKey key) {
  for (auto& layer : gameState.layers) {
    for (auto& obj : layer) {
      if (obj.objClass == key.first && obj.id == key.second) {
        return &obj;
      }
    }
  }

  for (auto& bullet : gameState.bullets) {
    if (bullet.objClass == key.first && bullet.id == key.second) {
      return &bullet;
    }
  }

  return nullptr;
}

int frozenImpactFrameFor(const GameObject& obj) {
  const bool hasActiveAnimation =
    obj.currentAnimation >= 0 &&
    obj.currentAnimation < static_cast<int>(obj.animations.size()) &&
    obj.animations[obj.currentAnimation].getFrameCount() > 0;
  if (!hasActiveAnimation) {
    return obj.spriteFrame;
  }

  if (obj.objClass == ObjectClass::Player) {
    switch (obj.currentAnimation) {
      case ANIM_SWING:
      case ANIM_RUN_ATTACK:
      case ANIM_SWING_2: {
        const int frameCount = obj.animations[obj.currentAnimation].getFrameCount();
        return std::max(1, (frameCount / 2) + 1);
      }
      default:
        break;
    }
  }

  return obj.animations[obj.currentAnimation].currentFrame() + 1;
}

void captureFrozenTarget(LocalHitStopTarget& out, GameObject& obj) {
  out.active = true;
  out.objClass = obj.objClass;
  out.id = obj.id;
  out.frozenRenderPosition =
    obj.renderPositionInitialized ? obj.renderPosition : obj.position;
  out.frozenSpriteFrame = frozenImpactFrameFor(obj);
}

void startLocalHitStop(
  game_engine::GameState& gameState,
  GameObjectKey attackerKey,
  GameObjectKey victimKey,
  float durationSeconds,
  uint32_t sequence = 0) {
  if (sequence != 0 && sequence <= gameState.localHitStop.lastSequence) {
    return;
  }

  gameState.localHitStop.remainingSeconds = durationSeconds;
  if (sequence != 0) {
    gameState.localHitStop.lastSequence = sequence;
  }
  for (auto& target : gameState.localHitStop.targets) {
    target = {};
  }

  std::size_t targetIndex = 0;
  if (attackerKey.first != ObjectClass::Projectile) {
    if (GameObject* attacker = findObjectByKey(gameState, attackerKey)) {
      captureFrozenTarget(gameState.localHitStop.targets[targetIndex++], *attacker);
    }
  }
  if (targetIndex < gameState.localHitStop.targets.size()) {
    if (GameObject* victim = findObjectByKey(gameState, victimKey)) {
      captureFrozenTarget(gameState.localHitStop.targets[targetIndex++], *victim);
    }
  }

  if (targetIndex == 0) {
    gameState.localHitStop.remainingSeconds = 0.0f;
  }
}

void startReplicatedHitStop(
  game_engine::GameState& gameState,
  const NetHitStopEvent& event) {
  if (!event.active) {
    return;
  }

  startLocalHitStop(
    gameState,
    {event.attackerClass, event.attackerId},
    {event.victimClass, event.victimId},
    game_engine::hitStopDurationSeconds(event.strength),
    event.sequence);
}

void stepLocalHitStop(game_engine::GameState& gameState, float deltaTime) {
  if (gameState.localHitStop.remainingSeconds <= 0.0f) {
    return;
  }

  gameState.localHitStop.remainingSeconds =
    std::max(0.0f, gameState.localHitStop.remainingSeconds - deltaTime);
  if (gameState.localHitStop.remainingSeconds > 0.0f) {
    return;
  }

  for (auto& target : gameState.localHitStop.targets) {
    target = {};
  }
}

void playLocalMultiplayerFireAudio(
  game_engine::Engine& engine,
  game::GameResources& resources,
  game_engine::GameState& gameState,
  float deltaTime) {
  resources.whooshCooldown.step(deltaTime);

  auto* client = engine.getGameClient();
  if (!client || !client->IsRegistered()) {
    return;
  }

  GameObject* localPlayer = findPlayerById(gameState, client->GetPlayerID());
  if (!localPlayer || localPlayer->objClass != ObjectClass::Player) {
    return;
  }

  if (localPlayer->data.player.state == PlayerState::dead) {
    return;
  }

  const auto& localInput = engine.getLocalInput();
  if (!localInput.fireHeld || !resources.audioShoot || !resources.whooshCooldown.isTimedOut()) {
    return;
  }

  if (localPlayer->data.player.manaPoints <= 10) {
    return;
  }

  resources.whooshCooldown.reset();
  MIX_PlayAudio(resources.mixer, resources.audioShoot);
}

void playSimulationAudio(
  game::GameResources& resources,
  const AudioStateMap& before,
  game_engine::GameState& gameState,
  uint32_t localPlayerID,
  float deltaTime,
  bool playLocalShotAudioFromStateDiff = false) {
  resources.stepAudioCooldown.step(deltaTime);

  GameObject* localPlayer = findPlayerById(gameState, localPlayerID);
  bool localPlayerWasSwinging = false;
  // For Multiplayer, audio you want only local player to hear
  if (localPlayer) {
    const auto beforeIt = before.find({ObjectClass::Player, localPlayerID});
    if (beforeIt != before.end()) {
      const auto& prev = beforeIt->second;
      localPlayerWasSwinging =
        prev.playerState == PlayerState::swingWeapon ||
        prev.currentAnimation == ANIM_SWING ||
        prev.currentAnimation == ANIM_RUN_ATTACK ||
        prev.currentAnimation == ANIM_SWING_2;
      if (!prev.jumpImpulseApplied && localPlayer->data.player.jumpImpulseApplied && resources.audioJump) {
        MIX_PlayAudio(resources.mixer, resources.audioJump);
      }

      const bool startedSwing =
        localPlayer->currentAnimation != prev.currentAnimation &&
        (localPlayer->currentAnimation == ANIM_SWING ||
         localPlayer->currentAnimation == ANIM_RUN_ATTACK ||
         localPlayer->currentAnimation == ANIM_SWING_2);
      if (startedSwing && resources.audioSword1) {
        MIX_PlayAudio(resources.mixer, resources.audioSword1);
      }

      const bool startedUltimate =
        (prev.playerState != PlayerState::ultimate &&
         localPlayer->data.player.state == PlayerState::ultimate) ||
        (localPlayer->currentAnimation != prev.currentAnimation &&
         localPlayer->currentAnimation == ANIM_ULTIMATE);
      if (startedUltimate && resources.audioUltimateAttack) {
        MIX_PlayAudio(resources.mixer, resources.audioUltimateAttack);
      }

      if (playLocalShotAudioFromStateDiff &&
          localPlayer->data.player.manaPoints < prev.manaPoints &&
          resources.audioShoot) {
        MIX_PlayAudio(resources.mixer, resources.audioShoot);
      }

      if (localPlayer->data.player.coinPickupCueCount > prev.coinPickupCueCount &&
          resources.audioCoinCollect) {
        MIX_PlayAudio(resources.mixer, resources.audioCoinCollect);
      }
      if (localPlayer->data.player.gemPickupCueCount > prev.gemPickupCueCount &&
          resources.audioGemCollect) {
        MIX_PlayAudio(resources.mixer, resources.audioGemCollect);
      }
      if (localPlayer->data.player.coinPurchaseCueCount > prev.coinPurchaseCueCount &&
          resources.audioCoinPurchase) {
        MIX_PlayAudio(resources.mixer, resources.audioCoinPurchase);
      }
      if (localPlayer->data.player.gemPurchaseCueCount > prev.gemPurchaseCueCount &&
          resources.audioGemPurchase) {
        MIX_PlayAudio(resources.mixer, resources.audioGemPurchase);
      }
      if (localPlayer->data.player.consumableUseCueCount > prev.consumableUseCueCount &&
          resources.audioDrinkSlurp) {
        MIX_PlayAudio(resources.mixer, resources.audioDrinkSlurp);
      }

      if (localPlayer->data.player.healthPoints < prev.healthPoints && resources.boneImpactHitTrack) {
        MIX_PlayTrack(resources.boneImpactHitTrack, 0);
      }
    }

    if (localPlayer->data.player.state == PlayerState::running &&
        localPlayer->grounded &&
        resources.m_currLevel &&
        resources.m_currLevel->audioStep &&
        resources.stepAudioCooldown.isTimedOut()) {
      resources.stepAudioCooldown.reset();
      MIX_PlayAudio(resources.mixer, resources.m_currLevel->audioStep);
    }
  }

  bool bulletCollided = false;
  bool enemyDamaged = false;
  bool enemyDied = false;
  bool enemyDamagedByMelee = false;
  bool localPlayerDied = false;
  AudioStateMap after = captureAudioState(gameState);
  for (const auto& [key, prev] : before) {
    const auto afterIt = after.find(key);
    if (afterIt == after.end()) {
      continue;
    }

    const auto& curr = afterIt->second;
    if (key.first == ObjectClass::Projectile &&
        prev.bulletState != BulletState::colliding &&
        curr.bulletState == BulletState::colliding) {
      bulletCollided = true;
    }
    if (key.first == ObjectClass::Enemy && curr.healthPoints < prev.healthPoints) {
      enemyDamaged = true;
      if (localPlayer &&
          (localPlayer->data.player.state == PlayerState::swingWeapon || localPlayerWasSwinging)) {
        enemyDamagedByMelee = true;
      }
      if (curr.enemyState == EnemyState::dead) {
        enemyDied = true;
      }
    }
    if (key.first == ObjectClass::Player &&
        key.second == localPlayerID &&
        prev.playerState != PlayerState::dead &&
        curr.playerState == PlayerState::dead) {
      localPlayerDied = true;
    }
  }

  if (localPlayerDied && resources.audioEnemyDie) {
    MIX_PlayAudio(resources.mixer, resources.audioEnemyDie);
  }
  if (enemyDied && resources.audioEnemyDie) {
    MIX_PlayAudio(resources.mixer, resources.audioEnemyDie);
  }
  if (enemyDamagedByMelee && resources.boneImpactHitTrack) {
    MIX_PlayTrack(resources.boneImpactHitTrack, 0);
  } else if (enemyDamaged && resources.enemyProjectileHitTrack) {
    MIX_PlayTrack(resources.enemyProjectileHitTrack, 0);
  } else if (bulletCollided && resources.hitTrack) {
    MIX_PlayTrack(resources.hitTrack, 0);
  }
}

void refreshPresentation(game::GameResources& resources, game_engine::GameState& gameState) {
  for (auto& layer : gameState.layers) {
    for (auto& obj : layer) {
      if (obj.objClass == ObjectClass::Player ||
          obj.objClass == ObjectClass::Enemy ||
          obj.objClass == ObjectClass::Material) {
        applyPresentation(resources, obj);
      }
    }
  }

  for (auto& bullet : gameState.bullets) {
    applyPresentation(resources, bullet);
  }
}

void resetActiveBossEncounter(game_engine::GameState& gameState) {
  for (auto& layer : gameState.layers) {
    for (auto& obj : layer) {
      if (obj.objClass != ObjectClass::Enemy ||
          !obj.data.enemy.isBoss ||
          !obj.data.enemy.shouldDisplayHP) {
        continue;
      }

      auto& boss = obj.data.enemy;
      boss.healthPoints = boss.maxHealthPoints;
      boss.shouldDisplayHP = false;
      boss.state = EnemyState::idle;
      boss.hitStopRemainingSeconds = 0.0f;
      boss.pendingKnockbackDirection = 0.0f;
      boss.pendingKnockbackMagnitude = 0.0f;
      boss.hasPendingKnockback = false;

      obj.velocity = glm::vec2(0.0f);
      obj.presentationVariant = PresentationVariant::Idle;
      obj.spriteFrame = 1;
      if (ANIM_IDLE >= 0 &&
          ANIM_IDLE < static_cast<int>(obj.animations.size()) &&
          obj.animations[ANIM_IDLE].getFrameCount() > 0) {
        obj.currentAnimation = ANIM_IDLE;
        obj.animations[ANIM_IDLE].reset();
      } else {
        obj.currentAnimation = -1;
      }
    }
  }
}

bool hasActiveBossHealthBar(const game_engine::GameState& gameState) {
  for (const auto& layer : gameState.layers) {
    for (const auto& obj : layer) {
      if (obj.objClass == ObjectClass::Enemy &&
          obj.data.enemy.isBoss &&
          obj.data.enemy.shouldDisplayHP) {
        return true;
      }
    }
  }
  return false;
}

void updateLevelCombatSoundtrack(
  game_engine::Engine& engine,
  game::GameResources& resources,
  const game_engine::GameState& gameState) {
  if (!resources.m_currLevel) {
    return;
  }

  const bool bossActive = hasActiveBossHealthBar(gameState) &&
                          resources.m_currLevel->bossTrack != nullptr;
  if (bossActive) {
    engine.stopAudioSoundtrack(resources.m_currLevel->backgroundTrack);
    engine.setAudioSoundtrack(resources.m_currLevel->bossTrack);
    return;
  }

  engine.stopAudioSoundtrack(resources.m_currLevel->bossTrack);
  engine.setAudioSoundtrack(resources.m_currLevel->backgroundTrack);
}

class DefaultSimulationSystem final : public game::ISimulationSystem {
public:
  void update(
    game_engine::Engine& engine,
    game::GameResources& resources,
    game::ProgressionService& progService,
    float deltaTime,
    const UIManager::UIActions& actions) override {

    SimContext ctx{engine, engine.getGameState(), resources, progService};
    const UIManager::GameView previousView = ctx.gameState.currentView;
    stepLocalHitStop(ctx.gameState, deltaTime);

    if (ctx.gameState.currentView == UIManager::GameView::LevelLoading) {
      return;
    }

    if (engine.isMultiplayerActive()) {
      if (auto* client = engine.getGameClient()) {
        if (ctx.gameState.currentView == UIManager::GameView::Playing ||
            ctx.gameState.currentView == UIManager::GameView::PauseMenu ||
            ctx.gameState.currentView == UIManager::GameView::ShopMenu ||
            ctx.gameState.currentView == UIManager::GameView::InventoryMenu) {
          updateLevelCombatSoundtrack(engine, resources, ctx.gameState);
        }
        playLocalMultiplayerFireAudio(engine, resources, ctx.gameState, deltaTime);

        if (engine.isHostMode()) {
          if (const auto nextLevel = engine.consumePendingHostLevelTransition()) {
            if (!game::switchToLevel(engine, resources, progService, *nextLevel)) {
              SDL_Log(
                "Failed to switch host to authoritative level %u",
                static_cast<unsigned>(*nextLevel));
              return;
            }
            ctx.gameState.currentView = UIManager::GameView::Playing;
            engine.synchronizeHostAuthoritativeState(true);
            engine.broadcastHostSnapshot(); // special case broadcast when switching levels
            game_engine::NetGameStateSnapshot hostSnapshot;

            if (engine.copyHostSnapshot(hostSnapshot)) {

              applyAuthoritativeSnapshot(
                ctx,
                hostSnapshot,
                client->GetPlayerID(),
                true);

              startReplicatedHitStop(ctx.gameState, hostSnapshot.hitStopEvent);

              if (ctx.gameState.playerIndex >= 0) {
                updateMapViewport(ctx, engine.getPlayer());
              }

              if (client->NeedsFullRebuild()) {
                client->MarkFullRebuildApplied();
              }
            }
          }

          (void)processPendingHostPlayerCommands(ctx, client->GetPlayerID());
        }

        const AudioStateMap before = captureAudioState(ctx.gameState);
        const InventorySnapshot beforeInventory =
          captureInventorySnapshot(ctx.gameState, client->GetPlayerID());
        const bool localUltimateWasUnlocked =
          localUltimateOneUnlocked(ctx.gameState, client->GetPlayerID());

        // read in GameState snapshot coming from the server
        client->ProcessServerMessages();
        game_engine::NetGameStateSnapshot latestSnapshot;
        if (client->CopyLatestSnapshot(latestSnapshot)) {
          bool levelChanged = false;
          if (!syncAuthoritativeLevel(ctx, latestSnapshot, levelChanged)) {
            return;
          }

          const bool forceFullRebuild = client->NeedsFullRebuild() || levelChanged;
          applyAuthoritativeSnapshot(
            ctx,
            latestSnapshot,
            client->GetPlayerID(),
            forceFullRebuild);
          persistLocalUltimateUnlockIfNew(
            ctx.engine,
            ctx.gameState,
            ctx.progService,
            client->GetPlayerID(),
            localUltimateWasUnlocked);
          startReplicatedHitStop(ctx.gameState, latestSnapshot.hitStopEvent);
          if (client->NeedsFullRebuild()) {
            client->MarkFullRebuildApplied();
          }
          if (engine.isClientMode()) {
            const InventorySnapshot afterInventory =
              captureInventorySnapshot(ctx.gameState, client->GetPlayerID());
            if (!(afterInventory == beforeInventory)) {
              if (GameObject* localPlayer = findPlayerById(ctx.gameState, client->GetPlayerID())) {
                persistInventorySnapshot(
                  ctx.engine,
                  ctx.progService,
                  localPlayer->data.player.inventory);
              }
            }
          }
          playSimulationAudio(
            resources,
            before,
            ctx.gameState,
            client->GetPlayerID(),
            deltaTime,
            false);
          updateLevelCombatSoundtrack(engine, resources, ctx.gameState);
          if (ctx.gameState.playerIndex >= 0) {
            auto& player = engine.getPlayer();
            updateMapViewport(ctx, player);
            if (player.data.player.state == PlayerState::dead && player.currentAnimation == -1) {
              ctx.gameState.currentView = UIManager::GameView::GameOver;
              if (previousView != UIManager::GameView::GameOver) {
                ctx.engine.setAudioSoundtrack(
                  ctx.resources.m_currLevel ? ctx.resources.m_currLevel->gameOverAudioTrack : nullptr,
                  0);
              }
            } else if (ctx.gameState.currentView == UIManager::GameView::GameOver &&
                       player.data.player.state != PlayerState::dead) {
              ctx.gameState.currentView = UIManager::GameView::Playing;
            }
          }
        }
      }
      return;
    }

    if (ctx.gameState.currentView != UIManager::GameView::Playing &&
        ctx.gameState.currentView != UIManager::GameView::PauseMenu &&
        ctx.gameState.currentView != UIManager::GameView::ShopMenu &&
        ctx.gameState.currentView != UIManager::GameView::InventoryMenu) {
      return;
    }

    updateLevelCombatSoundtrack(engine, resources, ctx.gameState);

    if (!actions.blockGameplayUpdates && ctx.gameState.playerIndex >= 0) {
      const AudioStateMap before = captureAudioState(ctx.gameState);
      std::unordered_map<uint32_t, game_engine::NetGameInput> playerInputs;
      playerInputs.emplace(engine.getPlayer().id, engine.getLocalInput());

      game_engine::GameplaySimulationHooks hooks;
      hooks.onPortalTriggered = [&](LevelIndex nextLevel) {
        game::switchToLevel(engine, resources, progService, nextLevel);
      };
      hooks.onHitConfirmed = [&](GameObjectKey attacker, GameObjectKey victim, HitStopStrength strength) {
        startLocalHitStop(
          ctx.gameState,
          attacker,
          victim,
          game_engine::hitStopDurationSeconds(strength));
      };
      hooks.onFlyingStoneCollected = [&](LevelIndex levelId) {
        applyLevelUnlockForFlyingStone(levelId, ctx.gameState, progService);
        engine.writeToSlotPath("slot_1", progService.serealizeSaveState());
      };
      hooks.cullProjectilesByViewport = true;
      hooks.projectileViewport = ctx.gameState.mapViewport;
      game_engine::stepGameplaySimulation(ctx.gameState, playerInputs, deltaTime, hooks);
      refreshPresentation(resources, ctx.gameState);
      updateLevelCombatSoundtrack(engine, resources, ctx.gameState);
      playSimulationAudio(resources, before, ctx.gameState, engine.getPlayer().id, deltaTime, true);
      updateMapViewport(ctx, engine.getPlayer());
    }

    const bool enteredGameOver =
      ctx.gameState.currentView == UIManager::GameView::GameOver &&
      previousView != UIManager::GameView::GameOver;
    if (enteredGameOver) {
      resetActiveBossEncounter(ctx.gameState);
      if (resources.m_currLevel) {
        engine.stopAudioSoundtrack(resources.m_currLevel->backgroundTrack);
        engine.stopAudioSoundtrack(resources.m_currLevel->bossTrack);
      }
      engine.setAudioSoundtrack(
        resources.m_currLevel ? resources.m_currLevel->gameOverAudioTrack : nullptr,
        0);
    } else if (ctx.gameState.currentView != UIManager::GameView::GameOver &&
               ctx.gameState.evaluateGameOver()) {
      resetActiveBossEncounter(ctx.gameState);
      if (resources.m_currLevel) {
        engine.stopAudioSoundtrack(resources.m_currLevel->backgroundTrack);
        engine.stopAudioSoundtrack(resources.m_currLevel->bossTrack);
      }
      engine.setAudioSoundtrack(
        resources.m_currLevel ? resources.m_currLevel->gameOverAudioTrack : nullptr,
        0);
    }
  }
};

} // namespace

namespace game {

std::unique_ptr<ISimulationSystem> createDefaultSimulationSystem() {
  return std::make_unique<DefaultSimulationSystem>();
}

} // namespace game
