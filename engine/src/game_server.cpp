#include "engine/net/game_server.h"

#include <algorithm>

#include "engine/engine.h"
#include "engine/gameplay_simulation.h"

namespace game_engine {

AuthoritativeContext::AuthoritativeContext(GameState&& initialState)
  : state(std::make_unique<GameState>(std::move(initialState))) {}

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
  dst.dynamic = src.dynamic;
  dst.grounded = src.grounded;
  dst.drawScale = src.drawScale;
  dst.spritePixelW = src.spritePixelW;
  dst.spritePixelH = src.spritePixelH;
  dst.baseCollider = src.baseCollider;
  dst.collider = src.collider;
  dst.colliderNorm = src.colliderNorm;
  dst.flashTimer = src.flashTimer;
  dst.shouldFlash = src.shouldFlash;
  dst.spriteFrame = src.spriteFrame;
  dst.renderPosition = src.renderPosition;
  dst.renderPositionInitialized = src.renderPositionInitialized;
  dst.texture = nullptr;
  return dst;
}

void resetPlayerRuntimeStatePreservingUnlocks(PlayerData& playerData) {
  const bool unlockedUltimateOne = playerData.unlockedUltimateOne;
  playerData = PlayerData();
  playerData.unlockedUltimateOne = unlockedUltimateOne;
}

void markPlayerDeadFromFall(GameObject& player) {
  if (player.objClass != ObjectClass::Player ||
      player.data.player.state == PlayerState::dead) {
    return;
  }

  player.data.player.healthPoints = 0;
  player.data.player.state = PlayerState::dead;
  player.presentationVariant = PresentationVariant::Die;
  player.currentAnimation = ANIM_DIE;
  if (player.currentAnimation >= 0 &&
      player.currentAnimation < static_cast<int>(player.animations.size())) {
    player.animations[player.currentAnimation].reset();
  }
  player.spriteFrame = 1;
  player.velocity = glm::vec2(0.0f);
}

} // namespace

GameServer::GameServer(uint16_t nPort, std::unique_ptr<AuthoritativeContext> authCtx)
  : net::server_interface<GameMsgHeaders>(nPort),
    m_authCtx(std::move(authCtx)) {
  refreshGameSnapshot();
}

bool GameServer::OnClientConnect(std::shared_ptr<net::connection<GameMsgHeaders>> client) {
  (void)client;
  return true;
}

void GameServer::OnClientValidated(std::shared_ptr<net::connection<GameMsgHeaders>> client) {
  net::message<GameMsgHeaders> msg;
  msg.header.id = GameMsgHeaders::Client_Accepted;
  client->Send(msg);
}

void GameServer::OnClientDisconnect(std::shared_ptr<net::connection<GameMsgHeaders>> client) {
  if (!client) {
    return;
  }
  if (removePlayer(client->GetID())) {
    m_vGarbageIDs.push_back(client->GetID());
  }
}

void GameServer::OnMessage(
  std::shared_ptr<net::connection<GameMsgHeaders>> client,
  net::message<GameMsgHeaders>& msg) {
  if (!client) {
    return;
  }

  switch (msg.header.id) {
    case GameMsgHeaders::Client_RegisterWithServer: {
      net::ByteReader reader(msg.body);
      const SpriteType spriteType = reader.read_enum<SpriteType>();
      if (registerPlayer(client->GetID(), spriteType)) {
        net::message<GameMsgHeaders> reply;
        reply.header.id = GameMsgHeaders::Client_AssignID;
        net::ByteWriter writer;
        writer.write_u32(client->GetID());
        reply.body = std::move(writer.buff);
        reply.header.bodySize = reply.body.size();
        MessageClient(client, reply);
        broadcastSnapshot();
      }
      break;
    }
    case GameMsgHeaders::Client_UnregisterWithServer:
      if (removePlayer(client->GetID())) {
        broadcastSnapshot();
      }
      break;
    case GameMsgHeaders::Game_PlayerInput: {
      NetGameInput input;
      input.deserealizeNetGameInput(msg.body);
      input.playerID = client->GetID();
      m_playerInputQueue.push_back(input);
      break;
    }
    case GameMsgHeaders::Game_PlayerRespawnRequest:
      if (respawnPlayer(client->GetID())) {
        broadcastSnapshot();
      }
      break;
    default:
      break;
  }
}

GameObject* GameServer::findPlayerById(uint32_t playerID) {
  std::scoped_lock lock(m_stateMu);
  if (!m_authCtx || !m_authCtx->state) {
    return nullptr;
  }

  auto& state = *m_authCtx->state;
  if (state.playerLayer < 0 || state.playerLayer >= static_cast<int>(state.layers.size())) {
    return nullptr;
  }

  for (auto& obj : state.layers[state.playerLayer]) {
    if (obj.objClass == ObjectClass::Player && obj.id == playerID) {
      return &obj;
    }
  }
  return nullptr;
}

void GameServer::applyPlayerInputs() {
  std::scoped_lock lock(m_stateMu);
  while (!m_playerInputQueue.empty()) {
    const NetGameInput input = m_playerInputQueue.pop_front();
    if (!m_authCtx) {
      continue;
    }
    auto sessionIt = m_playerSessions.find(input.playerID);
    if (sessionIt == m_playerSessions.end()) {
      continue;
    }
    if (input.inputSeq <= sessionIt->second.lastInputSeq) {
      continue;
    }
    sessionIt->second.lastInputSeq = input.inputSeq;
    m_authCtx->latestPlayerInputs[input.playerID] = input; // store only the latest input from each player
  }
}

void GameServer::step(float deltaTime) {
  std::scoped_lock lock(m_stateMu);
  if (!m_authCtx || !m_authCtx->state) {
    return;
  }

  GameState& state = *m_authCtx->state;
  ++m_authCtx->serverTick;
  state.m_stateLastUpdatedAt = m_authCtx->serverTick;

  GameplaySimulationHooks hooks;
  hooks.onPortalTriggered = [this](LevelIndex nextLevel) {
    std::scoped_lock lock(m_pendingLevelTransitionMu);
    if (!m_pendingLevelTransition.has_value()) {
      m_pendingLevelTransition = nextLevel;
    }
  };
  hooks.onHitConfirmed =
    [this](GameObjectKey attacker, GameObjectKey victim, HitStopStrength strength) {
      m_latestHitStopEvent.sequence = m_nextHitStopSequence++;
      m_latestHitStopEvent.active = true;
      m_latestHitStopEvent.attackerClass = attacker.first;
      m_latestHitStopEvent.attackerId = attacker.second;
      m_latestHitStopEvent.victimClass = victim.first;
      m_latestHitStopEvent.victimId = victim.second;
      m_latestHitStopEvent.strength = strength;
      m_hitStopEventDirty = true;
    };
  stepGameplaySimulation(state, m_authCtx->latestPlayerInputs, deltaTime, hooks);

  for (auto& [playerID, session] : m_playerSessions) {
    (void)session;
    if (GameObject* player = findPlayerById(playerID)) {
      if (!player->grounded && player->position.y > 1500.0f) {
        markPlayerDeadFromFall(*player);
      }
    }
  }

  for (auto& [playerID, input] : m_authCtx->latestPlayerInputs) {
    input.jumpPressed = false;
    input.meleePressed = false;
    input.ultimatePressed = false;
    (void)playerID;
  }

  for (auto& [playerID, session] : m_playerSessions) {
    if (GameObject* player = findPlayerById(playerID)) {
      session.lifecycle =
        player->data.player.state == PlayerState::dead ? PlayerSessionState::dead
                                                       : PlayerSessionState::alive;
    }
  }
}

void GameServer::refreshGameSnapshot() {
  std::scoped_lock lock(m_stateMu);
  if (!m_authCtx || !m_authCtx->state) {
    return;
  }
  m_currGameSnapshot = m_authCtx->state->extractNetSnapshot();
  m_currGameSnapshot.serverTick = m_authCtx->serverTick;
  m_currGameSnapshot.levelId = m_authCtx->state->currentLevelId;
  if (m_hitStopEventDirty) {
    m_currGameSnapshot.hitStopEvent = m_latestHitStopEvent;
    m_hitStopEventDirty = false;
  } else {
    m_currGameSnapshot.hitStopEvent.active = false;
  }
}

void GameServer::broadcastSnapshot() {
  refreshGameSnapshot();
  net::message<GameMsgHeaders> msg;
  msg.header.id = GameMsgHeaders::Game_Snapshot;
  msg.body = m_currGameSnapshot.serealizeNetGameStateSnapshot();
  msg.header.bodySize = msg.body.size();
  BroadcastToClients(msg);
}

bool GameServer::copyCurrentSnapshot(NetGameStateSnapshot& out) const {
  std::scoped_lock lock(m_stateMu);
  if (!m_authCtx || !m_authCtx->state) {
    return false;
  }
  out = m_currGameSnapshot;
  return true;
}

// rebuilds the host/server’s authoritative game world from a fresh GameState, while preserving the currently connected multiplayer roster.
void GameServer::resetAuthoritativeState(GameState&& initialState, bool refreshSpawnPositions) {
  std::scoped_lock lock(m_stateMu);
  if (!m_authCtx) {
    m_authCtx = std::make_unique<AuthoritativeContext>(std::move(initialState));
    refreshGameSnapshot();
    return;
  }

  m_authCtx->state = std::make_unique<GameState>(std::move(initialState));
  m_authCtx->latestPlayerInputs.clear();
  while (!m_playerInputQueue.empty()) {
    m_playerInputQueue.pop_front();
  }
  m_latestHitStopEvent = {};
  m_hitStopEventDirty = false;
  m_nextHitStopSequence = 1;
  {
    std::scoped_lock lock(m_pendingLevelTransitionMu);
    m_pendingLevelTransition.reset();
  }

  auto& state = *m_authCtx->state;
  if (state.playerLayer < 0 || state.playerLayer >= static_cast<int>(state.layers.size())) {
    refreshGameSnapshot();
    return;
  }

  auto& layer = state.layers[state.playerLayer];
  auto templateIt = std::find_if(
    layer.begin(),
    layer.end(),
    [](const GameObject& obj) { return obj.objClass == ObjectClass::Player; });
  if (templateIt == layer.end()) {
    refreshGameSnapshot();
    return;
  }

  const GameObject templatePlayer = cloneGameObject(*templateIt);
  layer.erase(
    std::remove_if(
      layer.begin(),
      layer.end(),
      [](const GameObject& obj) { return obj.objClass == ObjectClass::Player; }),
    layer.end());

  std::vector<std::pair<uint32_t, PlayerSession>> roster;
  roster.reserve(m_playerSessions.size());
  for (const auto& [playerID, session] : m_playerSessions) {
    roster.emplace_back(playerID, session);
  }
  std::sort(roster.begin(), roster.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

  for (std::size_t idx = 0; idx < roster.size(); ++idx) {
    if (refreshSpawnPositions) {
      m_playerSessions[roster[idx].first].spawnPosition = templatePlayer.position;
      m_playerSessions[roster[idx].first].spawnPosition.x += 48.0f * static_cast<float>(idx);
    }

    GameObject player = cloneGameObject(templatePlayer);
    player.id = roster[idx].first;
    player.spriteType = roster[idx].second.spriteType;
    player.position = m_playerSessions[roster[idx].first].spawnPosition;
    player.velocity = glm::vec2(0.0f);
    player.acceleration = templatePlayer.acceleration;
    player.data.player = templatePlayer.data.player;
    player.currentAnimation = ANIM_IDLE;
    player.presentationVariant = PresentationVariant::Idle;
    if (player.currentAnimation >= 0 &&
        player.currentAnimation < static_cast<int>(player.animations.size())) {
      player.animations[player.currentAnimation].reset();
    }
    player.spriteFrame = 1;
    player.shouldFlash = false;
    player.flashTimer.reset();
    player.collider = player.baseCollider;
    player.direction = 1.0f;
    layer.push_back(std::move(player));
    m_playerSessions[roster[idx].first].lifecycle = PlayerSessionState::alive;
  }

  refreshGameSnapshot();
}

bool GameServer::registerPlayer(uint32_t playerID, SpriteType spriteType) {
  std::scoped_lock lock(m_stateMu);
  if (!m_authCtx || !m_authCtx->state) {
    return false;
  }

  if (findPlayerById(playerID)) {
    return true;
  }

  auto& state = *m_authCtx->state;
  if (state.playerLayer < 0 || state.playerLayer >= static_cast<int>(state.layers.size())) {
    return false;
  }

  GameObject* templatePlayer = nullptr;
  for (auto& obj : state.layers[state.playerLayer]) {
    if (obj.objClass == ObjectClass::Player) {
      templatePlayer = &obj;
      break;
    }
  }
  if (!templatePlayer) {
    return false;
  }

  if (m_playerSessions.empty()) {
    const glm::vec2 spawnPosition = templatePlayer->position;
    templatePlayer->id = playerID;
    templatePlayer->spriteType = spriteType;
    resetPlayerRuntimeStatePreservingUnlocks(templatePlayer->data.player);
    templatePlayer->velocity = glm::vec2(0.0f);
    templatePlayer->currentAnimation = ANIM_IDLE;
    templatePlayer->presentationVariant = PresentationVariant::Idle;
    templatePlayer->spriteFrame = 1;
    m_playerSessions[playerID] = PlayerSession{
      .spriteType = spriteType,
      .lifecycle = PlayerSessionState::alive,
      .lastInputSeq = 0,
      .spawnPosition = spawnPosition,
    };
    return true;
  }

  GameObject newPlayer = cloneGameObject(*templatePlayer);
  newPlayer.id = playerID;
  newPlayer.spriteType = spriteType;
  newPlayer.position.x += 48.0f * static_cast<float>(m_playerSessions.size());
  newPlayer.velocity = glm::vec2(0.0f);
  resetPlayerRuntimeStatePreservingUnlocks(newPlayer.data.player);
  newPlayer.currentAnimation = ANIM_IDLE;
  newPlayer.presentationVariant = PresentationVariant::Idle;
  state.layers[state.playerLayer].push_back(std::move(newPlayer));
  m_playerSessions[playerID] = PlayerSession{
    .spriteType = spriteType,
    .lifecycle = PlayerSessionState::alive,
    .lastInputSeq = 0,
    .spawnPosition = state.layers[state.playerLayer].back().position,
  };
  return true;
}

bool GameServer::respawnPlayer(uint32_t playerID) {
  std::scoped_lock lock(m_stateMu);
  if (!m_authCtx || !m_authCtx->state) {
    return false;
  }

  auto sessionIt = m_playerSessions.find(playerID);
  if (sessionIt == m_playerSessions.end()) {
    return false;
  }

  GameObject* player = findPlayerById(playerID);
  if (!player) {
    return false;
  }

  sessionIt->second.lifecycle = PlayerSessionState::respawning;
  player->position = sessionIt->second.spawnPosition;
  player->velocity = glm::vec2(0.0f);
  resetPlayerRuntimeStatePreservingUnlocks(player->data.player);
  player->shouldFlash = false;
  player->flashTimer.reset();
  player->grounded = false;
  player->direction = 1.0f;
  player->currentAnimation = ANIM_IDLE;
  player->presentationVariant = PresentationVariant::Idle;
  if (player->currentAnimation >= 0 &&
      player->currentAnimation < static_cast<int>(player->animations.size())) {
    player->animations[player->currentAnimation].reset();
  }
  player->spriteFrame = 1;
  player->collider = player->baseCollider;
  player->renderPosition = player->position;
  player->renderPositionInitialized = true;
  sessionIt->second.lifecycle = PlayerSessionState::alive;
  m_authCtx->latestPlayerInputs[playerID] = NetGameInput{
    .playerID = playerID,
    .inputSeq = sessionIt->second.lastInputSeq,
  };
  return true;
}

bool GameServer::removePlayer(uint32_t playerID) {
  std::scoped_lock lock(m_stateMu);
  if (!m_authCtx || !m_authCtx->state) {
    return false;
  }

  bool changed = false;
  auto& state = *m_authCtx->state;
  if (state.playerLayer >= 0 && state.playerLayer < static_cast<int>(state.layers.size())) {
    auto& layer = state.layers[state.playerLayer];
    const auto oldSize = layer.size();
    layer.erase(
      std::remove_if(
        layer.begin(),
        layer.end(),
        [playerID](const GameObject& obj) {
          return obj.objClass == ObjectClass::Player && obj.id == playerID;
        }),
      layer.end());
    changed = changed || layer.size() != oldSize;
  }

  changed = m_authCtx->latestPlayerInputs.erase(playerID) > 0 || changed;
  changed = m_playerSessions.erase(playerID) > 0 || changed;
  return changed;
}

bool GameServer::HasPendingLevelTransition() const {
  std::scoped_lock lock(m_pendingLevelTransitionMu);
  return m_pendingLevelTransition.has_value();
}

std::optional<LevelIndex> GameServer::ConsumePendingLevelTransition() {
  std::scoped_lock lock(m_pendingLevelTransitionMu);
  if (!m_pendingLevelTransition.has_value()) {
    return std::nullopt;
  }

  const LevelIndex nextLevel = *m_pendingLevelTransition;
  m_pendingLevelTransition.reset();
  return nextLevel;
}

} // namespace game_engine
