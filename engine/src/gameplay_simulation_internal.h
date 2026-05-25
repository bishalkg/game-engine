#pragma once

#include <cstdint>
#include <unordered_map>

#include "engine/gameplay_simulation.h"

namespace game_engine {

struct DamageEnemyResult {
  bool applied = false;
  bool killed = false;
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
void emitHitConfirmed(
  const GameplaySimulationHooks& hooks,
  GameObjectKey attacker,
  GameObjectKey victim,
  HitStopStrength strength);
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
void damagePlayer(GameObject& player, int damage);
GameObject makeBulletFromPlayer(const GameObject& player, const GameState& state);
void updateDynamicObject(
  GameState& state,
  GameObject& obj,
  const std::unordered_map<uint32_t, NetGameInput>& playerInputs,
  const GameplaySimulationHooks& hooks,
  float deltaTime);
void collisionResponse(
  GameState& state,
  GameObject& objA,
  GameObject& objB,
  const SDL_FRect& rectC,
  const GameplaySimulationHooks& hooks);
void resolveObjectCollisions(
  GameState& state,
  GameObject& obj,
  const GameplaySimulationHooks& hooks);
void resolveBulletCollisions(
  GameState& state,
  GameObject& bullet,
  const GameplaySimulationHooks& hooks);
void purgeCollectedMaterials(GameState& state);
void purgeFinishedDeadEnemies(GameState& state);

} // namespace game_engine
