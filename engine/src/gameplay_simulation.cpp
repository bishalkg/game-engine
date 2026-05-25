#include "engine/gameplay_simulation.h"

#include <algorithm>
#include <cmath>

#include "gameplay_simulation_internal.h"
#include "engine/engine.h"

namespace game_engine {

float hitStopDurationSeconds(HitStopStrength strength) {
  switch (strength) {
    case HitStopStrength::Heavy:
      return GameplayImpactTuning::heavyHitStopSeconds;
    case HitStopStrength::Normal:
    default:
      return GameplayImpactTuning::normalHitStopSeconds;
  }
}

uint16_t hitStopDurationMs(HitStopStrength strength) {
  return static_cast<uint16_t>(std::lround(hitStopDurationSeconds(strength) * 1000.0f));
}

float enemyKnockbackMagnitude(EnemyImpactType impactType) {
  switch (impactType) {
    case EnemyImpactType::Projectile:
      return GameplayImpactTuning::projectileEnemyKnockback;
    case EnemyImpactType::Ultimate:
      return GameplayImpactTuning::ultimateEnemyKnockback;
    case EnemyImpactType::Melee:
    default:
      return GameplayImpactTuning::meleeEnemyKnockback;
  }
}

// stepGameplaySimulation updates the servers authoritative game state using each players inputs
// and also updates each enemy and bullet object
void stepGameplaySimulation(
  GameState& state,
  const std::unordered_map<uint32_t, NetGameInput>& playerInputs,
  float deltaTime,
  const GameplaySimulationHooks& hooks) {
  for (auto& layer : state.layers) {
    for (auto& obj : layer) {
      if (obj.dynamic) {
        updateDynamicObject(state, obj, playerInputs, hooks, deltaTime);
      }
    }
  }

  for (auto& bullet : state.bullets) {
    updateDynamicObject(state, bullet, playerInputs, hooks, deltaTime);
  }

  for (auto& layer : state.layers) {
    for (auto& obj : layer) {
      if (obj.dynamic) {
        resolveObjectCollisions(state, obj, hooks);
      }
    }
  }

  for (auto& bullet : state.bullets) {
    resolveBulletCollisions(state, bullet, hooks);
  }

  state.bullets.erase(
    std::remove_if(
      state.bullets.begin(),
      state.bullets.end(),
      [](const GameObject& bullet) { return bullet.data.bullet.state == BulletState::inactive; }),
    state.bullets.end());

  purgeFinishedDeadEnemies(state);

  purgeCollectedMaterials(state);
}

} // namespace game_engine
