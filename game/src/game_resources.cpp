#include "game/game_resources.h"

#include "engine/engine.h"

namespace game {

bool initializeGameResources(game_engine::Engine& engine, bool headless) {
  auto& resources = engine.getResources();
  auto& sdlState = engine.getSDLState();
  auto& gameState = engine.getGameState();

  resources.loadAllAssets(sdlState, gameState, headless);
  if (!resources.m_currLevel ||
      !resources.m_currLevel->texCharacterMap[SpriteType::Player_Knight].texIdle) {
    return false;
  }

  return true;
}

} // namespace game
