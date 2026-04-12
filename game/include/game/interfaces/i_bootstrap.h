#pragma once

namespace game_engine {
class Engine;
}

namespace game {
  struct ProgressionProfile; // forward declare to avoid import
  struct GameResources;

struct IBootstrap {
  virtual ~IBootstrap() = default;
  virtual bool initialize(
    game_engine::Engine& engine,
    GameResources& resources,
    const ProgressionProfile& profile,
    bool headless = false) = 0;
};

} // namespace game
