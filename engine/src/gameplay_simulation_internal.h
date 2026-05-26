#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "engine/gameplay_simulation.h"

namespace game_engine {

struct DamageEnemyResult {
  bool applied = false;
  bool killed = false;
};

struct SimulationHitConfirmedEvent {
  GameObjectKey attacker;
  GameObjectKey victim;
  HitStopStrength strength = HitStopStrength::Normal;
};

struct SimulationPortalTriggeredEvent {
  LevelIndex nextLevel{LevelIndex::LEVEL_1};
};

struct SimulationFlyingStoneCollectedEvent {
  LevelIndex levelId{LevelIndex::LEVEL_1};
};

struct SimulationBossDefeatedEvent {
  GameObject* boss = nullptr;
};

struct SimulationEvents {
  std::vector<SimulationHitConfirmedEvent> hitConfirmed;
  std::vector<SimulationPortalTriggeredEvent> portalTriggered;
  std::vector<SimulationFlyingStoneCollectedEvent> flyingStoneCollected;
  std::vector<SimulationBossDefeatedEvent> bossDefeated;
};

struct MotionIntent {
  GameObject* object = nullptr;
  float direction = 0.0f;
};

enum class CollisionSubjectKind {
  DynamicObject,
  Bullet,
};

struct CollisionPair {
  GameObject* subject = nullptr;
  GameObject* other = nullptr;
  SDL_FRect overlap{0.0f, 0.0f, 0.0f, 0.0f};
  CollisionSubjectKind kind = CollisionSubjectKind::DynamicObject;
};

struct PhysicsStepResult {
  std::vector<CollisionPair> gameplayCollisions;
};

const NetGameInput& inputForPlayer(
  const std::unordered_map<uint32_t, NetGameInput>& playerInputs,
  uint32_t playerID);

bool hasAnimation(const GameObject& obj, int animIndex);
void setPresentation(GameObject& obj, PresentationVariant presentation);
void setAnimation(GameObject& obj, int animIndex, bool reset = true);
void setAnimationAndPresentation(
  GameObject& obj,
  int animIndex,
  PresentationVariant presentation,
  bool reset = true);
bool bossBlocksPortalTransition(const GameState& state);
void syncSpriteFrame(GameObject& obj);
void clearFlash(GameObject& obj, float deltaTime);
SDL_FRect worldRect(const GameObject& obj);
SDL_FRect baseFacing(const GameObject& obj);
bool isPlayerInHurtRecovery(const GameObject& player);
bool isPlayerInHurtCooldown(const GameObject& player);
void clearPlayerCombatState(GameObject& player);
void setPlayerLocomotionStateFromMotion(GameObject& player);
void widenColliderForSwing(GameObject& obj);
void expandColliderForUltimate(GameObject& obj);
bool isUltimateDamageActive(const GameObject& obj);
SDL_FRect physicsColliderFor(const GameObject& obj, ObjectClass otherClass);
SDL_FRect collisionRect(const GameObject& obj, ObjectClass otherClass);
GameObject* findPlayerById(GameState& state, uint32_t playerID);
GameObject* findClosestLivingPlayer(GameState& state, const GameObject& source);
uint32_t nextDynamicId(const GameState& state);
void awardUltimateCharge(GameState& state, uint32_t playerID, int amount);
void awardMaterialToPlayer(GameObject& player, const MaterialData& material);
void recordHitConfirmed(
  SimulationEvents& events,
  GameObjectKey attacker,
  GameObjectKey victim,
  HitStopStrength strength);
void recordPortalTriggered(SimulationEvents& events, LevelIndex nextLevel);
void recordFlyingStoneCollected(SimulationEvents& events, LevelIndex levelId);
void recordBossDefeated(SimulationEvents& events, GameObject& boss);
void dispatchSimulationEvents(
  const SimulationEvents& events,
  const GameplaySimulationHooks& hooks);
void unlockFlyingStoneRewardForAllPlayers(GameState& state);
void spawnFlyingStoneDrops(GameState& state, const SimulationEvents& events);
void clearEnemyPendingKnockback(GameObject& enemy);
void queueEnemyHitImpact(
  GameObject& enemy,
  HitStopStrength strength,
  float direction,
  float magnitude);
void clearDynamicCollider(GameObject& obj);
bool stepEnemyHitStop(GameObject& enemy, float deltaTime);
DamageEnemyResult damageEnemy(
  GameState& state,
  GameObject& enemy,
  int damage,
  uint32_t sourcePlayerId = 0,
  uint32_t sourceUltimateCastId = 0,
  bool ignoreHurtCooldown = false,
  HitStopStrength hitStopStrength = HitStopStrength::Normal,
  float knockbackDirection = 0.0f,
  float knockbackMagnitude = 0.0f);
bool damagePlayer(GameObject& player, int damage);
GameObject makeBulletFromPlayer(const GameObject& player, const GameState& state);
void integrateMotion(GameObject& obj, float direction, float deltaTime);
float updatePlayer(
  GameState& state,
  GameObject& obj,
  const std::unordered_map<uint32_t, NetGameInput>& playerInputs,
  float deltaTime);
float updateEnemy(GameState& state, GameObject& obj, float deltaTime);
void updateProjectile(
  GameObject& obj,
  const GameplaySimulationHooks& hooks,
  float deltaTime);
void updateMaterial(GameObject& obj);
float updateDynamicObject(
  GameState& state,
  GameObject& obj,
  const std::unordered_map<uint32_t, NetGameInput>& playerInputs,
  const GameplaySimulationHooks& hooks,
  float deltaTime);
void updateDynamicObjects(
  GameState& state,
  const std::unordered_map<uint32_t, NetGameInput>& playerInputs,
  const GameplaySimulationHooks& hooks,
  float deltaTime,
  std::vector<MotionIntent>& objectMotion,
  std::vector<MotionIntent>& bulletMotion);
std::vector<CollisionPair> detectCollisions(
  GameState& state,
  GameObject& subject,
  CollisionSubjectKind subjectKind);
void resolveSolidCollisions(
  GameState& state,
  GameObject& subject,
  const std::vector<CollisionPair>& collisions);
PhysicsStepResult physicsStep(
  GameState& state,
  const std::vector<MotionIntent>& objectMotion,
  const std::vector<MotionIntent>& bulletMotion,
  float deltaTime);
void applyGameplayCollisions(
  GameState& state,
  const PhysicsStepResult& physicsResult,
  SimulationEvents& events);
void purgeCollectedMaterials(GameState& state);
void purgeFinishedDeadEnemies(GameState& state);
void purgeDeadOrCollectedObjects(GameState& state);

} // namespace game_engine
