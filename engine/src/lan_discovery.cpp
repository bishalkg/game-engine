#include "engine/net/lan_discovery.h"

#include <array>
#include <algorithm>

namespace game_engine {

namespace {

uint64_t nowMs() {
  using clock = std::chrono::steady_clock;
  return static_cast<uint64_t>(
    std::chrono::duration_cast<std::chrono::milliseconds>(clock::now().time_since_epoch()).count());
}

bool isWouldBlock(const asio::error_code& ec) {
  return ec == asio::error::would_block || ec == asio::error::try_again;
}

} // namespace

std::vector<uint8_t> DiscoveryRequest::serialize() const {
  net::ByteWriter writer;
  writer.write_u32(MAGIC);
  writer.write_u16(VERSION);
  return writer.buff;
}

bool DiscoveryRequest::deserialize(const std::vector<uint8_t>& bytes) {
  try {
    net::ByteReader reader(bytes);
    return reader.read_u32() == MAGIC && reader.read_u16() == VERSION;
  } catch (...) {
    return false;
  }
}

std::vector<uint8_t> DiscoveryResponse::serialize() const {
  net::ByteWriter writer;
  writer.write_u32(MAGIC);
  writer.write_u16(VERSION);
  writer.write_bool(ready);
  writer.write_u16(gamePort);
  writer.write_enum<LevelIndex>(levelId);
  writer.write_u32(playerCount);
  writer.write_string(hostName);
  return writer.buff;
}

bool DiscoveryResponse::deserialize(const std::vector<uint8_t>& bytes) {
  try {
    net::ByteReader reader(bytes);
    if (reader.read_u32() != MAGIC || reader.read_u16() != VERSION) {
      return false;
    }
    ready = reader.read_bool();
    gamePort = reader.read_u16();
    levelId = reader.read_enum<LevelIndex>();
    playerCount = reader.read_u32();
    hostName = reader.read_string();
    return true;
  } catch (...) {
    return false;
  }
}

DiscoveryHostService::~DiscoveryHostService() {
  stop();
}

bool DiscoveryHostService::start(uint16_t discoveryPort) {
  if (m_started) {
    return true;
  }

  asio::error_code ec;
  m_discoveryPort = discoveryPort;
  m_socket = std::make_unique<asio::ip::udp::socket>(m_context);
  m_socket->open(asio::ip::udp::v4(), ec);
  if (ec) {
    m_socket.reset();
    return false;
  }

  m_socket->set_option(asio::socket_base::reuse_address(true), ec);
  m_socket->bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), discoveryPort), ec);
  if (ec) {
    m_socket.reset();
    return false;
  }

  m_socket->non_blocking(true, ec);
  if (ec) {
    m_socket.reset();
    return false;
  }

  m_hostName = asio::ip::host_name(ec);
  if (ec || m_hostName.empty()) {
    m_hostName = "LAN Host";
  }
  m_started = true;
  return true;
}

void DiscoveryHostService::stop() {
  m_ready = false;
  m_started = false;
  if (m_socket) {
    asio::error_code ec;
    m_socket->close(ec);
    m_socket.reset();
  }
}

void DiscoveryHostService::updateInfo(
  std::string hostName,
  LevelIndex levelId,
  uint32_t playerCount,
  uint16_t gamePort) {
  if (!hostName.empty()) {
    m_hostName = std::move(hostName);
  }
  m_levelId = levelId;
  m_playerCount = playerCount;
  m_gamePort = gamePort;
}

void DiscoveryHostService::poll() {
  if (!m_started || !m_socket) {
    return;
  }

  std::array<uint8_t, 1024> recvBuffer{};
  for (;;) {
    asio::ip::udp::endpoint remote;
    asio::error_code ec;
    const size_t bytes = m_socket->receive_from(asio::buffer(recvBuffer), remote, 0, ec);
    if (ec) {
      if (isWouldBlock(ec)) {
        break;
      }
      break;
    }

    std::vector<uint8_t> payload(recvBuffer.begin(), recvBuffer.begin() + bytes);
    DiscoveryRequest request;
    if (!request.deserialize(payload) || !m_ready) {
      continue;
    }

    DiscoveryResponse response;
    response.ready = true;
    response.gamePort = m_gamePort;
    response.levelId = m_levelId;
    response.playerCount = m_playerCount;
    response.hostName = m_hostName;
    const auto responseBytes = response.serialize();
    m_socket->send_to(asio::buffer(responseBytes), remote, 0, ec);
  }
}

DiscoveryBrowserService::~DiscoveryBrowserService() {
  stop();
}

bool DiscoveryBrowserService::start(uint16_t discoveryPort) {
  if (m_started) {
    return true;
  }

  asio::error_code ec;
  m_discoveryPort = discoveryPort;
  m_socket = std::make_unique<asio::ip::udp::socket>(m_context);
  m_socket->open(asio::ip::udp::v4(), ec);
  if (ec) {
    m_socket.reset();
    return false;
  }

  m_socket->set_option(asio::socket_base::broadcast(true), ec);
  m_socket->set_option(asio::socket_base::reuse_address(true), ec);
  m_socket->bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), 0), ec);
  if (ec) {
    m_socket.reset();
    return false;
  }

  m_socket->non_blocking(true, ec);
  if (ec) {
    m_socket.reset();
    return false;
  }

  m_lastBroadcastAtMs = 0;
  m_sessions.clear();
  m_started = true;
  return true;
}

void DiscoveryBrowserService::stop() {
  m_started = false;
  m_lastBroadcastAtMs = 0;
  if (m_socket) {
    asio::error_code ec;
    m_socket->close(ec);
    m_socket.reset();
  }
  m_sessions.clear();
}

void DiscoveryBrowserService::clearSessions() {
  m_sessions.clear();
}

void DiscoveryBrowserService::broadcastQuery(uint64_t now) {
  if (!m_socket) {
    return;
  }
  constexpr uint64_t kBroadcastIntervalMs = 750;
  if (now - m_lastBroadcastAtMs < kBroadcastIntervalMs) {
    return;
  }

  m_lastBroadcastAtMs = now;
  const auto requestBytes = DiscoveryRequest{}.serialize();
  asio::error_code ec;

  const asio::ip::udp::endpoint broadcastEndpoint(
    asio::ip::address_v4::broadcast(),
    m_discoveryPort);
  m_socket->send_to(asio::buffer(requestBytes), broadcastEndpoint, 0, ec);

  const asio::ip::udp::endpoint loopbackEndpoint(
    asio::ip::make_address_v4("127.0.0.1"),
    m_discoveryPort);
  m_socket->send_to(asio::buffer(requestBytes), loopbackEndpoint, 0, ec);
}

void DiscoveryBrowserService::poll() {
  if (!m_started || !m_socket) {
    return;
  }

  const uint64_t now = nowMs();
  broadcastQuery(now);

  std::array<uint8_t, 1024> recvBuffer{};
  for (;;) {
    asio::ip::udp::endpoint remote;
    asio::error_code ec;
    const size_t bytes = m_socket->receive_from(asio::buffer(recvBuffer), remote, 0, ec);
    if (ec) {
      if (isWouldBlock(ec)) {
        break;
      }
      break;
    }

    std::vector<uint8_t> payload(recvBuffer.begin(), recvBuffer.begin() + bytes);
    DiscoveryResponse response;
    if (!response.deserialize(payload) || !response.ready) {
      continue;
    }

    const std::string hostAddress = remote.address().to_string();
    auto it = std::find_if(
      m_sessions.begin(),
      m_sessions.end(),
      [&](const DiscoveredSessionInfo& session) {
        return session.hostAddress == hostAddress && session.gamePort == response.gamePort;
      });

    if (it == m_sessions.end()) {
      m_sessions.push_back(DiscoveredSessionInfo{
        .hostName = response.hostName,
        .hostAddress = hostAddress,
        .gamePort = response.gamePort,
        .levelId = response.levelId,
        .playerCount = response.playerCount,
        .lastSeenAtMs = now,
      });
    } else {
      it->hostName = response.hostName;
      it->levelId = response.levelId;
      it->playerCount = response.playerCount;
      it->lastSeenAtMs = now;
    }
  }

  constexpr uint64_t kSessionExpiryMs = 2500;
  m_sessions.erase(
    std::remove_if(
      m_sessions.begin(),
      m_sessions.end(),
      [now](const DiscoveredSessionInfo& session) {
        return now - session.lastSeenAtMs > kSessionExpiryMs;
      }),
    m_sessions.end());
}

} // namespace game_engine
