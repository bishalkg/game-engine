#pragma once

#include <optional>
#include <vector>

#include "engine/ui_manager.h"

namespace game {

enum class UIActionType {
  FinishLoading,
  BlockMainGameDraw,
  BlockGameplayUpdates,
  DrawSceneOverlay,
  DimBackground,
  DrawText,
  StopBackgroundTrack,
  StopGameOverSoundTrack,
  RestartLevel,
  AdjustVolume,
  StartSinglePlayer,
  StartMultiPlayerHost,
  StartMultiPlayerClient,
  NextView,
  QuitGame,
};

struct UIAction {
  UIActionType type;
  float value{0.0f};
  std::optional<UIManager::GameView> nextView;
};

class UIController {
public:
  std::vector<UIAction> fromEngineActions(const UIManager::UIActions& actions) const;
  UIManager::UIActions toEngineActions(const std::vector<UIAction>& actions) const;
};

} // namespace game
