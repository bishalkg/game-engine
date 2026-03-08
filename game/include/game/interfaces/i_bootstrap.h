#pragma once

namespace game_engine {
class Engine;
}

namespace game {

struct IBootstrap {
  virtual ~IBootstrap() = default;
  virtual bool initialize(game_engine::Engine& engine, bool headless = false) = 0;
};

} // namespace game
