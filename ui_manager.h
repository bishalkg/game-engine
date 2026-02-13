#pragma once

#include "imgui.h"
#include <optional>


namespace UIManager {

  class GameState; // need foreward declare

  enum class GameView {
      Playing,
      MainMenu,
      PauseMenu,
      LevelLoading,
      InventoryMenu,
      GameOver,
      MultiPlayerOptionsMenu, // this menu will show host or client buttons
  };

  struct LoadingSnapshot { float progress01; bool done; };
  struct UIActions {
    bool finishLoading = false;
    bool stopBackgroundTrack = false;
    std::optional<bool> startSinglePlayer;
    std::optional<GameView> nextView;
    bool quitGame = false;
  };
  struct UISnapshots {
    LoadingSnapshot loading; /* add title/pause data */
  };

  class UI_Manager {
    public:
      UI_Manager() = default;
      ~UI_Manager() = default;

      UIActions renderView(GameView view, const UISnapshots& snaps, ImGuiWindowFlags flags);

    private:
      UIActions drawLoading(const LoadingSnapshot&, ImGuiWindowFlags);
  };


}
