#pragma once

#include <functional>
#include <unordered_map>

#include "engine/net/game_net_common.h"

namespace game_engine {

struct GameState;

enum class EnemyImpactType : uint8_t {
  Melee,
  Projectile,
  Ultimate,
};

struct GameplayImpactTuning {
  static constexpr float normalHitStopSeconds = 10.0f / 60.0f;
  static constexpr float heavyHitStopSeconds = 20.0f / 60.0f;
  static constexpr float meleeEnemyKnockback = 20.0f;
  static constexpr float projectileEnemyKnockback = 10.0f;
  static constexpr float ultimateEnemyKnockback = 30.0f;
};

struct GameplaySimulationHooks {
  std::function<void(LevelIndex)> onPortalTriggered;
  std::function<void(GameObjectKey, GameObjectKey, HitStopStrength)> onHitConfirmed;
  bool cullProjectilesByViewport = false;
  SDL_FRect projectileViewport{};
};

float hitStopDurationSeconds(HitStopStrength strength);
uint16_t hitStopDurationMs(HitStopStrength strength);
float enemyKnockbackMagnitude(EnemyImpactType impactType);

void stepGameplaySimulation(
  GameState& state,
  const std::unordered_map<uint32_t, NetGameInput>& playerInputs,
  float deltaTime,
  const GameplaySimulationHooks& hooks = {});

} // namespace game_engine
