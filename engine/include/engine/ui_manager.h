#pragma once

#include "engine/level_types.h"
#include "imgui.h"
#include "engine/animation.h"
#include <SDL3/SDL_render.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>


namespace game_engine { struct SDLState; }

namespace UIManager {

  class GameState; // need foreward declare

  enum class GameView {
      Playing,
      MainMenu,
      PauseMenu,
      CutScene,
      CharacterSelect,
      LevelSection,
      LevelLoading,
      InventoryMenu,
      GameOver,
      MultiPlayerOptionsMenu, // this menu will show host or client buttons
      MultiplayerBrowse,
      MultiplayerHostWaiting,
  };

  struct MultiplayerSessionDisplay {
    std::string hostName;
    std::string hostAddress;
    std::string levelName;
    uint32_t playerCount = 0;
  };

  struct LoadingSnapshot { float progress01; bool done; };
  struct UIActions {
    bool finishLoading = false;
    bool blockMainGameDraw = false;
    bool blockGameplayUpdates = false;
    bool drawSceneOverlay = false;
    bool dimBackground = false;
    bool drawText = false;
    bool stopBackgroundTrack = false;
    bool stopGameOverSoundTrack = false;
    bool restartLevel = false;
    float newVolume = 0.0f;
    bool adjustVolume = false;
    std::optional<bool> startSinglePlayer;
    std::optional<bool> startMultiPlayerHost;
    std::optional<bool> startMultiPlayerClient;
    std::optional<SpriteType> selectedPlayerSprite;
    std::optional<size_t> selectedSessionIndex;
    std::optional<GameView> nextView;
    bool quitGame = false;
  };

  struct Cutscene {
    SDL_Texture* tex;
    std::shared_ptr<Animation> anim;
    std::vector<std::string> dialogue; // each animation can have multiple bubbles of dialogue
    // int currDialogueIdx = 0; // the current dialogue displayed
    int numFrameColumns;
    float frameW;
    float frameH;
    float yOffset = 0;
    float xOffset = 0;
    float scale = 1.0;
    bool loopScene = false;
  };

  struct UISnapshots {
    LoadingSnapshot loading; /* add title/pause data */
    int playerHP;
    int playerMana;
    int playerUltimate;
    bool playerUltimateReady = false;
    ImVec2 winDims;
    float deltaTime;
    float currVolume;

    Animation* mainMenuAnim{nullptr};
    SDL_Texture* mainMenuTex{nullptr};

    bool debugMode = false;
    bool advanceToNextScene = false;
    bool togglePauseGameplay = false;
    const std::vector<Cutscene>* cutscene{nullptr};
    int cutSceneID = -1; // test with main menu?
    std::vector<MultiplayerSessionDisplay> multiplayerSessions;
    std::string multiplayerStatus;
  };

  // cutscene manager gets set once when we enter GameView::CutScene

  struct CutscenePlayer {
    int cutSceneID = -1; // if ID changes, init new CutScene
    const std::vector<Cutscene>* scenes = nullptr; // pointed to vector is ready only
    size_t sceneIndex = 0;
    bool doneWithCurrScene = false; // indicates
    float elapsed = 0.0f;
    float charsPerSecond = 15.0f;
    int visibleChars = 0;

    int currDialogueIdx = 0; // the current dialogue displayed
    bool endOfCurrDialogue = false;
    bool showNextDialogue = false;
    // other things needed for scene

    // void step(float dt) {
    //   elapsed += dt;
    // }
    // bool loopScene = false;
    // bool doneWithCutscene = false;

    void start(int sceneID, const std::vector<Cutscene>* newScenes);

    void update(bool playNextScene, float deltaTime, const UISnapshots& snaps);

    bool isCutsceneComplete();
    bool isCurrentSceneComplete();

    const Cutscene& currScene();

  };

  class UI_Manager {
    public:
      UI_Manager(game_engine::SDLState& sdl, TTF_Font& ttfFont): sdlState(sdl),  font(ttfFont){};
      ~UI_Manager() = default;

      // Primitive frame lifecycle/helpers for game-side UI composition.
      void beginFrame();
      void endFrame(const game_engine::SDLState& sdlState);
      bool button(const char* label, ImVec2 size = ImVec2(0, 0));
      bool sliderFloat(const char* label, float* v, float v_min, float v_max);
      void text(const char* text);

      UIActions getRenderViewActions(GameView view, const UISnapshots& snaps, ImGuiWindowFlags flags, const game_engine::SDLState& sdlState);

      void renderPresent(const game_engine::SDLState& sdlState);
      void clearRenderer(const game_engine::SDLState& sdlState);

      void draw(const game_engine::SDLState& sdlState, float deltaTime, bool dimBackground, bool drawDialogue, int visible);


    private:
      UIActions drawLoading(const UISnapshots& snaps, ImGuiWindowFlags);
      UIActions drawGameOver(const LoadingSnapshot& /*unused*/ , ImGuiWindowFlags flags);
      UIActions drawMainMenu(const UISnapshots& snaps, ImGuiWindowFlags flags, const game_engine::SDLState& sdlState);
      UIActions drawGameplay(const UISnapshots& snaps, ImGuiWindowFlags flags);
      UIActions drawPausedMenu(const UISnapshots& snaps, ImGuiWindowFlags flags);
      UIActions drawCharacterSelectScreen(const UISnapshots& snaps, ImGuiWindowFlags flags);
      UIActions drawLevelSelectScreen(const UISnapshots& snaps, ImGuiWindowFlags flags);
      UIActions drawMultiplayerOptionsMenu(const UISnapshots& snaps, ImGuiWindowFlags flags);
      UIActions drawMultiplayerBrowse(const UISnapshots& snaps, ImGuiWindowFlags flags);
      UIActions drawMultiplayerHostWaiting(const UISnapshots& snaps, ImGuiWindowFlags flags);
      float drawCustomSlider(const std::string& label, float currVal, float v_min, float v_max);


      void drawPlayerBar(
        const std::string& name,
        int value,
        ImU32 color,
        float yOffset,
        bool highlightReady);


    private:
      ImVec2 defaultButtonSize = ImVec2(150, 50);
      CutscenePlayer cutscenePlr;
      game_engine::SDLState& sdlState;
      TTF_Font& font;
      bool wantsHandCursor = false;
      bool debugMode = false;
  };


}
