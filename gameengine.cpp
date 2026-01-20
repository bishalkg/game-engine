#include "gameengine.h"
// #include "game_net_common.h"

GameObject &game_engine::Engine::getPlayer() {
 return m_gameState.player(LAYER_IDX_CHARACTERS);
};

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
  if (!MIX_TrackPlaying(m_resources.backgroundTrack)) {
    SDL_PropertiesID opts = SDL_CreateProperties();
    SDL_SetNumberProperty(opts, MIX_PROP_PLAY_LOOPS_NUMBER, -1); // loop forever
    if (!MIX_PlayTrack(m_resources.backgroundTrack, opts)) {
        SDL_Log("Background Music Play failed: %s", SDL_GetError());
    }
    SDL_DestroyProperties(opts); // destory internal resources
  }
};

void game_engine::Engine::stopBackgroundSoundtrack() {
  if (MIX_TrackPlaying(m_resources.backgroundTrack)) {
    if (!MIX_StopTrack(m_resources.backgroundTrack, 10)) {
        SDL_Log("stopping Background Music Play failed: %s", SDL_GetError());
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

  // load game assets
  m_resources.loadAllAssets(m_sdlState);
  if (!m_resources.texIdle) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Idle Texture load failed", "Failed to load idle image", nullptr);
    cleanup();
    return false;
  }

  // setup game data
  m_gameState = GameState(m_sdlState);
  // gs.mapViewport.x = 0;
  // gs.mapViewport.y = 0;
  // gs.mapViewport.w = state.logW; // or state.width/state.logW
  // gs.mapViewport.h = state.logH; // or state.height/state.logH
  return initAllTiles();
}


void game_engine::Engine::runGameLoop() {
    // start the game loop
  uint64_t prevTime = SDL_GetTicks();
  m_gameRunning.store(true);

  GameState &gs = getGameState();
  SDLState &sdl = getSDLState();
  Resources &res = getResources();

  while (m_gameRunning.load()){
    uint64_t nowTime = SDL_GetTicks();
    float deltaTime = (nowTime - prevTime) / 1000.0f; // convert to seconds; time bw frames

    // TODO the type of player is enemy????? i think we're following the wrong guy
    GameObject &player = getPlayer();  // fetch each frame in case index changes
    // use gs.currentView directly so state changes take effect immediately

    updateImGuiMenuRenderState();

    clearRenderer();

    if (!handleMultiplayerConnections()) {
      return;
    }

    // runEventLoop takes in key inputs that we want the client to send to the server
    // we read in snapshots from the server and updateGamePlayState; reconcile each GameObjects position using the m_stateLastUpdatedAt
    game_engine::NetGameInput input; // populate this from runEventLoop
    input.tick = nowTime;
    runEventLoop(player, input);

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
    m_gameServer = std::make_unique<GameServer>(9000);
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
      } else {
        std::cout << "failed to connect to server" << std::endl;
        // return;
      }
    }
  }
  return true;
}

void game_engine::Engine::runGameServerLoopThread() {

  // determine delta for server loop
  while (m_gameRunning.load() && m_gameServer) {
    // TODO the server logic to read incoming inputs
    // generate the gameState
    // work will be done in OnMessage() override member func
    // this function is blocking until the server has work to do
    m_gameServer->ProcessIncomingMessages();


    // auto snapshot = m_gameState.extractNetSnapshot();
    // every delta, we take the current gameState, package it into a message and broadcast it to the clients
    net::message<game_engine::GameMsgHeaders> msg;
    msg.header.id = GameMsgHeaders::Game_Snapshot;
    // need serealization and deserealization for NetGameObject
    // msg.body =
    m_gameServer->BroadcastToClients(msg, nullptr);
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
  SDL_SetRenderLogicalPresentation(m_sdlState.renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
  ImGui::Render();
  ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_sdlState.renderer);
  SDL_SetRenderLogicalPresentation(m_sdlState.renderer, m_sdlState.logW, m_sdlState.logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  SDL_RenderPresent(m_sdlState.renderer);
}


void game_engine::Engine::runEventLoop(GameObject &player, game_engine::NetGameInput &net_input) {
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
          }
          else if (event.key.scancode == SDL_SCANCODE_F11) {
            m_sdlState.fullscreen = !m_sdlState.fullscreen;
            SDL_SetWindowFullscreen(m_sdlState.window, m_sdlState.fullscreen);
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

    float frameW = height;
    float frameH = width;
    if (obj.type == ObjectType::Enemy) {
        frameW = obj.data.enemy.srcW;   // e.g. 128
        frameH = obj.data.enemy.srcH;   // e.g. 128
    }

    // pull out specific sprite frame from sprite sheet
    float srcX = obj.currentAnimation != -1 ?
      obj.animations[obj.currentAnimation].currentFrame() * frameW
      : (obj.spriteFrame -1)*frameW;

    SDL_FRect src = { // src is position on animation sheet texture
      .x = srcX, // different starting x position in sprite sheet
      .y = 0,
      .w = frameW, // if source sheet is larger keep the actual w/h
      .h = frameH
    };

    // if (obj.type == ObjectType::enemy) {
    //   src.h = obj.data.enemy.srcH;
    //   src.w = obj.data.enemy.srcW;
    // }


    // obj.data.enemy.srcH // we can use these on dst to scale down the image
    // obj.data.enemy.srcW

    SDL_FRect dst = {
      .x = obj.position.x - m_gameState.mapViewport.x, // move objects according to updated viewport position in x AND y
      .y = obj.position.y - m_gameState.mapViewport.y,
      .w = width, // if source is larger but you want to shrink it, set the w/h you want to scale it to
      .h = height,
    };

    if (obj.type == ObjectType::Enemy) {
      dst.h = obj.data.enemy.srcH / 2.5f;
      dst.w = obj.data.enemy.srcW / 2.5f;
    }

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
  int mapWpx = m_resources.map->mapWidth * m_resources.map->tileWidth;
  int mapHpx = m_resources.map->mapHeight * m_resources.map->tileHeight;

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
      if (obj.type == ObjectType::Level) {
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

  if (m_gameState.currentView == GameScreen::Playing) {
      setBackgroundSoundtrack(); // TODO we will want to set background track per level

      // update & draw game world to sdl.renderer here (before ImGui::Render)
      updateAllObjects(deltaTime);

      updateMapViewport(player);

      // TODO wrap all below in Render() function
      // calculate viewport position based on player updated position
      // gs.mapViewport.x = (player.position.x + player.spritePixelW / 2) - gs.mapViewport.w / 2;
      // gs.mapViewport.y = (player.position.y + player.spritePixelH / 2) - gs.mapViewport.h / 2;

      // SDL_SetRenderDrawColor(sdl.renderer, 20, 10, 30, 255);

      // // clear the backbuffer before drawing onto it with black from draw color above
      // SDL_RenderClear(sdl.renderer);

      // Perform drawing commands:

      // draw background images
      // SDL_RenderTexture(sdl.renderer, res.texBg1, nullptr, nullptr);
      // this->drawParalaxBackground(res.texBg4, player.velocity.x, gs.bg4scroll, 0.075f, deltaTime);
      // this->drawParalaxBackground(res.texBg3, player.velocity.x, gs.bg3scroll, 0.15f, deltaTime);
      // this->drawParalaxBackground(res.texBg2, player.velocity.x, gs.bg2scroll, 0.3f, deltaTime);

      // draw all background objects
      // for (auto &tile : gs.backgroundTiles) {
      //   SDL_FRect dst{
      //     .x = tile.position.x - gs.mapViewport.x,
      //     .y = tile.position.y,
      //     .w = static_cast<float>(tile.texture->w),
      //     .h = static_cast<float>(tile.texture->h),
      //   };
      //   SDL_RenderTexture(sdl.renderer, tile.texture, nullptr, &dst);
      // }
      drawAllObjects(deltaTime);

      // draw all foreground objects
      // for (auto &tile : gs.foregroundTiles) {
      //   SDL_FRect dst{
      //     .x = tile.position.x - gs.mapViewport.x,
      //     .y = tile.position.y,
      //     .w = static_cast<float>(tile.texture->w),
      //     .h = static_cast<float>(tile.texture->h),
      //   };
      //   SDL_RenderTexture(sdl.renderer, tile.texture, nullptr, &dst);
      // }

      // debugging
      if (m_gameState.debugMode) {
        SDL_SetRenderDrawColor(m_sdlState.renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(
            m_sdlState.renderer,
            5,
            5,
            std::format("State3: {}  Direction: {} B: {}, G: {}, Px: {}, Py:{}, VPx: {}", static_cast<int>(player.data.player.state), player.direction, m_gameState.bullets.size(), player.grounded, player.position.x, player.position.y, m_gameState.mapViewport.x).c_str());
      }
    }

}

void game_engine::Engine::updateImGuiMenuRenderState() {
    // 4) Start a new ImGui frame
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // 5) Build your ImGui UI for THIS frame
    // (menus, pause, debug overlay, etc.)
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImVec2 buttonSize = ImVec2(150, 50); // TODO put in config

    switch (m_gameState.currentView) {
      case GameScreen::MainMenu:
        this->stopBackgroundSoundtrack();
        ImGui::Begin("Main Menu", nullptr, m_sdlState.ImGuiWindowFlags);
        ImGui::Text("Hello from ImGui in SDL3!");
        if (ImGui::Button("Single Player", buttonSize)) {
            std::cout << "start game" << std::endl;
            std::puts("Start clicked");
            m_gameState.currentView = GameScreen::Playing;
            m_gameType = game_engine::Engine::SinglePlayer;
        }
        if (ImGui::Button("Multiplayer",buttonSize)) {
          m_gameState.currentView = GameScreen::MultiPlayerOptionsMenu;
          std::cout << "multi game" << std::endl;
        }
        if (ImGui::Button("Quit", buttonSize)) {
            std::cout << "quit game" << std::endl;
            m_gameRunning.store(false);
        }
        ImGui::End();
        break;
      case GameScreen::PauseMenu:
        ImGui::Begin("Pause", nullptr, m_sdlState.ImGuiWindowFlags);
        if (ImGui::Button("Resume")) m_gameState.currentView = GameScreen::Playing;
        if (ImGui::Button("Quit")) m_gameRunning.store(false);
        ImGui::End();
        break;
      case GameScreen::MultiPlayerOptionsMenu:
        // drawSettings();
        ImGui::Begin("MultiPlayer Menu", nullptr, m_sdlState.ImGuiWindowFlags);
        if (ImGui::Button("Host A Game",buttonSize)) {
          m_gameType = game_engine::Engine::Host;
          m_gameState.currentView = GameScreen::Playing;
        }
        if (ImGui::Button("Join A Game", buttonSize)) {
          m_gameType = game_engine::Engine::Client;
          m_gameState.currentView = GameScreen::Playing;
        }
        if (ImGui::Button("Back to Menu", buttonSize)) {
          // todo; should reset game state unless saved
          m_gameState.currentView = GameScreen::MainMenu;
        }
        ImGui::End();
        break;
      case GameScreen::Playing:
        ImGuiWindowFlags ImGuiWindowFlags =
          m_sdlState.ImGuiWindowFlags | ImGuiWindowFlags_NoBackground;
          // ImGuiWindowFlags_NoSavedSettings;
          ImGui::Begin("HUD", nullptr, ImGuiWindowFlags);
          // Optional: remove padding so the button hugs the corner
          ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
          ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
          if (ImGui::Button("Back to Menu", buttonSize)) {
              m_gameState.currentView = GameScreen::MainMenu;
          }
          ImGui::SameLine(0, 2.0f);
          if (ImGui::Button("Save Game", buttonSize)) {
            // TODO
          }
          ImGui::PopItemFlag();
          ImGui::PopStyleVar();
          ImGui::End();
        break;
    }
}

// update updates the state of the passed in game object every render loop
void game_engine::Engine::updateGameObject(GameObject &obj, float deltaTime) {

  if (obj.currentAnimation != -1) {
    obj.animations[obj.currentAnimation].step(deltaTime);
  }

  // gravity applied globally; downward y force when not grounded
  if (obj.dynamic && !obj.grounded) {
    // if (obj.type == ObjectType::enemy) {
    //     std::cout << "grounded" << obj.grounded << std::endl;
    // }
    // increase downward velocity = acc*deltaTime every frame
    obj.velocity += GRAVITY * deltaTime;
  }

  float currDirection = 0;
  if (obj.type == ObjectType::Player) {

    // this way player cant spam jump and fly; but sometimes gets stuck and is unable to jump
    // if (state.keys[SDL_SCANCODE_DOWN]) {
    //   handleKeyInput(obj, SDL_SCANCODE_DOWN, false);
    // }
    // if (state.keys[SDL_SCANCODE_UP]) {
    //   handleKeyInput(obj, SDL_SCANCODE_UP, true);
    // }
    // keep previous key state somewhere (per frame)
    // bool upPressed = state.keys[SDL_SCANCODE_UP];
    // if (upPressed && !prevUpPressed) {
    //     handleKeyInput(obj, SDL_SCANCODE_UP, true);  // key just went down
    // }
    // if (!upPressed && prevUpPressed) {
    //     handleKeyInput(obj, SDL_SCANCODE_UP, false); // key just went up (if you care)
    // }
    // prevUpPressed = upPressed;

    // update direction
    if (m_sdlState.keys[SDL_SCANCODE_LEFT]) {
      currDirection += -1;
    }
    if (m_sdlState.keys[SDL_SCANCODE_RIGHT]) {
      currDirection += 1;
    }

    Timer &weaponTimer = obj.data.player.weaponTimer;
    weaponTimer.step(deltaTime);

    const auto handleShooting = [this, &obj, &weaponTimer, &currDirection](
      SDL_Texture *tex, SDL_Texture *shootTex, int animIndex, int shootAnimIndex){
    // TODO use similar condition to prevent double jump
      if (m_sdlState.keys[SDL_SCANCODE_A]) {

        // set player texture during shooting anims
        obj.texture = shootTex;
        obj.currentAnimation = shootAnimIndex;
        if (weaponTimer.isTimedOut()) {
          weaponTimer.reset();
          // create bullets
          GameObject bullet(4, 4);
          bullet.data.bullet = BulletData();
          bullet.type = ObjectType::Bullet;
          bullet.direction = obj.direction;
          bullet.texture = m_resources.texBullet;
          bullet.currentAnimation = m_resources.ANIM_BULLET_MOVING;
          bullet.collider = SDL_FRect{
            .x = 0, .y = 0,
            .w = static_cast<float>(m_resources.texBullet->h),
            .h = static_cast<float>(m_resources.texBullet->h),
          };
          const int yJitter = 50;
          const float yVelocity = SDL_rand(yJitter) - yJitter / 2.0f;
          bullet.velocity = glm::vec2(
            obj.velocity.x + 600.0f,
            yVelocity
          ) * obj.direction;
          bullet.maxSpeedX = 1000.0f;
          bullet.animations = m_resources.bulletAnims;

          // adjust depending on direction faced; lerp
          const float left = 4;
          const float right = 24;
          const float t = (obj.direction + 1) / 2.0f; // 0 or 1 taking into account neg sign
          const float xOffset = left + right * t;
          bullet.position = glm::vec2(
            obj.position.x + xOffset,
            obj.position.y + obj.spritePixelH / 2 + 1
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

        // MIX_SetTrackAudio(res.shootTrack, res.audioShoot); // or MIX_SetTrackAudioWithProperties
        if (!MIX_PlayTrack(m_resources.shootTrack, 0)) {
            // SDL_Log("Play failed: %s", SDL_GetError());
        };

      } else {
          obj.texture = tex;
          obj.currentAnimation = animIndex;
      }
    };

    // update animation state
    switch (obj.data.player.state) {
      case PlayerState::idle:
      {
        if (currDirection != 0) {
          obj.data.player.state = PlayerState::running;
          obj.texture = m_resources.texRun;
          obj.currentAnimation = m_resources.ANIM_PLAYER_RUN;
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

        handleShooting(m_resources.texIdle, m_resources.texShoot, m_resources.ANIM_PLAYER_IDLE, m_resources.ANIM_PLAYER_SHOOT);

        break;
      }
      case PlayerState::running:
      {
        if (currDirection == 0) {
          obj.data.player.state = PlayerState::idle;
        }

        // move in opposite dir of velocity, sliding
        if (obj.velocity.x * obj.direction < 0 && obj.grounded) {
          handleShooting(m_resources.texSlide, m_resources.texSlideShoot, m_resources.ANIM_PLAYER_SLIDE, m_resources.ANIM_PLAYER_SLIDE_SHOOT);
        } else {
          handleShooting(m_resources.texRun, m_resources.texRunShoot, m_resources.ANIM_PLAYER_RUN, m_resources.ANIM_PLAYER_RUN);
          // sprite sheets have same frames so we can seamlessly swap between the two sheets
        }

        break;
      }
      case PlayerState::jumping:
      {
        handleShooting(m_resources.texRun, m_resources.texRunShoot, m_resources.ANIM_PLAYER_RUN, m_resources.ANIM_PLAYER_RUN);
        // obj.texture = res.texRun;
        // obj.currentAnimation = res.ANIM_PLAYER_RUN;
        break;
      }
    }
  } else if (obj.type == ObjectType::Bullet) {

    switch (obj.data.bullet.state) {
      case BulletState::moving:
      {
        if (obj.position.x - m_gameState.mapViewport.x < 0 || obj.position.x - m_gameState.mapViewport.x > m_sdlState.logW ||
        obj.position.y - m_gameState.mapViewport.y < 0 ||
        obj.position.y - m_gameState.mapViewport.y > m_sdlState.logH) {
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
  } else if (obj.type == ObjectType::Enemy) {

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
          obj.texture = m_resources.texEnemyRun;
        } else {
          // stop them from moving when too far away
          obj.acceleration = glm::vec2(0);
          obj.velocity.x = 0;
          obj.texture = m_resources.texEnemy;
        }

        break;
      }
      case EnemyState::dying:
      {
        if (obj.data.enemy.damageTimer.step(deltaTime)) {
          obj.data.enemy.state = EnemyState::idle;
          obj.texture = m_resources.texEnemy;
          obj.currentAnimation = m_resources.ANIM_ENEMY;
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

  if (currDirection) {
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
        if (objB.type == ObjectType::Level) {
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
    if (foundGround && obj.type == ObjectType::Player) {
      obj.data.player.state = PlayerState::running;
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

  if (objA.type == ObjectType::Player) {
    switch (objB.type) {
      case ObjectType::Level:
      {
        defaultResponse();
        break;
      }
      case ObjectType::Enemy:
      {
        if (objB.data.enemy.state != EnemyState::dead) {
          objA.velocity = glm::vec2(50, 0) * - objA.direction;
        }
        break;
      }
      case ObjectType::Player:
      {
        break;
      }
    }
  } else if (objA.type == ObjectType::Bullet) {

    bool passthrough = false;
    switch (objA.data.bullet.state) {
      case BulletState::moving:
      {
        switch (objB.type) {
          case ObjectType::Level:
          {
            if (!MIX_PlayTrack(m_resources.hitTrack, 0)) {
            // SDL_Log("Play failed: %s", SDL_GetError());
            };

            break;
          }
          case ObjectType::Enemy:
          {
            EnemyData &d = objB.data.enemy;
            if (d.state != EnemyState::dead) {
              objB.direction = -1 * objA.direction;
              objB.shouldFlash = true;
              objB.flashTimer.reset();
              objB.texture = m_resources.texEnemyHit;
              objB.currentAnimation = m_resources.ANIM_ENEMY_HIT;
              d.state = EnemyState::dying;

              // damage and flag dead
              d.healthPoints -= 10;
              if (d.healthPoints <= 0) {
                d.state = EnemyState::dead;
                objB.texture = m_resources.texEnemyDie;
                objB.currentAnimation = m_resources.ANIM_ENEMY_DIE;
                MIX_PlayTrack(m_resources.enemyDieTrack, 0);
              }
              MIX_PlayTrack(m_resources.enemyHitTrack, 0);
            } else {
              passthrough = true;
            }
            break;
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
  else if (objA.type == ObjectType::Enemy) {
    defaultResponse(); // ensure enemy doesnt fall through floor
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

bool game_engine::Engine::initAllTiles() {

  struct LayerVisitor
  {

    const SDLState &state;
    GameState &gs;
    const Resources &res;

    LayerVisitor(const SDLState &state, GameState &gs, Resources &res): state(state), gs(gs), res(res){}

    const tmx::TileSet* pickTileset(uint32_t gid) {
      const tmx::TileSet* match = nullptr;
      for (const auto& ts : res.map->tileSets) {
          if (gid >= (uint32_t)ts.firstgid) match = &ts;
          else break;
      }
          return match; // assumes sets are sorted by firstGid
    }

    const GameObject createObject(int r, int c, SDL_Texture *tex, ObjectType type, float spriteH, float spriteW, int srcX, int srcY) {
      GameObject o(spriteH, spriteW);
      o.type = type;
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

      if (type == ObjectType::Level) {
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
      for (int r = 0; r < res.map->mapHeight; ++r){
        for (int c = 0; c < res.map->mapWidth; ++c) {
          const int tGid = layer.data[r * res.map->mapWidth + c]; // because its a 1D representation of a 2D map
          if (tGid) {

            // find the texture corresponding to this gID
            // const auto itr = std::find_if(res.map->tileSets.begin(), res.map->tileSets.end(),
            // [tGid](const tmx::TileSet &ts) {
            //   return tGid >= ts.firstgid && tGid < ts.firstgid + ts.texture.size();
            // });
            const tmx::TileSet* ts = pickTileset(tGid);

            if (!ts) continue;

            uint32_t localId = tGid - ts->firstgid; // local index within the tileSet data array
            int srcX = (localId % ts->columns) * ts->tileWidth; // col * tileWidth;
            int srcY = (localId / ts->columns) * ts->tileHeight; // row * tileHeight

            auto tile = createObject(r, c, ts->texture, ObjectType::Level, ts->tileHeight, ts->tileWidth, srcX, srcY);
            if (layer.name != "Level") {
              tile.collider.w = tile.collider.h = 0;
            }

            newLayer.push_back(tile);

          }

        }
      }
      gs.layers.push_back(std::move(newLayer));
    };

    void operator()(tmx::ObjectGroup &objectGroup)
    {
      std::vector<GameObject> newLayer;
      for (tmx::LayerObject &obj : objectGroup.objects)
      {
        glm::vec2 objStartingPos(
          obj.x - res.map->tileWidth / 2, //17
          obj.y - res.map->tileHeight / 2 //411
        );

        // glm::vec2 objPos{ obj.x, obj.y - res.map->tileHeight }; // raise by tile height so the bottom sits on Tiled Y
        // glm::vec2 objPos{ obj.y - res.map->tileHeight, obj.x}; // raise by tile height so the bottom sits on Tiled Y


        if (obj.type == "Enemy") {
          GameObject enemy = createObject(1, 1, res.texEnemy, ObjectType::Enemy, TILE_SIZE, TILE_SIZE, 0, 0);
          enemy.position = objStartingPos;
          enemy.data.enemy = EnemyData();
          enemy.data.enemy.srcH = 128;
          enemy.data.enemy.srcW = 128;
          enemy.currentAnimation = res.ANIM_ENEMY;
          enemy.animations = res.enemyAnims;
          enemy.dynamic = true;
          enemy.maxSpeedX = 15;
          // update collider x.y position to detemine how much overlap is allowed between objects
          enemy.collider = {
            .x = 11,
            .y = 10,
            .w = 32,
            .h = 32
          };
          newLayer.push_back(enemy);
        }
        if (obj.type == "Player") {
        {
          GameObject player = createObject(1, 1, res.texIdle, ObjectType::Player, 32, 32, 0, 0); // TODO update with new dimensions
          player.position = objStartingPos;
          player.data.player = PlayerData();
          player.animations = res.playerAnims; // copies via std::vector copy assignment
          player.currentAnimation = res.ANIM_PLAYER_IDLE;
          player.acceleration = glm::vec2(300, 0);
          player.maxSpeedX = 100;
          player.dynamic = true;
          player.collider = {
            .x = 11,
            .y = -3,
            .w = 10,
            .h = 26
          };
          newLayer.push_back(player);
          gs.playerIndex = newLayer.size() - 1;
          gs.playerLayer = gs.layers.size();
        }
        };
      };
      gs.layers.push_back(std::move(newLayer));
    }
  };

  LayerVisitor visitor(m_sdlState, m_gameState, m_resources);
  for (std::variant<tmx::Layer, tmx::ObjectGroup> &layer : m_resources.map->layers) {
    std::visit(visitor, layer);
  };


  return m_gameState.playerIndex != -1;
};

// bool game_engine::Engine::initAllTiles() {

//   /*
//     1 - Ground
//     2 - Panel
//     3 - Enemy
//     4 - Player
//     5 - Grass
//     6 - Brick
//   */

//   short map[MAP_ROWS][MAP_COLS] = {
//     0, 0, 0, 0, 4, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 3, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
//     0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
//     0, 0, 0, 0, 2, 0, 0, 0, 0, 3,2, 2, 2, 2, 2,2, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
//     0, 0, 0, 2, 2, 0, 0, 3, 2, 2,2, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
//     1, 1, 1, 1, 1, 1, 1, 1, 1, 1,1, 1, 1, 1, 1,1, 1, 1, 1, 1,1, 1, 1, 1, 1,1, 1, 1, 1, 1, 1, 1, 1, 1, 1,1, 1, 1, 1, 1,1, 1, 1, 1, 1,1, 1, 1, 1, 1,
//   };

//   short foregroundMap[MAP_ROWS][MAP_COLS] = {
//     0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
//     0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
//     0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
//     5, 5, 5, 0, 0, 5, 5, 5, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
//     0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
//   };

//   short backgroundMap[MAP_ROWS][MAP_COLS] = {
//     0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
//     0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
//     0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
//     0, 0, 0, 0, 0, 6, 6, 6, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
//     0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
//   };

//   // short maplayer[MAP_ROWS][MAP_COLS] really means short (*maplayer)[MAP_COLS]: a pointer to the first row (each row is an array of MAP_COLS shorts)
//   // const short (&maplayer)[MAP_ROWS][MAP_COLS] if want by reference
//   const auto loadMap = [this](short maplayer[MAP_ROWS][MAP_COLS]) {
//     const auto createObject = [this](int r, int c, SDL_Texture *tex, ObjectType type, float spriteH, float spriteW) {
//       GameObject o(spriteH, spriteW);
//       o.type = type;
//       o.position = glm::vec2(c * TILE_SIZE, state.logH - (MAP_ROWS - r) * TILE_SIZE);
//       o.texture = tex;
//       o.collider = {
//         .x = 0,
//         .y = 0,
//         .w = TILE_SIZE,
//         .h = TILE_SIZE
//       };
//       return o;
//     };

//     for (int r = 0; r < MAP_ROWS; r++) {
//       for (int c = 0; c < MAP_COLS; c++) {
//         switch (maplayer[r][c]) {
//           case 1: // Ground
//           {
//             GameObject ground = createObject(r, c, res.texGround, ObjectType::level, TILE_SIZE, TILE_SIZE);
//             ground.data.level = LevelData();
//             gs.layers[LAYER_IDX_LEVEL].push_back(ground); // we do this so we can update each object and destroy the objects with easy access
//             break;
//           }
//           case 2: // Panel
//           {
//             GameObject panel = createObject(r, c, res.texPanel, ObjectType::level, TILE_SIZE, TILE_SIZE);
//             panel.data.level = LevelData();
//             gs.layers[LAYER_IDX_LEVEL].push_back(panel);
//             break;
//           }
//           case 3: // Enemy
//           {
//             GameObject enemy = createObject(r, c, res.texEnemy, ObjectType::enemy, TILE_SIZE, TILE_SIZE);
//             enemy.data.enemy = EnemyData();
//             enemy.currentAnimation = res.ANIM_ENEMY;
//             enemy.animations = res.enemyAnims;
//             enemy.dynamic = true;
//             enemy.maxSpeedX = 15;
//             // enemy.collider = {
//             //   .x = 11,
//             //   .y = 6,
//             //   .w = 10,
//             //   .h = 26
//             // };
//             gs.layers[LAYER_IDX_CHARACTERS].push_back(enemy);
//             break;
//           }
//           case 4: // player
//           {
//             GameObject player = createObject(r, c, res.texIdle, ObjectType::player, 32, 32); // TODO update with new dimensions
//             player.data.player = PlayerData();
//             player.animations = res.playerAnims; // copies via std::vector copy assignment
//             player.currentAnimation = res.ANIM_PLAYER_IDLE;
//             player.acceleration = glm::vec2(300, 0);
//             player.maxSpeedX = 100;
//             player.dynamic = true;
//             player.collider = {
//               .x = 11,
//               .y = 6,
//               .w = 10,
//               .h = 26
//             };
//             gs.layers[LAYER_IDX_CHARACTERS].push_back(player);
//             gs.playerIndex = gs.layers[LAYER_IDX_CHARACTERS].size() - 1;
//             break;
//           }
//           case 5:
//           {
//             GameObject grass = createObject(r, c, res.texGrass, ObjectType::level, TILE_SIZE, TILE_SIZE);
//             grass.data.level = LevelData();
//             // gs.layers[LAYER_IDX_LEVEL].push_back(grass);
//             gs.foregroundTiles.push_back(grass);
//             break;
//           }
//           case 6:
//           {
//             GameObject brick = createObject(r, c, res.texBrick, ObjectType::level, TILE_SIZE, TILE_SIZE);
//             brick.data.level = LevelData();
//             // gs.layers[LAYER_IDX_LEVEL].push_back(brick);
//             gs.backgroundTiles.push_back(brick);
//             break;
//           }
//         }
//       }
//     }
//   };

//   loadMap(map);
//   loadMap(backgroundMap);
//   loadMap(foregroundMap);

//   // assert(gs.playerIndex != -1); // player index must be set
//   return gs.playerIndex != -1;
// };

void game_engine::Engine::handleKeyInput(GameObject &obj, SDL_Scancode key, bool keyDown, game_engine::NetGameInput &input) {

  // TODO send input msg to server

  if (obj.type == ObjectType::Player) {
    switch (obj.data.player.state) {
      case PlayerState::idle:
      {
        if (key == SDL_SCANCODE_UP && keyDown) {
          obj.velocity.y += JUMP_FORCE;
          obj.data.player.state = PlayerState::jumping;
          input.move = PlayerInput::Jump;
          input.shouldSendMessage = true;
        } else if (key == SDL_SCANCODE_S && keyDown) {

          // todo handle swing of sword
          obj.data.player.state = PlayerState::swingWeapon;
          input.move = PlayerInput::Swing;
          input.shouldSendMessage = true;
        }
        break;
      }
      case PlayerState::jumping:
      {
        if (!keyDown) { // once you stop holding down the key, set player state back to idle
          obj.velocity.y = 0;
          obj.data.player.state = PlayerState::idle;
        }
        break;
      }
      case PlayerState::running:
      {
        if (key == SDL_SCANCODE_UP && keyDown) {
          obj.velocity.y += JUMP_FORCE;
          obj.data.player.state = PlayerState::jumping;
          input.move = game_engine::PlayerInput::Jump;
          input.shouldSendMessage = true;
        }
        break;
      }
      case PlayerState::swingWeapon:
      {
        if (!keyDown) {
          // obj.velocity.y = 0;
          obj.data.player.state = PlayerState::idle;
        }
        break;
      }
    }

  }



};

void game_engine::Engine::drawParalaxBackground(SDL_Texture *texture, float xVelocity, float &scrollPos, float scrollFactor, float deltaTime) {
  scrollPos -= xVelocity * scrollFactor * deltaTime; // scroll position passed by reference, is updated every loop
  if (scrollPos <= -texture->w) {
    scrollPos = 0;
  }

  SDL_FRect dst{
    .x = scrollPos,
    .y = 40,
    .w = texture->w * 2.0f,
    .h = static_cast<float>(texture->h)
  };

  SDL_RenderTextureTiled(m_sdlState.renderer, texture, nullptr, 1, &dst);
};