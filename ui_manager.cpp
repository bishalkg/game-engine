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

  void UI_Manager::render(const game_engine::SDLState& sdlState, float deltaTime, const Scene& scene) {

    if (!scene.anim || !scene.tex) {
      return;
    }

    clearRenderer(sdlState);

        // 800w, 540h
    // float frameW = frameW;
    // float frameH = frameH;
    // scene.anim->step(deltaTime); // TODO this would step twice currently

      // select frame from sprite sheet
    // float srcX = m_resources.mainMenuAnim.currentFrame() * frameW;

    int cols = scene.numFrameColumns; // frames per row in your new sheet
    int frame = scene.anim->currentFrame();
    int col = frame % cols;
    int row = frame / cols;
    float srcX = col * scene.frameW;
    float srcY = row * scene.frameH;
    SDL_FRect src{srcX, srcY, scene.frameW, scene.frameH};

    // SDL_FRect src{srcX, 0, frameW, frameH};

    // // scale sprites up or down
    // float drawW = frameW / obj.drawScale;
    float drawW = scene.frameW / scene.scale; // 1.2
    float drawH = scene.frameH / scene.scale;

    SDL_FRect dst{
      scene.xOffset, // 0
      scene.yOffset, // -50
      drawW,
      drawH
    };

    SDL_RenderTexture(sdlState.renderer, scene.tex, &src, &dst);

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
    // ImVec2 anchor = ImVec2(offX + 880.0f * scalePos,
    //                       offY + 140.0f * scalePos);
    ImVec2 anchor = ImVec2(offX + 880.0f * scalePos, offY + 80.0f * scalePos);

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
    place("##single", [&]{
      act.nextView = GameView::CutScene;
      // act.nextView = GameView::Playing; // need to set this one the cutscene is done
      // act.startSinglePlayer = true;
    });
    place("##multi",  [&]{ act.nextView = GameView::MultiPlayerOptionsMenu; });
    place("##quit",   [&]{ act.quitGame = true; });

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
    ImGui::End();

    // animated backdrop: stepped in renderView before this call
    if (cutsceneMgr.scenes && !cutsceneMgr.scenes->empty()) {
      render(sdlState, snaps.deltaTime, cutsceneMgr.currScene());
    }
    // render(sdlState, snaps.deltaTime, Scene{
    //   .tex = snaps.mainMenuTex,
    //   .anim = snaps.mainMenuAnim,
    //   .scale = 1.2,
    //   .numFrameColumns = 10,
    //   .frameH = 540.0f,
    //   .frameW = 800.0f,
    //   .yOffset = -50
    // });
    act.blockGameLoopUpdates = true;

    return act;
  }

  UIActions UI_Manager::drawLoading(const UISnapshots& snaps, ImGuiWindowFlags flags) {
      UIActions act;
      ImGui::Begin("Loading", nullptr, flags);
      ImGui::Text("Loading level...");
      ImGui::ProgressBar(snaps.loading.progress01, ImVec2(200, 0));
      ImGui::End();
      if (snaps.loading.done) {
        act.finishLoading = true;

        if (snaps.cutscene != nullptr && snaps.cutSceneID >= 0) {
          cutsceneMgr.start(snaps.cutSceneID, snaps.cutscene);
          act.nextView = GameView::CutScene;
          act.blockGameLoopUpdates = true;
        } else {
          act.nextView = GameView::Playing;
        }

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
      // drawPlayerHealthbar(snaps.playerHP, flags); // mana bar

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
        case GameView::MainMenu:
        {
          if (cutsceneMgr.cutSceneID != snaps.cutSceneID && snaps.cutscene) {
            cutsceneMgr.start(snaps.cutSceneID, snaps.cutscene);
          }
          cutsceneMgr.update(snaps.advanceToNextScene, snaps.deltaTime, snaps);
          return drawMainMenu(snaps, flags, sdlState);
        }
        case GameView::LevelLoading: return drawLoading(snaps, flags);
        case GameView::GameOver: return drawGameOver(snaps.loading, flags);
        case GameView::Playing: return drawGameplay(snaps, flags);
        case GameView::InventoryMenu: return drawInventoryMenu(snaps, flags);
        case GameView::PauseMenu: return drawInventoryMenu(snaps, flags); // same as inventory menu because pauses game
        case GameView::MultiPlayerOptionsMenu: return drawMultiplayerOptionsMenu(snaps, flags);
        case GameView::CutScene:
        {
          bool playNextScene = snaps.advanceToNextScene; // user hit return/enter, force advance to next scene

          if (cutsceneMgr.cutSceneID != snaps.cutSceneID && snaps.cutscene) {
            std::cout << "starting cutscene" << std::endl;
            cutsceneMgr.start(snaps.cutSceneID, snaps.cutscene);
          }

          UIActions act;
          cutsceneMgr.update(snaps.advanceToNextScene, snaps.deltaTime, snaps);
          if (cutsceneMgr.scenes && !cutsceneMgr.scenes->empty() && !cutsceneMgr.isCutsceneComplete()) {
            render(sdlState, snaps.deltaTime, cutsceneMgr.currScene());
          } else {
            std::cout << "cutscene complete" << std::endl;
            act.nextView = GameView::Playing; // need to set this one the cutscene is done
            act.startSinglePlayer = true;
            return act;
          }

          // After LevelLoading completes, if currLevel has a cutscene
          // call cutsceneManager.start(data) and set currView = CutScene and blockGameLoopUpdates = true;
          // advanceToNextScene = true means go to next idx in vector.
          // cutsceneID->vector of {Animation, Texture}. pass all animations and textures stored on the currLevel for this particular cutscene. UI Manager just sees this vector and inputs from snap.
          // as long as the view is cutscene, invoke the cutSceneManagers update/render method.
          // blockGameLoopUpdates is true while we are not at the end of the vector.
          // we pass in user inputs also to skip to next cutscene animation. When user hits enter,
          // the cutsceneManager.update will step the vector of animations to the next index, and start
          // playing the next animation.
          // once we reach the last animation, after the use hit enter, we set GameView::Playing, and blockGameLoopUpdates = false;
          // should this happen after LevelLoading? or During LevelLoading?
          // UIActions act;
          // if (cutsceneMgr.isCutsceneComplete()) {
          //   std::cout << "cutscene complete" << std::endl;
          //   act.nextView = GameView::Playing; // need to set this one the cutscene is done
          //   act.startSinglePlayer = true;
          //   return act;
          // }

        }
        default:                     return {};
      }
  }


  void CutSceneManager::start(int sceneID, const std::vector<Scene>* newScenes) {
      cutSceneID = sceneID;
      scenes = newScenes;
      sceneIndex = 0;
      doneWithScene = false;
  }

  const Scene& CutSceneManager::currScene() {
    static const Scene kDummy{nullptr, nullptr, 1, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    if (!scenes || scenes->empty() || sceneIndex >= scenes->size()) {
      return kDummy;
    }
    return scenes->at(sceneIndex);
  }


void CutSceneManager::update(bool playNextScene, float deltaTime, const UISnapshots& /*snaps*/) {
    if (!scenes || scenes->empty() || sceneIndex >= scenes->size()) return;

    const Scene& scene = currScene();
    if (scene.anim) scene.anim->step(deltaTime);

    bool advance = playNextScene;
    if (!advance && scene.anim) {
      advance = scene.anim->isDone();
    }

    if (advance) {
      if (scene.anim) scene.anim->reset();
      ++sceneIndex;
      if (sceneIndex < scenes->size()) {
        auto &next = scenes->at(sceneIndex);
        if (next.anim) next.anim->reset();
      }
    }
  };



  bool CutSceneManager::isCutsceneComplete(){
    if (!scenes || scenes->empty()) {
      return true;
    }
    return sceneIndex >= scenes->size();
  };

  bool CutSceneManager::isCurrentSceneComplete(){
    if (!scenes || scenes->empty() || sceneIndex >= scenes->size()) return true;
    const auto &scene = scenes->at(sceneIndex);
    return scene.anim ? scene.anim->isDone() : true;
  };


}
