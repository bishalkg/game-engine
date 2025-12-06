#include <iostream>
#include <vector>
#include <string>
#include <format>
// #include <array>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <glm/glm.hpp>

#include "gameobject.h"
#include "gameengine.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "app.h"


void App::App::Run() {

  GameEngine::GameEngine game;
  if (!game.init(1600, 900, 640, 320)) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Game Init Failed", "Failed to init Game", nullptr);
    return;
  }

  game.runGameLoop();

  game.cleanupTextures();

  game.cleanup(); // Clean up SDL

  return;
};




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





