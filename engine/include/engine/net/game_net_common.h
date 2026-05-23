#pragma once

// #include <iostream>
// #include <vector>
#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <type_traits>
// #include <format>
// #include <array>
// #include <filesystem>

#include <SDL3/SDL.h>
#include <glm/glm.hpp>

#include "engine/gameobject.h"
#include "net/net_message.h"



namespace game_engine {

  static constexpr std::uint16_t VERSION = 9;
  static constexpr std::uint16_t MSG_SNAPSHOT = 1;

  // use std::ByteWriter, ByteReader to write and read GameStateSnapshot
  // transfer the GameStateSnapshot to the game_engines GameState during renderLoop update

  // input body from the client
  // read from message.body (byte array) using ByteReader
  // write from NetGameInput -> ByteWriter. pass the ByteWriterBuff as message.body
  struct NetGameInput {
    uint32_t playerID = 0;
    uint32_t inputSeq = 0;
    bool leftHeld = false;
    bool rightHeld = false;
    bool fireHeld = false;
    bool jumpPressed = false;
    bool meleePressed = false;
    bool ultimatePressed = false;
    bool shouldSendMessage = false; // not serialized; frame-local send hint only

    std::vector<uint8_t> serealizeNetGameInput() const {
      net::ByteWriter bytes;

      bytes.write_u32(playerID);
      bytes.write_u32(inputSeq);
      bytes.write_bool(leftHeld);
      bytes.write_bool(rightHeld);
      bytes.write_bool(fireHeld);
      bytes.write_bool(jumpPressed);
      bytes.write_bool(meleePressed);
      bytes.write_bool(ultimatePressed);

      return bytes.buff;
    };

    void deserealizeNetGameInput(const std::vector<uint8_t>& bytes) {

      net::ByteReader reader(bytes);

      playerID = reader.read_u32();
      inputSeq = reader.read_u32();
      leftHeld = reader.read_bool();
      rightHeld = reader.read_bool();
      fireHeld = reader.read_bool();
      jumpPressed = reader.read_bool();
      meleePressed = reader.read_bool();
      ultimatePressed = reader.read_bool();

    };
  };

  struct NetPlayerCommand {
    uint32_t playerID = 0;
    std::vector<uint8_t> payload;

    std::vector<uint8_t> serialize() const {
      net::ByteWriter writer;
      writer.write_u32(playerID);
      writer.write_u32(static_cast<uint32_t>(payload.size()));
      for (std::uint8_t byte : payload) {
        writer.write_u8(byte);
      }
      return writer.buff;
    }

    void deserialize(const std::vector<uint8_t>& bytes) {
      net::ByteReader reader(bytes);
      playerID = reader.read_u32();
      const uint32_t payloadSize = reader.read_u32();
      payload.clear();
      payload.reserve(payloadSize);
      for (uint32_t i = 0; i < payloadSize; ++i) {
        payload.push_back(reader.read_u8());
      }
    }
  };

  struct NetPersistedPlayerState {
    uint32_t coinCount = 0;
    uint32_t gemCount = 0;
    uint32_t healthPotionCount = 0;
    uint32_t manaPotionCount = 0;
    uint32_t attackUpCount = 0;
    uint32_t defenceUpCount = 0;
    int healthPoints = 100;
    int maxHealthPoints = 100;
    int manaPoints = 100;
    int maxManaPoints = 100;
    int ultimatePoints = 0;
    int maxUltimatePoints = 100;
    bool unlockedUltimateOne = false;
    int meleeDamage = 10;

    static NetPersistedPlayerState fromPlayerData(const PlayerData& player) {
      NetPersistedPlayerState state;
      state.coinCount = player.inventory.coins.count;
      state.gemCount = player.inventory.gems.count;
      state.healthPotionCount = player.inventory.healthPotions.count;
      state.manaPotionCount = player.inventory.manaPotions.count;
      state.attackUpCount = player.inventory.attackUps.count;
      state.defenceUpCount = player.inventory.defenceUps.count;
      state.healthPoints = player.healthPoints;
      state.maxHealthPoints = player.maxHealthPoints;
      state.manaPoints = player.manaPoints;
      state.maxManaPoints = player.maxManaPoints;
      state.ultimatePoints = player.ultimatePoints;
      state.maxUltimatePoints = player.maxUltimatePoints;
      state.unlockedUltimateOne = player.unlockedUltimateOne;
      state.meleeDamage = player.meleeDamage;
      return state;
    }

    void applyToPlayerData(PlayerData& player) const {
      player.inventory.coins.count = coinCount;
      player.inventory.gems.count = gemCount;
      player.inventory.healthPotions.count = healthPotionCount;
      player.inventory.manaPotions.count = manaPotionCount;
      player.inventory.attackUps.count = attackUpCount;
      player.inventory.defenceUps.count = defenceUpCount;
      player.healthPoints = healthPoints;
      player.maxHealthPoints = maxHealthPoints;
      player.manaPoints = manaPoints;
      player.maxManaPoints = maxManaPoints;
      player.ultimatePoints = ultimatePoints;
      player.maxUltimatePoints = maxUltimatePoints;
      player.unlockedUltimateOne = unlockedUltimateOne;
      player.meleeDamage = meleeDamage;
    }

    void writeTo(net::ByteWriter& writer) const {
      writer.write_u32(coinCount);
      writer.write_u32(gemCount);
      writer.write_u32(healthPotionCount);
      writer.write_u32(manaPotionCount);
      writer.write_u32(attackUpCount);
      writer.write_u32(defenceUpCount);
      writer.write_u32(static_cast<uint32_t>(healthPoints));
      writer.write_u32(static_cast<uint32_t>(maxHealthPoints));
      writer.write_u32(static_cast<uint32_t>(manaPoints));
      writer.write_u32(static_cast<uint32_t>(maxManaPoints));
      writer.write_u32(static_cast<uint32_t>(ultimatePoints));
      writer.write_u32(static_cast<uint32_t>(maxUltimatePoints));
      writer.write_bool(unlockedUltimateOne);
      writer.write_u32(static_cast<uint32_t>(meleeDamage));
    }

    void readFrom(net::ByteReader& reader) {
      coinCount = reader.read_u32();
      gemCount = reader.read_u32();
      healthPotionCount = reader.read_u32();
      manaPotionCount = reader.read_u32();
      attackUpCount = reader.read_u32();
      defenceUpCount = reader.read_u32();
      healthPoints = static_cast<int>(reader.read_u32());
      maxHealthPoints = static_cast<int>(reader.read_u32());
      manaPoints = static_cast<int>(reader.read_u32());
      maxManaPoints = static_cast<int>(reader.read_u32());
      ultimatePoints = static_cast<int>(reader.read_u32());
      maxUltimatePoints = static_cast<int>(reader.read_u32());
      unlockedUltimateOne = reader.read_bool();
      meleeDamage = static_cast<int>(reader.read_u32());
    }
  };


  // output body from the server.
  // after server consumes the input we update the GameState, and then periodically
  // we read from GameState and populate this NetGameStateSnapshot. We write to msg.body using the ByteWriter byte by byte in a fixed order.
  // On the client side we read from the byte buffer in the same order using ByteReader
  // and populate client side NetGameSnapShot. And then use that snapshot to update the clients GameState.

  struct NetGameObjectSnapshot {
    uint32_t id = 0;
    uint32_t layer; // flattened so need? or since all updateable objects are in the same layer may not need..
    ObjectClass type;   //  ObjectType type; uint32_t
    SpriteType spriteType = SpriteType::Player_Marie;
    glm::vec2 position, velocity, acceleration;
    uint32_t spriteFrame;
    uint32_t currentAnimation; // determined by the server
    float animElapsed = 0.0f;
    bool animTimedOut = false;
    PresentationVariant presentationVariant = PresentationVariant::Idle;
    float direction;
    float maxSpeedX;
    bool grounded;
    bool shouldFlash;
    // std::vector<Animation> animations; // keep this on each client
    // SDL_Texture *texture; // keep on each client
    // bool dynamic; // static property
    // float spritePixelW; // static property
    // float spritePixelH; // static property
    // SDL_FRect collider; // if server is determining collisions, dont need to send obj to client
    // Timer flashTimer; // determined by server, sends shouldFlash
    ObjectData data; // this is a union
  };

  using GameObjectKey = std::pair<ObjectClass, uint32_t>;
  struct GameObjectKeyHash {
    size_t operator()(const GameObjectKey& k) const noexcept {
      using U = std::underlying_type_t<ObjectClass>;
      const auto h1 = std::hash<U>{}(static_cast<U>(k.first));
      const auto h2 = std::hash<uint32_t>{}(k.second);
      return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
  };

  struct NetHitStopEvent {
    uint32_t sequence = 0;
    bool active = false;
    ObjectClass attackerClass = ObjectClass::Level;
    uint32_t attackerId = 0;
    ObjectClass victimClass = ObjectClass::Level;
    uint32_t victimId = 0;
    HitStopStrength strength = HitStopStrength::Normal;
  };

  struct NetGameStateSnapshot {
    uint64_t serverTick = 0;
    LevelIndex levelId = LevelIndex::LEVEL_1;
    uint64_t m_stateLastUpdatedAt; // when the gameState was last updated, by local or by server msg
    NetHitStopEvent hitStopEvent;
    std::unordered_map<GameObjectKey, NetGameObjectSnapshot, GameObjectKeyHash> m_gameObjects;
    // std::vector<NetGameObjectSnapshot> m_gameObjects;
    // std::vector<NetGameObjectSnapshot> m_projectiles; // bullets
    std::vector<std::uint8_t> serealizeNetGameStateSnapshot() const {

      net::ByteWriter w;

      w.write_u16(VERSION);
      w.write_u16(MSG_SNAPSHOT);

      w.write_u64(serverTick);
      w.write_enum<LevelIndex>(levelId);
      w.write_u64(m_stateLastUpdatedAt);
      w.write_u32(hitStopEvent.sequence);
      w.write_bool(hitStopEvent.active);
      w.write_enum<ObjectClass>(hitStopEvent.attackerClass);
      w.write_u32(hitStopEvent.attackerId);
      w.write_enum<ObjectClass>(hitStopEvent.victimClass);
      w.write_u32(hitStopEvent.victimId);
      w.write_enum<HitStopStrength>(hitStopEvent.strength);

      // write the unordered_map
      w.write_u32(m_gameObjects.size());
      for (auto &[key, obj] : m_gameObjects) {

          w.write_u32(obj.id); // std::pair<ObjectType, uint32_t>;
          w.write_u32(obj.layer);
          w.write_enum<ObjectClass>(obj.type); // std::pair<ObjectType, uint32_t>;
          w.write_enum<SpriteType>(obj.spriteType);
          w.write_glm_vec2(obj.position);
          w.write_glm_vec2(obj.velocity);
          w.write_glm_vec2(obj.acceleration);
          w.write_u32(obj.spriteFrame);
          w.write_u32(obj.currentAnimation);
          w.write_float(obj.animElapsed);
          w.write_bool(obj.animTimedOut);
          w.write_enum<PresentationVariant>(obj.presentationVariant);
          w.write_float(obj.direction);
          w.write_float(obj.maxSpeedX);
          w.write_bool(obj.grounded);
          w.write_bool(obj.shouldFlash);


        // for ObjectData Union
        switch (obj.type) {
          case ObjectClass::Player: {
            w.write_enum<PlayerState>(obj.data.player.state);
            w.write_u32(static_cast<uint32_t>(obj.data.player.healthPoints));
            w.write_u32(static_cast<uint32_t>(obj.data.player.maxHealthPoints));
            w.write_u32(static_cast<uint32_t>(obj.data.player.manaPoints));
            w.write_u32(static_cast<uint32_t>(obj.data.player.maxManaPoints));
            w.write_u32(static_cast<uint32_t>(obj.data.player.ultimatePoints));
            w.write_u32(static_cast<uint32_t>(obj.data.player.maxUltimatePoints));
            w.write_bool(obj.data.player.unlockedUltimateOne);
            w.write_u32(static_cast<uint32_t>(obj.data.player.meleeDamage));
            w.write_u32(obj.data.player.coinPickupCueCount);
            w.write_u32(obj.data.player.gemPickupCueCount);
            w.write_u32(obj.data.player.coinPurchaseCueCount);
            w.write_u32(obj.data.player.gemPurchaseCueCount);
            w.write_u32(obj.data.player.consumableUseCueCount);
            w.write_u32(obj.data.player.inventory.coins.count);
            w.write_u32(obj.data.player.inventory.gems.count);
            w.write_u32(obj.data.player.inventory.healthPotions.count);
            w.write_u32(obj.data.player.inventory.manaPotions.count);
            w.write_u32(obj.data.player.inventory.attackUps.count);
            w.write_u32(obj.data.player.inventory.defenceUps.count);
            break;
          }
          case ObjectClass::Projectile: {
            w.write_enum<BulletState>(obj.data.bullet.state);
            break;
          }
          case ObjectClass::Enemy: {
            w.write_enum<EnemyState>(obj.data.enemy.state);
            w.write_u32(static_cast<uint32_t>(obj.data.enemy.healthPoints));
            w.write_u32(static_cast<uint32_t>(obj.data.enemy.srcH));
            w.write_u32(static_cast<uint32_t>(obj.data.enemy.srcW));
            w.write_float(obj.data.enemy.hitStopRemainingSeconds);
            w.write_float(obj.data.enemy.pendingKnockbackDirection);
            w.write_float(obj.data.enemy.pendingKnockbackMagnitude);
            w.write_bool(obj.data.enemy.hasPendingKnockback);
            break;
          }
          case ObjectClass::Material: {
            w.write_u32(obj.data.material.count);
            w.write_u32(static_cast<uint32_t>(obj.data.material.state));
            w.write_u32(static_cast<uint32_t>(obj.data.material.type));
            break;
          }
          case ObjectClass::Level: {
            w.write_sdl_frect(obj.data.level.src);
            w.write_sdl_frect(obj.data.level.dst);
            break;
          }
          case ObjectClass::Portal:
          case ObjectClass::Background: {
            break;
          }
        }
      }

      return w.buff;
    };

    void deserealizeNetGameStateSnapshot(const std::vector<uint8_t>& bytes) {

      net::ByteReader r(bytes);

      auto version = r.read_u16();
      if (version != VERSION) throw std::runtime_error("bad message version");
      auto msg_snapshot = r.read_u16();
      if (msg_snapshot != MSG_SNAPSHOT) throw std::runtime_error("not a snapshot");
      serverTick = r.read_u64();
      levelId = r.read_enum<LevelIndex>();
      m_stateLastUpdatedAt = r.read_u64();
      hitStopEvent.sequence = r.read_u32();
      hitStopEvent.active = r.read_bool();
      hitStopEvent.attackerClass = r.read_enum<ObjectClass>();
      hitStopEvent.attackerId = r.read_u32();
      hitStopEvent.victimClass = r.read_enum<ObjectClass>();
      hitStopEvent.victimId = r.read_u32();
      hitStopEvent.strength = r.read_enum<HitStopStrength>();

      size_t length = r.read_u32(); // how many NetGameObjectSnapshot there are

      for (std::uint32_t idx = 0; idx < length; idx++) {
        NetGameObjectSnapshot obj;

        obj.id = r.read_u32(); // std::pair<ObjectType, uint32_t>;
        obj.layer = r.read_u32();
        obj.type = r.read_enum<ObjectClass>();
        obj.spriteType = r.read_enum<SpriteType>();
        obj.position = r.read_glm_vec2();
        obj.velocity = r.read_glm_vec2();
        obj.acceleration = r.read_glm_vec2();
        obj.spriteFrame = r.read_u32();
        obj.currentAnimation = r.read_u32();
        obj.animElapsed = r.read_float();
        obj.animTimedOut = r.read_bool();
        obj.presentationVariant = r.read_enum<PresentationVariant>();
        obj.direction = r.read_float();
        obj.maxSpeedX = r.read_float();
        obj.grounded = r.read_bool();
        obj.shouldFlash = r.read_bool();

        switch (obj.type) {
          case ObjectClass::Player: {
            new (&obj.data.player) PlayerData{}; // set active member
            obj.data.player.state = r.read_enum<PlayerState>();
            obj.data.player.healthPoints = static_cast<int>(r.read_u32());
            obj.data.player.maxHealthPoints = static_cast<int>(r.read_u32());
            obj.data.player.manaPoints = static_cast<int>(r.read_u32());
            obj.data.player.maxManaPoints = static_cast<int>(r.read_u32());
            obj.data.player.ultimatePoints = static_cast<int>(r.read_u32());
            obj.data.player.maxUltimatePoints = static_cast<int>(r.read_u32());
            obj.data.player.unlockedUltimateOne = r.read_bool();
            obj.data.player.meleeDamage = static_cast<int>(r.read_u32());
            obj.data.player.coinPickupCueCount = r.read_u32();
            obj.data.player.gemPickupCueCount = r.read_u32();
            obj.data.player.coinPurchaseCueCount = r.read_u32();
            obj.data.player.gemPurchaseCueCount = r.read_u32();
            obj.data.player.consumableUseCueCount = r.read_u32();
            obj.data.player.inventory.coins.count = r.read_u32();
            obj.data.player.inventory.gems.count = r.read_u32();
            obj.data.player.inventory.healthPotions.count = r.read_u32();
            obj.data.player.inventory.manaPotions.count = r.read_u32();
            obj.data.player.inventory.attackUps.count = r.read_u32();
            obj.data.player.inventory.defenceUps.count = r.read_u32();
            break;
          }
          case ObjectClass::Projectile: {
            new (&obj.data.bullet) BulletData{}; // set active member
            obj.data.bullet.state = r.read_enum<BulletState>();
            break;
          }
          case ObjectClass::Enemy: {
            new (&obj.data.enemy) EnemyData{}; // set active member
            obj.data.enemy.state = r.read_enum<EnemyState>();
            obj.data.enemy.healthPoints = r.read_u32();
            obj.data.enemy.srcH = r.read_u32();
            obj.data.enemy.srcW = r.read_u32();
            obj.data.enemy.hitStopRemainingSeconds = r.read_float();
            obj.data.enemy.pendingKnockbackDirection = r.read_float();
            obj.data.enemy.pendingKnockbackMagnitude = r.read_float();
            obj.data.enemy.hasPendingKnockback = r.read_bool();
            break;
          }
          case ObjectClass::Material: {
            new (&obj.data.material) MaterialData(0, MaterialType::none); // set active member
            obj.data.material.count = r.read_u32();
            obj.data.material.state = r.read_enum<MaterialState>();
            obj.data.material.type = r.read_enum<MaterialType>();
            break;
          }
          case ObjectClass::Level: {
            // already constructed as LevelData by default ctor; optional to reconstruct:
            new (&obj.data.level) LevelData{};
            obj.data.level.src = r.read_sdl_frect();
            obj.data.level.dst = r.read_sdl_frect();
            break;
          }
          case ObjectClass::Portal: // TODO
          case ObjectClass::Background: {
            new (&obj.data.level) LevelData{};
            break;
          }
        }
        m_gameObjects[{ obj.type, obj.id }] = obj;
      }
    };
  };



  enum class GameMsgHeaders : uint32_t {
    Server_GetStatus,
    Server_GetPing,

    Server_ShutdownOK,

    Client_Accepted,
    Client_AssignID,
    Client_RegisterWithServer,
    Client_UnregisterWithServer,

    Game_AddPlayer,
    Game_RemovePlayer,
    Game_UpdatePlayer,

    Game_Snapshot,
    Game_PlayerInput,
    Game_PlayerCommand,
    Game_PlayerRespawnRequest
  };

}
