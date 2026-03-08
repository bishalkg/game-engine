#include "game/ui_controller.h"

namespace game {

std::vector<UIAction> UIController::fromEngineActions(const UIManager::UIActions& actions) const {
  std::vector<UIAction> out;

  if (actions.finishLoading) out.push_back({UIActionType::FinishLoading});
  if (actions.blockMainGameDraw) out.push_back({UIActionType::BlockMainGameDraw});
  if (actions.blockGameplayUpdates) out.push_back({UIActionType::BlockGameplayUpdates});
  if (actions.drawSceneOverlay) out.push_back({UIActionType::DrawSceneOverlay});
  if (actions.dimBackground) out.push_back({UIActionType::DimBackground});
  if (actions.drawText) out.push_back({UIActionType::DrawText});
  if (actions.stopBackgroundTrack) out.push_back({UIActionType::StopBackgroundTrack});
  if (actions.stopGameOverSoundTrack) out.push_back({UIActionType::StopGameOverSoundTrack});
  if (actions.restartLevel) out.push_back({UIActionType::RestartLevel});
  if (actions.adjustVolume) out.push_back({UIActionType::AdjustVolume, actions.newVolume});
  if (actions.startSinglePlayer.has_value() && *actions.startSinglePlayer) out.push_back({UIActionType::StartSinglePlayer});
  if (actions.startMultiPlayerHost.has_value() && *actions.startMultiPlayerHost) out.push_back({UIActionType::StartMultiPlayerHost});
  if (actions.startMultiPlayerClient.has_value() && *actions.startMultiPlayerClient) out.push_back({UIActionType::StartMultiPlayerClient});
  if (actions.nextView.has_value()) out.push_back({UIActionType::NextView, 0.0f, actions.nextView});
  if (actions.quitGame) out.push_back({UIActionType::QuitGame});

  return out;
}

UIManager::UIActions UIController::toEngineActions(const std::vector<UIAction>& actions) const {
  UIManager::UIActions out{};

  for (const auto& action : actions) {
    switch (action.type) {
      case UIActionType::FinishLoading: out.finishLoading = true; break;
      case UIActionType::BlockMainGameDraw: out.blockMainGameDraw = true; break;
      case UIActionType::BlockGameplayUpdates: out.blockGameplayUpdates = true; break;
      case UIActionType::DrawSceneOverlay: out.drawSceneOverlay = true; break;
      case UIActionType::DimBackground: out.dimBackground = true; break;
      case UIActionType::DrawText: out.drawText = true; break;
      case UIActionType::StopBackgroundTrack: out.stopBackgroundTrack = true; break;
      case UIActionType::StopGameOverSoundTrack: out.stopGameOverSoundTrack = true; break;
      case UIActionType::RestartLevel: out.restartLevel = true; break;
      case UIActionType::AdjustVolume:
        out.adjustVolume = true;
        out.newVolume = action.value;
        break;
      case UIActionType::StartSinglePlayer: out.startSinglePlayer = true; break;
      case UIActionType::StartMultiPlayerHost: out.startMultiPlayerHost = true; break;
      case UIActionType::StartMultiPlayerClient: out.startMultiPlayerClient = true; break;
      case UIActionType::NextView: out.nextView = action.nextView; break;
      case UIActionType::QuitGame: out.quitGame = true; break;
    }
  }

  return out;
}

} // namespace game
