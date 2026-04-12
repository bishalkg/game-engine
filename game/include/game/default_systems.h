#pragma once

#include <memory>

#include "engine/level_types.h"
#include "game/game_resources.h"
#include "game/interfaces/i_bootstrap.h"
#include "game/interfaces/i_input_system.h"
#include "game/interfaces/i_ui_flow.h"
#include "game/interfaces/i_simulation_system.h"
#include "game/interfaces/i_render_system.h"

namespace game {

std::unique_ptr<IBootstrap> createDefaultBootstrap();
std::unique_ptr<IInputSystem> createDefaultInputSystem();
std::unique_ptr<IUIFlow> createDefaultUIFlow();
std::unique_ptr<ISimulationSystem> createDefaultSimulationSystem();
std::unique_ptr<IRenderSystem> createDefaultRenderSystem();

// Shared helper for UI flow and bootstrap-level transitions.
bool switchToLevel(game_engine::Engine& engine, GameResources& resources, ProgressionService& progService, LevelIndex levelId);

} // namespace game
