#include "gameengine.h"
#include "game_server.h"

GameObject &game_engine::Engine::getPlayer() {
 return m_gameState.player(LAYER_IDX_CHARACTERS);
};

game_engine::Engine::~Engine() {
  // ensure server thread and client tear down cleanly
  m_gameRunning.store(false);
  if (m_gameServer) {
    m_gameServer->Stop();
  }
  if (m_serverLoopThd.joinable()) {
    m_serverLoopThd.join();
  }
}

game_engine::SDLState &game_engine::Engine::getSDLState() {
  return m_sdlState;
};

game_engine::GameState &game_engine::Engine::getGameState() {
  return m_gameState;
};

game_engine::Resources &game_engine::Engine::getResources() {
  return m_resources;
};

void game_engine::Engine::setBackgroundSoundtrack() {
  if (!MIX_TrackPlaying(m_resources.m_currLevel->backgroundTrack)) {
    SDL_PropertiesID opts = SDL_CreateProperties();
    SDL_SetNumberProperty(opts, MIX_PROP_PLAY_LOOPS_NUMBER, -1); // loop forever
    if (!MIX_PlayTrack(m_resources.m_currLevel->backgroundTrack, opts)) {
        SDL_Log("Background Music Play failed: %s", SDL_GetError());
    }
    SDL_DestroyProperties(opts); // destory internal resources
  }
};

void game_engine::Engine::stopBackgroundSoundtrack() {
  if (MIX_TrackPlaying(m_resources.m_currLevel->backgroundTrack)) {
    if (!MIX_StopTrack(m_resources.m_currLevel->backgroundTrack, 10)) {
        SDL_Log("stopping Background Music Play failed: %s", SDL_GetError());
    }
  }
};

void game_engine::Engine::setGameOverSoundtrack() {
  if (!MIX_TrackPlaying(m_resources.m_currLevel->gameOverAudioTrack)) {
    SDL_PropertiesID opts = SDL_CreateProperties();
    SDL_SetNumberProperty(opts, MIX_PROP_PLAY_LOOPS_NUMBER, 0);
    if (!MIX_PlayTrack(m_resources.m_currLevel->gameOverAudioTrack, opts)) {
        SDL_Log("Game Over Music Play failed: %s", SDL_GetError());
    }
    SDL_DestroyProperties(opts); // destory internal resources
  }
};

void game_engine::Engine::stopGameOverSoundtrack() {
  if (MIX_TrackPlaying(m_resources.m_currLevel->gameOverAudioTrack)) {
    if (!MIX_StopTrack(m_resources.m_currLevel->gameOverAudioTrack, 10)) {
        SDL_Log("stopping Game Over Music Play failed: %s", SDL_GetError());
    }
  }
};

void game_engine::Engine::setWindowSize(int height, int width) {
  m_sdlState.width = width;
  m_sdlState.height = height;
}

bool game_engine::Engine::init(int width, int height, int logW, int logH) {

  // SDL, ImGUI are initialized

  // init window and renderer
  if (!initWindowAndRenderer(width, height, logW, logH)) {
    return false;
  };

  // mixer must be created before loading in audio files in res.load()
  if (!MIX_Init()) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL Mixer Failed to init", "Failed to init audio", nullptr);
    cleanup();
    return false;
  };

  // one global mixer
  m_resources.mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
  if (!m_resources.mixer) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Global Mixer Failed to create", "Failed to create audio mixer", nullptr);
    cleanup();
    return false;
  }

  MIX_SetMasterGain(m_resources.mixer, 0.5f);  // 50% master

  m_gameState = std::move(GameState(m_sdlState));
  // load game assets
  m_resources.loadAllAssets(m_sdlState, m_gameState, false);
  if (!m_resources.m_currLevel->texCharacterMap[SpriteType::Player_Knight].texIdle) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Idle Texture load failed", "Failed to load idle image", nullptr);
    cleanup();
    return false;
  }

  // setup game data
  // gs.mapViewport.x = 0;
  // gs.mapViewport.y = 0;
  // gs.mapViewport.w = state.logW; // or state.width/state.logW
  // gs.mapViewport.h = state.logH; // or state.height/state.logH
  return initAllTiles(m_gameState);
}


void game_engine::Engine::runGameLoop() {
    // start the game loop
  uint64_t prevTime = SDL_GetTicks();
  m_gameRunning.store(true);

  while (m_gameRunning.load()){
    uint64_t nowTime = SDL_GetTicks();
    float deltaTime = (nowTime - prevTime) / 1000.0f; // convert to seconds; time bw frames

    GameObject &player = getPlayer();


    // runEventLoop takes in key inputs that we want the client to send to the server
    // we read in snapshots from the server and updateGamePlayState; reconcile each GameObjects position using the m_stateLastUpdatedAt
    game_engine::NetGameInput input; // populate this from runEventLoop
    input.tick = nowTime;
    runEventLoop(player, input);

    // ui_manager to handle this
    if (updateImGuiMenuRenderState()) {
      ImGui::Render();
      continue;
    }

    clearRenderer();

    if (!handleMultiplayerConnections()) {
      return;
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
    updateGameplayState(deltaTime, player);

    renderUpdates();

    prevTime = nowTime;
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

bool game_engine::Engine::handleMultiplayerConnections() {
    // create a client; need a GameClient that inherits from client_interface
    // wait for connection to the server to be made.
    // non-host client should be able to join and exit at any time.
  if (m_gameType == game_engine::Engine::Host && !m_gameServer) {
    std::cout << "starting server" << std::endl;
    // TODO buildAuthoritativeState() -> { gameState, headlessResources }
    m_gameServer = std::make_unique<GameServer>(9000, m_gameState, m_resources);
    bool successStart = m_gameServer->Start();
    if (!successStart) {
      return false;
    }
    m_serverLoopThd = std::thread(&game_engine::Engine::runGameServerLoopThread, this);
  }

  // host creates client
  if (!m_gameClient && (m_gameType == game_engine::Engine::Host || m_gameType == game_engine::Engine::Client)) {
    std::cout << "starting client" << std::endl;
    m_gameClient = std::make_unique<GameClient>();
  }

  // every render loop tries to connect to server
  if (m_gameClient) {
    if (!isConnectedToServer) {
      if (m_gameClient->Connect("127.0.0.1", 9000)) {
        isConnectedToServer = true;
        std::cout << "client connected to server" << std::endl;
      } else {
        std::cout << "failed to connect to server" << std::endl;
        // return;
      }
    }
  }
  return true;
}

void game_engine::Engine::asyncSwitchToLevel(LevelIndex lvl) {
  m_gameState.currentView = UIManager::GameView::LevelLoading;
  m_gameState.setLevelLoadProgress(0);
  m_levelLoadThd = std::thread(&game_engine::Engine::initNextLevel, this, lvl);
}

void game_engine::Engine::initNextLevel(LevelIndex lvl) {

  m_gameState.setLevelLoadProgress(10);

  m_resources.loadLevel(lvl, m_sdlState, m_gameState, m_resources.m_masterAudioGain, false);

  // throw away old world; create new gameState that will be updated in initAllTiles
  GameState newGameState(m_sdlState);
  newGameState.currentView = UIManager::GameView::LevelLoading;

  // rebuild layers/objects from the newly loaded map
  initAllTiles(newGameState); // this mutates gameState
  m_gameState.setLevelLoadProgress(70);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  {
    std::lock_guard<std::mutex> lock(m_levelMutex);
    m_gameState = std::move(newGameState);
  }

  // m_gameState = std::move(newGameState); // now its safe to transfer ownership
  m_gameState.setLevelLoadProgress(100);
}

void game_engine::Engine::runGameServerLoopThread() {

  using clock = std::chrono::steady_clock;
  const double dt = 1.0/60.0; // 60Hz
  const size_t maxInputsPerTick = 64;
  auto prev = clock::now();
  double accum = 0.0;

  // determine delta for server loop
  while (m_gameRunning.load() && m_gameServer) {
    // right now the loop blocks on ProcessIncomingMessages() and only broadcasts right after a message arrives. That ties your tick rate to network traffic; if no input arrives, clients get no snapshots. Run the server loop on a fixed timestep (e.g., sleep to 60Hz), call ProcessIncomingMessages(nMaxMessagesPerTick, /*enableWaiting=*/false) to drain some inputs, then step simulation and broadcast
    m_gameServer->ProcessIncomingMessages(maxInputsPerTick, false);

    auto now = clock::now();
    accum += std::chrono::duration<double>(now - prev).count();
    prev = now;

    // run as many fixed steps as have accumulated time
    while (accum >= dt) {
      m_gameServer->applyPlayerInputs();
      // 1) pop inputs from your queue and apply to authoritative state
      //    e.g., serverState.applyInputs(inputQueue);
      // 2) advance physics/logic by dt
      //    e.g., serverState.step(dt);
      accum -= dt;
    }

    // Broadcast snapshot to all clients
    net::message<GameMsgHeaders> msg;
    msg.header.id = GameMsgHeaders::Game_Snapshot;
    msg.body = m_gameServer->m_currGameSnapshot.serealizeNetGameStateSnapshot();
    msg.header.bodySize = msg.body.size();
    m_gameServer->BroadcastToClients(msg);

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void game_engine::Engine::clearRenderer(){
    // clear the backbuffer before drawing onto it with black from draw color above
    SDL_SetRenderDrawColor(m_sdlState.renderer, 20, 10, 30, 255);
    SDL_RenderClear(m_sdlState.renderer);
}

void game_engine::Engine::renderUpdates(){
  // swap backbuffer to display new state
  // Textures live in GPU memory; the renderer batches copies/draws and flushes them on present.
  // 6) Render ImGui on top of your SDL frame
  // world.render()
  SDL_SetRenderLogicalPresentation(m_sdlState.renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);

  // ui.render()
  ImGui::Render();
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_sdlState.renderer);
  SDL_SetRenderLogicalPresentation(m_sdlState.renderer, m_sdlState.logW, m_sdlState.logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);

  // renderer.present()
  SDL_RenderPresent(m_sdlState.renderer);
}


void game_engine::Engine::runEventLoop(GameObject &player, game_engine::NetGameInput &net_input) {

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
          }
          break;
        }
      }
    }
    // }
}

bool game_engine::Engine::initWindowAndRenderer(int width, int height, int logW, int logH) {

  m_sdlState.width = width;
  m_sdlState.height = height;
  m_sdlState.logW = logW;
  m_sdlState.logH = logH;

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL Init Error", "Failed to initialize SDL.", nullptr);
    return false;
  };

  if (!SDL_CreateWindowAndRenderer("SDL Game Engine", m_sdlState.width, m_sdlState.height, SDL_WINDOW_RESIZABLE, &m_sdlState.window, &m_sdlState.renderer)) {

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL init error", SDL_GetError(), nullptr);

    this->cleanup();
    return false;
  }
  m_sdlState.keys = SDL_GetKeyboardState(nullptr);
  SDL_SetRenderVSync(m_sdlState.renderer, 1);

  // configure presentation
  SDL_SetRenderLogicalPresentation(m_sdlState.renderer, m_sdlState.logW , m_sdlState.logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);

  // Setup ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // optional

  // Setup ImGui style
  ImGui::StyleColorsDark();

  // Setup ImGui platform/renderer backends
  ImGui_ImplSDL3_InitForSDLRenderer(m_sdlState.window, m_sdlState.renderer);
  ImGui_ImplSDLRenderer3_Init(m_sdlState.renderer);

  return true;

}

void game_engine::Engine::cleanupTextures() {
  this->m_resources.unload();
}

void game_engine::Engine::cleanup() {
  if (m_resources.mixer) { MIX_DestroyMixer(m_resources.mixer); m_resources.mixer = nullptr; }
  MIX_Quit();
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  // if (state.renderer) { SDL_DestroyRenderer(state.renderer); state.renderer = nullptr; }
  // if (state.window) { SDL_DestroyWindow(state.window); state.window = nullptr; }
  // SDL_Quit();
}

void game_engine::Engine::drawObject(GameObject &obj, float height, float width, float deltaTime) {

  float frameW = obj.spritePixelW;
  float frameH = obj.spritePixelH;

  // select frame from sprite sheet
  float srcX = (obj.currentAnimation != -1)
                 ? obj.animations[obj.currentAnimation].currentFrame() * frameW
                 : (obj.spriteFrame - 1) * frameW;

  SDL_FRect src{srcX, 0, frameW, frameH};

  // scale sprites up or down
  float drawW = frameW / obj.drawScale;
  float drawH = frameH / obj.drawScale;

  SDL_FRect dst{
    obj.position.x - m_gameState.mapViewport.x,
    obj.position.y - m_gameState.mapViewport.y,
    drawW, drawH
  };

  SDL_FlipMode flipMode = obj.direction == -1 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

  // set flash animations on enemies
  if (obj.shouldFlash) {
    SDL_SetTextureColorModFloat(obj.texture, 2.5f, 1.0f, 1.0f);
  }

  SDL_RenderTextureRotated(m_sdlState.renderer, obj.texture, &src, &dst, 0, nullptr, flipMode);

  if (obj.shouldFlash) {
    SDL_SetTextureColorModFloat(obj.texture, 1.0f, 1.0f, 1.0f);
    if (obj.flashTimer.step(deltaTime)) {
      obj.shouldFlash = false;
    }
  }

  if (m_gameState.debugMode) {

    SDL_FRect spriteBox{
      .x = obj.position.x - m_gameState.mapViewport.x,
      .y = obj.position.y - m_gameState.mapViewport.y,
      .w = obj.collider.w,
      .h = obj.collider.h
    };
    SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_sdlState.renderer, 200, 100, 0, 100);
    SDL_RenderFillRect(m_sdlState.renderer, &spriteBox);
    SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_NONE);


    // display each objects collision hitbox
    SDL_FRect rectA{
      .x = obj.position.x + obj.collider.x - m_gameState.mapViewport.x,
      .y = obj.position.y + obj.collider.y - m_gameState.mapViewport.y,
      .w = obj.collider.w,
      .h = obj.collider.h
    };
    SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_sdlState.renderer, 255, 0, 0, 100);
    SDL_RenderFillRect(m_sdlState.renderer, &rectA);
    SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_NONE);

    SDL_FRect sensor{
      .x = obj.position.x + obj.collider.x - m_gameState.mapViewport.x,
      .y = obj.position.y + obj.collider.y + obj.collider.h - m_gameState.mapViewport.y,
      .w = obj.collider.w, .h = 1
    };
    SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_sdlState.renderer, 0, 0, 255, 255);
    SDL_RenderFillRect(m_sdlState.renderer, &sensor);
    SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_NONE);
  }
}


void game_engine::Engine::updateAllObjects(float deltaTime) {

  if (m_gameState.evaluateGameOver()) {
    setGameOverSoundtrack();
  }

  // if singleplayer let is pass through normal logic
  for (auto &layer : m_gameState.layers) {
    for (GameObject &obj : layer) { // for each obj in layer
      // optimization to avoid n*m comparisions
      if (obj.dynamic) {
        updateGameObject(obj, deltaTime);
      }
      // if (obj.type == ObjectType::player) {
      //   bool left  = state.keys ? state.keys[SDL_SCANCODE_LEFT]  : false;
      //   bool right = state.keys ? state.keys[SDL_SCANCODE_RIGHT] : false;
      //   SDL_Log("pos=(%.2f,%.2f) vel=(%.2f,%.2f) left=%d right=%d", obj.position.x, obj.position.y, obj.velocity.x, obj.velocity.y, int(left), int(right));
      // }
    }
  }

  // update bullet physics
  for (GameObject &bullet : m_gameState.bullets) {
    updateGameObject(bullet, deltaTime);
  }


  // if multiplayer, we need to use the latest GameSnapshot to update the objects
  if (m_gameType == Engine::GameRunMode::Client || m_gameType == Engine::GameRunMode::Host) {
    // m_gameClient will have the latest snapshot so use a getter to get the data



  }

}

void game_engine::Engine::updateMapViewport(GameObject& player) {
  if (!m_resources.m_currLevel || !m_resources.m_currLevel->map) return;

  int mapWpx = m_resources.m_currLevel->map->mapWidth * m_resources.m_currLevel->map->tileWidth;
  int mapHpx = m_resources.m_currLevel->map->mapHeight * m_resources.m_currLevel->map->tileHeight;

  m_gameState.mapViewport.x = std::clamp(
      (player.position.x + player.spritePixelW * 0.5f) - m_gameState.mapViewport.w * 0.5f,
      0.0f,
      std::max(0.0f, float(mapWpx - m_gameState.mapViewport.w)));

  m_gameState.mapViewport.y = std::clamp(
      (player.position.y + player.spritePixelH * 0.5f) - m_gameState.mapViewport.h * 0.5f,
      0.0f,
      std::max(0.0f, float(mapHpx - m_gameState.mapViewport.h)));
}

void game_engine::Engine::drawAllObjects(float deltaTime) {
  // draw all interactable objects
  for (auto &layer : m_gameState.layers) {
    for (GameObject &obj : layer) {

      if (obj.objClass == ObjectClass::Background) {
        // draw background images;
        // TODO needs to be sorted by background layers; comes from a map of uncertain order
        drawParalaxBackground(obj.texture, getPlayer().velocity.x, obj.bgscroll, obj.scrollFactor, deltaTime, -80.0f);

      } else if (obj.objClass == ObjectClass::Level) {
        // if level tile, let src and dst override so that
        // src points to a specfic 32x32 tile texture from the whole png; dst is where on our window we want to place it
        SDL_FRect dst = obj.data.level.dst;
        dst.x -= m_gameState.mapViewport.x;  // if you scroll horizontally
        dst.y -= m_gameState.mapViewport.y;  // if vertical scrolling
        // dst.w = static_cast<float>(obj.texture->w);
        // dst.h = static_cast<float>(obj.texture->h);
        SDL_RenderTexture(m_sdlState.renderer, obj.texture, &obj.data.level.src, &dst);

        if (m_gameState.debugMode) {
          // display each objects collision hitbox
          SDL_FRect rectA{
            .x = obj.position.x + obj.collider.x - m_gameState.mapViewport.x,
            .y = obj.position.y + obj.collider.y - m_gameState.mapViewport.y,
            .w = obj.collider.w,
            .h = obj.collider.h
          };
          SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_BLEND);
          SDL_SetRenderDrawColor(m_sdlState.renderer, 255, 0, 0, 100);
          SDL_RenderFillRect(m_sdlState.renderer, &rectA);
          SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_NONE);

          SDL_FRect sensor{
            .x = obj.position.x + obj.collider.x - m_gameState.mapViewport.x,
            .y = obj.position.y + obj.collider.y + obj.collider.h - m_gameState.mapViewport.y,
            .w = obj.collider.w, .h = 1
          };
          SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_BLEND);
          SDL_SetRenderDrawColor(m_sdlState.renderer, 0, 0, 255, 255);
          SDL_RenderFillRect(m_sdlState.renderer, &sensor);
          SDL_SetRenderDrawBlendMode(m_sdlState.renderer, SDL_BLENDMODE_NONE);
        }
      } else {
        drawObject(obj, obj.spritePixelH, obj.spritePixelW, deltaTime);
      }
    }
  }

  // draw bullets
  for (GameObject &bullet: m_gameState.bullets) {
    if (bullet.data.bullet.state != BulletState::inactive) {
      drawObject(bullet, bullet.collider.h, bullet.collider.w, deltaTime);
    }
  }
}

void game_engine::Engine::updateGameplayState(float deltaTime, GameObject& player) {
if (m_gameState.currentView == UIManager::GameView::LevelLoading) return;


  if (m_gameState.currentView == UIManager::GameView::Playing) {
      setBackgroundSoundtrack(); // TODO we will want to set background track per level

      // update & draw game world to sdl.renderer here (before ImGui::Render)
      updateAllObjects(deltaTime);

      updateMapViewport(player);

      drawAllObjects(deltaTime);

      // debugging
      if (m_gameState.debugMode) {
        SDL_SetRenderDrawColor(m_sdlState.renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(
            m_sdlState.renderer,
            5,
            5,
            std::format("State: {}  Direction: {} B: {}, G: {}, Px: {}, Py:{}, VPx: {}", static_cast<int>(player.data.player.state), player.direction, m_gameState.bullets.size(), player.grounded, player.position.x, player.position.y, m_gameState.mapViewport.x).c_str());
      }
    }

}

void game_engine::Engine::applyUIActions(const UIManager::UIActions& a) {
  if (a.stopBackgroundTrack) { stopBackgroundSoundtrack(); }
  if (a.startSinglePlayer) m_gameType = SinglePlayer;
  if (a.quitGame) m_gameRunning.store(false);
  if (a.finishLoading) {
    if (m_levelLoadThd.joinable()) m_levelLoadThd.join();
  }
  if (a.nextView) m_gameState.currentView = *a.nextView;
    // if (a.startGame) { /* … */ }
    // if (a.quit) { /* … */ }
    // etc.
}

bool game_engine::Engine::updateImGuiMenuRenderState() {

  UIManager::UISnapshots snaps;
  auto actions = m_resources.m_uiManager->renderView(m_gameState.currentView, snaps, m_sdlState.ImGuiWindowFlags);

  applyUIActions(actions);


    // // 4) Start a new ImGui frame
    // ImGui_ImplSDLRenderer3_NewFrame();
    // ImGui_ImplSDL3_NewFrame();
    // ImGui::NewFrame();

    // // 5) Build your ImGui UI for THIS frame
    // // (menus, pause, debug overlay, etc.)
    // ImGuiIO& io = ImGui::GetIO();
    // ImGui::SetNextWindowPos(ImVec2(0, 0));
    // ImGui::SetNextWindowSize(io.DisplaySize);

    // ImVec2 buttonSize = ImVec2(150, 50); // TODO put in config

    // switch (m_gameState.currentView) {
    //   case UIManager::GameView::MainMenu:
    //   {
    //     this->stopBackgroundSoundtrack();
    //     ImGui::Begin("Main Menu", nullptr, m_sdlState.ImGuiWindowFlags);
    //     ImGui::Text("Hello from ImGui in SDL3!");
    //     if (ImGui::Button("Single Player", buttonSize)) {
    //         std::cout << "start game" << std::endl;
    //         std::puts("Start clicked");
    //         m_gameState.currentView = UIManager::GameView::Playing;
    //         m_gameType = game_engine::Engine::SinglePlayer;
    //     }
    //     if (ImGui::Button("Multiplayer",buttonSize)) {
    //       m_gameState.currentView = UIManager::GameView::MultiPlayerOptionsMenu;
    //       std::cout << "multi game" << std::endl;
    //     }
    //     if (ImGui::Button("Quit", buttonSize)) {
    //         std::cout << "quit game" << std::endl;
    //         m_gameRunning.store(false);
    //     }
    //     ImGui::End();
    //     break;
    //   }
    //   case UIManager::GameView::LevelLoading:
    //   {
    //     {
    //       std::lock_guard<std::mutex> lock(m_levelMutex);
    //       uint8_t p = m_gameState.getLevelLoadProgress();
    //       if (p >= 100) {
    //         if (m_levelLoadThd.joinable()) {
    //             m_levelLoadThd.join();
    //         }
    //         m_gameState.currentView = UIManager::GameView::Playing;
    //         break;
    //       }

    //       ImGui::Begin("Loading", nullptr, m_sdlState.ImGuiWindowFlags);
    //       ImGui::Text("Loading level...");
    //       ImGui::ProgressBar(p <= 1.0f ? p : p * 0.01f, ImVec2(200, 0));
    //       ImGui::End();
    //       return true;
    //     }
    //     break;
    //   }
    //   case UIManager::GameView::GameOver:
    //   {
    //     // When player
    //     stopBackgroundSoundtrack();
    //     // setGameOverSoundtrack();
    //     ImGui::Begin("GameOver", nullptr, m_sdlState.ImGuiWindowFlags);
    //     ImGui::Text("GAME OVER");
    //     if (ImGui::Button("Try Again")) {
    //       asyncSwitchToLevel(m_resources.m_currLevelIdx);
    //       stopGameOverSoundtrack();
    //     }
    //     ImGui::End();
    //     break;
    //   }
    //   case UIManager::GameView::InventoryMenu:
    //   {
    //     ImGui::Begin("Inventory Menu", nullptr, m_sdlState.ImGuiWindowFlags);
    //     if (ImGui::Button("Resume")) m_gameState.currentView = UIManager::GameView::Playing;
    //     if (ImGui::Button("Quit")) m_gameRunning.store(false);
    //     // Want to continue rendering the screen underneath. The Pause Menu just overlays.

    //     ImGui::End();
    //     break;
    //   }
    //   case UIManager::GameView::PauseMenu:
    //   {
    //     ImGui::Begin("Pause", nullptr, m_sdlState.ImGuiWindowFlags);
    //     if (ImGui::Button("Resume")) m_gameState.currentView = UIManager::GameView::Playing;
    //     if (ImGui::Button("Quit")) m_gameRunning.store(false);
    //     // Want to continue rendering the screen underneath. The Pause Menu just overlays.

    //     ImGui::End();
    //     break;
    //   }
    //   case UIManager::GameView::MultiPlayerOptionsMenu:
    //   {
    //     // drawSettings();
    //     ImGui::Begin("MultiPlayer Menu", nullptr, m_sdlState.ImGuiWindowFlags);
    //     if (ImGui::Button("Host A Game",buttonSize)) {
    //       m_gameType = game_engine::Engine::Host;
    //       m_gameState.currentView = UIManager::GameView::Playing;
    //     }
    //     if (ImGui::Button("Join A Game", buttonSize)) {
    //       m_gameType = game_engine::Engine::Client;
    //       m_gameState.currentView = UIManager::GameView::Playing;
    //     }
    //     if (ImGui::Button("Back to Menu", buttonSize)) {
    //       // todo; should reset game state unless saved
    //       m_gameState.currentView = UIManager::GameView::MainMenu;
    //     }
    //     ImGui::End();
    //     break;
    //   }
    //   case UIManager::GameView::Playing:
    //   {
    //     if (m_gameState.drawMenuSettingsDuringGameplay(buttonSize)) {
    //        asyncSwitchToLevel(m_resources.m_currLevelIdx);
    //     }
    //     m_gameState.drawPlayerHealthBar();
    //     break;
    //   }
    // }

    return false;
}

// update updates the state of the passed in game object every render loop
void game_engine::Engine::updateGameObject(GameObject &obj, float deltaTime) {

  EntityResources entityRes = m_resources.m_currLevel->texCharacterMap[obj.spriteType];

  if (obj.currentAnimation != -1) {
    obj.animations[obj.currentAnimation].step(deltaTime);
  }

  // gravity applied globally; downward y force when not grounded
  if (obj.dynamic && !obj.grounded) {
    // increase downward velocity = acc*deltaTime every frame
    obj.velocity += GRAVITY * deltaTime;
  }

  // const auto widenColliderForSwing = [&](GameObject& o, float currDirection) {
  //   if (obj.objClass == ObjectClass::Player && obj.data.player.state == PlayerState::swingWeapon) {

  //     // const float drawW = o.spritePixelW / o.drawScale;
  //     // const float extra  = 0.2f * drawW;

  //     // SDL_FRect c = baseFacing(o);
  //     // c.w += extra;
  //     // if (currDirection < 0) c.x -= extra; // extend to the left
  //     // o.collider = c;

  //     const float drawW = o.spritePixelW / o.drawScale;
  //     const float extra  = 0.2f * drawW;           // how much to extend the sword
  //     const auto& base   = o.baseCollider;

  //     o.collider = base;
  //     o.collider.w = base.w + extra;
  //     if (currDirection < 0) {
  //         // facing left: shift left so the leading edge moves outward
  //         o.collider.x = base.x + extra;
  //     } else {
  //         // facing right: leave x as base; extension goes to the right
  //         o.collider.x = base.x;
  //     }
  //   }
  // };
  const auto baseFacing = [&](const GameObject& o) {
      SDL_FRect c = o.baseCollider;
      if (o.direction < 0) {
          float drawW = o.spritePixelW / o.drawScale;
          c.x = drawW - (c.x + c.w); // mirror the base for left
      }
      return c;
  };

  const auto widenColliderForSwing = [&](GameObject& o) {
      const float drawW = o.spritePixelW / o.drawScale;
      const float extra = 0.5f * drawW;

      SDL_FRect c = baseFacing(o);
      c.w += extra;
      if (o.direction < 0) c.x -= extra; // extend left
      o.collider = c;
  };

  float currDirection = 0;
  if (obj.objClass == ObjectClass::Player) {

    // update direction
    if (m_sdlState.keys[SDL_SCANCODE_LEFT]) {
      currDirection += -1;
    }
    if (m_sdlState.keys[SDL_SCANCODE_RIGHT]) {
      currDirection += 1;
    }

    Timer &weaponTimer = obj.data.player.weaponTimer;
    weaponTimer.step(deltaTime);

    const auto handleAttacking = [this, &obj, &entityRes, &weaponTimer, &currDirection, deltaTime, widenColliderForSwing](
      SDL_Texture *tex, SDL_Texture *attackTex, int animIndex, int attackAnimIndex, bool handleJump){


        if (m_sdlState.keys[SDL_SCANCODE_S] && obj.data.player.state != PlayerState::swingWeapon) {

          obj.texture = attackTex;
          obj.currentAnimation = attackAnimIndex;
          obj.animations[attackAnimIndex].reset();
          obj.data.player.state = PlayerState::swingWeapon;
          widenColliderForSwing(obj);
          MIX_PlayAudio(m_resources.mixer, m_resources.audioSword1);

        } else if (m_sdlState.keys[SDL_SCANCODE_A]) {

          obj.texture = attackTex;
          obj.currentAnimation = attackAnimIndex;

          if (obj.animations[attackAnimIndex].currentFrame() == 4) {
            // obj.animations[shootAnimIndex].freezeAtFrame();
            // obj.currentAnimation = -1;   // use spriteFrame path
            // obj.spriteFrame      = 4;

          } else {
            obj.currentAnimation = attackAnimIndex;
          }


          m_resources.whooshCooldown.step(deltaTime); // whooshCooldown should have same length as bullet weaponTimer
          // When you shoot (no loops, no track index needed):
          if (m_resources.whooshCooldown.isTimedOut()) {
            m_resources.whooshCooldown.reset();
            MIX_PlayAudio(m_resources.mixer, m_resources.audioShoot);
          }

          if (weaponTimer.isTimedOut()) {
            weaponTimer.reset();
            // create bullets
            GameObject bullet(128, 128);
            bullet.drawScale = 2.0f;
            bullet.colliderNorm = { .x=0.0, .y=0.40, .w=0.5, .h=0.1 };
            bullet.applyScale();

            bullet.data.bullet = BulletData();
            bullet.objClass = ObjectClass::Bullet;
            bullet.direction = obj.direction;
            bullet.texture = m_resources.texBullet;
            bullet.currentAnimation = m_resources.ANIM_BULLET_MOVING;
            const int yJitter = 50;
            const float yVelocity = SDL_rand(yJitter) - yJitter / 1.5f;
            bullet.velocity = glm::vec2(
              obj.velocity.x + 200.0f,
              yVelocity
            ) * obj.direction;
            bullet.maxSpeedX = 1000.0f;
            bullet.animations = m_resources.bulletAnims;

            // adjust depending on direction faced; lerp
            const float left = -10;
            const float right = 50;
            const float t = (obj.direction + 1) / 2.0f; // 0 or 1 taking into account neg sign
            const float xOffset = left + right * t;
            bullet.position = glm::vec2(
              obj.position.x + xOffset,
              obj.position.y + (obj.spritePixelH/bullet.drawScale) / 3.0
            );

            bool foundInactive = false;
            for (int i = 0; i < m_gameState.bullets.size() && !foundInactive; i++) {
              if (m_gameState.bullets[i].data.bullet.state == BulletState::inactive) {
                foundInactive = true;
                m_gameState.bullets[i] = bullet;
              }
            }

            // only add new if no inactive found
            if (!foundInactive) {
              this->m_gameState.bullets.push_back(bullet); // push bullets so we can draw them
            }
          }


        } else if (handleJump) {
          if (obj.currentAnimation != m_resources.ANIM_JUMP &&
              obj.currentAnimation != -1) {
              obj.currentAnimation = m_resources.ANIM_JUMP;
              obj.animations[m_resources.ANIM_JUMP].reset();
              obj.texture = entityRes.texJump;
          }

          if (obj.currentAnimation == m_resources.ANIM_JUMP &&
              obj.animations[m_resources.ANIM_JUMP].isDone()) {
              obj.currentAnimation = -1;              // mark as finished so it won’t restart
          }

        } else {
        obj.animations[m_resources.ANIM_SHOOT].reset();
        obj.animations[m_resources.ANIM_SLIDE_SHOOT].reset();
        // obj.animations[shootAnimIndex].unfreezeAnim();
        // and then we need to set freezeAtFrame() when .currentFrame() == 4
        // and unfreezeAnim when SDL_SCANCODE_A is no longer being pressed and set obj.currentAnim accordingly
        obj.texture = tex;
        obj.currentAnimation = animIndex;
      }
    };

    const bool wantSwing = m_sdlState.keys[SDL_SCANCODE_S];
    const bool canSwing  = (obj.data.player.state != PlayerState::swingWeapon);
    // update animation state
    switch (obj.data.player.state) {
      case PlayerState::idle:
      {
        obj.collider = baseFacing(obj);
        if (currDirection != 0) {
          obj.data.player.state = PlayerState::running;
          obj.texture = entityRes.texRun;
          obj.currentAnimation = m_resources.ANIM_RUN;
        } else {
          // decelerate faster than we speed up
          if (obj.velocity.x) {
            const float factor = obj.velocity.x > 0 ? -1.5f : 1.5f;
            float amount = factor * obj.acceleration.x * deltaTime;
            if (std::abs(obj.velocity.x) < std::abs(amount)) {
              obj.velocity.x = 0;
            } else {
              obj.velocity.x += amount;
            }
          }
        }

        if (wantSwing && canSwing) {
            handleAttacking(entityRes.texRun, entityRes.texRunAttack, m_resources.ANIM_RUN, m_resources.ANIM_RUN_ATTACK, false);
        } else {
            handleAttacking(entityRes.texIdle, entityRes.texShoot, m_resources.ANIM_IDLE, m_resources.ANIM_SHOOT, false);
        }

        break;
      }
      case PlayerState::hurt: {
        if (obj.data.player.damageTimer.step(deltaTime)) {
          obj.data.player.state = PlayerState::idle;
          obj.texture = entityRes.texIdle;
          obj.currentAnimation = m_resources.ANIM_IDLE;
          obj.data.player.damageTimer.reset();
        }
        break;
      }
      case PlayerState::dead:
      {
        if (obj.currentAnimation != -1 && obj.animations[obj.currentAnimation].isDone()) {
          obj.currentAnimation = -1; // prevent animation from looping after death
          obj.spriteFrame = 4;

          m_gameState.currentView = UIManager::GameView::GameOver;
          setGameOverSoundtrack();
        }
        break;
      }
      case PlayerState::running:
      {
        if (currDirection == 0) {
          obj.data.player.state = PlayerState::idle;
        }

        m_resources.stepAudioCooldown.step(deltaTime);
        if (m_resources.stepAudioCooldown.isTimedOut()) {
          m_resources.stepAudioCooldown.reset();
          MIX_PlayAudio(m_resources.mixer, m_resources.m_currLevel->audioStep);
        }

        // move in opposite dir of velocity, sliding
        if (obj.velocity.x * obj.direction < 0 && obj.grounded) {
          handleAttacking(entityRes.texSlide, entityRes.texSlideShoot, m_resources.ANIM_SLIDE, m_resources.ANIM_SLIDE_SHOOT, false);
        } else {
          if (wantSwing && canSwing) {
            handleAttacking(entityRes.texRun, entityRes.texRunAttack, m_resources.ANIM_RUN, m_resources.ANIM_RUN_ATTACK, false);
          } else {
            handleAttacking(entityRes.texRun, entityRes.texRunShoot, m_resources.ANIM_RUN, m_resources.ANIM_RUN, false);
          }
        }

        break;
      }
      case PlayerState::jumping:
      {

        if (!obj.data.player.jumpImpulseApplied) {
          obj.data.player.jumpWindupTimer.step(deltaTime);
          if (obj.data.player.jumpWindupTimer.isTimedOut()) {
              obj.velocity.y += JUMP_FORCE;   // upward impulse
              obj.data.player.jumpImpulseApplied = true;
              MIX_PlayAudio(m_resources.mixer, m_resources.audioJump);
          }
        } else {
          int n = obj.animations[m_resources.ANIM_JUMP].getFrameCount(); // e.g. 6 frames -> indices 0..5

          // Airborne: hold second‑to‑last frame once reached
          if (!obj.grounded && obj.currentAnimation == m_resources.ANIM_JUMP) {
              if (obj.animations[m_resources.ANIM_JUMP].currentFrame() >= n - 2) {
                  obj.currentAnimation = -1;           // stop timered anim
                  obj.spriteFrame = (n - 2) + 1;       // freeze on frame n-2 (1‑based)
                  obj.data.player.playLandingFrame = true;
              }
          }

          if (obj.grounded) {
              if (obj.data.player.playLandingFrame) {
                  obj.currentAnimation = -1;           // show landing frame once
                  obj.spriteFrame = (n - 1) + 1;       // last frame (1‑based)
                  obj.data.player.playLandingFrame = false;
                  break;                               // render this frame; state stays jumping this tick
              } else {
                  obj.velocity.y = 0;
                  obj.data.player.state = PlayerState::idle; // or running
                  obj.animations[m_resources.ANIM_JUMP].reset();
              }
          }
        }

        if (wantSwing && canSwing) {
          handleAttacking(entityRes.texRun, entityRes.texRunAttack, m_resources.ANIM_RUN, m_resources.ANIM_RUN_ATTACK, false);
        } else {
          handleAttacking(entityRes.texJump, entityRes.texRunShoot, m_resources.ANIM_JUMP, m_resources.ANIM_JUMP, true);
        }
        break;
      }
      case PlayerState::swingWeapon: { // handle swinging weapon like handleShooting
        // sets to idle immediately in next loop even when currentAnimation=10
        if (obj.currentAnimation == m_resources.ANIM_RUN_ATTACK &&
            obj.animations[m_resources.ANIM_RUN_ATTACK].isDone()) {

            obj.collider = baseFacing(obj);

            obj.data.player.state = PlayerState::idle;
            obj.texture = entityRes.texIdle;
            obj.currentAnimation = m_resources.ANIM_IDLE;
            obj.animations[m_resources.ANIM_RUN_ATTACK].reset();
            obj.animations[m_resources.ANIM_IDLE].reset();
        } else if (obj.currentAnimation == m_resources.ANIM_SWING &&
                  obj.animations[m_resources.ANIM_SWING].isDone()) {
            obj.data.player.state = PlayerState::idle;
            obj.collider = baseFacing(obj);
            obj.texture = entityRes.texIdle;
            obj.currentAnimation = m_resources.ANIM_IDLE;
            obj.animations[m_resources.ANIM_SWING].reset();
            obj.animations[m_resources.ANIM_IDLE].reset();
        }

        break;
      }
    }
  } else if (obj.objClass == ObjectClass::Bullet) {

    obj.data.bullet.liveTimer.step(deltaTime);
    switch (obj.data.bullet.state) {
      case BulletState::moving:
      {
        if (
          obj.position.x - m_gameState.mapViewport.x < 0 ||
          obj.position.x - m_gameState.mapViewport.x > m_sdlState.logW ||
          obj.position.y - m_gameState.mapViewport.y < 0 ||
          obj.position.y - m_gameState.mapViewport.y > m_sdlState.logH ||
          obj.data.bullet.liveTimer.isTimedOut()
        ) {
            obj.data.bullet.liveTimer.reset();
            // obj.position.x = 0; obj.position.y = 0;
            obj.data.bullet.state = BulletState::inactive;
        }
        break;
      }
      case BulletState::colliding:
      {
        if (obj.animations[obj.currentAnimation].isDone()) {
          obj.data.bullet.state = BulletState::inactive;
        }
        break;
      }
    }
  } else if (obj.objClass == ObjectClass::Enemy) {

    EntityResources entityRes = m_resources.m_currLevel->texCharacterMap[obj.spriteType];
    switch (obj.data.enemy.state) {
      case EnemyState::idle:
      {
        glm::vec2 distToPlayer = this->getPlayer().position - obj.position;
        if (glm::length(distToPlayer) < 100) {
          // face the enemy towards the player
          currDirection = 1;
          if (distToPlayer.x < 0) {
            currDirection = -1;
          };
          obj.acceleration = glm::vec2(30, 0);
          obj.texture = entityRes.texRun;


          // step the attack timer here
          // if the timer is done switch state to attack
          if (obj.data.enemy.attackTimer.step(deltaTime)) {
            obj.data.enemy.state = EnemyState::attack;
            obj.texture = entityRes.texAttack;
            obj.currentAnimation = m_resources.ANIM_SWING;
            obj.data.enemy.attackTimer.reset();
          }


        } else {
          // stop them from moving when too far away
          obj.acceleration = glm::vec2(0);
          obj.velocity.x = 0;
          obj.texture = entityRes.texIdle;
        }

        break;
      }
      case EnemyState::attack:
      {
        if (obj.data.enemy.idleTimer.step(deltaTime)) {
          obj.data.enemy.state = EnemyState::idle;
          obj.texture = entityRes.texIdle;
          obj.currentAnimation = m_resources.ANIM_IDLE;
          obj.data.enemy.idleTimer.reset();
        }
      }
      case EnemyState::hurt:
      {
        if (obj.data.enemy.damageTimer.step(deltaTime)) {
          obj.data.enemy.state = EnemyState::idle;
          obj.texture = entityRes.texIdle;
          obj.currentAnimation = m_resources.ANIM_IDLE;
          obj.data.enemy.damageTimer.reset();
        }
        break;
      }
      case EnemyState::dead:
      {
        obj.velocity.x = 0;
        if (obj.currentAnimation != -1 && obj.animations[obj.currentAnimation].isDone()) {
          // stop animations for dead enemy
          obj.currentAnimation = -1;
          obj.spriteFrame = 18; // TODO this is because enemy has 18 frames
        }
        break;
      }
    }
  }

  if (currDirection && obj.direction != currDirection) {
    // Direction changed - mirror the collision box offset to match flipped character
    // and adjust position to keep collision box at same world position
    float drawW = obj.spritePixelW / obj.drawScale;
    float oldColliderX = obj.collider.x;
    float newColliderX = drawW - obj.collider.x - obj.collider.w;
    obj.collider.x = newColliderX;
    float positionShift = newColliderX - oldColliderX;
    obj.position.x -= positionShift; // Keep world position constant

    // If this is the player, adjust camera to prevent screen jump
    // if (obj.objClass == ObjectClass::Player) {
    //   // m_gameState.mapViewport.x -= positionShift/2;
    // }

    obj.direction = currDirection;
  } else if (currDirection) {
    obj.direction = currDirection;
  }
  // update velocity based on currDirection (which way we're facing),
  // acceleration and deltaTime
  obj.velocity += currDirection * obj.acceleration * deltaTime;
  if (std::abs(obj.velocity.x) > obj.maxSpeedX) { // cap the max velocity
    obj.velocity.x = currDirection * obj.maxSpeedX;
  }
  // update position based on velocity
  obj.position += obj.velocity * deltaTime;

  // handle collision detection
  bool foundGround = false;
  for (auto &layer : m_gameState.layers) {
    for (GameObject &objB: layer){
      // if (obj.type == ObjectType::enemy) {
      //   std::cout << "found Ground" << foundGround << std::endl;
      // }
      if (&obj != &objB && objB.collider.h != 0 && objB.collider.w != 0) {
        this->handleCollision(obj, objB, deltaTime);

        // update ground sensor only when landing on level tiles
        if (objB.objClass == ObjectClass::Level) {
          SDL_FRect sensor{
            .x = obj.position.x + obj.collider.x,
            .y = obj.position.y + obj.collider.y + obj.collider.h,
            .w = obj.collider.w, .h = 1
          };

          SDL_FRect rectB{
            .x = objB.position.x + objB.collider.x,
            .y = objB.position.y + objB.collider.y,
            .w = objB.collider.w, .h = objB.collider.h
          };

          SDL_FRect dummyRectC{0};

          if (SDL_GetRectIntersectionFloat(&sensor, &rectB, &dummyRectC)) {
            foundGround = true;
          }
        }

      }
    }
  }

  if (obj.grounded != foundGround) {
    // switching grounded state
    obj.grounded = foundGround;
    if (foundGround && obj.objClass == ObjectClass::Player && !obj.data.player.playLandingFrame) {
        obj.data.player.state = PlayerState::running;

        if (obj.grounded && obj.data.player.jumpImpulseApplied) {
          obj.data.player.state = PlayerState::idle;
          obj.data.player.jumpImpulseApplied = false;
          obj.data.player.jumpWindupTimer.reset();
        }
      }

  }
}

/*
 collisionResponse will dictates what you want to happen given a collision has been detected
 The defaultResponse handles vertical and horizontal collisions by preventing sprites from overlapping due to collision
*/
void game_engine::Engine::collisionResponse(const SDL_FRect &rectA, const SDL_FRect &rectB, const SDL_FRect &rectC, GameObject &objA, GameObject &objB, float deltaTime) {

  // logRectEvery(rectC, 100000);
  const auto defaultResponse = [&]() {
    if (rectC.w < rectC.h) {
      // horizontal collision
      if (objA.velocity.x > 0) {
        // traveling to right, colliding object must be to the right, so sub .w; need extra 0.1 to escape collision for next frame
        objA.position.x -= rectC.w+0.1;
      } else if (objA.velocity.x < 0) {
        objA.position.x += rectC.w+0.1;
      }
      objA.velocity.x = 0; // reset velocity to 0 so object stops
    } else {
      //vertical collison
      if (objA.velocity.y > 0) {
        objA.position.y -= rectC.h; // down
      } else if (objA.velocity.y < 0) {
        objA.position.y += rectC.h; // up
      }
      objA.velocity.y = 0;
    }
  };

  if (objA.objClass == ObjectClass::Player) {
    switch (objB.objClass) {
      case ObjectClass::Level:
      {
        if (objB.data.level.isHazard) {
            EntityResources entityRes = m_resources.m_currLevel->texCharacterMap.at(objA.spriteType);
            PlayerData &d = objA.data.player;
            if (d.state != PlayerState::dead) {
              // objA.direction = -1 * objA.direction;
              objA.position.y -= rectC.h; // up
              objA.shouldFlash = true;
              objA.flashTimer.reset();
              objA.texture = entityRes.texHit;
              objA.currentAnimation = m_resources.ANIM_HIT;
              d.state = PlayerState::hurt;

              // damage and flag dead
              if (!d.damageTimer.isTimedOut()) {
                d.healthPoints -= 50.0;
              }
              if (d.healthPoints <= 0) {
                d.state = PlayerState::dead;
                objA.texture = entityRes.texDie;
                objA.currentAnimation = m_resources.ANIM_DIE;
                MIX_PlayTrack(m_resources.enemyDieTrack, 0);
                objA.velocity = glm::vec2(0);
                // m_gameState.currentView = GameView::GameOver;
              }
              MIX_PlayTrack(m_resources.enemyProjectileHitTrack, 0);
            }

        } else {
          defaultResponse();
        }
        break;
      }
      case ObjectClass::Enemy:
      {
        if (objB.data.enemy.state != EnemyState::dead) {


          if (objA.data.player.state == PlayerState::swingWeapon) {
            // when swinging weapon and colliding, reduce enemy health
            EntityResources entityRes = m_resources.m_currLevel->texCharacterMap.at(objB.spriteType);
            EnemyData &d = objB.data.enemy;

            // function damageObject(state, damageAmount, entityRes, dieTrack, hitTrack)
            if (d.state != EnemyState::dead) {
              objB.direction = -1 * objA.direction;
              objB.shouldFlash = true;
              objB.flashTimer.reset();
              objB.texture = entityRes.texHit;
              objB.currentAnimation = m_resources.ANIM_HIT;
              d.state = EnemyState::hurt;

              // damage and flag dead
              d.healthPoints -= 10;
              if (d.healthPoints <= 0) {
                d.state = EnemyState::dead;
                objB.texture = entityRes.texDie;
                objB.currentAnimation = m_resources.ANIM_DIE;
                MIX_PlayAudio(m_resources.mixer, m_resources.audioEnemyDie);
              }
              // MIX_PlayAudio(m_resources.mixer, m_resources.audioBoneImpact);
              MIX_PlayTrack(m_resources.boneImpactHitTrack, 0);
              // TODO wrap this in a func() so that it doesnt allow more than one sound to occur until its done.
              // PlayTrack doesnt allow repeating while the track is already playing so might be better to use it instead actually.
            }
          } else {
            // push back player
            objA.velocity = glm::vec2(50, 0) * - objA.direction;
          }
        }
        break;
      }
      case ObjectClass::Portal:
      {
        asyncSwitchToLevel(objB.data.portal.nextLevel);
        break;
      }
      case ObjectClass::Player:
      {
        break;
      }
    }
  } else if (objA.objClass == ObjectClass::Bullet) {

    bool passthrough = false;
    switch (objA.data.bullet.state) {
      case BulletState::moving:
      {
        switch (objB.objClass) {
          case ObjectClass::Level:
          {
            if (!MIX_PlayTrack(m_resources.hitTrack, 0)) {
            // SDL_Log("Play failed: %s", SDL_GetError());
            };
            // if (!MIX_PlayAudio(m_resources.mixer, m_resources.audioShootHit)) {
            // // SDL_Log("Play failed: %s", SDL_GetError());
            // };

            break;
          }
          case ObjectClass::Enemy:
          {

            EntityResources entityRes = m_resources.m_currLevel->texCharacterMap.at(objB.spriteType);
            EnemyData &d = objB.data.enemy;

            // function damageObject(state, damageAmount, entityRes, dieTrack, hitTrack)
            if (d.state != EnemyState::dead) {
              objB.direction = -1 * objA.direction;
              objB.shouldFlash = true;
              objB.flashTimer.reset();
              objB.texture = entityRes.texHit;
              objB.currentAnimation = m_resources.ANIM_HIT;
              d.state = EnemyState::hurt;

              // damage and flag dead
              d.healthPoints -= 10;
              if (d.healthPoints <= 0) {
                d.state = EnemyState::dead;
                objB.texture = entityRes.texDie;
                objB.currentAnimation = m_resources.ANIM_DIE;
                MIX_PlayAudio(m_resources.mixer, m_resources.audioEnemyDie);
              }
              // MIX_PlayAudio(m_resources.mixer, m_resources.audioProjectileEnemyHit);
              MIX_PlayTrack(m_resources.enemyProjectileHitTrack, 0);
            } else {
              passthrough = true;
            }
            break;
          }
          case ObjectClass::Player:
          {
             passthrough = true;
          }
        }

        if (!passthrough) {
          defaultResponse();
          objA.velocity *= 0;
          objA.data.bullet.state = BulletState::colliding;
          objA.texture = m_resources.texBulletHit;
          objA.currentAnimation = m_resources.ANIM_BULLET_HIT;
        }
        break;
      }
    }

  }
  else if (objA.objClass == ObjectClass::Enemy) {
    switch (objB.objClass) {
      case ObjectClass::Player:
      {
        if (objA.data.enemy.state == EnemyState::attack) {
          EntityResources playerRes = m_resources.m_currLevel->texCharacterMap.at(objB.spriteType);
            PlayerData &playerData = objB.data.player;
            if (playerData.state != PlayerState::dead) {
              // objA.direction = -1 * objA.direction;
              // objB.position.y -= rectC.h; // up
              objB.shouldFlash = true;
              objB.flashTimer.reset();
              objB.texture = playerRes.texHit;
              objB.currentAnimation = m_resources.ANIM_HIT;
              playerData.state = PlayerState::hurt;

              // damage and flag dead
              if (!playerData.damageTimer.isTimedOut()) {
                playerData.healthPoints -= 10.0;
              }
              if (playerData.healthPoints <= 0) {
                playerData.state = PlayerState::dead;
                objB.texture = playerRes.texDie;
                objB.currentAnimation = m_resources.ANIM_DIE;
                MIX_PlayAudio(m_resources.mixer, m_resources.audioEnemyDie);
                objB.velocity = glm::vec2(0);
                // m_gameState.currentView = GameView::GameOver;
              }
              // MIX_PlayAudio(m_resources.mixer, m_resources.audioProjectileEnemyHit);
              MIX_PlayTrack(m_resources.enemyProjectileHitTrack, 0);
            }
        }
        // if collision if with player and enemy is swinging
        // then reduce players health and do damage
        break;
      }
      case ObjectClass::Level:
      {
        if (objB.data.level.isHazard) {
            EntityResources entityRes = m_resources.m_currLevel->texCharacterMap.at(objA.spriteType);
            EnemyData &d = objA.data.enemy;
            if (d.state != EnemyState::dead) {
              // objA.direction = -1 * objA.direction;
              objA.position.y -= rectC.h; // up
              objA.shouldFlash = true;
              objA.flashTimer.reset();
              objA.texture = entityRes.texHit;
              objA.currentAnimation = m_resources.ANIM_HIT;
              d.state = EnemyState::hurt;

              // damage and flag dead
              if (!d.damageTimer.isTimedOut()) {
                d.healthPoints -= 50.0;
              }
              if (d.healthPoints <= 0) {
                d.state = EnemyState::dead;
                objA.texture = entityRes.texDie;
                objA.currentAnimation = m_resources.ANIM_DIE;
                MIX_PlayAudio(m_resources.mixer, m_resources.audioEnemyDie);
                objA.velocity = glm::vec2(0);
                // m_gameState.currentView = GameView::GameOver;
              }
              // MIX_PlayAudio(m_resources.mixer, m_resources.audioProjectileEnemyHit);
              MIX_PlayTrack(m_resources.enemyProjectileHitTrack, 0);
            }

        } else {
          defaultResponse();
        }
        break;
      }
      case ObjectClass::Enemy:
      {
        if (objB.data.enemy.state != EnemyState::dead) {
          objA.velocity = glm::vec2(50, 0) * - objA.direction;
        }
        break;
      }
    }
  }

}

void game_engine::Engine::handleCollision(GameObject &a, GameObject &b, float deltaTime) {

  SDL_FRect rectA{
    .x = a.position.x + a.collider.x,
    .y = a.position.y + a.collider.y,
    .w = a.collider.w,
    .h = a.collider.h
  };
  SDL_FRect rectB{
    .x = b.position.x + b.collider.x,
    .y = b.position.y + b.collider.y,
    .w = b.collider.w,
    .h = b.collider.h
  };
  SDL_FRect rectC{ 0 };

  if (SDL_GetRectIntersectionFloat(&rectA, &rectB, &rectC)) {
    // found interection
    this->collisionResponse(rectA, rectB, rectC, a, b, deltaTime);
  };
};

bool game_engine::Engine::initAllTiles(GameState &newGameState) {

  struct LayerVisitor
  {

    const SDLState &state;
    GameState &gs;
    Resources &res;
    int countModColliders = 0;

    LayerVisitor(const SDLState &state, GameState &gs, Resources &res): state(state), gs(gs), res(res){}

    const tmx::TileSet* pickTileset(uint32_t gid) {
      const tmx::TileSet* match = nullptr;
      for (const auto& ts : res.m_currLevel->map->tileSets) {
        if (gid >= (uint32_t)ts.firstgid) match = &ts;
        else break;
      }
      return match; // assumes sets are sorted by firstGid
    }

    const GameObject createObject(int r, int c, SDL_Texture *tex, ObjectClass type, float spriteH, float spriteW, int srcX, int srcY) {
      GameObject o(spriteH, spriteW);
      o.objClass = type;
      // o.position = glm::vec2(c * TILE_SIZE, state.logH - (20 - r) * TILE_SIZE);
      o.texture = tex;

      // default collider for level objects
      // TODO I need to define a specific collider for each type of level tile
      o.collider = {
        .x = 0,
        .y = 0, // update collider x.y position to  detemine how much overlap is allowed between objects
        .w = spriteW,
        .h = spriteH
      };

      if (type == ObjectClass::Level || type == ObjectClass::Portal) {
        o.position = { c * spriteW, r * spriteH };
        // o.position = glm::vec2(c * TILE_SIZE, state.logH - (20 - r) * TILE_SIZE);
        // pick out the exact tile from the tilesheet
        o.data.level.src = SDL_FRect{
          .x = static_cast<float>(srcX),
          .y = static_cast<float>(srcY),
          .w = static_cast<float>(spriteW),
          .h = static_cast<float>(spriteH)
        };
        o.data.level.dst = SDL_FRect{
          .x = static_cast<float>(c) * spriteW,
          .y = static_cast<float>(r) * spriteH,
          .w = static_cast<float>(spriteW),
          .h = static_cast<float>(spriteH)
        };

      }
      return o;
    };

    // for each std::varient thats in tileSet
    void operator()(tmx::Layer &layer)
    {
      std::vector<GameObject> newLayer;

      if (!layer.img.has_value()) {
        for (int r = 0; r < res.m_currLevel->map->mapHeight; ++r){

          for (int c = 0; c < res.m_currLevel->map->mapWidth; ++c) {
            const uint32_t rawGid = layer.data[r * res.m_currLevel->map->mapWidth + c]; // packed Tiled GID (includes flip flags)

            // Tiled encodes flips in the top 3 bits; mask them off so we lookup the real tile index.
            // Without this the computed srcY can overflow the texture height, causing tiles to vanish.
            const uint32_t gid = rawGid & 0x1FFFFFFF; // clear H/V/diag flip flags
            if (gid) {

              // find the texture corresponding to this gID
              // const auto itr = std::find_if(res.map->tileSets.begin(), res.map->tileSets.end(),
              // [tGid](const tmx::TileSet &ts) {
              //   return tGid >= ts.firstgid && tGid < ts.firstgid + ts.texture.size();
              // });
              const tmx::TileSet* ts = pickTileset(gid);

              if (!ts) continue;

              uint32_t localId = rawGid - ts->firstgid; // local index within the tileSet data array
              int srcX = (localId % ts->columns) * ts->tileWidth; // col * tileWidth;
              int srcY = (localId / ts->columns) * ts->tileHeight; // row * tileHeight

              bool isHazard = false;
              if (layer.name == "Hazard") {
                isHazard = true;
              }

              auto tile = createObject(r, c, ts->texture, ObjectClass::Level, ts->tileHeight, ts->tileWidth, srcX, srcY);

              if (layer.name != "Level") {
                tile.collider.w = tile.collider.h = 0;
              }

              if (layer.name == "Level" || isHazard) { // 4616 firstgid of Tileset
                if (auto it = ts->tiles.find(localId); it != ts->tiles.end() && it->second.collider) {
                  tile.collider = *(it->second.collider);
                  tile.data.level.isHazard = isHazard;
                  countModColliders += 1;
                };

              };

              newLayer.push_back(tile);

            }

          }
        }
      } else if (layer.name == "Background_4") { // 4
        auto bgImg = createObject(0, 0, res.m_currLevel->map->tileSets[res.m_currLevel->bg4Idx].texture, ObjectClass::Background, 0, 0, 0, 0);
        bgImg.bgscroll = 0;
        bgImg.scrollFactor = 0.2f;
        newLayer.push_back(bgImg);
      } else if (layer.name == "Background_3") { // 3
        auto bgImg = createObject(0, 0, res.m_currLevel->map->tileSets[res.m_currLevel->bg3Idx].texture, ObjectClass::Background, 0, 0, 0, 0);
        bgImg.bgscroll = 0;
        bgImg.scrollFactor = 0.2f;
        newLayer.push_back(bgImg);
      } else if (layer.name == "Background_2") { // 2
        auto bgImg = createObject(0, 0, res.m_currLevel->map->tileSets[res.m_currLevel->bg2Idx].texture, ObjectClass::Background, 0, 0, 0, 0);
        bgImg.bgscroll = 0;
        bgImg.scrollFactor = 0.3f;
        newLayer.push_back(bgImg);
      } else if (layer.name == "Background_1") { // 1
        auto bgImg = createObject(0, 0, res.m_currLevel->map->tileSets[res.m_currLevel->bg1Idx].texture, ObjectClass::Background, 0, 0, 0, 0);
        bgImg.bgscroll = 0;
        bgImg.scrollFactor = 0.4f;
        newLayer.push_back(bgImg);
      }

      gs.layers.push_back(std::move(newLayer));
    };

    void operator()(tmx::ObjectGroup &objectGroup)
    {
      std::vector<GameObject> newLayer;
      for (tmx::LayerObject &obj : objectGroup.objects)
      {
        glm::vec2 objStartingPos(
          obj.x - res.m_currLevel->map->tileWidth / 2, //17
          obj.y - res.m_currLevel->map->tileHeight / 2 //411
        );

        if (obj.type == "Portal") {
            GameObject portal = createObject(1, 1, nullptr, ObjectClass::Portal, 32, 32, 0, 0); // todo populate w/h passing from tmx file

            LevelIndex lvl = LevelIndex::LEVEL_2;
            if (res.m_currLevel->lvlIdx == LevelIndex::LEVEL_1) {
              lvl = LevelIndex::LEVEL_2;
            }
            if (res.m_currLevel->lvlIdx == LevelIndex::LEVEL_2) {
              lvl = LevelIndex::LEVEL_3;
            }
            portal.data.portal = PortalData(lvl);
            portal.colliderNorm = { .x=0.0, .y=0.5, .w=1.0, .h=1.0}; // TODO setting .y = 0.5 and h = 0.5 worked well for level2
            portal.applyScale();
            portal.position = objStartingPos;

            newLayer.push_back(std::move(portal));
        }


        if (obj.type == "Enemy") {

          SpriteType spriteType = CHARACTER_NAME_TO_SPRITE_TYPE.at(obj.name);
          GameObject enemy = createObject(1, 1, res.m_currLevel->texCharacterMap.at(spriteType).texIdle, ObjectClass::Enemy, 128, 128, 0, 0);
          enemy.spriteType = spriteType;
          // set the appropriate texture based on the obj.name

          // enemy.data.enemy.srcW = 128; // unsued
          // enemy.data.enemy.srcH = 128;// unsued
          switch (spriteType) {
            case SpriteType::Minotaur_1:
            {
              enemy.drawScale = 2.0f;
              break;
            }
            case SpriteType::Skeleton_Warrior:
            case SpriteType::Red_Werewolf:
            case SpriteType::Skeleton_Pikeman:
            {
              enemy.drawScale = 1.5f;
              break;
            }
          }
          float wFrac = 0.30f, hFrac = 0.6f;
          enemy.colliderNorm = { .x=0.35f, .y=0.4, .w=wFrac, .h=0.6}; // TODO setting .y = 0.5 and h = 0.5 worked well for level2
          enemy.applyScale();

          float feetY   = objStartingPos.y;                // baseline from Tiled
          float centerX = objStartingPos.x;                // baseline from Tiled
          enemy.position.x = centerX - enemy.collider.w * 0.5f;        // collider centered
          enemy.position.y = feetY - (enemy.collider.y + enemy.collider.h); // collider bottom on feet
          enemy.data.enemy = EnemyData();
          enemy.currentAnimation = res.ANIM_IDLE;
          enemy.animations = res.m_currLevel->texCharacterMap.at(spriteType).anims;
          enemy.dynamic = true;
          enemy.maxSpeedX = 15;
          newLayer.push_back(std::move(enemy));
        }

        // Must handle multiple players here; all players start in same position, so here we create a player
        if (obj.type == "Player") {
          SpriteType spriteType = CHARACTER_NAME_TO_SPRITE_TYPE.at(obj.name);
          int texDim = 128;
          // if (spriteType == SpriteType::Player_Marie) {
          //   texDim = 96;
          // }

          GameObject player = createObject(1, 1, res.m_currLevel->texCharacterMap.at(spriteType).texIdle, ObjectClass::Player, texDim, texDim, 0, 0); // NEED TO PASS DOWN CHARACTER TILE SIXES
          // Marie: 96
          // Knight: Mage: 128
          player.spriteType = spriteType;
          player.drawScale = 1.5f;

          float wFrac = 0.30f, hFrac = 0.40f;
          // Position collision box to overlay on character (character is left of center in sprite)
          // adjust
          player.colliderNorm = { .x=0.10f, .y=0.9f - hFrac, .w=wFrac, .h=hFrac };
          switch (spriteType) {
            case SpriteType::Player_Knight:
            {
              player.colliderNorm = { .x=0.1f, .y=0.5f, .w=wFrac, .h=0.5f };
              break;
            }
            case SpriteType::Player_Mage:
            {
              player.colliderNorm = { .x=0.30f, .y=0.5f, .w=wFrac, .h=0.5f };
              break;
            }
            case SpriteType::Player_Marie:
            {
              player.colliderNorm = { .x=0.30f, .y=0.5f, .w=wFrac, .h=0.5f };
              player.drawScale = 2.0f;
              break;
            }
          };

          player.applyScale();

          // TODO need a collider for attacks that get used during the attack state.

          float drawW = player.spritePixelW / player.drawScale;
          float drawH = player.spritePixelH / player.drawScale;

          // obj.x/obj.y from Tiled: treat as centerX/feetY
          float centerX = obj.x;
          float feetY   = obj.y;

          // place sprite top-left so its bottom is at feetY and center on centerX
          player.position.x = centerX - drawW * 0.5f;
          player.position.y = feetY   - drawH;

          player.data.player = PlayerData();
          player.animations = res.m_currLevel->texCharacterMap.at(spriteType).anims;
          player.currentAnimation = res.ANIM_IDLE;
          player.acceleration = glm::vec2(500, 0);
          player.maxSpeedX = 100;
          player.dynamic = true;

          newLayer.push_back(player);
          gs.playerIndex = newLayer.size() - 1;
          gs.playerLayer = gs.layers.size();
        }
      };
      gs.layers.push_back(std::move(newLayer));
    }
  };

  LayerVisitor visitor(m_sdlState, newGameState, m_resources);
  for (std::variant<tmx::Layer, tmx::ObjectGroup> &layer : m_resources.m_currLevel->map->layers) {
    std::visit(visitor, layer);
  };

  std::cout << "count:" << visitor.countModColliders << std::endl;


  return newGameState.playerIndex != -1;
};

void game_engine::Engine::handleKeyInput(GameObject &obj, SDL_Scancode key, bool keyDown, game_engine::NetGameInput &input) {

  // TODO send input msg to server

  if (obj.objClass == ObjectClass::Player) {
    switch (obj.data.player.state) {
      case PlayerState::idle:
      {
        if (key == SDL_SCANCODE_UP && obj.grounded) {
          // obj.velocity.y += JUMP_FORCE;
          obj.data.player.state = PlayerState::jumping;

          // obj.animations[m_resources.ANIM_JUMP].reset();

          input.move = PlayerInput::Jump;
          input.shouldSendMessage = true;
          // obj.animations[m_resources.ANIM_JUMP].reset();
          // obj.currentAnimation = m_resources.ANIM_JUMP;
          obj.data.player.jumpWindupTimer.reset(); // 100 ms delay (set duration in ctor)
          obj.data.player.jumpImpulseApplied = false;
        }
        break;
      }
      case PlayerState::jumping:
      {
          // While airborne: play jump anim, but clamp to frame n-2 once reached
          if (!obj.grounded && obj.currentAnimation == m_resources.ANIM_JUMP) {
              int n   = obj.animations[m_resources.ANIM_JUMP].getFrameCount();
              int cap = std::max(0, n - 2);
              if (obj.spriteFrame >= cap) {
                  obj.spriteFrame = cap;           // hold second-to-last frame
                  obj.data.player.playLandingFrame = true;
              }
          }

          if (obj.grounded) {
              if (obj.data.player.playLandingFrame) {
                  int n = obj.animations[m_resources.ANIM_JUMP].getFrameCount();
                  obj.currentAnimation = m_resources.ANIM_JUMP;
                  obj.spriteFrame = n - 1;         // show last frame on landing
                  obj.data.player.playLandingFrame = false;
                  break; // let this frame render the landing frame
              } else {
                  obj.velocity.y = 0;
                  obj.data.player.state = PlayerState::idle; // or running
                  obj.animations[m_resources.ANIM_JUMP].reset();
              }
          }
          break;
      }
      case PlayerState::running:
      {
        if (key == SDL_SCANCODE_UP && obj.grounded) {
          // obj.velocity.y += JUMP_FORCE;
          obj.data.player.state = PlayerState::jumping;

          // obj.animations[m_resources.ANIM_JUMP].reset();
          obj.data.player.jumpWindupTimer.reset(); // 100 ms delay (set duration in ctor)
          obj.data.player.jumpImpulseApplied = false;

          input.move = game_engine::PlayerInput::Jump;
          input.shouldSendMessage = true;
        }
        break;
      }
    }

  }



};

void game_engine::Engine::drawParalaxBackground(SDL_Texture* tex,
                                   float camVelX,
                                   float& scrollPos,
                                   float scrollFactor,
                                   float dt,
                                   float baseY = -175.0f) { // horoz/vert offsets should be from tilesheet

    scrollPos -= camVelX * scrollFactor * dt;
    auto scrollY = 0 * scrollFactor * dt;   // factorY ≈ 0 for sky

    float w = static_cast<float>(tex->w);
    scrollPos = std::fmod(scrollPos, w);
    if (scrollPos > 0) scrollPos -= w;

    SDL_FRect dst1{ scrollPos,        baseY + scrollY, w, (float)tex->h };
    SDL_FRect dst2{ scrollPos + w,    baseY + scrollY, w, (float)tex->h };
    // SDL_FRect dst1{ scrollPos,        baseY*1.5f + scrollY, w*1.5f, (float)tex->h*1.5f };
    // SDL_FRect dst2{ scrollPos + w,    baseY*1.5f + scrollY, w*1.5f, (float)tex->h*1.5f };
    SDL_RenderTexture(m_sdlState.renderer, tex, nullptr, &dst1);
    SDL_RenderTexture(m_sdlState.renderer, tex, nullptr, &dst2);
    // // advance
    // scrollPos -= cameraVelX * scrollFactor * dt;

    // // keep in [-w, 0)
    // float w = static_cast<float>(tex->w); // or query via SDL_GetTextureSize
    // scrollPos = std::fmod(scrollPos, w);
    // if (scrollPos > 0) scrollPos -= w;

    // SDL_FRect dst1{ scrollPos,       y, w, static_cast<float>(tex->h) };
    // SDL_FRect dst2{ scrollPos + w,   y, w, static_cast<float>(tex->h) };

    // SDL_RenderTexture(m_sdlState.renderer, tex, nullptr, &dst1);
    // SDL_RenderTexture(m_sdlState.renderer, tex, nullptr, &dst2);
}

// void game_engine::Engine::drawParalaxBackground(SDL_Texture *texture, float xVelocity, float &scrollPos, float scrollFactor, float deltaTime) {
//   scrollPos -= xVelocity * scrollFactor * deltaTime; // scroll position passed by reference, is updated every loop
//   float wrap = static_cast<float>(texture->w);
//   if (scrollPos <= -wrap || scrollPos >= wrap) {
//     scrollPos = 0;
//   }

//   SDL_FRect dst{
//     .x = scrollPos,
//     .y = -175.0f,
//     .w = texture->w * 2.0f,
//     .h = static_cast<float>(texture->h)
//   };

//   SDL_RenderTextureTiled(m_sdlState.renderer, texture, nullptr, 1.0, &dst);
// };
