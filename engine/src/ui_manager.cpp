#include "engine/ui_manager.h"
#include "engine/engine.h"
#include "imgui.h"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui_impl_sdl3.h"

#include <iostream>

namespace UIManager {

  void UI_Manager::beginFrame() {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
  }

  void UI_Manager::endFrame(const game_engine::SDLState& sdlState) {
    renderPresent(sdlState);
  }

  bool UI_Manager::button(const char* label, ImVec2 size) {
    return ImGui::Button(label, size);
  }

  bool UI_Manager::sliderFloat(const char* label, float* v, float v_min, float v_max) {
    return ImGui::SliderFloat(label, v, v_min, v_max);
  }

  void UI_Manager::text(const char* text) {
    ImGui::TextUnformatted(text);
  }


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

  void UI_Manager::draw(const game_engine::SDLState& sdlState, float deltaTime, bool dimBackground, bool drawDialogue, int visible) {

    const Cutscene& scene = cutscenePlr.currScene();

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
    if (drawDialogue && !scene.dialogue.empty()) {
      // 2) build a text surface with SDL_ttf
      const auto text = scene.dialogue.at(cutscenePlr.currDialogueIdx);
      std::string shown = text.substr(0, visible);
      // std::cout << "visible chars: " << shown << std::endl;
      SDL_Color fg{0,0,0,0};
      // SDL_Surface* surf = TTF_RenderText_Blended(&font, text.c_str(), text.length(), fg);   // blended = alpha
      TTF_SetFontHinting(&font, TTF_HINTING_MONO);
      SDL_Surface* surf = TTF_RenderText_Solid(&font, shown.c_str(), shown.length(), fg);   // blended = alpha

      if (surf) {
          // 3) turn it into a texture so the renderer can draw it
          SDL_Texture* textTex = SDL_CreateTextureFromSurface(sdlState.renderer, surf);
          // SDL_DestroySurface(surf); //

          if (textTex) {
              SDL_SetTextureScaleMode(textTex, SDL_SCALEMODE_NEAREST); // optional, keeps pixel fonts crisp
              SDL_FRect textDst{
                  dst.x + 60.0f,                  // position on top of your panel
                  dst.y + dst.h - (float)surf->h - 60.0f,
                  (float)surf->w, (float)surf->h
              };
              SDL_RenderTexture(sdlState.renderer, textTex, nullptr, &textDst);
              // SDL_DestroyTexture(textTex);
          }
      }
    }

  }



  float UI_Manager::drawCustomSlider(const std::string& label, float currVal, float v_min, float v_max) {

    float newVal = currVal;

    ImGui::PushID(label.c_str());
    ImGui::Text("%s", label.c_str());

    ImVec2 p = ImGui::GetCursorScreenPos();
    float width = 100.0f;
    float height = 15.0f;

    // ImU32 color_bg = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
    // ImU32 color_end = ImGui::GetColorU32(ImVec4(0.0f, 0.5f, 1.0f, 1.0f));
    // ImU32 color_handle = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    ImGui::InvisibleButton(label.c_str(), ImVec2(width, height));

    if (ImGui::IsItemActive()) {
        float t = (ImGui::GetMousePos().x - p.x) / width;
        t = std::clamp(t, 0.0f, 1.0f);
        newVal = v_min + t * (v_max - v_min);
    }

    float fillWidth = (newVal - v_min) / (v_max - v_min) * width;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + width, p.y + height), IM_COL32(50,50,50,255), height * 0.3f);
    dl->AddRectFilled(p, ImVec2(p.x + fillWidth, p.y + height), IM_COL32(0,128,255,255), height * 0.3f);
    dl->AddCircleFilled(ImVec2(p.x + fillWidth, p.y + height * 0.5f), height * 0.4f, IM_COL32(255,255,255,255));


    ImGui::PopID();

    return newVal;

  }



  // Local helper for the main menu (not a member).
  UIActions UI_Manager::drawMainMenu(const UISnapshots& snaps, ImGuiWindowFlags flags, const game_engine::SDLState& sdlState) {

    UIActions act;

    ImGui::Begin("##menu_hitboxes", nullptr, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|
                                    ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
                                    ImGuiWindowFlags_NoScrollbar);
    // draw audio slider
    act.newVolume = drawCustomSlider("volume", snaps.currVolume, 0.0f, 1.0f);
    if (act.newVolume != snaps.currVolume) {
      act.adjustVolume = true;
    }

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
    float spacing = 12.0f * scaleSize;

    ImGui::SetNextWindowPos(ImVec2(0,0));
    ImGui::SetNextWindowSize(ImVec2((float)outW, (float)outH));


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

    act.stopBackgroundTrack = true;
    place("##single", [&]{
      // act.nextView = GameView::CutScene;
      act.nextView = GameView::CharacterSelect;
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
    if (cutscenePlr.scenes && !cutscenePlr.scenes->empty()) {
      draw(sdlState, snaps.deltaTime, false, false, 0);
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
          cutscenePlr.start(snaps.cutSceneID, snaps.cutscene);
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

      if (snaps.cutSceneID != cutscenePlr.cutSceneID && snaps.cutscene) {
        cutscenePlr.start(snaps.cutSceneID, snaps.cutscene);
      }

      auto scn = cutscenePlr.currScene();

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

    UIActions UI_Manager::drawCharacterSelectScreen(const UISnapshots& snaps, ImGuiWindowFlags flags) {
      UIActions act;
      act.blockGameplayUpdates = true;
      act.drawSceneOverlay = true;
      act.dimBackground = true;

      // cutscene draws the textures
      if (snaps.cutSceneID != cutscenePlr.cutSceneID && snaps.cutscene) {
        cutscenePlr.start(snaps.cutSceneID, snaps.cutscene);
      }

      cutscenePlr.update(snaps.advanceToNextScene, snaps.deltaTime, snaps);

      auto scn = cutscenePlr.currScene();

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
      ImGui::Begin("##character_select", nullptr,
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

      place("##marie", [&]{
        // act.restartLevel = true;
        // act.stopGameOverSoundTrack = true;
        act.nextView = GameView::Playing; // TODO hook this up to the next levels cutscene

      });
      place("##bonkfather", [&]{
        // act.restartLevel = true;
        // act.stopGameOverSoundTrack = true;
        act.nextView = GameView::Playing;
      });

      if (anyHovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      }
      ImGui::PopStyleVar(2);
      ImGui::PopStyleColor(4);
      ImGui::End();


      act.blockMainGameDraw = true;
      // animated backdrop: stepped in renderView before this call
      if (cutscenePlr.scenes && !cutscenePlr.scenes->empty()) {
        draw(sdlState, snaps.deltaTime, false, false, 0);
      } else {
        ImGui::Render(); // must force render to close out imgui cycle.
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
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
      ImGui::ProgressBar(hpFrac, ImVec2(150, 24));
      ImGui::PopStyleColor();
      ImGui::PopStyleVar();
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
          if (cutscenePlr.cutSceneID != snaps.cutSceneID && snaps.cutscene) {
            cutscenePlr.start(snaps.cutSceneID, snaps.cutscene);
          }
          cutscenePlr.update(snaps.advanceToNextScene, snaps.deltaTime, snaps);
          return drawMainMenu(snaps, flags, sdlState);
        }
        case GameView::CharacterSelect: return drawCharacterSelectScreen(snaps, flags);
        case GameView::LevelLoading: return drawLoading(snaps, flags);
        case GameView::GameOver: return drawGameOver(snaps.loading, flags);
        case GameView::Playing: return drawGameplay(snaps, flags);
        case GameView::InventoryMenu: return drawPausedMenu(snaps, flags);
        case GameView::PauseMenu: return drawPausedMenu(snaps, flags); // same as inventory menu because pauses game
        case GameView::MultiPlayerOptionsMenu: return drawMultiplayerOptionsMenu(snaps, flags);
        case GameView::CutScene:
        {
          bool playNextScene = snaps.advanceToNextScene; // user hit return/enter, force advance to next scene

          if (cutscenePlr.cutSceneID != snaps.cutSceneID && snaps.cutscene) {
            std::cout << "starting cutscene" << std::endl;
            cutscenePlr.start(snaps.cutSceneID, snaps.cutscene);
          }

          UIActions act;
          cutscenePlr.update(snaps.advanceToNextScene, snaps.deltaTime, snaps);
          if (cutscenePlr.scenes && !cutscenePlr.scenes->empty() && !cutscenePlr.isCutsceneComplete()) {
            act.blockMainGameDraw = true;
            draw(sdlState, snaps.deltaTime, false, true, cutscenePlr.visibleChars);
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


  void CutscenePlayer::start(int sceneID, const std::vector<Cutscene>* newScenes) {
      cutSceneID = sceneID;
      scenes = newScenes;
      sceneIndex = 0;
      doneWithCurrScene = false;
  }

  const Cutscene& CutscenePlayer::currScene() {
    static const Cutscene kDummy{nullptr, nullptr, {}, 1, 0, 0.0f, 0.0f, 0.0f, 1.0f};
    if (!scenes || scenes->empty() || sceneIndex >= scenes->size()) {
      return kDummy;
    }
    return scenes->at(sceneIndex);
  }

  void CutscenePlayer::update(bool usrWantsNextScene, float deltaTime, const UISnapshots& snaps) {
    if (!scenes || sceneIndex >= scenes->size()) return;

    bool finalDialogueComplete = false;
    int lenCurrText = 0;

    const Cutscene& scene = currScene();
    if (scene.anim) {
      scene.anim->step(deltaTime);

      if (!scene.dialogue.empty()) {
        elapsed += deltaTime;

        if (showNextDialogue && usrWantsNextScene) { // endOfCurrDialogue
          showNextDialogue = false; // reset
          if (currDialogueIdx < scene.dialogue.size() - 1) {
            showNextDialogue = false;
            elapsed = 0;
            currDialogueIdx += 1;
          } else {
            // next dialogue
            elapsed = 0;
            currDialogueIdx = 0;
            finalDialogueComplete = true;
            showNextDialogue = false;
          }
        }

        if (!showNextDialogue) {
          int visible = (int)std::floor(elapsed * charsPerSecond);
          auto text = scene.dialogue.at(currDialogueIdx);
          lenCurrText = text.length();
          visibleChars = std::clamp(visible, 0, (int)text.size());

          if (visibleChars >= text.length()) {
            // signal done with currDialogueIndex so that loop we can set the new dialogueIndex
            showNextDialogue = true;
          }
        }

      }

    };

    if (usrWantsNextScene) {
      if (finalDialogueComplete) {
        if (scene.anim) scene.anim->reset();
        if (sceneIndex < scenes->size()) {
          std::cout << "play next scene" << std::endl;
          sceneIndex++;
        }
      } else if (visibleChars != 0) {
        visibleChars = lenCurrText;
        showNextDialogue = true;
      }
    }

  };


  bool CutscenePlayer::isCutsceneComplete(){
    if (!scenes || scenes->empty()) {
      return true;
    }
    // use a bool to indicate whole cutscene is done
    return sceneIndex >= scenes->size();
  };

  bool CutscenePlayer::isCurrentSceneComplete(){
    if (!scenes || scenes->empty() || sceneIndex >= scenes->size()) return true;
    const auto &scene = scenes->at(sceneIndex);
    return scene.anim ? scene.anim->isDone() : true;
  };


}
