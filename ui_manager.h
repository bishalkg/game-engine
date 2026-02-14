#pragma once

#include "imgui.h"
#include "animation.h"
#include <SDL3/SDL_render.h>
#include <optional>
#include <vector>


namespace game_engine { struct SDLState; }

namespace UIManager {

  class GameState; // need foreward declare

  enum class GameView {
      Playing,
      MainMenu,
      PauseMenu,
      CutScene,
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

  struct Scene {
    SDL_Texture* tex;
    Animation* anim;
    // other things needed for scene
  };

  struct UISnapshots {
    LoadingSnapshot loading; /* add title/pause data */
    int playerHP;
    ImVec2 winDims;
    float deltaTime;

    Animation* mainMenuAnim{nullptr};
    SDL_Texture* mainMenuTex{nullptr};

    bool advanceToNextScene = false;
    const std::vector<Scene>* cutscene{nullptr};
    int cutSceneID = -1; // test with main menu?
  };

  // cutscene manager gets set once when we enter GameView::CutScene

  struct CutSceneManager {
    int cutSceneID = -1; // if ID changes, init new CutScene
    const std::vector<Scene>* scenes = nullptr; // pointed to vector is ready only
    size_t index = 0;
    bool blocking = false;

    void start(int sceneID, const std::vector<Scene>* newScenes);

    void update(float deltaTime, const UISnapshots& snaps);

    void render(const game_engine::SDLState& sdlState);

    bool isFinished();

  };

  class UI_Manager {
    public:
      UI_Manager() = default;
      ~UI_Manager() = default;

      UIActions renderView(GameView view, const UISnapshots& snaps, ImGuiWindowFlags flags, const game_engine::SDLState& sdlState);

      void renderPresent(const game_engine::SDLState& sdlState);
      void clearRenderer(const game_engine::SDLState& sdlState);

      // move to cutscene manager.
      void renderMainMenu(const game_engine::SDLState& sdlState, float deltaTime, Animation* anim, SDL_Texture* tex);


    private:
      UIActions drawLoading(const UISnapshots& snaps, ImGuiWindowFlags);
      UIActions drawGameOver(const LoadingSnapshot& /*unused*/ , ImGuiWindowFlags flags);
      UIActions drawMainMenu(const UISnapshots& snaps, ImGuiWindowFlags flags, const game_engine::SDLState& sdlState);
      UIActions drawGameplay(const UISnapshots& snaps, ImGuiWindowFlags flags);
      UIActions drawInventoryMenu(const UISnapshots& snaps, ImGuiWindowFlags flags);
      UIActions drawMultiplayerOptionsMenu(const UISnapshots& snaps, ImGuiWindowFlags flags);


      void drawPlayerHealthbar(const int playerHP, ImGuiWindowFlags flags);


    private:
      ImVec2 defaultButtonSize = ImVec2(150, 50);
      CutSceneManager cutsceneMgr;

  };


}
