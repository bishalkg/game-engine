#pragma once

namespace game_engine {
class Engine;
}

namespace game {
  struct ProgressionProfile; // forward declare to avoid import
  struct ProgressionService;
  struct GameResources;

struct IBootstrap {
  virtual ~IBootstrap() = default;
  virtual bool initialize(
    game_engine::Engine& engine,
    GameResources& resources,
    ProgressionService& progService,
    bool headless = false) = 0;
};

} // namespace game
