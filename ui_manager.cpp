#include "ui_manager.h"
#include "gameengine.h"
#include "imgui.h"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui_impl_sdl3.h"

#include <iostream>

namespace UIManager {


  void UI_Manager::renderPresent(const game_engine::SDLState& sdlState) {

    SDL_SetRenderLogicalPresentation(sdlState.renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);

    // ui.render()
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdlState.renderer);
    SDL_SetRenderLogicalPresentation(sdlState.renderer, sdlState.logW, sdlState.logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    // renderer.present()
    SDL_RenderPresent(sdlState.renderer);
  }

  void UI_Manager::clearRenderer(const game_engine::SDLState& sdlState){
    // clear the backbuffer before drawing onto it with black from draw color above
    SDL_SetRenderDrawColor(sdlState.renderer, 20, 10, 30, 255);
    SDL_RenderClear(sdlState.renderer);
  }


  // void UI_Manager::renderMainMenu(const game_engine::SDLState& sdlState, float deltaTime, const Animation* anim, SDL_Texture* tex) {

  //   if (!anim || !tex) {
  //     return;
  //   }

  //   // 800w, 540h
  //   float frameW = 800;
  //   float frameH = 540;

  //     // select frame from sprite sheet
  //   float srcX = anim->currentFrame() * frameW;

  //   SDL_FRect src{srcX, 0, frameW, frameH};

  //   // scale sprites up or down
  //   // float drawW = frameW / obj.drawScale;
  //   float drawW = frameW / 1.5;
  //   float drawH = frameH / 1.5;

  //   SDL_FRect dst{
  //     0,
  //     0,
  //     drawW,
  //     drawH
  //   };

  //   SDL_RenderTexture(sdlState.renderer, tex, nullptr, &dst);

  // }

  // Local helper for the main menu (not a member).
  UIActions UI_Manager::drawMainMenu(const UISnapshots& snaps, ImGuiWindowFlags flags, const game_engine::SDLState& sdlState) {

  // renderMainMenu(sdlState, snaps.deltaTime, &snaps.mainMenuAnim, snaps.mainMenuTex);

    // , float deltaTime, Animation& anim, SDL_Texture* tex
    // compute tile layout

    // collect imgui interaction state

    //render with sdl and renderer; give renderer ref, and sdl_state ref to UI_Manager

    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(snaps.winDims);
    ImGui::Begin("##title_hitboxes", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);

      UIActions act;
      act.stopBackgroundTrack = true;
      // ImGui::Begin("Main Menu", nullptr, flags);
      if (ImGui::Button("Single Player", defaultButtonSize)) {
          std::cout << "start game" << std::endl;
          act.nextView = GameView::Playing;
          act.startSinglePlayer = true;
      }
      if (ImGui::Button("Multiplayer", defaultButtonSize)) {
          act.nextView = GameView::MultiPlayerOptionsMenu;
          std::cout << "multi game" << std::endl;
      }
      if (ImGui::Button("Quit", defaultButtonSize)) {
          std::cout << "quit game" << std::endl;
          act.quitGame = true;
      }
      ImGui::End();
      return act;
  }

  UIActions UI_Manager::drawLoading(const LoadingSnapshot& ls, ImGuiWindowFlags flags) {
      UIActions act;
      ImGui::Begin("Loading", nullptr, flags);
      ImGui::Text("Loading level...");
      ImGui::ProgressBar(ls.progress01, ImVec2(200, 0));
      ImGui::End();
      if (ls.done) {
        act.finishLoading = true;
        act.nextView = GameView::Playing;
      } else {
        act.blockGameLoopUpdates = true; // block updating gameState while next level is loading
        ImGui::Render(); // must force render
      }
      return act;
  }

  UIActions UI_Manager::drawInventoryMenu(const UISnapshots& snaps, ImGuiWindowFlags flags) {
      UIActions act;
      ImGui::Begin("Inventory Menu", nullptr, flags);
      if (ImGui::Button("Resume")) act.nextView = UIManager::GameView::Playing;
      if (ImGui::Button("Quit")) act.quitGame = true;
      // Want to continue rendering the screen underneath. The Pause Menu just overlays.
      ImGui::End();
      return act;
  }

  UIActions UI_Manager::drawMultiplayerOptionsMenu(const UISnapshots& snaps, ImGuiWindowFlags flags) {
      UIActions act;
      ImGui::Begin("MultiPlayer Menu", nullptr, flags);
      if (ImGui::Button("Host A Game", defaultButtonSize)) {
        act.startMultiPlayerHost = true;
        act.nextView = UIManager::GameView::Playing;
      }
      if (ImGui::Button("Join A Game", defaultButtonSize)) {
        act.startMultiPlayerClient = true;
        act.nextView = UIManager::GameView::Playing;
      }
      if (ImGui::Button("Back to Menu", defaultButtonSize)) {
        act.nextView = UIManager::GameView::MainMenu;
      }
      ImGui::End();
      return act;
  }


  UIActions UI_Manager::drawGameOver(const LoadingSnapshot&, ImGuiWindowFlags flags) {
      UIActions act;
      act.stopBackgroundTrack = true;
      ImGui::Begin("GameOver", nullptr, flags);
      ImGui::Text("GAME OVER");
      if (ImGui::Button("Try Again")) {
        act.restartLevel = true;
        act.stopGameOverSoundTrack = true;
        act.nextView = GameView::Playing;
      }
      ImGui::End();
      return act;
  }

  UIActions UI_Manager::drawGameplay(const UISnapshots& snaps, ImGuiWindowFlags flags) {
      UIActions act;
      ImGuiWindowFlags windowFlags = flags | ImGuiWindowFlags_NoBackground;
      ImGui::Begin("HUD", nullptr, windowFlags);
      // Optional: remove padding so the button hugs the corner
      ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
      if (ImGui::Button("Back to Menu", defaultButtonSize)) {
          act.nextView = UIManager::GameView::MainMenu;
          // currentView = UIManager::GameView::MainMenu;
      }
      ImGui::SameLine(0, 2.0f);
      if (ImGui::Button("Save Game", defaultButtonSize)) {
        // TODO
      }
      ImGui::SameLine(0, 2.0f);
      if (ImGui::Button("Pause Game", defaultButtonSize)) {
        act.nextView = UIManager::GameView::PauseMenu;
          // currentView = UIManager::GameView::PauseMenu;
      }
      ImGui::SameLine(0, 2.0f);
      if (ImGui::Button("Start Over (Debug)", defaultButtonSize)) {
        act.restartLevel = true;
        act.stopGameOverSoundTrack = true;
      }

      ImGui::PopItemFlag();
      ImGui::PopStyleVar();
      ImGui::End();

      drawPlayerHealthbar(snaps.playerHP, flags);


      return act;
  }


  void UI_Manager::drawPlayerHealthbar(const int playerHP, ImGuiWindowFlags flags) {
      ImGui::SetNextWindowPos(ImVec2(10, 10));
      ImGui::Begin("HUD", nullptr, ImGuiWindowFlags_NoTitleBar |
                                  ImGuiWindowFlags_NoBackground |
                                  ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove);
      float hpFrac = static_cast<float>(playerHP) / 100.0; // 0..1
      ImGui::Text("HP");
      ImGui::PushStyleColor(ImGuiCol_PlotHistogram, IM_COL32(0, 200, 0, 255)); // green
      ImGui::ProgressBar(hpFrac, ImVec2(150, 24));ImGui::PopStyleColor();
      ImGui::End();
  }

  UIActions UI_Manager::renderView(GameView view, const UISnapshots& snaps, ImGuiWindowFlags flags, const game_engine::SDLState& sdlState) {
      ImGui_ImplSDLRenderer3_NewFrame();
      ImGui_ImplSDL3_NewFrame();
      ImGui::NewFrame();

      ImGuiIO& io = ImGui::GetIO();
      ImGui::SetNextWindowPos(ImVec2(0, 0));
      ImGui::SetNextWindowSize(io.DisplaySize);

      switch (view) {
        case GameView::MainMenu:     return drawMainMenu(snaps, flags, sdlState);
        case GameView::LevelLoading: return drawLoading(snaps.loading, flags);
        case GameView::GameOver: return drawGameOver(snaps.loading, flags);
        case GameView::Playing: return drawGameplay(snaps, flags);
        case GameView::InventoryMenu: return drawInventoryMenu(snaps, flags);
        case GameView::PauseMenu: return drawInventoryMenu(snaps, flags); // same as inventory menu because pauses game
        case GameView::MultiPlayerOptionsMenu: return drawMultiplayerOptionsMenu(snaps, flags);
        default:                     return {};
      }
  }


}
