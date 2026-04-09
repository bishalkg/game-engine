#include "game/default_systems.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "engine/engine.h"
#include "engine/gameplay_simulation.h"

namespace {

using game::EntityResources;

struct SimContext {
  game_engine::Engine& engine;
  game_engine::GameState& gameState;
  game::GameResources& resources;
};

struct AudioObjectState {
  ObjectClass objClass = ObjectClass::Level;
  int currentAnimation = -1;
  PlayerState playerState = PlayerState::idle;
  EnemyState enemyState = EnemyState::idle;
  BulletState bulletState = BulletState::inactive;
  int healthPoints = 0;
  int manaPoints = 0;
  bool jumpImpulseApplied = false;
};

using AudioStateMap =
  std::unordered_map<game_engine::GameObjectKey, AudioObjectState, game_engine::GameObjectKeyHash>;

SDL_Texture* pickEntityTexture(
  const EntityResources& entityRes,
  ObjectClass objClass,
  const ObjectData& data,
  int currentAnimation) {
  if (objClass == ObjectClass::Projectile) {
    return nullptr;
  }

  switch (currentAnimation) {
    case ANIM_RUN:
      return entityRes.texRun ? entityRes.texRun : entityRes.texWalk;
    case ANIM_SHOOT:
      return entityRes.texShoot ? entityRes.texShoot : entityRes.texIdle;
    case ANIM_SLIDE:
      return entityRes.texSlide ? entityRes.texSlide : entityRes.texRun;
    case ANIM_SLIDE_SHOOT:
      return entityRes.texSlideShoot ? entityRes.texSlideShoot : entityRes.texSlide;
    case ANIM_SWING:
      return entityRes.texAttack ? entityRes.texAttack : entityRes.texIdle;
    case ANIM_JUMP:
      return entityRes.texJump ? entityRes.texJump : entityRes.texRun;
    case ANIM_HIT:
      return entityRes.texHit ? entityRes.texHit : entityRes.texIdle;
    case ANIM_DIE:
      return entityRes.texDie ? entityRes.texDie : entityRes.texIdle;
    case ANIM_RUN_ATTACK:
      return entityRes.texRunAttack ? entityRes.texRunAttack : entityRes.texRun;
    case ANIM_SWING_2:
      return entityRes.texAttack2 ? entityRes.texAttack2 : entityRes.texAttack;
    case ANIM_IDLE:
    default:
      break;
  }

  if (objClass == ObjectClass::Player) {
    switch (data.player.state) {
      case PlayerState::jumping:
        return entityRes.texJump ? entityRes.texJump : entityRes.texRun;
      case PlayerState::hurt:
        return entityRes.texHit ? entityRes.texHit : entityRes.texIdle;
      case PlayerState::dead:
        return entityRes.texDie ? entityRes.texDie : entityRes.texIdle;
      default:
        return entityRes.texIdle;
    }
  }

  switch (data.enemy.state) {
    case EnemyState::hurt:
      return entityRes.texHit ? entityRes.texHit : entityRes.texIdle;
    case EnemyState::dead:
      return entityRes.texDie ? entityRes.texDie : entityRes.texIdle;
    case EnemyState::attack:
      return entityRes.texAttack ? entityRes.texAttack : entityRes.texRun;
    default:
      return entityRes.texIdle;
  }
}

void applyReplicatedAnimation(GameObject& obj, const game_engine::NetGameObjectSnapshot& snap) {
  if (snap.currentAnimation == UINT32_MAX) {
    obj.currentAnimation = -1;
    obj.spriteFrame = static_cast<int>(snap.spriteFrame);
    return;
  }

  obj.currentAnimation = static_cast<int>(snap.currentAnimation);
  obj.spriteFrame = static_cast<int>(snap.spriteFrame);
  if (obj.currentAnimation >= 0 && obj.currentAnimation < static_cast<int>(obj.animations.size())) {
    obj.animations[obj.currentAnimation].setElapsed(snap.animElapsed);
  }
}

void applyPresentation(game::GameResources& resources, GameObject& obj) {
  if (obj.objClass == ObjectClass::Projectile) {
    obj.texture = obj.currentAnimation == ANIM_RUN ? resources.texBulletHit : resources.texBullet;
    return;
  }

  const auto it = resources.m_currLevel->texCharacterMap.find(obj.spriteType);
  if (it == resources.m_currLevel->texCharacterMap.end()) {
    obj.texture = nullptr;
    return;
  }

  obj.texture = pickEntityTexture(
    it->second,
    obj.objClass,
    obj.data,
    obj.currentAnimation >= 0 ? obj.currentAnimation : ANIM_IDLE);
}

GameObject buildReplicatedObject(SimContext& ctx, const game_engine::NetGameObjectSnapshot& snap) {
  GameObject obj(128, 128);
  obj.id = snap.id;
  obj.objClass = snap.type;
  obj.spriteType = snap.spriteType;
  obj.position = snap.position;
  obj.velocity = snap.velocity;
  obj.acceleration = snap.acceleration;
  obj.direction = snap.direction;
  obj.maxSpeedX = snap.maxSpeedX;
  obj.grounded = snap.grounded;
  obj.shouldFlash = snap.shouldFlash;
  obj.dynamic = true;

  if (snap.type == ObjectClass::Projectile) {
    obj.drawScale = 2.0f;
    obj.colliderNorm = {.x = 0.0f, .y = 0.40f, .w = 0.5f, .h = 0.1f};
    obj.applyScale();
    obj.animations = ctx.resources.bulletAnims;
    obj.data.bullet = snap.data.bullet;
  } else {
    const auto& entityRes = ctx.resources.m_currLevel->texCharacterMap.at(snap.spriteType);
    obj.animations = entityRes.anims;

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
  applyPresentation(ctx.resources, obj);
  return obj;
}

void updateReplicatedObject(
  SimContext& ctx,
  GameObject& obj,
  const game_engine::NetGameObjectSnapshot& snap) {
  obj.spriteType = snap.spriteType;
  obj.position = snap.position;
  obj.velocity = snap.velocity;
  obj.acceleration = snap.acceleration;
  obj.direction = snap.direction;
  obj.maxSpeedX = snap.maxSpeedX;
  obj.grounded = snap.grounded;
  obj.shouldFlash = snap.shouldFlash;

  if (obj.objClass == ObjectClass::Projectile) {
    obj.data.bullet = snap.data.bullet;
  } else if (obj.objClass == ObjectClass::Player) {
    obj.data.player = snap.data.player;
  } else if (obj.objClass == ObjectClass::Enemy) {
    obj.data.enemy = snap.data.enemy;
  }

  applyReplicatedAnimation(obj, snap);
  applyPresentation(ctx.resources, obj);
}

void reconcileReplicatedLayer(
  SimContext& ctx,
  const game_engine::NetGameStateSnapshot& snapshot) {
  auto& layer = ctx.gameState.layers[ctx.gameState.playerLayer];
  std::unordered_map<game_engine::GameObjectKey, std::size_t, game_engine::GameObjectKeyHash> existing;
  for (std::size_t idx = 0; idx < layer.size(); ++idx) {
    const auto& obj = layer[idx];
    if (obj.dynamic && (obj.objClass == ObjectClass::Player || obj.objClass == ObjectClass::Enemy)) {
      existing[{obj.objClass, obj.id}] = idx;
    }
  }

  std::unordered_set<game_engine::GameObjectKey, game_engine::GameObjectKeyHash> seen;
  for (const auto& [key, snap] : snapshot.m_gameObjects) {
    if (snap.type != ObjectClass::Player && snap.type != ObjectClass::Enemy) {
      continue;
    }
    seen.insert(key);
    const auto it = existing.find(key);
    if (it == existing.end()) {
      layer.push_back(buildReplicatedObject(ctx, snap));
      existing[key] = layer.size() - 1;
    } else {
      updateReplicatedObject(ctx, layer[it->second], snap);
    }
  }

  layer.erase(
    std::remove_if(
      layer.begin(),
      layer.end(),
      [&seen](const GameObject& obj) {
        return obj.dynamic &&
               (obj.objClass == ObjectClass::Player || obj.objClass == ObjectClass::Enemy) &&
               !seen.contains({obj.objClass, obj.id});
      }),
    layer.end());
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

void applyAuthoritativeSnapshot(
  SimContext& ctx,
  const game_engine::NetGameStateSnapshot& snapshot,
  uint32_t localPlayerID) {
  if (!ctx.resources.m_currLevel || ctx.gameState.playerLayer < 0 ||
      ctx.gameState.playerLayer >= static_cast<int>(ctx.gameState.layers.size())) {
    return;
  }

  ctx.gameState.m_stateLastUpdatedAt = snapshot.m_stateLastUpdatedAt;
  ctx.gameState.currentLevelId = snapshot.levelId;

  reconcileReplicatedLayer(ctx, snapshot);
  reconcileReplicatedBullets(ctx, snapshot);

  ctx.gameState.playerIndex = -1;
  for (int idx = 0; idx < static_cast<int>(ctx.gameState.layers[ctx.gameState.playerLayer].size()); ++idx) {
    const auto& obj = ctx.gameState.layers[ctx.gameState.playerLayer][idx];
    if (obj.objClass == ObjectClass::Player && obj.id == localPlayerID) {
      ctx.gameState.playerIndex = idx;
      break;
    }
  }
  if (ctx.gameState.playerIndex == -1) {
    for (int idx = 0; idx < static_cast<int>(ctx.gameState.layers[ctx.gameState.playerLayer].size()); ++idx) {
      if (ctx.gameState.layers[ctx.gameState.playerLayer][idx].objClass == ObjectClass::Player) {
        ctx.gameState.playerIndex = idx;
        break;
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
  if (gameState.playerLayer < 0 || gameState.playerLayer >= static_cast<int>(gameState.layers.size())) {
    return nullptr;
  }
  for (auto& obj : gameState.layers[gameState.playerLayer]) {
    if (obj.objClass == ObjectClass::Player && obj.id == playerID) {
      return &obj;
    }
  }
  return nullptr;
}

void playSimulationAudio(
  game::GameResources& resources,
  const AudioStateMap& before,
  game_engine::GameState& gameState,
  uint32_t localPlayerID,
  float deltaTime) {
  resources.stepAudioCooldown.step(deltaTime);
  resources.whooshCooldown.step(deltaTime);

  GameObject* localPlayer = findPlayerById(gameState, localPlayerID);
  if (localPlayer) {
    const auto beforeIt = before.find({ObjectClass::Player, localPlayerID});
    if (beforeIt != before.end()) {
      const auto& prev = beforeIt->second;
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

      if (localPlayer->data.player.manaPoints < prev.manaPoints &&
          resources.audioShoot &&
          resources.whooshCooldown.isTimedOut()) {
        resources.whooshCooldown.reset();
        MIX_PlayAudio(resources.mixer, resources.audioShoot);
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
      if (curr.enemyState == EnemyState::dead) {
        enemyDied = true;
      }
    }
  }

  if (enemyDied && resources.audioEnemyDie) {
    MIX_PlayAudio(resources.mixer, resources.audioEnemyDie);
  } else if (enemyDamaged && resources.enemyProjectileHitTrack) {
    MIX_PlayTrack(resources.enemyProjectileHitTrack, 0);
  } else if (bulletCollided && resources.hitTrack) {
    MIX_PlayTrack(resources.hitTrack, 0);
  }
}

void refreshPresentation(game::GameResources& resources, game_engine::GameState& gameState) {
  for (auto& layer : gameState.layers) {
    for (auto& obj : layer) {
      if (obj.objClass == ObjectClass::Player || obj.objClass == ObjectClass::Enemy) {
        applyPresentation(resources, obj);
      }
    }
  }

  for (auto& bullet : gameState.bullets) {
    applyPresentation(resources, bullet);
  }
}

class DefaultSimulationSystem final : public game::ISimulationSystem {
public:
  void update(
    game_engine::Engine& engine,
    game::GameResources& resources,
    float deltaTime,
    const UIManager::UIActions& actions) override {
    SimContext ctx{engine, engine.getGameState(), resources};

    if (ctx.gameState.currentView == UIManager::GameView::LevelLoading) {
      return;
    }

    if (engine.isMultiplayerActive()) {
      if (auto* client = engine.getGameClient()) {
        const AudioStateMap before = captureAudioState(ctx.gameState);
        client->ProcessServerMessages();
        game_engine::NetGameStateSnapshot latestSnapshot;
        if (client->CopyLatestSnapshot(latestSnapshot)) {
          applyAuthoritativeSnapshot(ctx, latestSnapshot, client->GetPlayerID());
          playSimulationAudio(resources, before, ctx.gameState, client->GetPlayerID(), deltaTime);
          if (ctx.gameState.playerIndex >= 0) {
            auto& player = engine.getPlayer();
            updateMapViewport(ctx, player);
            if (player.data.player.state == PlayerState::dead && player.currentAnimation == -1) {
              ctx.gameState.currentView = UIManager::GameView::GameOver;
            }
          }
        }
      }
      return;
    }

    if (ctx.gameState.currentView != UIManager::GameView::Playing &&
        ctx.gameState.currentView != UIManager::GameView::PauseMenu) {
      return;
    }

    engine.setAudioSoundtrack(
      resources.m_currLevel ? resources.m_currLevel->backgroundTrack : nullptr);

    if (!actions.blockGameplayUpdates && ctx.gameState.playerIndex >= 0) {
      const AudioStateMap before = captureAudioState(ctx.gameState);
      std::unordered_map<uint32_t, game_engine::NetGameInput> playerInputs;
      playerInputs.emplace(engine.getPlayer().id, engine.getLocalInput());

      game_engine::GameplaySimulationHooks hooks;
      hooks.onPortalTriggered = [&](LevelIndex nextLevel) {
        game::switchToLevel(engine, resources, nextLevel);
      };
      hooks.cullProjectilesByViewport = true;
      hooks.projectileViewport = ctx.gameState.mapViewport;
      game_engine::stepGameplaySimulation(ctx.gameState, playerInputs, deltaTime, hooks);
      refreshPresentation(resources, ctx.gameState);
      playSimulationAudio(resources, before, ctx.gameState, engine.getPlayer().id, deltaTime);
      updateMapViewport(ctx, engine.getPlayer());
    }

    if (ctx.gameState.currentView == UIManager::GameView::GameOver) {
      engine.setAudioSoundtrack(
        resources.m_currLevel ? resources.m_currLevel->gameOverAudioTrack : nullptr,
        0);
    } else if (ctx.gameState.evaluateGameOver()) {
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
