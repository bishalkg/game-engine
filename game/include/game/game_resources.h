#pragma once

namespace game_engine {
class Engine;
}

namespace game {

// Bootstraps game-owned resources and initial world state on top of engine runtime services.
bool initializeGameResources(game_engine::Engine& engine, bool headless = false);

} // namespace game
