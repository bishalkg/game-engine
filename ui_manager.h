#pragma once

#include "imgui.h"
#include <optional>


namespace game_engine { struct SDLState; }

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
    bool blockGameLoopUpdates = false;
    bool stopBackgroundTrack = false;
    bool stopGameOverSoundTrack = false;
    bool restartLevel = false;
    std::optional<bool> startSinglePlayer;
    std::optional<bool> startMultiPlayerHost;
    std::optional<bool> startMultiPlayerClient;
    std::optional<GameView> nextView;
    bool quitGame = false;
  };
  struct UISnapshots {
    LoadingSnapshot loading; /* add title/pause data */
    int playerHP;
    ImVec2 winDims;
  };

  class UI_Manager {
    public:
      UI_Manager() = default;
      ~UI_Manager() = default;

      UIActions renderView(GameView view, const UISnapshots& snaps, ImGuiWindowFlags flags, const game_engine::SDLState& sdlState);

      void renderPresent(const game_engine::SDLState& sdlState);
      void clearRenderer(const game_engine::SDLState& sdlState);


    private:
      UIActions drawLoading(const LoadingSnapshot&, ImGuiWindowFlags);
      UIActions drawGameOver(const LoadingSnapshot& /*unused*/ , ImGuiWindowFlags flags);
      UIActions drawMainMenu(const UISnapshots& snaps, ImGuiWindowFlags flags, const game_engine::SDLState& sdlState);
      UIActions drawGameplay(const UISnapshots& snaps, ImGuiWindowFlags flags);
      UIActions drawInventoryMenu(const UISnapshots& snaps, ImGuiWindowFlags flags);
      UIActions drawMultiplayerOptionsMenu(const UISnapshots& snaps, ImGuiWindowFlags flags);


      void drawPlayerHealthbar(const int playerHP, ImGuiWindowFlags flags);


    private:
      ImVec2 defaultButtonSize = ImVec2(150, 50);

  };


}
