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
  dst.presentationVariant = src.presentationVariant;
  dst.texture = nullptr;
  dst.dynamic = src.dynamic;
  dst.grounded = src.grounded;
  dst.renderPosition = src.renderPosition;
  dst.renderPositionInitialized = src.renderPositionInitialized;
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

bool isMultiplayerBootstrapView(UIManager::GameView view) {
  switch (view) {
    case UIManager::GameView::MainMenu:
    case UIManager::GameView::MultiPlayerOptionsMenu:
    case UIManager::GameView::CharacterSelect:
    case UIManager::GameView::MultiplayerBrowse:
      return true;
    default:
      return false;
  }
}

} // namespace

GameObject &game_engine::Engine::getPlayer() {
 return m_gameState.player(LAYER_IDX_CHARACTERS);
};

game_engine::Engine::~Engine() {
  // ensure server thread and client tear down cleanly
  m_gameRunning.store(false);
  resetMultiplayerNetworkingState();
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
  resetMultiplayerNetworkingState();
  m_gameType = SinglePlayer;
}

void game_engine::Engine::setRunModeHost() {
  resetMultiplayerNetworkingState();
  m_gameType = Host;
  m_selectedJoinHost = "127.0.0.1";
  m_selectedJoinPort = GAME_SERVER_PORT;
  m_hasSelectedJoinTarget = true;
}

void game_engine::Engine::setRunModeClient() {
  resetMultiplayerNetworkingState();
  m_gameType = Client;
  m_hasSelectedJoinTarget = false;
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

  m_serverLoopRunning.store(false);
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
    GAME_SERVER_PORT,
    std::make_unique<AuthoritativeContext>(std::move(authState)));
}

bool game_engine::Engine::handleMultiplayerConnections() {
  if (!isMultiplayerActive()) {
    return true;
  }

  const auto returnClientToBrowse = [this]() {
    resetMultiplayerNetworkingState();
    m_gameType = Client;
    m_gameState.currentView = UIManager::GameView::MultiplayerBrowse;
    m_multiplayerStatus = "Host disconnected";
  };

  if (m_gameType == Host) {
    if (!m_discoveryHost) {
      m_discoveryHost = std::make_unique<DiscoveryHostService>();
    }
    if (m_discoveryHost && !m_discoveryHost->isStarted()) {
      if (!m_discoveryHost->start()) {
        m_multiplayerStatus = "Failed to start LAN discovery";
        return false;
      }
    }
    if (m_discoveryHost) {
      const uint32_t playerCount = m_gameServer ? static_cast<uint32_t>(m_gameServer->m_playerSessions.size()) : 0;
      m_discoveryHost->updateInfo("LAN Host", m_gameState.currentLevelId, playerCount, GAME_SERVER_PORT);
      m_discoveryHost->setReady(m_serverReadyForDiscovery);
      m_discoveryHost->poll();
    }
  } else if (m_discoveryHost) {
    m_discoveryHost->stop();
    m_discoveryHost.reset();
    m_serverReadyForDiscovery = false;
  }

  if (m_gameType == Client && m_gameState.currentView == UIManager::GameView::MultiplayerBrowse) {
    if (!m_discoveryBrowser) {
      m_discoveryBrowser = std::make_unique<DiscoveryBrowserService>();
    }
    if (m_discoveryBrowser && !m_discoveryBrowser->isStarted()) {
      if (!m_discoveryBrowser->start()) {
        m_multiplayerStatus = "Failed to browse LAN games";
        return false;
      }
    }
    if (m_discoveryBrowser) {
      m_discoveryBrowser->poll();
    }
    m_multiplayerStatus = "Searching LAN for ready hosts...";
    return true;
  }

  if (m_discoveryBrowser) {
    m_discoveryBrowser->stop();
    m_discoveryBrowser.reset();
  }

  if (isMultiplayerBootstrapView(m_gameState.currentView)) {
    return true;
  }

  if (m_gameType == Host && !m_gameServer) {
    std::cout << "starting server" << std::endl;
    m_gameServer = buildAuthoritativeStateForServer();
    bool successStart = m_gameServer->Start();
    if (!successStart) {
      m_multiplayerStatus = "Failed to start host server";
      return false;
    }
    m_serverLoopRunning.store(true);
    m_serverLoopThd = std::thread(&game_engine::Engine::runGameServerLoopThread, this);
  }

  if (m_gameType == Client && !m_hasSelectedJoinTarget) {
    m_multiplayerStatus = "Select a LAN host first";
    return true;
  }

  if (!m_gameClient) {
    std::cout << "starting client" << std::endl;
    m_gameClient = std::make_unique<GameClient>();
  }

  if (m_gameClient) {
    if (m_gameType == Client &&
        isConnectedToServer &&
        !m_gameClient->IsConnected() &&
        (m_gameClient->IsRegistered() || m_gameClient->IsClientValidated())) {
      returnClientToBrowse();
      return true;
    }

    if (!isConnectedToServer) {
      const std::string host = (m_gameType == Host) ? "127.0.0.1" : m_selectedJoinHost;
      const uint16_t port = (m_gameType == Host) ? GAME_SERVER_PORT : m_selectedJoinPort;
      if (m_gameClient->Connect(host, port)) {
        isConnectedToServer = true;
        m_multiplayerStatus = "Connected to server";
        std::cout << "client connected to server" << std::endl;
      } else {
        m_multiplayerStatus = "Waiting for server connection...";
        std::cout << "failed to connect to server" << std::endl;
        return true;
      }
    }

    m_gameClient->ProcessServerMessages();
    if (m_gameClient->IsClientValidated() && !m_gameClient->IsRegistered()) {
      m_gameClient->RegisterWithServer(m_gameState.selectedPlayerSprite);
      m_multiplayerStatus = "Registering with server...";
    } else if (m_gameClient->IsRegistered()) {
      m_multiplayerStatus = "Connected";
    }
  }

  if (m_gameType == Host) {
    const bool hostReady = m_gameClient && m_gameClient->IsRegistered();
    m_serverReadyForDiscovery = hostReady;
    if (m_discoveryHost) {
      const uint32_t playerCount = m_gameServer ? static_cast<uint32_t>(m_gameServer->m_playerSessions.size()) : 0;
      m_discoveryHost->updateInfo("LAN Host", m_gameState.currentLevelId, playerCount, GAME_SERVER_PORT);
      m_discoveryHost->setReady(hostReady);
    }
    if (hostReady && m_gameState.currentView == UIManager::GameView::MultiplayerHostWaiting) {
      m_gameState.currentView = UIManager::GameView::Playing;
    }
  }
  return true;
}

void game_engine::Engine::resetMultiplayerNetworkingState() {
  m_serverReadyForDiscovery = false;
  m_hasSelectedJoinTarget = false;
  isConnectedToServer = false;
  m_serverLoopRunning.store(false);
  m_localInput = NetGameInput{};
  m_localInputSeq = 0;
  m_inputSendAccumulator = 0.0f;
  m_multiplayerStatus.clear();

  if (m_discoveryBrowser) {
    m_discoveryBrowser->stop();
    m_discoveryBrowser.reset();
  }
  if (m_discoveryHost) {
    m_discoveryHost->stop();
    m_discoveryHost.reset();
  }
  if (m_gameClient) {
    m_gameClient.reset();
  }
  if (m_gameServer) {
    m_gameServer->Stop();
  }
  if (m_serverLoopThd.joinable()) {
    m_serverLoopThd.join();
  }
  m_gameServer.reset();
}

void game_engine::Engine::submitLocalInput(NetGameInput input) {
  if (isMultiplayerActive() && m_gameClient) {
    if (m_gameClient) {
      m_localInput.playerID = m_gameClient->GetPlayerID();
    }
    m_localInput.leftHeld = input.leftHeld;
    m_localInput.rightHeld = input.rightHeld;
    m_localInput.fireHeld = input.fireHeld;
    m_localInput.jumpPressed = m_localInput.jumpPressed || input.jumpPressed;
    m_localInput.meleePressed = m_localInput.meleePressed || input.meleePressed;
    m_localInput.shouldSendMessage =
      m_localInput.leftHeld || m_localInput.rightHeld || m_localInput.fireHeld ||
      m_localInput.jumpPressed || m_localInput.meleePressed;
  } else {
    input.shouldSendMessage =
      input.leftHeld || input.rightHeld || input.fireHeld || input.jumpPressed || input.meleePressed;
    m_localInput = input;
  }
}

void game_engine::Engine::flushLocalInput(float deltaTime) {
  if (!isMultiplayerActive() || !m_gameClient || !m_gameClient->IsRegistered()) {
    return;
  }

  constexpr float kInputSendInterval = 1.0f / 60.0f;
  m_inputSendAccumulator += deltaTime;
  const bool hasEdgeInput = m_localInput.jumpPressed || m_localInput.meleePressed;
  if (m_inputSendAccumulator < kInputSendInterval && !hasEdgeInput) {
    return;
  }

  m_inputSendAccumulator = 0.0f;
  NetGameInput outgoing = m_localInput;
  outgoing.playerID = m_gameClient->GetPlayerID();
  outgoing.inputSeq = ++m_localInputSeq;
  outgoing.shouldSendMessage =
    outgoing.leftHeld || outgoing.rightHeld || outgoing.fireHeld ||
    outgoing.jumpPressed || outgoing.meleePressed;
  m_gameClient->SendInput(outgoing);

  m_localInput.jumpPressed = false;
  m_localInput.meleePressed = false;
  m_localInput.shouldSendMessage =
    m_localInput.leftHeld || m_localInput.rightHeld || m_localInput.fireHeld;
}

void game_engine::Engine::restartMultiplayerSession() {
  if (!isMultiplayerActive()) {
    return;
  }

  m_localInput = NetGameInput{};
  m_inputSendAccumulator = 0.0f;

  if (isHostMode() && m_gameServer) {
    synchronizeHostAuthoritativeState();
    m_gameState.currentView = UIManager::GameView::MultiplayerRespawnWait;
    m_gameServer->broadcastSnapshot();
    return;
  }

  if (isClientMode() && m_gameClient) {
    m_gameClient->RequestRespawn();
    m_gameState.currentView = UIManager::GameView::MultiplayerRespawnWait;
  }
}

void game_engine::Engine::synchronizeHostAuthoritativeState(bool refreshSpawnPositions) {
  if (!isHostMode() || !m_gameServer) {
    return;
  }

  m_localInput = NetGameInput{};
  m_inputSendAccumulator = 0.0f;
  auto authState = cloneAuthoritativeGameState(m_gameState, m_sdlState);
  m_gameServer->resetAuthoritativeState(std::move(authState), refreshSpawnPositions);
  if (m_gameClient) {
    m_gameClient->ClearLatestSnapshot();
  }
}

std::optional<LevelIndex> game_engine::Engine::consumePendingHostLevelTransition() {
  if (!isHostMode() || !m_gameServer) {
    return std::nullopt;
  }
  return m_gameServer->ConsumePendingLevelTransition();
}

void game_engine::Engine::broadcastHostSnapshot() {
  if (!isHostMode() || !m_gameServer) {
    return;
  }
  m_gameServer->broadcastSnapshot();
}

bool game_engine::Engine::copyHostSnapshot(NetGameStateSnapshot& out) const {
  if (!isHostMode() || !m_gameServer) {
    return false;
  }
  return m_gameServer->copyCurrentSnapshot(out);
}

std::vector<game_engine::DiscoveredSessionInfo> game_engine::Engine::copyDiscoveredSessions() const {
  if (!m_discoveryBrowser) {
    return {};
  }
  return m_discoveryBrowser->sessions();
}

bool game_engine::Engine::selectDiscoveredSession(size_t index) {
  if (!m_discoveryBrowser) {
    return false;
  }
  const auto& sessions = m_discoveryBrowser->sessions();
  if (index >= sessions.size()) {
    return false;
  }

  m_selectedJoinHost = sessions[index].hostAddress;
  m_selectedJoinPort = sessions[index].gamePort;
  m_hasSelectedJoinTarget = true;
  m_multiplayerStatus = "Selected host " + sessions[index].hostName;
  return true;
}

bool game_engine::Engine::hasSelectedJoinTarget() const {
  return m_hasSelectedJoinTarget;
}

bool game_engine::Engine::isServerReadyForDiscovery() const {
  return m_serverReadyForDiscovery;
}

const std::string& game_engine::Engine::getMultiplayerStatus() const {
  return m_multiplayerStatus;
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
  constexpr uint64_t kSnapshotEveryTicks = 2;
  auto prev = clock::now();
  double accum = 0.0;
  uint64_t tickCount = 0;

  // determine delta for server loop
  while (m_gameRunning.load() && m_serverLoopRunning.load() && m_gameServer) {
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
      if (tickCount % kSnapshotEveryTicks == 0) {
        m_gameServer->broadcastSnapshot();
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  m_serverLoopRunning.store(false);
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
