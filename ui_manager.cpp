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

  void UI_Manager::draw(const game_engine::SDLState& sdlState, float deltaTime, bool dimBackground) {

    const Scene& scene = cutsceneMgr.currScene();

    if (!scene.anim || !scene.tex) {
      return;
    }


    if (dimBackground) {
      SDL_SetRenderDrawBlendMode(sdlState.renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(sdlState.renderer, 0, 0, 0, 80); // 140/255 alpha
      SDL_FRect cover{0, 0, (float)sdlState.logW, (float)sdlState.logH};
      SDL_RenderFillRect(sdlState.renderer, &cover);
    } else {
      clearRenderer(sdlState);
    }

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

    // renderPresent(sdlState);

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
    bool anyHovered = false;
    auto place = [&](const char* id, auto onClick) {
        ImGui::SetCursorScreenPos(pos);
        if (ImGui::Button(id, ImVec2(btnW, btnH))) onClick();
        anyHovered |= ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly);
        pos.y += btnH + spacing;
    };
    if (anyHovered) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

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
    if (anyHovered) {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    act.blockMainGameDraw = true;
    // animated backdrop: stepped in renderView before this call
    if (cutsceneMgr.scenes && !cutsceneMgr.scenes->empty()) {
      draw(sdlState, snaps.deltaTime, false);
    } else {
      ImGui::Render(); // must force render to close out imgui cycle.
    }

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

        // act.nextView = GameView::Playing;
        if (snaps.cutscene != nullptr && snaps.cutSceneID >= 0) {
          std::cout << "start cutscene" << std::endl;
          cutsceneMgr.start(snaps.cutSceneID, snaps.cutscene);
          act.nextView = GameView::CutScene;
          act.blockMainGameDraw = true;
          ImGui::Render(); // must force render to close out imgui cycle.
        } else {
          std::cout << "gameviw playing" << std::endl;
          act.nextView = GameView::Playing;
        }

      } else {
        act.blockMainGameDraw = true; // block updating gameState while next level is loading
        ImGui::Render(); // must force render to close out imgui cycle.
      }
      return act;
  }

  UIActions UI_Manager::drawPausedMenu(const UISnapshots& snaps, ImGuiWindowFlags flags) {
      UIActions act;
      act.blockGameplayUpdates = true;
      act.drawSceneOverlay = true;
      act.dimBackground = true;

      if (snaps.cutSceneID != cutsceneMgr.cutSceneID && snaps.cutscene) {
        cutsceneMgr.start(snaps.cutSceneID, snaps.cutscene);
      }

      auto scn = cutsceneMgr.currScene();

      // 1) Reference = the PNG’s pixel size
      const float refW = scn.frameW;   // texture width
      const float refH = scn.frameH;  // texture height

      // 2) Measure the button stack in the PNG (in pixels of the art)
      const float btnOriginX = 265.0f; // left edge of the first green button in the art
      const float btnOriginY = 40.0f; // top edge of the first button in the art
      const float btnW_ref   = 114.0f;
      const float btnH_ref   = 20.0f;
      const float btnGap_ref = 12.0f;  // vertical gap between buttons

      // 3) Scale & offset to current window (letterboxed)
      int outW, outH; SDL_GetRenderOutputSize(sdlState.renderer, &outW, &outH);
      float scale  = std::min(outW / refW, outH / refH);

      float offX   = (outW - refW * scale) * 0.5f;
      float offY   = (outH - refH * scale) * 0.5f;

      // 4) Place buttons in that space
      ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0,0,0,0));
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.15f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1,1,1,0.25f));
      ImGui::Begin("##pause_hitboxes", nullptr,
          ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|
          ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
          ImGuiWindowFlags_NoScrollbar);

      ImVec2 pos(offX + btnOriginX * scale, offY + btnOriginY * scale);
      bool anyHovered = false;
      auto place = [&](const char* id, auto onClick) {
        ImGui::SetCursorScreenPos(pos);
        if (ImGui::Button(id, ImVec2(btnW_ref * scale, btnH_ref * scale))) onClick();
        anyHovered |= ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly);
        pos.y += (btnH_ref + btnGap_ref) * scale;
      };

      place("##resume", [&]{ act.nextView = GameView::Playing; });
      place("##restart", [&]{
        act.restartLevel = true;
        act.stopGameOverSoundTrack = true;
        act.nextView = GameView::Playing;
      });
      place("##settings", [&]{ act.nextView = GameView::MultiPlayerOptionsMenu;  });
      place("##levels", [&]{  });
      place("##inventory", [&]{  });
      place("##equipment",  [&]{ });
      place("##shop",   [&]{ });
      place("##craft",   [&]{ });
      place("##quit",   [&]{ act.nextView = UIManager::GameView::MainMenu; act.stopBackgroundTrack = true; });
      if (anyHovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      }
      ImGui::PopStyleVar(2);
      ImGui::PopStyleColor(4);
      ImGui::End();


      if (snaps.togglePauseGameplay) {
         act.nextView = GameView::Playing;
      }

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
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
      if (ImGui::Button("Pause Game", defaultButtonSize) || snaps.togglePauseGameplay) {
        act.nextView = UIManager::GameView::PauseMenu;
      }
      if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

      ImGui::PopItemFlag();
      ImGui::PopStyleVar(2);
      ImGui::End();

      drawPlayerHealthbar("HP", snaps.playerHP, IM_COL32(0, 200, 0, 255), flags);
      drawPlayerHealthbar("Mana", snaps.playerMana, IM_COL32(186, 154, 255, 255), flags);

      return act;
  }


  void UI_Manager::drawPlayerHealthbar(const std::string& name, const int value, ImU32 color, ImGuiWindowFlags flags) {
      ImGui::SetNextWindowPos(ImVec2(10, 10));
      ImGui::Begin("HUD", nullptr, ImGuiWindowFlags_NoTitleBar |
                                  ImGuiWindowFlags_NoBackground |
                                  ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove);
      float hpFrac = static_cast<float>(value) / 100.0; // 0..1
      ImGui::TextUnformatted(name.c_str());
      ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color); // green
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
        case GameView::InventoryMenu: return drawPausedMenu(snaps, flags);
        case GameView::PauseMenu: return drawPausedMenu(snaps, flags); // same as inventory menu because pauses game
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
            act.blockMainGameDraw = true;
            draw(sdlState, snaps.deltaTime, false);
            return act;
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

  void CutSceneManager::update(bool playNextScene, float deltaTime, const UISnapshots& snaps) {
    if (!scenes || sceneIndex >= scenes->size()) return;

    // if (doneWithScene) return;

    const Scene& scene = currScene();
    if (scene.anim) {
      scene.anim->step(deltaTime);
      // std::cout << "step scene delta" << std::endl;
    };

    if (isCurrentSceneComplete()) {
      // std::cout << "done scene" << std::endl;

      // still printing this even after the scene is done?
      // how does it default loop forever?

      if (!scene.loopScene) { // set holdLastFrame on Animation.
        // sceneIndex++; only advance to next scene on user input.
        doneWithScene = true;
        if (sceneIndex < scenes->size()) {
          std::cout << "increment scene index" << std::endl;
          // sceneIndex++;
          // doneWithCutscene = true;
          // if (scenes->at(sceneIndex).anim) scenes->at(sceneIndex).anim->reset();
        }
      }
      // indicate that the scene is complete and stop stepping...will freeze on the last frame!
    }

    // is currentAnimation finished?
    // is cutSceneFinished?
    // do we want to advance automatically with isFinished?
    // do we need to let the final frame play out first
    if (playNextScene) { // || isCurrentSceneComplete()
      std::cout << "play next scene" << std::endl;
      if (scene.anim) scene.anim->reset();
      if (sceneIndex < scenes->size()) {
        sceneIndex++;
        // if (scenes->at(sceneIndex).anim) scenes->at(sceneIndex).anim->reset();
      }
    }

    // scenes[index].data()->anim->step(deltaTime);

    // run animation delta on current animation index
    // if playNextScene increment index
  };


  bool CutSceneManager::isCutsceneComplete(){
    if (!scenes || scenes->empty()) {
      return true;
    }
    // use a bool to indicate whole cutscene is done
    return sceneIndex >= scenes->size();
  };

  bool CutSceneManager::isCurrentSceneComplete(){
    if (!scenes || scenes->empty() || sceneIndex >= scenes->size()) return true;
    const auto &scene = scenes->at(sceneIndex);
    return scene.anim ? scene.anim->isDone() : true;
  };


}
