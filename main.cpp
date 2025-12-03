#include <iostream>
#include <vector>
#include <string>
#include <format>
#include <array>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <glm/glm.hpp>

// #include "animation.h"
#include "gameobject.h"
#include "gameengine.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"


using namespace std;

int main(int argc, char *argv[]) {


  GameEngine::GameEngine game;
  // 1) SDL, ImGUI are initialized
  if (!game.init(1600, 900, 640, 320)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game Init Failed", "Failed to init Game", nullptr);
        return 0;
  }

  // start the game loop
  uint64_t prevTime = SDL_GetTicks();
  bool running = true;

  GameEngine::GameState &gs = game.getGameState();
  GameEngine::SDLState &sdl = game.getSDLState();
  GameEngine::Resources &res = game.getResources();
  while (running){

    GameObject &player = game.getPlayer();  // fetch each frame in case index changes
    // use gs.currentView directly so state changes take effect immediately

    uint64_t nowTime = SDL_GetTicks();
    float deltaTime = (nowTime - prevTime) / 1000.0f; // convert to seconds; time bw frames

    // event loop
    SDL_Event event{0};
    while (SDL_PollEvent(&event)) {

      // 2) Give every event to ImGui first
      ImGui_ImplSDL3_ProcessEvent(&event);

      // always honor quit
      if (event.type == SDL_EVENT_QUIT) {
        running = false;
        continue;
      }

      // 3) only handle game input if ImGui doesn't want it
      ImGuiIO& io = ImGui::GetIO();
      bool uiWantsKeyboard = io.WantCaptureKeyboard;
      bool uiWantsMouse    = io.WantCaptureMouse;

      if (!uiWantsKeyboard && !uiWantsMouse) {
          // TODO abstract this to game.handleGameInput(event);
        switch (event.type) {
          case SDL_EVENT_QUIT:
          {
            running = false;
            break;
          }
          case SDL_EVENT_WINDOW_RESIZED:
          {
            game.setWindowSize(event.window.data2, event.window.data1);
            break;
          }
          case SDL_EVENT_KEY_DOWN: // non-continuous presses
          {
            game.handleKeyInput(player, event.key.scancode, true);
            break;
          }
          case SDL_EVENT_KEY_UP:
          {
            game.handleKeyInput(player, event.key.scancode, false);
            if (event.key.scancode == SDL_SCANCODE_Q) {
              gs.debugMode = !gs.debugMode;
            }
            break;
          }
        }
      }
    }


    // 4) Start a new ImGui frame
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // 5) Build your ImGui UI for THIS frame
    // (menus, pause, debug overlay, etc.)

    //
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImVec2 buttonSize = ImVec2(150, 50);

    switch (gs.currentView) {
      case GameEngine::GameScreen::MainMenu:
        ImGui::Begin("Main Menu", nullptr, sdl.ImGuiWindowFlags);
        ImGui::Text("Hello from ImGui in SDL3!");
        if (ImGui::Button("Start", buttonSize)) {
            // startGame();
            std::cout << "start game" << std::endl;
            std::puts("Start clicked");
            gs.currentView = GameEngine::GameScreen::Playing;
        }
        if (ImGui::Button("Multiplayer",buttonSize)) {
          gs.currentView = GameEngine::GameScreen::MultiPlayerOptionsMenu;
          std::cout << "multi game" << std::endl;
        }
        if (ImGui::Button("Quit", buttonSize)) {
            std::cout << "quit game" << std::endl;
            running = false;
        }
        ImGui::End();
        break;
      case GameEngine::GameScreen::PauseMenu:
        ImGui::Begin("Pause", nullptr, sdl.ImGuiWindowFlags);
        if (ImGui::Button("Resume")) gs.currentView = GameEngine::GameScreen::Playing;
        if (ImGui::Button("Quit")) running = false;
        ImGui::End();
        break;
      case GameEngine::GameScreen::MultiPlayerOptionsMenu:
        // drawSettings();
        ImGui::Begin("MultiPlayer Menu", nullptr, sdl.ImGuiWindowFlags);
        if (ImGui::Button("Host",buttonSize)) {
          // todo
        }
        if (ImGui::Button("Client",buttonSize)) {
          // todo
        }
        if (ImGui::Button("Back to Menu",buttonSize)) {
          // todo
          gs.currentView = GameEngine::GameScreen::MainMenu;
        }
        ImGui::End();
        break;
      case GameEngine::GameScreen::Playing:
        ImGuiWindowFlags ImGuiWindowFlags =
          sdl.ImGuiWindowFlags | ImGuiWindowFlags_NoBackground;
          // ImGuiWindowFlags_NoSavedSettings;
          ImGui::Begin("HUD", nullptr, ImGuiWindowFlags);
          // Optional: remove padding so the button hugs the corner
          ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
          ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
          if (ImGui::Button("Back to Menu", buttonSize)) {
              gs.currentView = GameEngine::GameScreen::MainMenu;
          }
          ImGui::PopStyleVar();
          ImGui::PopItemFlag();
          ImGui::End();
        break;
    }

    // clear the backbuffer before drawing onto it with black from draw color above
    SDL_SetRenderDrawColor(sdl.renderer, 20, 10, 30, 255);
    SDL_RenderClear(sdl.renderer);


    bool playing = (gs.currentView == GameEngine::GameScreen::Playing);
    if (playing) {
      // update & draw game world to sdl.renderer here (before ImGui::Render)
      // TODO make into helper: UpdateAllObjects()
      // update all objects;
      for (auto &layer : gs.layers) {
        for (GameObject &obj : layer) { // for each obj in layer
          if (obj.dynamic) {
            game.updateGameObject(obj, deltaTime);
          }
        }
      }

      // update bullet physics
      for (GameObject &bullet : gs.bullets) {
        game.updateGameObject(bullet, deltaTime);
      }

      // TODO wrap all below in Render() function
      // calculate viewport position based on player updated position
      gs.mapViewport.x = (player.position.x + player.spritePixelW / 2) - gs.mapViewport.w / 2;

      // SDL_SetRenderDrawColor(sdl.renderer, 20, 10, 30, 255);

      // // clear the backbuffer before drawing onto it with black from draw color above
      // SDL_RenderClear(sdl.renderer);

      // Perform drawing commands:

      // draw background images
      SDL_RenderTexture(sdl.renderer, res.texBg1, nullptr, nullptr);
      game.drawParalaxBackground(res.texBg4, player.velocity.x, gs.bg4scroll, 0.075f, deltaTime);
      game.drawParalaxBackground(res.texBg3, player.velocity.x, gs.bg3scroll, 0.15f, deltaTime);
      game.drawParalaxBackground(res.texBg2, player.velocity.x, gs.bg2scroll, 0.3f, deltaTime);

      // draw all background objects
      for (auto &tile : gs.backgroundTiles) {
        SDL_FRect dst{
          .x = tile.position.x - gs.mapViewport.x,
          .y = tile.position.y,
          .w = static_cast<float>(tile.texture->w),
          .h = static_cast<float>(tile.texture->h),
        };
        SDL_RenderTexture(sdl.renderer, tile.texture, nullptr, &dst);
      }

      // draw all interactable objects
      for (auto &layer : gs.layers) {
        for (GameObject &obj : layer) {
          game.drawObject(obj, obj.spritePixelH, obj.spritePixelW, deltaTime);
        }
      }

      // draw bullets
      for (GameObject &bullet: gs.bullets) {
        if (bullet.data.bullet.state != BulletState::inactive) {
          game.drawObject(bullet, bullet.collider.h, bullet.collider.w, deltaTime);
        }
      }

      // draw all foreground objects
      for (auto &tile : gs.foregroundTiles) {
        SDL_FRect dst{
          .x = tile.position.x - gs.mapViewport.x,
          .y = tile.position.y,
          .w = static_cast<float>(tile.texture->w),
          .h = static_cast<float>(tile.texture->h),
        };
        SDL_RenderTexture(sdl.renderer, tile.texture, nullptr, &dst);
      }

      // debugging
      if (gs.debugMode) {
        SDL_SetRenderDrawColor(sdl.renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(
            sdl.renderer,
            5,
            5,
            std::format("State3: {}  Direction: {} B: {}, G: {}", static_cast<int>(player.data.player.state), player.direction, gs.bullets.size(), player.grounded).c_str());
      }
    }

    // swap backbuffer to display new state
    // Textures live in GPU memory; the renderer batches copies/draws and flushes them on present.
    // 6) Render ImGui on top of your SDL frame
    SDL_SetRenderLogicalPresentation(sdl.renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdl.renderer);

    SDL_SetRenderLogicalPresentation(sdl.renderer, sdl.logW, sdl.logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    SDL_RenderPresent(sdl.renderer);

    prevTime = nowTime;
  };

  game.cleanupTextures();
  game.cleanup(); // Clean up SDL

  std::cout << "Exited cleanly." << std::endl;
  return 0;
}


// Template pseudocode for multiplayer loop
    // SharedNetworkInterface net;      // your abstraction for queues/sockets
    // SharedGameStateForClients renderState;  // what clients use to draw (snapshot buffer)
    // will also need to implement networking with asio (udp? tcp?)

// int main() {
//     // init SDL, ImGui, networking, etc.

//     bool isHost = true;   // this client is also the server (host game)
//     std::atomic<bool> running = true;

//     // Shared state between server thread and this client (host)
//     SharedNetworkInterface net;      // your abstraction for queues/sockets
//     SharedGameStateForClients renderState;  // what clients use to draw (snapshot buffer)

//     std::thread serverThread;
//     if (isHost) {
//         serverThread = std::thread([&] {
//             RunServerLoop(running, net);
//         });
//     }

//     // Connect this client to "server"
//     ClientConnection conn = net.connectLocalOrRemote(...);

//     RunClientLoop(running, conn, renderState);

//     // shutdown
//     running = false;
//     if (serverThread.joinable()) {
//         serverThread.join();
//     }

//     // cleanup SDL/ImGui/etc.
// }


// void RunServerLoop(std::atomic<bool>& running, SharedNetworkInterface& net) {
//     const double tickRate   = 60.0;           // 60 ticks per second
//     const double dt         = 1.0 / tickRate; // seconds per tick
//     double accumulator      = 0.0;

//     GameWorld world; // authoritative state (positions, velocities, etc.)
//     world.init();

//     auto lastTime = Now(); // some high-res clock

//     while (running) {
//         auto now = Now();
//         double frameTime = SecondsBetween(lastTime, now);
//         lastTime = now;
//         accumulator += frameTime;

//         // Process all incoming messages/inputs
//         net.pollIncomingMessages([&](const ClientInputMessage& msg){
//             ApplyInputToPlayerCommandBuffer(world, msg);
//         });

//         // Step the world in fixed-size chunks
//         while (accumulator >= dt) {
//             world.step(dt); // physics, AI, collisions, etc.
//             accumulator -= dt;
//         }

//         // Periodically broadcast snapshots to clients
//         // (could be every tick or e.g. 20 times/sec)
//         ServerStateSnapshot snapshot = MakeSnapshotFromWorld(world);
//         net.broadcastToAllClients(snapshot);

//         // Avoid busy-waiting: sleep a bit to approximate tick rate
//         SleepSmallAmount();
//     }
// or this:
// const float dt = 1.0f / 60.0f;
// while (running) {
//     double start = now();

//     // 1) Drain any queued inputs from all clients (since last tick)
//     InputBatch inputs = inputQueue.popAll();
//     gameSimulation.applyInputs(inputs);

//     // 2) Step the authoritative world
//     gameSimulation.update(dt);

//     // 3) Every N ticks, send a snapshot to all clients
//     if (tickCount % 3 == 0) { // e.g. 20 snapshots/sec
//         WorldSnapshot snap = gameSimulation.makeSnapshot();
//         netServer.broadcast(snap);
//     }

//     sleep_until(start + dt);  // keep fixed timestep
//     tickCount++;
// }

// }


// void RunClientLoop(std::atomic<bool>& running,
//                    ClientConnection& conn,
//                    SharedGameStateForClients& renderState)
// {
//     // SDL + ImGui init already done

//     while (running) {
//         // --- 1. Handle SDL events ---
//         SDL_Event event;
//         while (SDL_PollEvent(&event)) {
//             ImGui_ImplSDL3_ProcessEvent(&event);

//             if (event.type == SDL_EVENT_QUIT) {
//                 running = false;
//             }

//             // Decide if ImGui wants input or game does
//             ImGuiIO& io = ImGui::GetIO();
//             bool uiWantsKeyboard = io.WantCaptureKeyboard;
//             bool uiWantsMouse    = io.WantCaptureMouse;

//             if (!uiWantsKeyboard && !uiWantsMouse) {
//                 HandleGameInputAndBuildCommands(event, conn);
//             }
//         }

//         // --- 2. Receive latest server state snapshots ---
//         conn.pollMessages([&](const ServerStateSnapshot& snapshot) {
//             // Thread-safe write into some client-side buffer
//             renderState.updateFromSnapshot(snapshot);
//         });

//         // --- 3. Start ImGui frame ---
//         ImGui_ImplSDLRenderer3_NewFrame();
//         ImGui_ImplSDL3_NewFrame();
//         ImGui::NewFrame();

//         // --- 4. Clear and render world ---
//         SDL_SetRenderDrawColor(g_Renderer, 20, 20, 20, 255);
//         SDL_RenderClear(g_Renderer);

//         // Copy the latest state into a local (non-shared) struct
//         // to minimize time holding locks
//         ClientVisibleState visibleState;
//         renderState.copyTo(visibleState); // e.g. with a mutex lock

//         // Now render visibleState (positions, animations) with SDL
//         RenderWorldFromState(g_Renderer, visibleState);

//         // --- 5. Build ImGui UI (debug/menu/etc.) ---
//         DrawGameUIWithImGui(visibleState);

//         // --- 6. Render ImGui on top ---
//         ImGui::Render();
//         ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData());

//         // --- 7. Present ---
//         SDL_RenderPresent(g_Renderer);
//     }
// }
