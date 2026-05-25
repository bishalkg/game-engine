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
  SimulationEvents events;
  std::vector<MotionIntent> objectMotion;
  std::vector<MotionIntent> bulletMotion;

  updateDynamicObjects(state, playerInputs, hooks, deltaTime, objectMotion, bulletMotion);
  const PhysicsStepResult physicsResult =
    physicsStep(state, objectMotion, bulletMotion, deltaTime);
  applyGameplayCollisions(state, physicsResult, events);
  purgeDeadOrCollectedObjects(state);
  dispatchSimulationEvents(events, hooks);
}

} // namespace game_engine
