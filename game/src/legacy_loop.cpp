#include "engine/engine.h"

void game_engine::Engine::runGameLoop() {
    // start the game loop
  uint64_t prevTime = SDL_GetTicks();
  UIManager::UI_Manager& uiManager = m_resources.m_uiManager;
  m_gameRunning.store(true);

  while (m_gameRunning.load()){

    uint64_t nowTime = SDL_GetTicks();
    float deltaTime = (nowTime - prevTime) / 1000.0f; // convert to seconds; time bw frames
    prevTime = nowTime;

    GameObject &player = getPlayer();


    // runEventLoop takes in key inputs that we want the client to send to the server
    // we read in snapshots from the server and updateGamePlayState; reconcile each GameObjects position using the m_stateLastUpdatedAt
    game_engine::NetGameInput input; // populate this from runEventLoop
    input.tick = nowTime;
    UIManager::UISnapshots snaps;
    runEventLoop(player, input, snaps);

    // ui_manager to handle this
    UIManager::UIActions actions = updateUI(uiManager, deltaTime, snaps);

    if (!actions.blockMainGameDraw) {

      uiManager.clearRenderer(m_sdlState);

      if (!handleMultiplayerConnections()) {
        return; // if we dont update gamePlayState will it render previous state? uiManager.renderPresent inside uiManager in cutscenes
      }

      if (isConnectedToServer && m_gameClient) {
        m_gameClient->OnUserUpdate(deltaTime);
        if (m_gameClient->IsClientValidated()) {

          // for jump and swing presses only send message on initial press to prevent double jump..this is broken right now

          // only send msg if there is a real player input; we need to use the scancode up and down logic to handle continuous
          // vs single time actions

          if (m_sdlState.keys[SDL_SCANCODE_LEFT]) {
            input.move = game_engine::PlayerInput::MoveLeft;
            input.shouldSendMessage = true;
          }
          if (m_sdlState.keys[SDL_SCANCODE_RIGHT]) {
            input.move = game_engine::PlayerInput::MoveRight;
            input.shouldSendMessage = true;
          }
          // if (m_sdlState.keys[SDL_SCANCODE_UP]) {
          //   input.move = game_engine::PlayerInput::Up;
          //   sendMsg = true;
          // }
          if (m_sdlState.keys[SDL_SCANCODE_A]) {
            input.fireProjectile = game_engine::PlayerInput::Fire;
            input.shouldSendMessage = true;
          }
          // if (m_sdlState.keys[SDL_SCANCODE_S]) {
          //   input.swingWeapon = game_engine::PlayerInput::Swing;
          //   sendMsg = true;
          // }

          if (input.shouldSendMessage) {
            // game_engine::NetGameInput input; // populate this from runEventLoop
            net::message<GameMsgHeaders> msg;
            msg.header.id = GameMsgHeaders::Game_PlayerInput;
            msg.body = input.serealizeNetGameInput();
            msg.header.bodySize = msg.body.size();
            m_gameClient->Send(msg);
          }
        }
      }

      // world.update()
      // if (!ui.isBlockingGameSimulation()) {
      //     world.update(dt);
      // }
      updateGameplayState(deltaTime, player, actions);
    }

    uiManager.renderPresent(m_sdlState);

  };

  if (m_gameServer) {
    // triggers .wakeAll() on the servers incoming queue
    // serverThd loop should wake up, next loop see that m_gameServer is false and quit out of the loop, then it should be joinable
    // net::message<GameMsgHeaders> msg;
    // msg.header.id = GameMsgHeaders::Server_ShutdownOK;
    // m_gameClient->Send(msg);
    m_gameServer->Stop();
  }
  if (m_serverLoopThd.joinable()) {
    m_serverLoopThd.join();
  }
  m_gameServer.reset();

};

void game_engine::Engine::runEventLoop(GameObject &player, game_engine::NetGameInput &net_input, UIManager::UISnapshots &snaps) {

    //     // Route input: if UI is modal, don’t drive gameplay
    // if (ui.isBlockingGameInput()) {
    //     ui.handleInput(input);        // arrows/confirm/back handled by UI
    // } else {
    //     gameplay.handleInput(input);  // movement, actions, etc.
    //     ui.handleNonBlocking(input);  // e.g., HUD hover/tooltips if you want
    // }

    // event loop
    SDL_Event event{0};
    while (SDL_PollEvent(&event)) {

      // 2) Give every event to ImGui first
      ImGui_ImplSDL3_ProcessEvent(&event);

      // always honor quit
      if (event.type == SDL_EVENT_QUIT) {
        m_gameRunning.store(false);
        break;
      }

      // 3) only handle game input if ImGui doesn't want it
      // ImGuiIO& io = ImGui::GetIO();
      // bool uiWantsKeyboard = io.WantCaptureKeyboard;
      // bool uiWantsMouse    = io.WantCaptureMouse;

      // if (!uiWantsKeyboard || !uiWantsMouse) {
      // TODO abstract this to game.handleGameInput(event);
      switch (event.type) {
        case SDL_EVENT_QUIT:
        {
          m_gameRunning.store(false);
          break;
        }
        case SDL_EVENT_WINDOW_RESIZED:
        {
          setWindowSize(event.window.data2, event.window.data1);
          break;
        }
        case SDL_EVENT_KEY_DOWN: // non-continuous presses
        {
          handleKeyInput(player, event.key.scancode, true, net_input);
          break;
        }
        case SDL_EVENT_KEY_UP:
        {
          handleKeyInput(player, event.key.scancode, false, net_input);
          if (event.key.scancode == SDL_SCANCODE_Q) {
            m_gameState.debugMode = !m_gameState.debugMode;
          } else if (event.key.scancode == SDL_SCANCODE_F11) {
            m_sdlState.fullscreen = !m_sdlState.fullscreen;
            SDL_SetWindowFullscreen(m_sdlState.window, m_sdlState.fullscreen);
          } else if (event.key.scancode == SDL_SCANCODE_TAB) {
            m_gameState.currentView = UIManager::GameView::InventoryMenu;
          } else if (
            m_gameState.currentView == UIManager::GameView::CutScene
            && event.key.scancode == SDL_SCANCODE_RETURN)
          {
            snaps.advanceToNextScene = true;
          }
          else if (
            (m_gameState.currentView == UIManager::GameView::Playing ||
            m_gameState.currentView == UIManager::GameView::PauseMenu) &&
            event.key.scancode == SDL_SCANCODE_P) {
            snaps.togglePauseGameplay = true;
          }
          break;
        }
      }
    }
}
