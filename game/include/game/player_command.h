#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

#include "engine/gameobject.h"
#include "net/net_message.h"

namespace game {

enum class PlayerCommandType : std::uint8_t {
  ShopPurchase = 0,
  InventoryUse = 1,
};

enum class ShopPurchaseCommand : std::uint8_t {
  HealthPotion = 0,
  ManaPotion = 1,
  AttackUp = 2,
  DefenceUp = 3,
};

enum class InventoryUseCommand : std::uint8_t {
  HealthPotion = 0,
  ManaPotion = 1,
};

enum class PlayerCommandAudioCue : std::uint8_t {
  None = 0,
  CoinPurchase,
  GemPurchase,
  ConsumableUse,
};

struct PlayerCommand {
  PlayerCommandType type = PlayerCommandType::ShopPurchase;
  std::uint8_t value = 0;

  std::vector<std::uint8_t> serialize() const {
    net::ByteWriter writer;
    writer.write_u8(static_cast<std::uint8_t>(type));
    writer.write_u8(value);
    return writer.buff;
  }

  static std::optional<PlayerCommand> deserialize(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() != 2) {
      return std::nullopt;
    }

    net::ByteReader reader(bytes);
    PlayerCommand cmd;
    cmd.type = static_cast<PlayerCommandType>(reader.read_u8());
    cmd.value = reader.read_u8();
    return cmd;
  }
};

struct PlayerCommandResult {
  bool applied = false;
  PlayerCommandAudioCue audioCue = PlayerCommandAudioCue::None;
};

inline PlayerCommandResult applyPlayerCommand(PlayerData& player, const PlayerCommand& cmd) {
  auto& inventory = player.inventory;

  switch (cmd.type) {
    case PlayerCommandType::ShopPurchase: {
      MaterialData* currency = nullptr;
      MaterialData* item = nullptr;
      std::uint32_t cost = 0;
      PlayerCommandAudioCue audioCue = PlayerCommandAudioCue::None;

      switch (static_cast<ShopPurchaseCommand>(cmd.value)) {
        case ShopPurchaseCommand::HealthPotion:
          currency = &inventory.coins;
          item = &inventory.healthPotions;
          cost = 15;
          audioCue = PlayerCommandAudioCue::CoinPurchase;
          break;
        case ShopPurchaseCommand::ManaPotion:
          currency = &inventory.coins;
          item = &inventory.manaPotions;
          cost = 15;
          audioCue = PlayerCommandAudioCue::CoinPurchase;
          break;
        case ShopPurchaseCommand::AttackUp:
          currency = &inventory.gems;
          item = &inventory.attackUps;
          cost = 10;
          audioCue = PlayerCommandAudioCue::GemPurchase;
          break;
        case ShopPurchaseCommand::DefenceUp:
          currency = &inventory.gems;
          item = &inventory.defenceUps;
          cost = 10;
          audioCue = PlayerCommandAudioCue::GemPurchase;
          break;
      }

      if (!currency || !item || currency->count < cost) {
        return {};
      }

      currency->count -= cost;
      ++item->count;
      switch (audioCue) {
        case PlayerCommandAudioCue::CoinPurchase:
          ++player.coinPurchaseCueCount;
          break;
        case PlayerCommandAudioCue::GemPurchase:
          ++player.gemPurchaseCueCount;
          break;
        case PlayerCommandAudioCue::ConsumableUse:
        case PlayerCommandAudioCue::None:
          break;
      }
      return {.applied = true, .audioCue = audioCue};
    }
    case PlayerCommandType::InventoryUse: {
      MaterialData* item = nullptr;
      int* statValue = nullptr;
      int maxStatValue = 0;

      switch (static_cast<InventoryUseCommand>(cmd.value)) {
        case InventoryUseCommand::HealthPotion:
          item = &inventory.healthPotions;
          statValue = &player.healthPoints;
          maxStatValue = player.maxHealthPoints;
          break;
        case InventoryUseCommand::ManaPotion:
          item = &inventory.manaPotions;
          statValue = &player.manaPoints;
          maxStatValue = player.maxManaPoints;
          break;
      }

      if (!item || !statValue || item->count == 0 || *statValue >= maxStatValue) {
        return {};
      }

      *statValue = std::clamp(*statValue + 20, 0, maxStatValue);
      --item->count;
      ++player.consumableUseCueCount;
      return {.applied = true, .audioCue = PlayerCommandAudioCue::ConsumableUse};
    }
  }

  return {};
}

} // namespace game
