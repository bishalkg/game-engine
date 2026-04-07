#pragma once

namespace game_engine {
class Engine;
}

namespace game {

struct GameResources;

struct IBootstrap {
  virtual ~IBootstrap() = default;
  virtual bool initialize(
    game_engine::Engine& engine,
    GameResources& resources,
    bool headless = false) = 0;
};

} // namespace game
