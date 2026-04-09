#include "engine/engine.h"
#include "engine/igame_rules.h"
#include "engine/net/game_server.h"

namespace {

ObjectData cloneObjectData(const GameObject& src) {
  ObjectData data;
  switch (src.objClass) {
    case ObjectClass::Player:
      new (&data.player) PlayerData(src.data.player);
      break;
    case ObjectClass::Enemy:
      new (&data.enemy) EnemyData(src.data.enemy);
      break;
    case ObjectClass::Projectile:
      new (&data.bullet) BulletData(src.data.bullet);
      break;
    case ObjectClass::Portal:
      new (&data.portal) PortalData(src.data.portal.nextLevel);
      break;
    case ObjectClass::Level:
    case ObjectClass::Background:
      new (&data.level) LevelData(src.data.level);
      break;
  }
  return data;
}

GameObject cloneGameObject(const GameObject& src) {
  GameObject dst(src.spritePixelH, src.spritePixelW);
  dst.id = src.id;
  dst.objClass = src.objClass;
  dst.spriteType = src.spriteType;
  dst.data = cloneObjectData(src);
  dst.position = src.position;
  dst.velocity = src.velocity;
  dst.acceleration = src.acceleration;
  dst.direction = src.direction;
  dst.maxSpeedX = src.maxSpeedX;
  dst.animations = src.animations;
  dst.currentAnimation = src.currentAnimation;
  dst.texture = nullptr;
  dst.dynamic = src.dynamic;
  dst.grounded = src.grounded;
  dst.bgscroll = src.bgscroll;
  dst.scrollFactor = src.scrollFactor;
  dst.drawScale = src.drawScale;
  dst.spritePixelW = src.spritePixelW;
  dst.spritePixelH = src.spritePixelH;
  dst.baseCollider = src.baseCollider;
  dst.collider = src.collider;
  dst.colliderNorm = src.colliderNorm;
  dst.flashTimer = src.flashTimer;
  dst.shouldFlash = src.shouldFlash;
  dst.spriteFrame = src.spriteFrame;
  return dst;
}

game_engine::GameState cloneAuthoritativeGameState(
  const game_engine::GameState& src,
  const game_engine::SDLState& sdlState) {
  game_engine::GameState dst(sdlState);
  dst.currentView = src.currentView;
  dst.currentLevelId = src.currentLevelId;
  dst.m_stateLastUpdatedAt = src.m_stateLastUpdatedAt;
  dst.debugMode = src.debugMode;
  dst.selectedPlayerSprite = src.selectedPlayerSprite;
  dst.playerLayer = src.playerLayer;
  dst.playerIndex = src.playerIndex;
  dst.mapViewport = src.mapViewport;
  dst.bg2scroll = src.bg2scroll;
  dst.bg3scroll = src.bg3scroll;
  dst.bg4scroll = src.bg4scroll;

  dst.layers.reserve(src.layers.size());
  for (const auto& layer : src.layers) {
    auto& newLayer = dst.layers.emplace_back();
    newLayer.reserve(layer.size());
    for (const auto& obj : layer) {
      newLayer.push_back(cloneGameObject(obj));
    }
  }

  dst.bullets.reserve(src.bullets.size());
  for (const auto& bullet : src.bullets) {
    dst.bullets.push_back(cloneGameObject(bullet));
  }

  return dst;
}

} // namespace

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

MIX_Mixer* game_engine::Engine::getMixer() {
  return m_mixer;
};

void game_engine::Engine::setAudioSoundtrack(MIX_Track* track, int loops) {
  if (track && !MIX_TrackPlaying(track)) {
    SDL_PropertiesID opts = SDL_CreateProperties();
    SDL_SetNumberProperty(opts, MIX_PROP_PLAY_LOOPS_NUMBER, loops);
    if (!MIX_PlayTrack(track, opts)) {
        SDL_Log("Music Play failed: %s", SDL_GetError());
    }
    SDL_DestroyProperties(opts); // destory internal resources
  }
};

void game_engine::Engine::stopAudioSoundtrack(MIX_Track* track) {
  if (track && MIX_TrackPlaying(track)) {
    if (!MIX_StopTrack(track, 10)) {
        SDL_Log("stopping Background Music Play failed: %s", SDL_GetError());
    }
  }
};

void game_engine::Engine::setWindowSize(int height, int width) {
  m_sdlState.width = width;
  m_sdlState.height = height;
}

void game_engine::Engine::setRunModeSinglePlayer() {
  m_gameType = SinglePlayer;
}

void game_engine::Engine::setRunModeHost() {
  m_gameType = Host;
}

void game_engine::Engine::setRunModeClient() {
  m_gameType = Client;
}

void game_engine::Engine::requestQuit() {
  m_gameRunning.store(false);
}

bool game_engine::Engine::isRunning() const {
  return m_gameRunning.load();
}

bool game_engine::Engine::isHostMode() const {
  return m_gameType == Host;
}

bool game_engine::Engine::isClientMode() const {
  return m_gameType == Client;
}

bool game_engine::Engine::init(int width, int height, int logW, int logH) {

  // SDL, ImGUI are initialized

  // init window and renderer
  if (!initWindowAndRenderer(width, height, logW, logH)) {
    return false;
  };

  // TTF_Init();
  // m_resources.font = TTF_OpenFont("data/cutscenes/fonts/to_the_point_regular.ttf", 12);

  // mixer must be created before loading in audio files in res.load()
  if (!MIX_Init()) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL Mixer Failed to init", "Failed to init audio", nullptr);
    cleanup();
    return false;
  };

  // one global mixer
  m_mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
  if (!m_mixer) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Global Mixer Failed to create", "Failed to create audio mixer", nullptr);
    cleanup();
    return false;
  }

  MIX_SetMasterGain(m_mixer, 0.5f);  // 50% master

  m_gameState = std::move(GameState(m_sdlState));
  return true;
}

void game_engine::Engine::run(eng::IGameRules& rules) {
  if (!rules.onInit(*this)) { // init the rules for particular game
    return;
  }

  m_gameRunning.store(true);
  uint64_t prevTime = SDL_GetTicks();

  while (m_gameRunning.load()) {
    const uint64_t nowTime = SDL_GetTicks();
    const float deltaTime = (nowTime - prevTime) / 1000.0f;
    prevTime = nowTime;

    SDL_Event event{0};
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) {
        m_gameRunning.store(false);
      }
      rules.onEvent(*this, event);
    }

    rules.onUpdate(*this, deltaTime);
    rules.onRender(*this, deltaTime);
  }

  rules.onShutdown(*this);

  if (m_gameServer) {
    m_gameServer->Stop();
  }
  if (m_serverLoopThd.joinable()) {
    m_serverLoopThd.join();
  }
  m_gameServer.reset();
}

std::unique_ptr<game_engine::GameServer> game_engine::Engine::buildAuthoritativeStateForServer() {
  auto authState = cloneAuthoritativeGameState(m_gameState, m_sdlState);
  return std::make_unique<GameServer>(
    9000,
    std::make_unique<AuthoritativeContext>(std::move(authState)));
}

bool game_engine::Engine::handleMultiplayerConnections() {
    // create a client; need a GameClient that inherits from client_interface
    // wait for connection to the server to be made.
    // non-host client should be able to join and exit at any time.
  if (m_gameType == game_engine::Engine::Host && !m_gameServer) {
    std::cout << "starting server" << std::endl;
    m_gameServer = buildAuthoritativeStateForServer();
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

    m_gameClient->ProcessServerMessages();
    if (m_gameClient->IsClientValidated() && !m_gameClient->IsRegistered()) {
      m_gameClient->RegisterWithServer(m_gameState.selectedPlayerSprite);
    }
  }
  return true;
}

void game_engine::Engine::submitLocalInput(NetGameInput input) {
  input.inputSeq = ++m_localInputSeq;
  if (m_gameClient) {
    input.playerID = m_gameClient->GetPlayerID();
  }

  input.shouldSendMessage =
    input.leftHeld || input.rightHeld || input.fireHeld || input.jumpPressed || input.meleePressed;
  m_localInput = input;

  if (isMultiplayerActive() && m_gameClient) {
    m_gameClient->SendInput(m_localInput);
  }
}

void game_engine::Engine::restartMultiplayerSession() {
  if (!isMultiplayerActive()) {
    return;
  }

  if (isHostMode() && m_gameServer) {
    auto authState = cloneAuthoritativeGameState(m_gameState, m_sdlState);
    m_gameServer->resetAuthoritativeState(std::move(authState));
    if (m_gameClient) {
      m_gameClient->ClearLatestSnapshot();
    }
    m_gameState.currentView = UIManager::GameView::Playing;
    return;
  }

  if (isClientMode() && m_gameClient) {
    m_gameClient->RequestRespawn();
    m_gameState.currentView = UIManager::GameView::Playing;
  }
}

const game_engine::NetGameInput& game_engine::Engine::getLocalInput() const {
  return m_localInput;
}

game_engine::GameClient* game_engine::Engine::getGameClient() {
  return m_gameClient.get();
}

const game_engine::GameClient* game_engine::Engine::getGameClient() const {
  return m_gameClient.get();
}

bool game_engine::Engine::isMultiplayerActive() const {
  return m_gameType == Host || m_gameType == Client;
}

void game_engine::Engine::runGameServerLoopThread() {

  using clock = std::chrono::steady_clock;
  const double dt = 1.0/60.0; // 60Hz
  const size_t maxInputsPerTick = 64;
  auto prev = clock::now();
  double accum = 0.0;
  uint64_t tickCount = 0;

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
      m_gameServer->step(static_cast<float>(dt));
      ++tickCount;
      accum -= dt;
      if (tickCount % 3 == 0) {
        m_gameServer->refreshSnapshot();
        net::message<GameMsgHeaders> msg;
        msg.header.id = GameMsgHeaders::Game_Snapshot;
        msg.body = m_gameServer->m_currGameSnapshot.serealizeNetGameStateSnapshot();
        msg.header.bodySize = msg.body.size();
        m_gameServer->BroadcastToClients(msg);
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
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
  SDL_SetRenderLogicalPresentation(m_sdlState.renderer, m_sdlState.logW , m_sdlState.logH, SDL_LOGICAL_PRESENTATION_INTEGER_SCALE);

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
}

void game_engine::Engine::cleanup() {
  if (m_mixer) { MIX_DestroyMixer(m_mixer); m_mixer = nullptr; }
  MIX_Quit();
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  // if (state.renderer) { SDL_DestroyRenderer(state.renderer); state.renderer = nullptr; }
  // if (state.window) { SDL_DestroyWindow(state.window); state.window = nullptr; }
  SDL_Quit();
}
