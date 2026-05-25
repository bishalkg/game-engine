#include "engine/ui_manager.h"
#include "engine/engine.h"
#include "imgui.h"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui_impl_sdl3.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>

namespace UIManager {

  namespace {
    constexpr float kHudWidgetScale = 1.0f;
    constexpr float kHudWidgetFrameSize = 32.0f;
    constexpr float kHudWidgetDrawSize = kHudWidgetFrameSize * kHudWidgetScale;
    constexpr float kHudWidgetGap = 0.0f;
    constexpr float kHudWidgetRightMargin = 4.0f;
    constexpr float kHudWidgetBottomMargin = 4.0f;
    constexpr float kHudTextBoxTopOffset = 20.0f;
    constexpr float kHudTextBoxHeight = 1.0f;
    constexpr float kNumbersSlotWidth = 32.0f;
    constexpr float kNumbersRowHeight = 16.0f;
    constexpr float kNumbersGlyphOffsetX = 11.0f;
    constexpr float kNumbersGlyphWidth = 11.0f;
    constexpr float kNumbersGlyphHeight = 16.0f;
    constexpr int kNumbersAtlasColumns = 10;
    constexpr float kShopWalletCountY = 296.0f;
    constexpr float kShopCoinCountX = 256.0f;
    constexpr float kShopGemCountX = 386.0f;
    constexpr float kInventoryWalletCountY = 296.0f;
    constexpr float kInventoryCoinCountX = 256.0f;
    constexpr float kInventoryGemCountX = 386.0f;

    struct InventoryCountAnchor {
      float x;
      float y;
    };

    constexpr InventoryCountAnchor kInventoryCountAnchors[] = {
      {219.0f, 108.0f},
      {268.0f, 108.0f},
      {318.0f, 108.0f},
      {366.0f, 108.0f},
    };

    void drawCountGlyph(
      SDL_Renderer* renderer,
      SDL_Texture* numbersHudTex,
      uint32_t count,
      float dstX,
      float dstY) {
      if (!numbersHudTex || count == 0) {
        return;
      }

      const uint32_t clampedCount = std::min<uint32_t>(count, 99);
      const uint32_t slotRow = clampedCount < 10 ? 0 : clampedCount / 10;
      const uint32_t slotCol = clampedCount < 10 ? (clampedCount - 1) : (clampedCount % 10);

      SDL_FRect src{
        static_cast<float>(slotCol) * kNumbersSlotWidth + kNumbersGlyphOffsetX,
        static_cast<float>(slotRow) * kNumbersRowHeight,
        kNumbersGlyphWidth,
        kNumbersGlyphHeight,
      };
      SDL_FRect textDst{
        dstX,
        dstY,
        kNumbersGlyphWidth,
        kNumbersGlyphHeight,
      };
      SDL_RenderTexture(renderer, numbersHudTex, &src, &textDst);
    }

    void drawHudCountGlyph(
      SDL_Renderer* renderer,
      SDL_Texture* numbersHudTex,
      uint32_t count,
      float widgetX,
      float widgetY) {
      if (!numbersHudTex || count == 0) {
        return;
      }

      drawCountGlyph(
        renderer,
        numbersHudTex,
        count,
        widgetX + std::floor((kHudWidgetDrawSize - kNumbersGlyphWidth) * 0.5f),
        widgetY + kHudTextBoxTopOffset +
          std::floor((kHudTextBoxHeight - kNumbersGlyphHeight) * 0.5f));
    }
  } // namespace

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
    SDL_SetRenderLogicalPresentation(
      sdlState.renderer,
      sdlState.logW,
      sdlState.logH,
      SDL_LOGICAL_PRESENTATION_LETTERBOX);
    drawGameplayHudCounts(sdlState);

    SDL_SetRenderLogicalPresentation(sdlState.renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);

    // Always use ImGui software cursor and let backend hide the OS cursor.
    ImGuiIO& io = ImGui::GetIO();
    io.MouseDrawCursor = true;
    ImGui::SetMouseCursor(wantsHandCursor ? ImGuiMouseCursor_Hand : ImGuiMouseCursor_Arrow);

    // ui.render()
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdlState.renderer);

    // Cursor-state probe for debugging hover/cursor behavior.
    // constexpr bool kShowCursorDebug = false;
    if (debugMode) {
      char cursorDebug[128];
      std::snprintf(
        cursorDebug,
        sizeof(cursorDebug),
        "wantsHand=%d mouseDraw=%d imguiCursor=%d osVisible=%d noCurChange=%d",
        wantsHandCursor ? 1 : 0,
        io.MouseDrawCursor ? 1 : 0,
        static_cast<int>(ImGui::GetMouseCursor()),
        SDL_CursorVisible() ? 1 : 0,
        (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) ? 1 : 0);
      SDL_SetRenderDrawColor(sdlState.renderer, 255, 255, 0, 255);
      SDL_RenderDebugText(sdlState.renderer, 8.0f, 8.0f, cursorDebug);
    }

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
    // scene.anim->step(deltaTime); // TODO this would step twice currently

      // select frame from sprite sheet
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

    if (cutscenePlr.cutSceneID == -6 && cachedGameplayHud.numbersHudTex) {
      drawCountGlyph(
        sdlState.renderer,
        cachedGameplayHud.numbersHudTex,
        cachedGameplayHud.playerCoins,
        dst.x + kShopCoinCountX,
        dst.y + kShopWalletCountY);
      drawCountGlyph(
        sdlState.renderer,
        cachedGameplayHud.numbersHudTex,
        cachedGameplayHud.playerGems,
        dst.x + kShopGemCountX,
        dst.y + kShopWalletCountY);
    } else if (cutscenePlr.cutSceneID == -7 && cachedGameplayHud.numbersHudTex) {
      drawCountGlyph(
        sdlState.renderer,
        cachedGameplayHud.numbersHudTex,
        cachedGameplayHud.playerCoins,
        dst.x + kInventoryCoinCountX,
        dst.y + kInventoryWalletCountY);
      drawCountGlyph(
        sdlState.renderer,
        cachedGameplayHud.numbersHudTex,
        cachedGameplayHud.playerGems,
        dst.x + kInventoryGemCountX,
        dst.y + kInventoryWalletCountY);

      const uint32_t itemCounts[] = {
        cachedGameplayHud.playerHealthPotions,
        cachedGameplayHud.playerManaPotions,
        cachedGameplayHud.playerAttackUps,
        cachedGameplayHud.playerDefenceUps,
      };
      for (size_t idx = 0; idx < std::size(itemCounts); ++idx) {
        drawCountGlyph(
          sdlState.renderer,
          cachedGameplayHud.numbersHudTex,
          itemCounts[idx],
          dst.x + kInventoryCountAnchors[idx].x,
          dst.y + kInventoryCountAnchors[idx].y);
      }
    }

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

  void UI_Manager::drawGameplayHudCounts(const game_engine::SDLState& sdlState) {
    if (!gameplayHudActive) {
      return;
    }
    if (!cachedGameplayHud.coinCountHudTex || !cachedGameplayHud.gemCountHudTex ||
        !cachedGameplayHud.numbersHudTex ||
        !cachedGameplayHud.coinCountHudAnim || !cachedGameplayHud.gemCountHudAnim) {
      return;
    }

    auto drawWidget = [&](SDL_Texture* texture, Animation& anim, uint32_t count, float x, float y) {
      const int frameCount = anim.getFrameCount();
      if (frameCount <= 0) {
        return;
      }

      const int frame = anim.currentFrame() % frameCount;
      SDL_FRect src{
        static_cast<float>(frame) * kHudWidgetFrameSize,
        0.0f,
        kHudWidgetFrameSize,
        kHudWidgetFrameSize,
      };
      SDL_FRect dst{x, y, kHudWidgetDrawSize, kHudWidgetDrawSize};
      SDL_RenderTexture(sdlState.renderer, texture, &src, &dst);
      drawHudCountGlyph(sdlState.renderer, cachedGameplayHud.numbersHudTex, count, x, y);
    };

    const float groupWidth = (kHudWidgetDrawSize * 2.0f) + kHudWidgetGap;
    const float startX =
      static_cast<float>(sdlState.logW) - kHudWidgetRightMargin - groupWidth;
    const float startY =
      static_cast<float>(sdlState.logH) - kHudWidgetBottomMargin - kHudWidgetDrawSize;

    drawWidget(
      cachedGameplayHud.coinCountHudTex,
      *cachedGameplayHud.coinCountHudAnim,
      cachedGameplayHud.playerCoins,
      startX,
      startY);
    drawWidget(
      cachedGameplayHud.gemCountHudTex,
      *cachedGameplayHud.gemCountHudAnim,
      cachedGameplayHud.playerGems,
      startX + kHudWidgetDrawSize + kHudWidgetGap,
      startY);
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

    if (cutscenePlr.cutSceneID != snaps.cutSceneID && snaps.cutscene) {
      cutscenePlr.start(snaps.cutSceneID, snaps.cutscene);
    }
    cutscenePlr.update(snaps.advanceToNextScene, snaps.deltaTime, snaps);

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
        anyHovered |= ImGui::IsItemHovered();
        pos.y += btnH + spacing;
    };
    act.stopBackgroundTrack = true;
    place("##single", [&]{
      act.startSinglePlayer = true;
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
      wantsHandCursor = true;
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
      ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
      ImGui::SetNextWindowSize(ImVec2(static_cast<float>(outW), static_cast<float>(outH)));
      ImGui::Begin("##pause_hitboxes", nullptr,
          ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|
          ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
          ImGuiWindowFlags_NoScrollbar);

      ImVec2 pos(offX + btnOriginX * scale, offY + btnOriginY * scale);
      bool anyHovered = false;
      auto place = [&](const char* id, auto onClick) {
        ImGui::SetCursorScreenPos(pos);
        if (ImGui::Button(id, ImVec2(btnW_ref * scale, btnH_ref * scale))) onClick();
        anyHovered |= ImGui::IsItemHovered();
        pos.y += (btnH_ref + btnGap_ref) * scale;
      };

      place("##resume", [&]{ act.nextView = GameView::Playing; });
      place("##restart", [&]{
        act.restartLevel = true;
        act.stopGameOverSoundTrack = true;
        act.nextView = GameView::Playing;
      });
      place("##settings", [&]{ act.nextView = GameView::MultiPlayerOptionsMenu;  });
      place("##levels", [&]{ act.nextView = GameView::LevelSelection; });
      place("##inventory", [&]{ act.nextView = GameView::InventoryMenu; });
      place("##equipment",  [&]{ });
      place("##shop",   [&]{ act.nextView = GameView::ShopMenu; });
      place("##craft",   [&]{ });
      place("##quit",   [&]{ act.nextView = UIManager::GameView::MainMenu; act.stopBackgroundTrack = true; });
      ImGui::PopStyleVar(2);
      ImGui::PopStyleColor(4);
      ImGui::End();
      if (anyHovered) {
        wantsHandCursor = true;
      }

      drawStatusBars(snaps);

      if (snaps.togglePauseGameplay) {
         act.nextView = GameView::Playing;
      }

      return act;
  }

  UIActions UI_Manager::drawShopMenu(const UISnapshots& snaps, ImGuiWindowFlags flags) {
      UIActions act;
      act.blockGameplayUpdates = true;
      act.drawSceneOverlay = true;
      act.dimBackground = true;

      if (snaps.cutSceneID != cutscenePlr.cutSceneID && snaps.cutscene) {
        cutscenePlr.start(snaps.cutSceneID, snaps.cutscene);
      }

      auto scn = cutscenePlr.currScene();

      const float refW = scn.frameW;
      const float refH = scn.frameH;

      int outW, outH;
      SDL_GetRenderOutputSize(sdlState.renderer, &outW, &outH);
      float scale = std::min(outW / refW, outH / refH);
      float offX = (outW - refW * scale) * 0.5f;
      float offY = (outH - refH * scale) * 0.5f;

      ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0,0,0,0));
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.15f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1,1,1,0.25f));
      ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
      ImGui::SetNextWindowSize(ImVec2(static_cast<float>(outW), static_cast<float>(outH)));
      ImGui::Begin("##shop_hitboxes", nullptr,
          ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|
          ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
          ImGuiWindowFlags_NoScrollbar);

      bool anyHovered = false;
      auto place = [&](const char* id, float x, float y, float w, float h, auto onClick) {
        ImGui::SetCursorScreenPos(ImVec2(offX + x * scale, offY + y * scale));
        if (ImGui::Button(id, ImVec2(w * scale, h * scale))) onClick();
        anyHovered |= ImGui::IsItemHovered();
      };

      place("##close_shop", 408.0f, 35.0f, 18.0f, 18.0f, [&]{
        act.nextView = GameView::PauseMenu;
      });
      place("##shop_hp", 223.0f, 130.0f, 50.0f, 28.0f, [&]{
        act.shopPurchase = ShopPurchase::HealthPotion;
      });
      place("##shop_mana", 293.0f, 130.0f, 50.0f, 28.0f, [&]{
        act.shopPurchase = ShopPurchase::ManaPotion;
      });
      place("##shop_attack", 223.0f, 247.0f, 50.0f, 28.0f, [&]{
        act.shopPurchase = ShopPurchase::AttackUp;
      });
      place("##shop_defence", 293.0f, 247.0f, 50.0f, 28.0f, [&]{
        act.shopPurchase = ShopPurchase::DefenceUp;
      });

      ImGui::PopStyleVar(2);
      ImGui::PopStyleColor(4);
      ImGui::End();
      if (anyHovered) {
        wantsHandCursor = true;
      }

      drawStatusBars(snaps);

      return act;
  }

  UIActions UI_Manager::drawInventoryMenu(const UISnapshots& snaps, ImGuiWindowFlags flags) {
      UIActions act;
      act.blockGameplayUpdates = true;
      act.drawSceneOverlay = true;
      act.dimBackground = true;

      if (snaps.cutSceneID != cutscenePlr.cutSceneID && snaps.cutscene) {
        cutscenePlr.start(snaps.cutSceneID, snaps.cutscene);
      }

      auto scn = cutscenePlr.currScene();
      const float refW = scn.frameW;
      const float refH = scn.frameH;

      int outW, outH;
      SDL_GetRenderOutputSize(sdlState.renderer, &outW, &outH);
      float scale = std::min(outW / refW, outH / refH);
      float offX = (outW - refW * scale) * 0.5f;
      float offY = (outH - refH * scale) * 0.5f;

      ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0,0,0,0));
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.15f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1,1,1,0.25f));
      ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
      ImGui::SetNextWindowSize(ImVec2(static_cast<float>(outW), static_cast<float>(outH)));
      ImGui::Begin("##inventory_hitboxes", nullptr,
          ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|
          ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
          ImGuiWindowFlags_NoScrollbar);

      bool anyHovered = false;
      auto place = [&](const char* id, float x, float y, float w, float h, auto onClick) {
        ImGui::SetCursorScreenPos(ImVec2(offX + x * scale, offY + y * scale));
        if (ImGui::Button(id, ImVec2(w * scale, h * scale))) onClick();
        anyHovered |= ImGui::IsItemHovered();
      };

      place("##close_inventory", 449.0f, 40.0f, 18.0f, 18.0f, [&]{
        act.nextView = GameView::PauseMenu;
      });
      place("##inventory_hp", 219.0f, 82.0f, 42.0f, 42.0f, [&]{
        act.inventoryUse = InventoryUse::HealthPotion;
      });
      place("##inventory_mana", 269.0f, 82.0f, 42.0f, 42.0f, [&]{
        act.inventoryUse = InventoryUse::ManaPotion;
      });

      ImGui::PopStyleVar(2);
      ImGui::PopStyleColor(4);
      ImGui::End();
      if (anyHovered) {
        wantsHandCursor = true;
      }

      drawStatusBars(snaps);

      return act;
  }

  UIActions UI_Manager::drawLevelSelectScreen(const UISnapshots& snaps, ImGuiWindowFlags flags) {
    UIActions act;
    act.blockGameplayUpdates = true;
    act.drawSceneOverlay = true;
    act.dimBackground = true;

    // draws the texture
    if (snaps.cutSceneID != cutscenePlr.cutSceneID && snaps.cutscene) {
      cutscenePlr.start(snaps.cutSceneID, snaps.cutscene);
    }

    // set the animation index according to the levels user has unlocked
    // because progression is sequential, just pass total number of levels completed.
    cutscenePlr.setAnimIndex(static_cast<int>(snaps.levelProgressionIdx));

    // cutscenePlr.update(snaps.advanceToNextScene, snaps.deltaTime, snaps); // nothing to animate in level

    auto scn = cutscenePlr.currScene();
    // we we will need the players current unlocked levels from the save file
    // then we create badges for each unlocked level, only these badges are clickable.

    // 1) Reference = the PNG’s pixel size
    const float refW = scn.frameW;   // texture width
    const float refH = scn.frameH;  // texture height

    // 2) Measure the button stack in the PNG (in pixels of the art)
    const float btnOriginX = 92.0f; // left edge of the first green button in the art
    const float btnOriginY = 130.0f; // top edge of the first button in the art
    const float btnW_ref   = 65.0f;
    const float btnH_ref   = 108.0f;
    const float btnGap_ref = 40.0f;  // default gap between buttons; mostly using custom for this

    // 3) Scale & offset to current window (letterboxed)
    int outW, outH;
    SDL_GetRenderOutputSize(sdlState.renderer, &outW, &outH);
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
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(outW), static_cast<float>(outH)));
    ImGui::Begin("##level_select", nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|
        ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoScrollbar);

    ImVec2 pos(offX + btnOriginX * scale, offY + btnOriginY * scale);
    bool anyHovered = false;
    auto place = [&](const char* id, float gap, auto onClick) {
      ImGui::SetCursorScreenPos(pos);
      if (ImGui::Button(id, ImVec2(btnW_ref * scale, btnH_ref * scale))) onClick();
      anyHovered |= ImGui::IsItemHovered();
      pos.x += (btnW_ref + gap) * scale;
    };

    auto setLevelIfAllowed = [&](LevelIndex nextLevelIdx) {
      if (nextLevelIdx <= snaps.levelProgressionIdx) {
        act.selectedLevel = nextLevelIdx;
        // act.nextView = GameView::Playing;
      }
    };

    // TODO can only clik on level if its unlocked
    place("##level1", 34, [&]{
      setLevelIfAllowed(LevelIndex::LEVEL_1);
      // act.nextView = GameView::Playing;
    });
    place("##level2", 34, [&]{
      setLevelIfAllowed(LevelIndex::LEVEL_2);
      // act.nextView = GameView::Playing;
    });
    place("##level3", 34, [&]{
      setLevelIfAllowed(LevelIndex::LEVEL_3);
      // act.nextView = GameView::Playing;
    });
    place("##level4", 36, [&]{
      setLevelIfAllowed(LevelIndex::LEVEL_4);
      // act.nextView = GameView::Playing;
    });
    place("##level5", btnGap_ref, [&]{
      setLevelIfAllowed(LevelIndex::LEVEL_5);
      // act.nextView = GameView::Playing;
    });

    // TODO DRY refactor, reuse placeQuit
    const float qbtnOriginX = 510.0f; // left edge of the first green button in the art
    const float qbtnOriginY = 42.0f; // top edge of the first button in the art
    const float qbtnW_ref   = 60.0f;
    const float qbtnH_ref   = 17.0f;
    ImVec2 posQuit(offX + qbtnOriginX * scale, offY + qbtnOriginY * scale);
    auto placeQuit = [&](const char* id, auto onClick) {
      ImGui::SetCursorScreenPos(posQuit);
      if (ImGui::Button(id, ImVec2(qbtnW_ref * scale, qbtnH_ref * scale))) onClick();
      anyHovered |= ImGui::IsItemHovered();
    };
    placeQuit("##quit", [&]{
      act.nextView = GameView::PauseMenu;
    });

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::End();
    if (anyHovered) {
      wantsHandCursor = true;
    }

    // TODO conditionally draw backdrop or not depending on if level selection is before game starts
    // or was from the pause menu
    bool levelSelectFromPauseMenu = true;
    if (levelSelectFromPauseMenu) {
      // for now level can only be accessed once in the game.

    } else {
      act.blockMainGameDraw = true;
      // animated backdrop: stepped in renderView before this call
      if (cutscenePlr.scenes && !cutscenePlr.scenes->empty()) {
        draw(sdlState, snaps.deltaTime, false, false, 0);
      } else {
        ImGui::Render(); // must force render to close out imgui cycle.
      }
    }

    return act;

  };

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
    const float btnOriginX = 130.0f; // left edge of the first green button in the art
    const float btnOriginY = 100.0f; // top edge of the first button in the art
    const float btnW_ref   = 140.0f;
    const float btnH_ref   = 150.0f;
    const float btnGap_ref = 100.0f;  // vertical gap between buttons

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
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(outW), static_cast<float>(outH)));
    ImGui::Begin("##character_select", nullptr,
        ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|
        ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoScrollbar);

    ImVec2 pos(offX + btnOriginX * scale, offY + btnOriginY * scale);
    bool anyHovered = false;
    auto place = [&](const char* id, auto onClick) {
      ImGui::SetCursorScreenPos(pos);
      if (ImGui::Button(id, ImVec2(btnW_ref * scale, btnH_ref * scale))) onClick();
      anyHovered |= ImGui::IsItemHovered();
      pos.x += (btnW_ref + btnGap_ref) * scale;
    };

    place("##marie", [&]{
      act.selectedPlayerSprite = SpriteType::Player_Marie;
    });
    place("##bonkfather", [&]{
      act.selectedPlayerSprite = SpriteType::Player_Bonkfather;
    });

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    ImGui::End();
    if (anyHovered) {
      wantsHandCursor = true;
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

  UIActions UI_Manager::drawMultiplayerOptionsMenu(const UISnapshots& snaps, ImGuiWindowFlags flags) {
      UIActions act;
      ImGui::Begin("MultiPlayer Menu", nullptr, flags);
      if (ImGui::Button("Host A Game", defaultButtonSize)) {
        act.startMultiPlayerHost = true;
        act.nextView = UIManager::GameView::CharacterSelect;
      }
      if (ImGui::Button("Join A Game", defaultButtonSize)) {
        act.startMultiPlayerClient = true;
        act.nextView = UIManager::GameView::MultiplayerBrowse;
      }
      if (ImGui::Button("Back to Menu", defaultButtonSize)) {
        act.nextView = UIManager::GameView::MainMenu;
      }
      ImGui::End();
      return act;
  }

  UIActions UI_Manager::drawMultiplayerBrowse(const UISnapshots& snaps, ImGuiWindowFlags flags) {
      UIActions act;
      act.blockMainGameDraw = true;
      act.blockGameplayUpdates = true;
      clearRenderer(sdlState);
      ImGui::Begin("Available Games", nullptr, flags);
      ImGui::TextUnformatted("Available LAN Games");
      if (!snaps.multiplayerStatus.empty()) {
        ImGui::TextWrapped("%s", snaps.multiplayerStatus.c_str());
      }
      ImGui::Separator();

      if (snaps.multiplayerSessions.empty()) {
        ImGui::TextUnformatted("No ready hosts found yet.");
      } else {
        for (size_t idx = 0; idx < snaps.multiplayerSessions.size(); ++idx) {
          const auto& session = snaps.multiplayerSessions[idx];
          const std::string buttonLabel =
            "Join " + session.hostName + "##session_" + std::to_string(idx);
          if (ImGui::Button(buttonLabel.c_str(), ImVec2(300, 0))) {
            act.selectedSessionIndex = idx;
            act.nextView = UIManager::GameView::CharacterSelect;
          }
          ImGui::Text(
            "%s  |  %s  |  %u player(s)",
            session.hostAddress.c_str(),
            session.levelName.c_str(),
            session.playerCount);
          ImGui::Spacing();
        }
      }

      if (ImGui::Button("Back", defaultButtonSize)) {
        act.nextView = UIManager::GameView::MultiPlayerOptionsMenu;
      }
      ImGui::End();
      return act;
  }

  UIActions UI_Manager::drawMultiplayerHostWaiting(const UISnapshots& snaps, ImGuiWindowFlags flags) {
      UIActions act;
      act.blockMainGameDraw = true;
      act.blockGameplayUpdates = true;
      clearRenderer(sdlState);
      ImGui::Begin("Hosting", nullptr, flags);
      ImGui::TextUnformatted("Starting LAN host...");
      if (!snaps.multiplayerStatus.empty()) {
        ImGui::TextWrapped("%s", snaps.multiplayerStatus.c_str());
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
      }
      ImGui::End();
      return act;
  }

  UIActions UI_Manager::drawGameplay(const UISnapshots& snaps, ImGuiWindowFlags flags) {
      UIActions act;
      gameplayHudActive = snaps.showGameplayHud;
      cachedGameplayHud = snaps.gameplayHud;

      if (cachedGameplayHud.coinCountHudAnim) {
        cachedGameplayHud.coinCountHudAnim->step(snaps.deltaTime);
      }
      if (cachedGameplayHud.gemCountHudAnim) {
        cachedGameplayHud.gemCountHudAnim->step(snaps.deltaTime);
      }

      ImGuiWindowFlags windowFlags = flags | ImGuiWindowFlags_NoBackground;
      ImGui::Begin("HUD", nullptr, windowFlags);
      ImGui::SetCursorPos(ImVec2(
        ImGui::GetWindowWidth() - defaultButtonSize.x - 8.0f,
        4.0f));
      // Optional: remove padding so the button hugs the corner
      ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
      if (ImGui::Button("Pause Game", defaultButtonSize) || snaps.togglePauseGameplay) {
        act.nextView = UIManager::GameView::PauseMenu;
      }
      if (ImGui::IsItemHovered()) {
        wantsHandCursor = true;
      }
      ImGui::PopItemFlag();
      ImGui::PopStyleVar(2);
      ImGui::End();

      drawStatusBars(snaps);

      return act;
  }

  void UI_Manager::drawStatusBars(const UISnapshots& snaps) {
      drawPlayerBar("HP", snaps.playerHP, snaps.maxPlayerHP, IM_COL32(0, 200, 0, 255), 10.0f, 10.0f, 150, 24, false);
      drawPlayerBar("Mana", snaps.playerMana, snaps.maxPlayerMana, IM_COL32(186, 154, 255, 255), 10.0f, 56.0f, 150, 24, false);
      if (snaps.playerUltimateUnlocked) {
        drawPlayerBar(
          "Ultimate",
          snaps.playerUltimate,
          snaps.maxUltimatePoints,
          IM_COL32(220, 40, 40, 255),
          10.0f,
          102.0f,
          150, 24,
          snaps.playerUltimateReady);
      }

      drawBossStatusBar(snaps);
  }

  void UI_Manager::drawBossStatusBar(const UISnapshots& snaps) {
      if (snaps.currBossHP <= 0 || snaps.maxBossHP <= 0) {
        return;
      }

      int outW = 0;
      int outH = 0;
      SDL_GetRenderOutputSize(sdlState.renderer, &outW, &outH);
      if (outW <= 0 || outH <= 0) {
        return;
      }

      constexpr float refW = 640.0f;
      constexpr float refH = 360.0f;
      const float scale = std::min(outW / refW, outH / refH);
      const float gameW = refW * scale;
      const float gameH = refH * scale;
      const float gameLeft = (outW - gameW) * 0.5f;
      const float gameTop = (outH - gameH) * 0.5f;

      const float barW = std::min(gameW * 0.6f, 460.0f * scale);
      const float barH = std::max(20.0f, 20.0f * scale);
      const float bottomMargin = 320.0f * scale;
      const float labelHeight = ImGui::GetTextLineHeightWithSpacing();
      const float windowPaddingY = 8.0f;
      const float estimatedWindowH = labelHeight + barH + windowPaddingY;

      const float x = gameLeft + (gameW - barW) * 0.5f;
      const float y = gameTop + gameH - bottomMargin - estimatedWindowH;

      drawPlayerBar(
        "Boss Health",
        snaps.currBossHP,
        snaps.maxBossHP,
        IM_COL32(220, 50, 50, 255),
        x,
        y,
        static_cast<int>(barW),
        static_cast<int>(barH),
        false);
  }


  void UI_Manager::drawPlayerBar(
    const std::string& name,
    int value,
    int maxValue,
    ImU32 color,
    float xOffset,
    float yOffset,
    int sizeX, int sizeY,
    bool highlightReady) {
      ImGui::SetNextWindowPos(ImVec2(xOffset, yOffset));
      const std::string windowName = "HUD##" + name;

      // ImGui::BeginGroup();
      ImGui::Begin(windowName.c_str(), nullptr, ImGuiWindowFlags_NoTitleBar |
                                            ImGuiWindowFlags_NoBackground |
                                            ImGuiWindowFlags_NoResize |
                                            ImGuiWindowFlags_NoMove |
                                            ImGuiWindowFlags_NoScrollbar |
                                          ImGuiWindowFlags_AlwaysAutoResize);

      float hpFrac = static_cast<float>(value) / static_cast<float>(maxValue); // 0..1
      ImGui::TextUnformatted(name.c_str());
      ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
      const bool flashHighlight =
        highlightReady && std::fmod(ImGui::GetTime(), 1.0) < 0.5;
      ImGui::PushStyleColor(
        ImGuiCol_Border,
        flashHighlight ? IM_COL32(255, 220, 180, 255) : IM_COL32(255, 255, 255, 80));

      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, flashHighlight ? 2.0f : 1.0f);
      ImGui::ProgressBar(hpFrac, ImVec2(sizeX, sizeY)); // 150 24

      if (flashHighlight) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        drawList->AddRect(
          min,
          max,
          IM_COL32(255, 120, 120, 255),
          8.0f,
          0,
          2.0f);
      }
      ImGui::PopStyleVar(2);
      ImGui::PopStyleColor(2);
      ImGui::End();
  }

  UIActions UI_Manager::getRenderViewActions(GameView view, const UISnapshots& snaps, ImGuiWindowFlags flags, const game_engine::SDLState& sdlState) {
      ImGuiIO& io = ImGui::GetIO();
      io.MouseDrawCursor = true;
      ImGui_ImplSDLRenderer3_NewFrame();
      ImGui_ImplSDL3_NewFrame();
      ImGui::NewFrame();
      wantsHandCursor = false;
      gameplayHudActive = false;
      cachedGameplayHud = snaps.gameplayHud;

      ImGui::SetNextWindowPos(ImVec2(0, 0));
      ImGui::SetNextWindowSize(io.DisplaySize);
      debugMode = snaps.debugMode;

      switch (view) {
        case GameView::MainMenu: return drawMainMenu(snaps, flags, sdlState);
        case GameView::CharacterSelect: return drawCharacterSelectScreen(snaps, flags);
        case GameView::LevelLoading: return drawLoading(snaps, flags);
        case GameView::GameOver: return drawGameOver(snaps.loading, flags);
        case GameView::Playing: return drawGameplay(snaps, flags);
        case GameView::InventoryMenu: return drawInventoryMenu(snaps, flags);
        case GameView::PauseMenu: return drawPausedMenu(snaps, flags); // same as inventory menu because pauses game
        case GameView::ShopMenu: return drawShopMenu(snaps, flags);
        case GameView::LevelSelection: return drawLevelSelectScreen(snaps, flags);
        case GameView::MultiPlayerOptionsMenu: return drawMultiplayerOptionsMenu(snaps, flags);
        case GameView::MultiplayerBrowse: return drawMultiplayerBrowse(snaps, flags);
        case GameView::MultiplayerHostWaiting: return drawMultiplayerHostWaiting(snaps, flags);
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

  void CutscenePlayer::setAnimIndex(int animIdx) {
    if (!scenes || sceneIndex >= scenes->size()) return;
    scenes->at(sceneIndex).anim->setFixedFrameIdx(animIdx);
  };

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
