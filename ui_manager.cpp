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


  void UI_Manager::renderMainMenu(const game_engine::SDLState& sdlState, float deltaTime, Animation* anim, SDL_Texture* tex) {

    if (!anim || !tex) {
      return;
    }

    clearRenderer(sdlState);

        // 800w, 540h
    float frameW = 800;
    float frameH = 540;
    anim->step(deltaTime);

      // select frame from sprite sheet
    // float srcX = m_resources.mainMenuAnim.currentFrame() * frameW;

    int cols = 10; // frames per row in your new sheet
    int frame = anim->currentFrame();
    int col = frame % cols;
    int row = frame / cols;
    float srcX = col * frameW;
    float srcY = row * frameH;
    SDL_FRect src{srcX, srcY, frameW, frameH};

    // SDL_FRect src{srcX, 0, frameW, frameH};

    // // scale sprites up or down
    // float drawW = frameW / obj.drawScale;
    float drawW = frameW / 1.2;
    float drawH = frameH / 1.2;

    SDL_FRect dst{
      0,
      -50,
      drawW,
      drawH
    };

    SDL_RenderTexture(sdlState.renderer, tex, &src, &dst);

    renderPresent(sdlState);

  }

  // Local helper for the main menu (not a member).
  UIActions UI_Manager::drawMainMenu(const UISnapshots& snaps, ImGuiWindowFlags flags, const game_engine::SDLState& sdlState) {

        // renderer output and logical ref
    int outW, outH; SDL_GetRenderOutputSize(sdlState.renderer, &outW, &outH);
    const float refW = 1600.0f, refH = 900.0f;
    // letterbox scale/offset
    float scalePos = std::min(outW / refW, outH / refH);
    float offX = (outW - refW * scalePos) * 0.5f;
    float offY = (outH - refH * scalePos) * 0.5f;

    // anchor in reference pixels (where the art is)
    ImVec2 anchor = ImVec2(offX + 880.0f * scalePos,
                          offY + 140.0f * scalePos);

    // Only downscale size (don’t upscale)
    float scaleSize = std::min(scalePos, 1.0f);
    float btnW = 500.0f * scaleSize;
    float btnH = 125.0f * scaleSize;
    float spacing = 5.0f * scaleSize;

    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(ImVec2((float)outW, (float)outH));
    ImGui::Begin("##menu_hitboxes", nullptr, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|
                                        ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
                                        ImGuiWindowFlags_NoScrollbar);


    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.2f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1,1,1,0.3f));

    // draw buttons at anchor + vertical spacing
    ImVec2 pos = anchor;
    auto place = [&](const char* id, auto onClick) {
        ImGui::SetCursorScreenPos(pos);
        if (ImGui::Button(id, ImVec2(btnW, btnH))) onClick();
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        pos.y += btnH + spacing;
    };

    UIActions act;
    act.stopBackgroundTrack = true;
    place("##single", [&]{ act.nextView = GameView::Playing; act.startSinglePlayer = true; });
    place("##multi",  [&]{ act.nextView = GameView::MultiPlayerOptionsMenu; });
    place("##quit",   [&]{ act.quitGame = true; });

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
    ImGui::End();


    renderMainMenu(sdlState, snaps.deltaTime, snaps.mainMenuAnim, snaps.mainMenuTex);
    act.blockGameLoopUpdates = true;

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
