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
  dst.texture = nullptr;
  return dst;
}

} // namespace

GameServer::GameServer(uint16_t nPort, std::unique_ptr<AuthoritativeContext> authCtx)
  : net::server_interface<GameMsgHeaders>(nPort),
    m_authCtx(std::move(authCtx)) {
  refreshSnapshot();
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
  removePlayer(client->GetID());
  m_vGarbageIDs.push_back(client->GetID());
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
        refreshSnapshot();
      }
      break;
    }
    case GameMsgHeaders::Client_UnregisterWithServer:
      removePlayer(client->GetID());
      break;
    case GameMsgHeaders::Game_PlayerInput: {
      NetGameInput input;
      input.deserealizeNetGameInput(msg.body);
      input.playerID = client->GetID();
      m_playerInputQueue.push_back(input);
      break;
    }
    default:
      break;
  }
}

GameObject* GameServer::findPlayerById(uint32_t playerID) {
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
  while (!m_playerInputQueue.empty()) {
    const NetGameInput input = m_playerInputQueue.pop_front();
    if (!m_authCtx) {
      continue;
    }
    m_authCtx->latestPlayerInputs[input.playerID] = input;
  }
}

void GameServer::step(float deltaTime) {
  if (!m_authCtx || !m_authCtx->state) {
    return;
  }

  auto& state = *m_authCtx->state;
  ++m_authCtx->serverTick;
  state.m_stateLastUpdatedAt = m_authCtx->serverTick;

  stepGameplaySimulation(state, m_authCtx->latestPlayerInputs, deltaTime);

  for (auto& [playerID, input] : m_authCtx->latestPlayerInputs) {
    input.jumpPressed = false;
    input.meleePressed = false;
    (void)playerID;
  }
}

void GameServer::refreshSnapshot() {
  if (!m_authCtx || !m_authCtx->state) {
    return;
  }
  m_currGameSnapshot = m_authCtx->state->extractNetSnapshot();
  m_currGameSnapshot.serverTick = m_authCtx->serverTick;
  m_currGameSnapshot.levelId = m_authCtx->state->currentLevelId;
}

void GameServer::resetAuthoritativeState(GameState&& initialState) {
  if (!m_authCtx) {
    m_authCtx = std::make_unique<AuthoritativeContext>(std::move(initialState));
    refreshSnapshot();
    return;
  }

  m_authCtx->state = std::make_unique<GameState>(std::move(initialState));
  m_authCtx->latestPlayerInputs.clear();
  while (!m_playerInputQueue.empty()) {
    m_playerInputQueue.pop_front();
  }

  auto& state = *m_authCtx->state;
  if (state.playerLayer < 0 || state.playerLayer >= static_cast<int>(state.layers.size())) {
    refreshSnapshot();
    return;
  }

  auto& layer = state.layers[state.playerLayer];
  auto templateIt = std::find_if(
    layer.begin(),
    layer.end(),
    [](const GameObject& obj) { return obj.objClass == ObjectClass::Player; });
  if (templateIt == layer.end()) {
    refreshSnapshot();
    return;
  }

  const GameObject templatePlayer = cloneGameObject(*templateIt);
  layer.erase(
    std::remove_if(
      layer.begin(),
      layer.end(),
      [](const GameObject& obj) { return obj.objClass == ObjectClass::Player; }),
    layer.end());

  std::vector<std::pair<uint32_t, SpriteType>> roster;
  roster.reserve(m_mapPlayerRoster.size());
  for (const auto& [playerID, snapshot] : m_mapPlayerRoster) {
    roster.emplace_back(playerID, snapshot.spriteType);
  }
  std::sort(roster.begin(), roster.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

  for (std::size_t idx = 0; idx < roster.size(); ++idx) {
    GameObject player = cloneGameObject(templatePlayer);
    player.id = roster[idx].first;
    player.spriteType = roster[idx].second;
    player.position.x += 48.0f * static_cast<float>(idx);
    player.velocity = glm::vec2(0.0f);
    player.acceleration = templatePlayer.acceleration;
    player.data.player = PlayerData();
    player.currentAnimation = ANIM_IDLE;
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
  }

  refreshSnapshot();
}

bool GameServer::registerPlayer(uint32_t playerID, SpriteType spriteType) {
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

  if (m_mapPlayerRoster.empty()) {
    templatePlayer->id = playerID;
    templatePlayer->spriteType = spriteType;
    m_mapPlayerRoster[playerID] =
      NetGameObjectSnapshot{.id = playerID, .type = ObjectClass::Player, .spriteType = spriteType};
    return true;
  }

  GameObject newPlayer = cloneGameObject(*templatePlayer);
  newPlayer.id = playerID;
  newPlayer.spriteType = spriteType;
  newPlayer.position.x += 48.0f * static_cast<float>(m_mapPlayerRoster.size());
  newPlayer.velocity = glm::vec2(0.0f);
  newPlayer.data.player = PlayerData();
  newPlayer.currentAnimation = ANIM_IDLE;
  state.layers[state.playerLayer].push_back(std::move(newPlayer));
  m_mapPlayerRoster[playerID] =
    NetGameObjectSnapshot{.id = playerID, .type = ObjectClass::Player, .spriteType = spriteType};
  return true;
}

void GameServer::removePlayer(uint32_t playerID) {
  if (!m_authCtx || !m_authCtx->state) {
    return;
  }

  auto& state = *m_authCtx->state;
  if (state.playerLayer >= 0 && state.playerLayer < static_cast<int>(state.layers.size())) {
    auto& layer = state.layers[state.playerLayer];
    layer.erase(
      std::remove_if(
        layer.begin(),
        layer.end(),
        [playerID](const GameObject& obj) {
          return obj.objClass == ObjectClass::Player && obj.id == playerID;
        }),
      layer.end());
  }

  m_authCtx->latestPlayerInputs.erase(playerID);
  m_mapPlayerRoster.erase(playerID);
  refreshSnapshot();
}

} // namespace game_engine
