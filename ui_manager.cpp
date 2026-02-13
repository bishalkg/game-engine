#include "ui_manager.h"
#include "imgui.h"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui_impl_sdl3.h"
#include <iostream>

namespace UIManager {

namespace {
// Local helper for the main menu (not a member).
UIActions drawMainMenu(const LoadingSnapshot& /*unused*/, ImGuiWindowFlags flags) {
    ImVec2 buttonSize = ImVec2(150, 50); // TODO put in config

    UIActions act;
    act.stopBackgroundTrack = true;
    ImGui::Begin("Main Menu", nullptr, flags);
    if (ImGui::Button("Single Player", buttonSize)) {
        std::cout << "start game" << std::endl;
        act.nextView = GameView::Playing;
        act.startSinglePlayer = true;
    }
    if (ImGui::Button("Multiplayer", buttonSize)) {
        act.nextView = GameView::MultiPlayerOptionsMenu;
        std::cout << "multi game" << std::endl;
    }
    if (ImGui::Button("Quit", buttonSize)) {
        std::cout << "quit game" << std::endl;
        act.quitGame = false;
    }
    ImGui::End();
    return act;
}

} // namespace

UIActions UI_Manager::drawLoading(const LoadingSnapshot& ls, ImGuiWindowFlags flags) {
    UIActions act;
    ImGui::Begin("Loading", nullptr, flags);
    ImGui::Text("Loading level...");
    ImGui::ProgressBar(ls.progress01, ImVec2(200, 0));
    ImGui::End();
    if (ls.done) act.finishLoading = true;
    return act;
}

UIActions UI_Manager::renderView(GameView view, const UISnapshots& snaps, ImGuiWindowFlags flags) {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    switch (view) {
      case GameView::MainMenu:     return drawMainMenu(snaps.loading, flags);
      case GameView::LevelLoading: return drawLoading(snaps.loading, flags);
      default:                     return {};
    }
}



// bool updateImGuiMenuRenderState() {
//     // 4) Start a new ImGui frame
//     ImGui_ImplSDLRenderer3_NewFrame();
//     ImGui_ImplSDL3_NewFrame();
//     ImGui::NewFrame();

//     // 5) Build your ImGui UI for THIS frame
//     // (menus, pause, debug overlay, etc.)
//     ImGuiIO& io = ImGui::GetIO();
//     ImGui::SetNextWindowPos(ImVec2(0, 0));
//     ImGui::SetNextWindowSize(io.DisplaySize);

//     ImVec2 buttonSize = ImVec2(150, 50); // TODO put in config

//     switch (m_gameState.currentView) {
//       case UIManager::GameView::MainMenu:
//       {
//         this->stopBackgroundSoundtrack();
//         ImGui::Begin("Main Menu", nullptr, m_sdlState.ImGuiWindowFlags);
//         ImGui::Text("Hello from ImGui in SDL3!");
//         if (ImGui::Button("Single Player", buttonSize)) {
//             std::cout << "start game" << std::endl;
//             std::puts("Start clicked");
//             m_gameState.currentView = UIManager::GameView::Playing;
//             m_gameType = game_engine::Engine::SinglePlayer;
//         }
//         if (ImGui::Button("Multiplayer",buttonSize)) {
//           m_gameState.currentView = UIManager::GameView::MultiPlayerOptionsMenu;
//           std::cout << "multi game" << std::endl;
//         }
//         if (ImGui::Button("Quit", buttonSize)) {
//             std::cout << "quit game" << std::endl;
//             m_gameRunning.store(false);
//         }
//         ImGui::End();
//         break;
//       }
//       case UIManager::GameView::LevelLoading:
//       {
//         {
//           std::lock_guard<std::mutex> lock(m_levelMutex);
//           uint8_t p = m_gameState.getLevelLoadProgress();
//           if (p >= 100) {
//             if (m_levelLoadThd.joinable()) {
//                 m_levelLoadThd.join();
//             }
//             m_gameState.currentView = UIManager::GameView::Playing;
//             break;
//           }

//           ImGui::Begin("Loading", nullptr, m_sdlState.ImGuiWindowFlags);
//           ImGui::Text("Loading level...");
//           ImGui::ProgressBar(p <= 1.0f ? p : p * 0.01f, ImVec2(200, 0));
//           ImGui::End();
//           return true;
//         }
//         break;
//       }
//       case UIManager::GameView::GameOver:
//       {
//         // When player
//         stopBackgroundSoundtrack();
//         // setGameOverSoundtrack();
//         ImGui::Begin("GameOver", nullptr, m_sdlState.ImGuiWindowFlags);
//         ImGui::Text("GAME OVER");
//         if (ImGui::Button("Try Again")) {
//           asyncSwitchToLevel(m_resources.m_currLevelIdx);
//           stopGameOverSoundtrack();
//         }
//         ImGui::End();
//         break;
//       }
//       case UIManager::GameView::InventoryMenu:
//       {
//         ImGui::Begin("Inventory Menu", nullptr, m_sdlState.ImGuiWindowFlags);
//         if (ImGui::Button("Resume")) m_gameState.currentView = UIManager::GameView::Playing;
//         if (ImGui::Button("Quit")) m_gameRunning.store(false);
//         // Want to continue rendering the screen underneath. The Pause Menu just overlays.

//         ImGui::End();
//         break;
//       }
//       case UIManager::GameView::PauseMenu:
//       {
//         ImGui::Begin("Pause", nullptr, m_sdlState.ImGuiWindowFlags);
//         if (ImGui::Button("Resume")) m_gameState.currentView = UIManager::GameView::Playing;
//         if (ImGui::Button("Quit")) m_gameRunning.store(false);
//         // Want to continue rendering the screen underneath. The Pause Menu just overlays.

//         ImGui::End();
//         break;
//       }
//       case UIManager::GameView::MultiPlayerOptionsMenu:
//       {
//         // drawSettings();
//         ImGui::Begin("MultiPlayer Menu", nullptr, m_sdlState.ImGuiWindowFlags);
//         if (ImGui::Button("Host A Game",buttonSize)) {
//           m_gameType = game_engine::Engine::Host;
//           m_gameState.currentView = UIManager::GameView::Playing;
//         }
//         if (ImGui::Button("Join A Game", buttonSize)) {
//           m_gameType = game_engine::Engine::Client;
//           m_gameState.currentView = UIManager::GameView::Playing;
//         }
//         if (ImGui::Button("Back to Menu", buttonSize)) {
//           // todo; should reset game state unless saved
//           m_gameState.currentView = UIManager::GameView::MainMenu;
//         }
//         ImGui::End();
//         break;
//       }
//       case UIManager::GameView::Playing:
//       {
//         if (m_gameState.drawMenuSettingsDuringGameplay(buttonSize)) {
//            asyncSwitchToLevel(m_resources.m_currLevelIdx);
//         }
//         m_gameState.drawPlayerHealthBar();
//         break;
//       }
//     }

//     return false;
// }



}
