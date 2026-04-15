#include <cassert>
#include <cmath>
#include <iostream>
#include <unordered_map>

#include "engine/engine.h"
#include "engine/gameobject.h"
#include "engine/gameplay_simulation.h"
#include "engine/net/game_net_common.h"

namespace {

bool closeVec2(const glm::vec2& a, const glm::vec2& b, float eps = 1e-5f) {
  return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps;
}

bool closeRect(const SDL_FRect& a, const SDL_FRect& b, float eps = 1e-4f) {
  return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps &&
         std::fabs(a.w - b.w) < eps && std::fabs(a.h - b.h) < eps;
}

bool equalSnapshots(const game_engine::NetGameObjectSnapshot& a,
                    const game_engine::NetGameObjectSnapshot& b) {
  if (a.id != b.id || a.layer != b.layer || a.type != b.type || a.spriteType != b.spriteType) return false;
  if (!closeVec2(a.position, b.position) || !closeVec2(a.velocity, b.velocity) ||
      !closeVec2(a.acceleration, b.acceleration)) return false;
  if (a.spriteFrame != b.spriteFrame || a.currentAnimation != b.currentAnimation) return false;
  if (std::fabs(a.animElapsed - b.animElapsed) > 1e-5f) return false;
  if (a.animTimedOut != b.animTimedOut) return false;
  if (a.presentationVariant != b.presentationVariant) return false;
  if (std::fabs(a.direction - b.direction) > 1e-5f) return false;
  if (std::fabs(a.maxSpeedX - b.maxSpeedX) > 1e-5f) return false;
  if (a.grounded != b.grounded || a.shouldFlash != b.shouldFlash) return false;

  switch (a.type) {
    case ObjectClass::Player:
      return a.data.player.state == b.data.player.state &&
             a.data.player.healthPoints == b.data.player.healthPoints &&
             a.data.player.manaPoints == b.data.player.manaPoints &&
             a.data.player.ultimatePoints == b.data.player.ultimatePoints;
    case ObjectClass::Enemy:
      return a.data.enemy.state == b.data.enemy.state &&
             a.data.enemy.healthPoints == b.data.enemy.healthPoints &&
             a.data.enemy.srcH == b.data.enemy.srcH &&
             a.data.enemy.srcW == b.data.enemy.srcW &&
             std::fabs(a.data.enemy.hitStopRemainingSeconds - b.data.enemy.hitStopRemainingSeconds) < 1e-5f &&
             std::fabs(a.data.enemy.pendingKnockbackDirection - b.data.enemy.pendingKnockbackDirection) < 1e-5f &&
             std::fabs(a.data.enemy.pendingKnockbackMagnitude - b.data.enemy.pendingKnockbackMagnitude) < 1e-5f &&
             a.data.enemy.hasPendingKnockback == b.data.enemy.hasPendingKnockback;
    case ObjectClass::Projectile:
      return a.data.bullet.state == b.data.bullet.state;
    case ObjectClass::Level:
      return a.data.level.src.x == b.data.level.src.x &&
             a.data.level.src.y == b.data.level.src.y &&
             a.data.level.src.w == b.data.level.src.w &&
             a.data.level.src.h == b.data.level.src.h &&
             a.data.level.dst.x == b.data.level.dst.x &&
             a.data.level.dst.y == b.data.level.dst.y &&
             a.data.level.dst.w == b.data.level.dst.w &&
             a.data.level.dst.h == b.data.level.dst.h;
    case ObjectClass::Portal:
    case ObjectClass::Background:
      return true;
  }
  return false;
}

game_engine::NetGameStateSnapshot makeSnapshot() {
  using namespace game_engine;
  NetGameStateSnapshot snap{};
  snap.serverTick = 100;
  snap.levelId = LevelIndex::LEVEL_2;
  snap.m_stateLastUpdatedAt = 42;
  snap.hitStopEvent.sequence = 7;
  snap.hitStopEvent.active = true;
  snap.hitStopEvent.attackerClass = ObjectClass::Player;
  snap.hitStopEvent.attackerId = 1;
  snap.hitStopEvent.victimClass = ObjectClass::Enemy;
  snap.hitStopEvent.victimId = 2;
  snap.hitStopEvent.strength = HitStopStrength::Heavy;

  NetGameObjectSnapshot player{};
  player.id = 1;
  player.layer = 0;
  player.type = ObjectClass::Player;
  player.spriteType = SpriteType::Player_Mage;
  player.position = {1.f, 2.f};
  player.velocity = {0.5f, -0.25f};
  player.acceleration = {0.f, 0.1f};
  player.spriteFrame = 3;
  player.currentAnimation = 1;
  player.animElapsed = 0.33f;
  player.animTimedOut = false;
  player.presentationVariant = PresentationVariant::Run;
  player.direction = 1.f;
  player.maxSpeedX = 10.f;
  player.grounded = true;
  player.shouldFlash = false;
  new (&player.data.player) PlayerData{};
  player.data.player.state = PlayerState::running;
  player.data.player.healthPoints = 88;
  player.data.player.manaPoints = 42;
  player.data.player.ultimatePoints = 17;
  snap.m_gameObjects[{player.type, player.id}] = player;

  NetGameObjectSnapshot enemy{};
  enemy.id = 2;
  enemy.layer = 1;
  enemy.type = ObjectClass::Enemy;
  enemy.spriteType = SpriteType::Red_Werewolf;
  enemy.position = {5.f, 6.f};
  enemy.velocity = {0.f, 0.f};
  enemy.acceleration = {0.f, 0.f};
  enemy.spriteFrame = 2;
  enemy.currentAnimation = 0;
  enemy.animElapsed = 0.12f;
  enemy.animTimedOut = true;
  enemy.presentationVariant = PresentationVariant::Hit;
  enemy.direction = -1.f;
  enemy.maxSpeedX = 4.f;
  enemy.grounded = false;
  enemy.shouldFlash = true;
  new (&enemy.data.enemy) EnemyData{};
  enemy.data.enemy.state = EnemyState::hurt;
  enemy.data.enemy.healthPoints = 50;
  enemy.data.enemy.srcH = 16;
  enemy.data.enemy.srcW = 24;
  enemy.data.enemy.hitStopRemainingSeconds = 0.05f;
  enemy.data.enemy.pendingKnockbackDirection = -1.0f;
  enemy.data.enemy.pendingKnockbackMagnitude = 90.0f;
  enemy.data.enemy.hasPendingKnockback = true;
  snap.m_gameObjects[{enemy.type, enemy.id}] = enemy;

  NetGameObjectSnapshot bullet{};
  bullet.id = 3;
  bullet.layer = 1;
  bullet.type = ObjectClass::Projectile;
  bullet.spriteType = SpriteType::Player_Mage;
  bullet.position = {7.f, 8.f};
  bullet.velocity = {2.f, 0.f};
  bullet.acceleration = {0.f, 0.f};
  bullet.spriteFrame = 1;
  bullet.currentAnimation = 0;
  bullet.animElapsed = 0.45f;
  bullet.animTimedOut = false;
  bullet.presentationVariant = PresentationVariant::ProjectileMoving;
  bullet.direction = 1.f;
  bullet.maxSpeedX = 20.f;
  bullet.grounded = false;
  bullet.shouldFlash = false;
  new (&bullet.data.bullet) BulletData{};
  bullet.data.bullet.state = BulletState::moving;
  snap.m_gameObjects[{bullet.type, bullet.id}] = bullet;

  NetGameObjectSnapshot level{};
  level.id = 4;
  level.layer = 0;
  level.type = ObjectClass::Level;
  level.spriteType = SpriteType::Player_Marie;
  level.position = {0.f, 0.f};
  level.velocity = {0.f, 0.f};
  level.acceleration = {0.f, 0.f};
  level.spriteFrame = 0;
  level.currentAnimation = 0;
  level.animElapsed = 0.0f;
  level.animTimedOut = false;
  level.presentationVariant = PresentationVariant::Idle;
  level.direction = 0.f;
  level.maxSpeedX = 0.f;
  level.grounded = true;
  level.shouldFlash = false;
  new (&level.data.level) LevelData{};
  level.data.level.src = SDL_FRect{1.f, 2.f, 3.f, 4.f};
  level.data.level.dst = SDL_FRect{5.f, 6.f, 7.f, 8.f};
  snap.m_gameObjects[{level.type, level.id}] = level;

  return snap;
}

void testNetGameInputRoundTrip() {
  using namespace game_engine;
  NetGameInput in{};
  in.playerID = 123;
  in.inputSeq = 456;
  in.leftHeld = true;
  in.rightHeld = false;
  in.fireHeld = true;
  in.jumpPressed = true;
  in.meleePressed = true;
  in.ultimatePressed = true;
  auto bytes = in.serealizeNetGameInput();

  NetGameInput out{};
  out.deserealizeNetGameInput(bytes);

  assert(out.playerID == in.playerID);
  assert(out.inputSeq == in.inputSeq);
  assert(out.leftHeld == in.leftHeld);
  assert(out.rightHeld == in.rightHeld);
  assert(out.fireHeld == in.fireHeld);
  assert(out.jumpPressed == in.jumpPressed);
  assert(out.meleePressed == in.meleePressed);
  assert(out.ultimatePressed == in.ultimatePressed);
}

void testNetGameStateSnapshotRoundTrip() {
  using namespace game_engine;
  auto snap = makeSnapshot();
  auto bytes = snap.serealizeNetGameStateSnapshot();

  NetGameStateSnapshot decoded{};
  decoded.deserealizeNetGameStateSnapshot(bytes);

  assert(decoded.serverTick == snap.serverTick);
  assert(decoded.levelId == snap.levelId);
  assert(decoded.m_stateLastUpdatedAt == snap.m_stateLastUpdatedAt);
  assert(decoded.hitStopEvent.sequence == snap.hitStopEvent.sequence);
  assert(decoded.hitStopEvent.active == snap.hitStopEvent.active);
  assert(decoded.hitStopEvent.attackerClass == snap.hitStopEvent.attackerClass);
  assert(decoded.hitStopEvent.attackerId == snap.hitStopEvent.attackerId);
  assert(decoded.hitStopEvent.victimClass == snap.hitStopEvent.victimClass);
  assert(decoded.hitStopEvent.victimId == snap.hitStopEvent.victimId);
  assert(decoded.hitStopEvent.strength == snap.hitStopEvent.strength);
  assert(decoded.m_gameObjects.size() == snap.m_gameObjects.size());

  for (auto& [key, obj] : snap.m_gameObjects) {
    auto it = decoded.m_gameObjects.find(key);
    assert(it != decoded.m_gameObjects.end());
    assert(equalSnapshots(obj, it->second));
  }
}

game_engine::GameState makeGameplayState() {
  game_engine::GameState state;
  state.currentView = UIManager::GameView::Playing;
  state.currentLevelId = LevelIndex::LEVEL_1;
  state.m_stateLastUpdatedAt = 0;
  state.debugMode = false;
  state.selectedPlayerSprite = SpriteType::Player_Bonkfather;
  state.playerLayer = 1;
  state.playerIndex = 0;
  state.mapViewport = SDL_FRect{0.0f, 0.0f, 640.0f, 360.0f};
  state.bg2scroll = 0.0f;
  state.bg3scroll = 0.0f;
  state.bg4scroll = 0.0f;
  state.layers.resize(2);
  return state;
}

void assignPlayerAnimations(GameObject& player) {
  player.animations.resize(13);
  player.animations[ANIM_IDLE] = Animation(1, 1.0f);
  player.animations[ANIM_RUN] = Animation(8, 0.6f);
  player.animations[ANIM_SHOOT] = Animation(7, 0.4f);
  player.animations[ANIM_SLIDE_SHOOT] = Animation(7, 0.4f);
  player.animations[ANIM_SWING] = Animation(6, 0.4f);
  player.animations[ANIM_JUMP] = Animation(6, 0.5f);
  player.animations[ANIM_HIT] = Animation(4, 0.4f);
  player.animations[ANIM_DIE] = Animation(8, 0.6f);
  player.animations[ANIM_RUN_ATTACK] = Animation(6, 0.4f);
  player.animations[ANIM_SWING_2] = Animation(12, 0.7f);
  player.animations[ANIM_ULTIMATE] = Animation(34, 1.7f);
  player.currentAnimation = ANIM_IDLE;
  player.presentationVariant = PresentationVariant::Idle;
}

void assignEnemyAnimations(GameObject& enemy) {
  enemy.animations.resize(13);
  enemy.animations[ANIM_IDLE] = Animation(1, 1.0f);
  enemy.animations[ANIM_RUN] = Animation(6, 0.6f);
  enemy.animations[ANIM_SWING] = Animation(5, 0.5f);
  enemy.animations[ANIM_HIT] = Animation(3, 0.5f);
  enemy.animations[ANIM_DIE] = Animation(5, 0.5f);
  enemy.currentAnimation = ANIM_IDLE;
  enemy.presentationVariant = PresentationVariant::Idle;
}

GameObject makePlayer(uint32_t id = 1) {
  GameObject player(128, 128);
  player.id = id;
  player.objClass = ObjectClass::Player;
  player.data.player = PlayerData();
  player.spriteType = SpriteType::Player_Bonkfather;
  player.dynamic = true;
  player.grounded = true;
  player.drawScale = 2.0f;
  player.maxSpeedX = 15.0f;
  player.acceleration = glm::vec2(30.0f, 0.0f);
  player.colliderNorm = {.x = 0.30f, .y = 0.5f, .w = 0.30f, .h = 0.5f};
  player.applyScale();
  assignPlayerAnimations(player);
  return player;
}

GameObject makeProjectile(
  uint32_t id,
  float x,
  float y,
  float direction,
  uint32_t ownerPlayerId = 1) {
  GameObject bullet(128, 128);
  bullet.id = id;
  bullet.objClass = ObjectClass::Projectile;
  bullet.spriteType = SpriteType::Player_Bonkfather;
  bullet.drawScale = 2.0f;
  bullet.colliderNorm = {.x = 0.0f, .y = 0.40f, .w = 0.5f, .h = 0.1f};
  bullet.applyScale();
  bullet.data.bullet = BulletData();
  bullet.data.bullet.ownerPlayerId = ownerPlayerId;
  bullet.data.bullet.state = BulletState::moving;
  bullet.dynamic = true;
  bullet.position = glm::vec2(x, y);
  bullet.direction = direction;
  bullet.velocity.x = 200.0f * direction;
  bullet.animations.resize(2);
  bullet.animations[ANIM_IDLE] = Animation(9, 1.0f);
  bullet.animations[ANIM_RUN] = Animation(4, 0.15f);
  bullet.currentAnimation = ANIM_IDLE;
  bullet.presentationVariant = PresentationVariant::ProjectileMoving;
  bullet.spriteFrame = 1;
  return bullet;
}

GameObject makeEnemy(float x, int health = 100) {
  GameObject enemy(128, 128);
  enemy.id = 2;
  enemy.objClass = ObjectClass::Enemy;
  enemy.data.enemy = EnemyData();
  enemy.spriteType = SpriteType::Skeleton_Warrior;
  enemy.dynamic = true;
  enemy.grounded = true;
  enemy.drawScale = 1.5f;
  enemy.position = glm::vec2(x, 0.0f);
  enemy.maxSpeedX = 15.0f;
  enemy.colliderNorm = {.x = 0.35f, .y = 0.4f, .w = 0.30f, .h = 0.6f};
  enemy.applyScale();
  enemy.data.enemy.healthPoints = health;
  assignEnemyAnimations(enemy);
  return enemy;
}

GameObject makeFloor() {
  GameObject floor(32, 32);
  floor.id = 10;
  floor.objClass = ObjectClass::Level;
  floor.dynamic = false;
  floor.position = glm::vec2(-200.0f, 64.0f);
  floor.collider = SDL_FRect{0.0f, 0.0f, 1000.0f, 40.0f};
  floor.baseCollider = floor.collider;
  floor.data.level = LevelData{};
  floor.data.level.isHazard = false;
  return floor;
}

GameObject makeHazard() {
  GameObject hazard(32, 32);
  hazard.id = 11;
  hazard.objClass = ObjectClass::Level;
  hazard.dynamic = false;
  hazard.position = glm::vec2(-10.0f, 32.0f);
  hazard.collider = SDL_FRect{0.0f, 0.0f, 120.0f, 32.0f};
  hazard.baseCollider = hazard.collider;
  hazard.data.level = LevelData{};
  hazard.data.level.isHazard = true;
  return hazard;
}

void testPassiveUltimateChargeGain() {
  auto state = makeGameplayState();
  state.layers[0].push_back(makeFloor());
  state.layers[1].push_back(makePlayer());

  game_engine::stepGameplaySimulation(state, {}, 1.0f);

  assert(state.layers[1][0].data.player.ultimatePoints == 33);
}

void testKillRewardGainFromMelee() {
  auto state = makeGameplayState();
  state.layers[0].push_back(makeFloor());
  state.layers[1].push_back(makePlayer());
  state.layers[1].push_back(makeEnemy(12.0f, 10));

  std::unordered_map<uint32_t, game_engine::NetGameInput> inputs;
  inputs.emplace(1, game_engine::NetGameInput{.playerID = 1, .meleePressed = true});

  game_engine::stepGameplaySimulation(state, inputs, 0.05f);

  assert(state.layers[1][0].data.player.ultimatePoints == 33);
  assert(state.layers[1][1].data.enemy.state == EnemyState::dead);
}

void testUltimateRequiresFullMeter() {
  auto state = makeGameplayState();
  state.layers[0].push_back(makeFloor());
  state.layers[1].push_back(makePlayer());
  state.layers[1][0].data.player.ultimatePoints = 99;
  state.layers[1][0].data.player.unlockedUltimateOne = true;

  std::unordered_map<uint32_t, game_engine::NetGameInput> inputs;
  inputs.emplace(1, game_engine::NetGameInput{.playerID = 1, .ultimatePressed = true});

  game_engine::stepGameplaySimulation(state, inputs, 0.1f);

  assert(state.layers[1][0].data.player.state != PlayerState::ultimate);
  assert(state.layers[1][0].data.player.ultimatePoints == 99);
}

void testUltimatePreventsDamage() {
  auto state = makeGameplayState();
  state.layers[0].push_back(makeFloor());
  state.layers[0].push_back(makeHazard());
  state.layers[1].push_back(makePlayer());
  state.layers[1][0].data.player.ultimatePoints = 100;
  state.layers[1][0].data.player.unlockedUltimateOne = true;

  std::unordered_map<uint32_t, game_engine::NetGameInput> inputs;
  inputs.emplace(1, game_engine::NetGameInput{.playerID = 1, .ultimatePressed = true});

  game_engine::stepGameplaySimulation(state, inputs, 0.05f);

  assert(state.layers[1][0].data.player.state == PlayerState::ultimate);
  assert(state.layers[1][0].data.player.healthPoints == 100);
}

void testUltimateOnlyHitsEnemyOncePerCast() {
  auto state = makeGameplayState();
  state.layers[0].push_back(makeFloor());
  state.layers[1].push_back(makePlayer());
  state.layers[1].push_back(makeEnemy(8.0f, 100));
  state.layers[1][0].data.player.ultimatePoints = 100;
  state.layers[1][0].data.player.unlockedUltimateOne = true;

  std::unordered_map<uint32_t, game_engine::NetGameInput> inputs;
  inputs.emplace(1, game_engine::NetGameInput{.playerID = 1, .ultimatePressed = true});

  game_engine::stepGameplaySimulation(state, inputs, 0.05f);
  assert(state.layers[1][1].data.enemy.healthPoints == 100);

  inputs[1].ultimatePressed = false;
  game_engine::stepGameplaySimulation(state, inputs, 1.35f);
  assert(state.layers[1][1].data.enemy.healthPoints == 50);

  game_engine::stepGameplaySimulation(state, inputs, 0.05f);
  assert(state.layers[1][1].data.enemy.healthPoints == 50);
}

void testUltimateColliderResetsAfterAnimation() {
  auto state = makeGameplayState();
  state.layers[0].push_back(makeFloor());
  state.layers[1].push_back(makePlayer());
  state.layers[1][0].data.player.ultimatePoints = 100;
  state.layers[1][0].data.player.unlockedUltimateOne = true;
  const SDL_FRect baseCollider = state.layers[1][0].baseCollider;

  std::unordered_map<uint32_t, game_engine::NetGameInput> inputs;
  inputs.emplace(1, game_engine::NetGameInput{.playerID = 1, .ultimatePressed = true});

  game_engine::stepGameplaySimulation(state, inputs, 0.05f);
  assert(!closeRect(state.layers[1][0].collider, baseCollider));

  inputs[1].ultimatePressed = false;
  game_engine::stepGameplaySimulation(state, inputs, 2.0f);

  assert(state.layers[1][0].data.player.state != PlayerState::ultimate);
  assert(closeRect(state.layers[1][0].collider, baseCollider));
}

void testEnemyHitStopSnapshotRoundTrip() {
  using namespace game_engine;
  auto snap = makeSnapshot();
  auto bytes = snap.serealizeNetGameStateSnapshot();

  NetGameStateSnapshot decoded{};
  decoded.deserealizeNetGameStateSnapshot(bytes);

  const auto enemyKey = std::make_pair(ObjectClass::Enemy, 2u);
  assert(decoded.m_gameObjects.contains(enemyKey));
  const auto& enemy = decoded.m_gameObjects.at(enemyKey).data.enemy;
  assert(std::fabs(enemy.hitStopRemainingSeconds - 0.05f) < 1e-5f);
  assert(std::fabs(enemy.pendingKnockbackDirection - (-1.0f)) < 1e-5f);
  assert(std::fabs(enemy.pendingKnockbackMagnitude - 90.0f) < 1e-5f);
  assert(enemy.hasPendingKnockback);
}

void testEnemyKnockbackDelayedUntilHitStopEnds() {
  auto state = makeGameplayState();
  state.layers[0].push_back(makeFloor());
  state.layers[1].push_back(makePlayer());
  state.layers[1].push_back(makeEnemy(12.0f, 100));

  std::unordered_map<uint32_t, game_engine::NetGameInput> inputs;
  inputs.emplace(1, game_engine::NetGameInput{.playerID = 1, .meleePressed = true});

  game_engine::stepGameplaySimulation(state, inputs, 0.05f);

  auto& player = state.layers[1][0];
  auto& enemy = state.layers[1][1];
  const float impactX = enemy.position.x;
  assert(player.data.player.state == PlayerState::swingWeapon);
  assert(enemy.data.enemy.state == EnemyState::hurt);
  assert(enemy.data.enemy.healthPoints == 90);
  assert(enemy.data.enemy.hitStopRemainingSeconds > 0.0f);
  assert(enemy.data.enemy.hasPendingKnockback);
  assert(std::fabs(enemy.velocity.x) < 1e-5f);

  inputs[1].meleePressed = false;
  game_engine::stepGameplaySimulation(state, inputs, 0.01f);

  assert(player.data.player.state == PlayerState::swingWeapon);
  assert(std::fabs(enemy.position.x - impactX) < 1e-4f);
  assert(std::fabs(enemy.velocity.x) < 1e-5f);
  player.position.x -= 100.0f;

  game_engine::stepGameplaySimulation(
    state,
    inputs,
    game_engine::hitStopDurationSeconds(HitStopStrength::Normal));

  assert(enemy.data.enemy.hitStopRemainingSeconds == 0.0f);
  assert(!enemy.data.enemy.hasPendingKnockback);
  assert(enemy.velocity.x > 0.0f);
  assert(enemy.position.x > impactX);
}

void testSwingingPlayerDoesNotSlideThroughHurtEnemy() {
  auto state = makeGameplayState();
  state.layers[0].push_back(makeFloor());
  state.layers[1].push_back(makePlayer());
  state.layers[1].push_back(makeEnemy(12.0f, 100));

  std::unordered_map<uint32_t, game_engine::NetGameInput> inputs;
  inputs.emplace(1, game_engine::NetGameInput{.playerID = 1, .meleePressed = true});

  game_engine::stepGameplaySimulation(state, inputs, 0.05f);

  auto& player = state.layers[1][0];
  auto& enemy = state.layers[1][1];
  player.velocity.x = 20.0f;
  player.position.x = enemy.position.x - 1.0f;

  inputs[1].meleePressed = false;
  game_engine::stepGameplaySimulation(state, inputs, 0.01f);

  assert(player.position.x < enemy.position.x);
  assert(std::fabs(player.velocity.x) < 1e-5f);
}

void testAirborneSwingDoesNotSideTeleportAroundEnemy() {
  auto state = makeGameplayState();
  state.layers[0].push_back(makeFloor());
  state.layers[1].push_back(makePlayer());
  state.layers[1].push_back(makeEnemy(12.0f, 100));

  std::unordered_map<uint32_t, game_engine::NetGameInput> inputs;
  inputs.emplace(1, game_engine::NetGameInput{.playerID = 1, .meleePressed = true});

  game_engine::stepGameplaySimulation(state, inputs, 0.05f);

  auto& player = state.layers[1][0];
  auto& enemy = state.layers[1][1];
  const float beforeX = player.position.x;
  player.grounded = true;
  player.velocity.x = 20.0f;
  player.velocity.y = 40.0f;
  player.position.x = enemy.position.x - 1.0f;
  player.position.y = enemy.position.y - 20.0f;

  inputs[1].meleePressed = false;
  game_engine::stepGameplaySimulation(state, inputs, 0.01f);

  assert(player.position.x > beforeX);
}

void testProjectileHitUsesDelayedEnemyKnockback() {
  auto state = makeGameplayState();
  state.layers[0].push_back(makeFloor());
  state.layers[1].push_back(makePlayer());
  state.layers[1].push_back(makeEnemy(12.0f, 100));
  state.bullets.push_back(makeProjectile(9, 12.0f, 20.0f, 1.0f));

  game_engine::stepGameplaySimulation(state, {}, 0.01f);

  auto& enemy = state.layers[1][1];
  assert(enemy.data.enemy.state == EnemyState::hurt);
  assert(enemy.data.enemy.hitStopRemainingSeconds > 0.0f);
  assert(enemy.data.enemy.hasPendingKnockback);
  assert(std::fabs(enemy.velocity.x) < 1e-5f);

  game_engine::stepGameplaySimulation(
    state,
    {},
    game_engine::hitStopDurationSeconds(HitStopStrength::Normal));

  assert(enemy.velocity.x > 0.0f);
}

void testUltimateHitUsesDelayedEnemyKnockback() {
  auto state = makeGameplayState();
  state.layers[0].push_back(makeFloor());
  state.layers[1].push_back(makePlayer());
  state.layers[1].push_back(makeEnemy(8.0f, 100));
  state.layers[1][0].data.player.ultimatePoints = 100;
  state.layers[1][0].data.player.unlockedUltimateOne = true;

  std::unordered_map<uint32_t, game_engine::NetGameInput> inputs;
  inputs.emplace(1, game_engine::NetGameInput{.playerID = 1, .ultimatePressed = true});

  game_engine::stepGameplaySimulation(state, inputs, 0.05f);
  inputs[1].ultimatePressed = false;
  game_engine::stepGameplaySimulation(state, inputs, 1.35f);

  auto& enemy = state.layers[1][1];
  assert(enemy.data.enemy.state == EnemyState::hurt);
  assert(enemy.data.enemy.healthPoints == 50);
  assert(enemy.data.enemy.hitStopRemainingSeconds > 0.0f);
  assert(enemy.data.enemy.hasPendingKnockback);
  assert(std::fabs(enemy.velocity.x) < 1e-5f);

  game_engine::stepGameplaySimulation(
    state,
    inputs,
    game_engine::hitStopDurationSeconds(HitStopStrength::Heavy));

  assert(enemy.velocity.x > 0.0f);
}

void testFatalEnemyHitDisablesColliderImmediately() {
  auto state = makeGameplayState();
  state.layers[0].push_back(makeFloor());
  state.layers[1].push_back(makePlayer());
  state.layers[1].push_back(makeEnemy(12.0f, 10));

  std::unordered_map<uint32_t, game_engine::NetGameInput> inputs;
  inputs.emplace(1, game_engine::NetGameInput{.playerID = 1, .meleePressed = true});

  game_engine::stepGameplaySimulation(state, inputs, 0.05f);

  auto& enemy = state.layers[1][1];
  assert(enemy.data.enemy.state == EnemyState::dead);
  assert(enemy.collider.w == 0.0f);
  assert(enemy.collider.h == 0.0f);
}

void testDeadEnemyNoLongerBlocksPlayerCollision() {
  auto state = makeGameplayState();
  state.layers[0].push_back(makeFloor());
  state.layers[1].push_back(makePlayer());
  state.layers[1].back().position.x = 0.0f;
  state.layers[1].push_back(makeEnemy(0.0f, 10));

  std::unordered_map<uint32_t, game_engine::NetGameInput> hitInputs;
  hitInputs.emplace(1, game_engine::NetGameInput{.playerID = 1, .meleePressed = true});
  game_engine::stepGameplaySimulation(state, hitInputs, 0.05f);

  auto& player = state.layers[1][0];
  const float beforeMove = player.position.x;
  std::unordered_map<uint32_t, game_engine::NetGameInput> moveInputs;
  moveInputs.emplace(1, game_engine::NetGameInput{.playerID = 1, .rightHeld = true});
  game_engine::stepGameplaySimulation(state, moveInputs, 0.1f);

  assert(player.position.x > beforeMove);
}

void testDeadEnemyGetsPurgedAfterDeathAnimation() {
  auto state = makeGameplayState();
  state.layers[0].push_back(makeFloor());
  state.layers[1].push_back(makePlayer());
  state.layers[1].push_back(makeEnemy(12.0f, 10));

  std::unordered_map<uint32_t, game_engine::NetGameInput> inputs;
  inputs.emplace(1, game_engine::NetGameInput{.playerID = 1, .meleePressed = true});

  game_engine::stepGameplaySimulation(state, inputs, 0.05f);
  assert(state.layers[1].size() == 2);

  inputs[1].meleePressed = false;
  game_engine::stepGameplaySimulation(state, inputs, 1.0f);

  assert(state.layers[1].size() == 1);
  assert(state.layers[1][0].objClass == ObjectClass::Player);
}

} // namespace

int main(){
  testNetGameInputRoundTrip();
  testNetGameStateSnapshotRoundTrip();
  testEnemyHitStopSnapshotRoundTrip();
  testPassiveUltimateChargeGain();
  testKillRewardGainFromMelee();
  testUltimateRequiresFullMeter();
  testUltimatePreventsDamage();
  testUltimateOnlyHitsEnemyOncePerCast();
  testUltimateColliderResetsAfterAnimation();
  testEnemyKnockbackDelayedUntilHitStopEnds();
  testSwingingPlayerDoesNotSlideThroughHurtEnemy();
  testAirborneSwingDoesNotSideTeleportAroundEnemy();
  testProjectileHitUsesDelayedEnemyKnockback();
  testUltimateHitUsesDelayedEnemyKnockback();
  testFatalEnemyHitDisablesColliderImmediately();
  testDeadEnemyNoLongerBlocksPlayerCollision();
  testDeadEnemyGetsPurgedAfterDeathAnimation();
  std::cout << "All net_common tests passed\n";
  return 0;
}


// You can run it either from VS Code’s CMake Tools UI or the terminal:

// In VS Code: open the CMake Tools side bar → set the build preset you use → pick net_common_tests from the “CMake: Set Build Target” dropdown → click “Build” (or run the CMake: Build command). Then run the built binary from the terminal: ./build/net_common_tests.

// From the terminal: cmake --build build --target net_common_tests and then ./build/net_common_tests (from the repo root).
// If you want it in the VS Code “Test” explorer, add a ctest entry by registering the test in CMake (add_test(NAME net_common_tests COMMAND net_common_tests)) and then run “CMake: Run Tests”.
