#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <asio.hpp>

#include "engine/level_types.h"
#include "net/net_message.h"

namespace game_engine {

inline constexpr uint16_t GAME_SERVER_PORT = 9000;
inline constexpr uint16_t LAN_DISCOVERY_PORT = 9001;

struct DiscoveryRequest {
  static constexpr uint32_t MAGIC = 0x4A434C41; // "AJCL"
  static constexpr uint16_t VERSION = 1;

  std::vector<uint8_t> serialize() const;
  bool deserialize(const std::vector<uint8_t>& bytes);
};

struct DiscoveryResponse {
  static constexpr uint32_t MAGIC = 0x4A434C42; // "BJCL"
  static constexpr uint16_t VERSION = 1;

  bool ready = false;
  uint16_t gamePort = GAME_SERVER_PORT;
  LevelIndex levelId = LevelIndex::LEVEL_1;
  uint32_t playerCount = 0;
  std::string hostName;

  std::vector<uint8_t> serialize() const;
  bool deserialize(const std::vector<uint8_t>& bytes);
};

struct DiscoveredSessionInfo {
  std::string hostName;
  std::string hostAddress;
  uint16_t gamePort = GAME_SERVER_PORT;
  LevelIndex levelId = LevelIndex::LEVEL_1;
  uint32_t playerCount = 0;
  uint64_t lastSeenAtMs = 0;
};

class DiscoveryHostService {
public:
  DiscoveryHostService() = default;
  ~DiscoveryHostService();

  bool start(uint16_t discoveryPort = LAN_DISCOVERY_PORT);
  void stop();
  void poll();

  bool isStarted() const { return m_started; }
  void setReady(bool ready) { m_ready = ready; }
  void updateInfo(std::string hostName, LevelIndex levelId, uint32_t playerCount, uint16_t gamePort);

private:
  asio::io_context m_context;
  std::unique_ptr<asio::ip::udp::socket> m_socket;
  uint16_t m_discoveryPort = LAN_DISCOVERY_PORT;
  bool m_started = false;
  bool m_ready = false;
  std::string m_hostName;
  uint16_t m_gamePort = GAME_SERVER_PORT;
  LevelIndex m_levelId = LevelIndex::LEVEL_1;
  uint32_t m_playerCount = 0;
};

class DiscoveryBrowserService {
public:
  DiscoveryBrowserService() = default;
  ~DiscoveryBrowserService();

  bool start(uint16_t discoveryPort = LAN_DISCOVERY_PORT);
  void stop();
  void poll();
  void clearSessions();

  bool isStarted() const { return m_started; }
  const std::vector<DiscoveredSessionInfo>& sessions() const { return m_sessions; }

private:
  void broadcastQuery(uint64_t nowMs);

  asio::io_context m_context;
  std::unique_ptr<asio::ip::udp::socket> m_socket;
  uint16_t m_discoveryPort = LAN_DISCOVERY_PORT;
  bool m_started = false;
  uint64_t m_lastBroadcastAtMs = 0;
  std::vector<DiscoveredSessionInfo> m_sessions;
};

} // namespace game_engine
