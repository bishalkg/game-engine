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
  SelectPlayerCharacter,
  NextView,
  QuitGame,
};

struct UIAction {
  UIActionType type;
  float value{0.0f};
  std::optional<SpriteType> selectedPlayerSprite;
  std::optional<UIManager::GameView> nextView;
};

// UIManager = rendering + generic controls; UIController = your game’s decision logic and action semantics.
// Right now yours is pass-through, but it becomes valuable once UI behavior gets non-trivial.

//   Typical uses:

// Action remapping/translation

// Convert low-level UI events into domain actions (EquipItem, CraftRecipe, SpendSkillPoint) that UIManager shouldn’t know about.
// Validation + gating

// Example: UI says “Start Multiplayer”, but UIController blocks it until username/server settings are valid.
// Priority/conflict resolution

// If multiple UI signals happen in one frame (pause + inventory + cutscene skip), UIController decides precedence.
// Stateful UI workflows

// Multi-step flows like settings apply/revert, confirmation modals, tutorial steps, quest dialogue branches.
// Mode-specific behavior

// Same button meaning changes by mode (MainMenu, Pause, Inventory, Vendor, Crafting).
// Game economy/rules integration

// UI click “Buy” becomes “Buy if enough gold, stock > 0, inventory space exists”.
// Analytics/telemetry hooks

// Central place to log meaningful UI intents (OpenedInventory, ChangedVolume, AcceptedQuest).
class UIController {
public:
  std::vector<UIAction> fromEngineActions(const UIManager::UIActions& actions) const;
  UIManager::UIActions toEngineActions(const std::vector<UIAction>& actions) const;
};

} // namespace game
