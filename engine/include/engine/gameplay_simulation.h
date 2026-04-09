#pragma once

#include <functional>
#include <unordered_map>

#include "engine/net/game_net_common.h"

namespace game_engine {

struct GameState;

struct GameplaySimulationHooks {
  std::function<void(LevelIndex)> onPortalTriggered;
  bool cullProjectilesByViewport = false;
  SDL_FRect projectileViewport{};
};

void stepGameplaySimulation(
  GameState& state,
  const std::unordered_map<uint32_t, NetGameInput>& playerInputs,
  float deltaTime,
  const GameplaySimulationHooks& hooks = {});

} // namespace game_engine
