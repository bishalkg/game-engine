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
    bool blockMainGameDraw = false;
    bool blockGameplayUpdates = false;
    bool drawSceneOverlay = false;
    bool dimBackground = false;
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
    int numFrameColumns;
    float frameW;
    float frameH;
    float yOffset = 0;
    float xOffset = 0;
    float scale = 1.0;
    bool loopScene = false;
    // other things needed for scene
  };

  struct UISnapshots {
    LoadingSnapshot loading; /* add title/pause data */
    int playerHP;
    int playerMana;
    ImVec2 winDims;
    float deltaTime;

    Animation* mainMenuAnim{nullptr};
    SDL_Texture* mainMenuTex{nullptr};

    bool advanceToNextScene = false;
    bool togglePauseGameplay = false;
    const std::vector<Scene>* cutscene{nullptr};
    int cutSceneID = -1; // test with main menu?
  };

  // cutscene manager gets set once when we enter GameView::CutScene

  struct CutSceneManager {
    int cutSceneID = -1; // if ID changes, init new CutScene
    const std::vector<Scene>* scenes = nullptr; // pointed to vector is ready only
    size_t sceneIndex = 0;
    bool doneWithScene = false; // indicates whether you are done with the current animation in the scenes vector.
    // bool loopScene = false;
    // bool doneWithCutscene = false;

    void start(int sceneID, const std::vector<Scene>* newScenes);

    void update(bool playNextScene, float deltaTime, const UISnapshots& snaps);

    bool isCutsceneComplete();
    bool isCurrentSceneComplete();

    const Scene& currScene();

  };

  class UI_Manager {
    public:
      UI_Manager(game_engine::SDLState& sdl): sdlState(sdl) {};
      ~UI_Manager() = default;

      UIActions renderView(GameView view, const UISnapshots& snaps, ImGuiWindowFlags flags, const game_engine::SDLState& sdlState);

      void renderPresent(const game_engine::SDLState& sdlState);
      void clearRenderer(const game_engine::SDLState& sdlState);

      void draw(const game_engine::SDLState& sdlState, float deltaTime, bool dimBackground);


    private:
      UIActions drawLoading(const UISnapshots& snaps, ImGuiWindowFlags);
      UIActions drawGameOver(const LoadingSnapshot& /*unused*/ , ImGuiWindowFlags flags);
      UIActions drawMainMenu(const UISnapshots& snaps, ImGuiWindowFlags flags, const game_engine::SDLState& sdlState);
      UIActions drawGameplay(const UISnapshots& snaps, ImGuiWindowFlags flags);
      UIActions drawPausedMenu(const UISnapshots& snaps, ImGuiWindowFlags flags);
      UIActions drawMultiplayerOptionsMenu(const UISnapshots& snaps, ImGuiWindowFlags flags);


      void drawPlayerHealthbar(const std::string& name, const int playerHP, ImU32 color, ImGuiWindowFlags flags);


    private:
      ImVec2 defaultButtonSize = ImVec2(150, 50);
      CutSceneManager cutsceneMgr;
      game_engine::SDLState& sdlState;
  };


}
