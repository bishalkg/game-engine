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

#include "gameobject.h"
#include "net/net_message.h"



namespace game_engine {

  static constexpr std::uint16_t VERSION = 1;
  static constexpr std::uint16_t MSG_SNAPSHOT = 1;

  // use std::ByteWriter, ByteReader to write and read GameStateSnapshot
  // transfer the GameStateSnapshot to the game_engines GameState during renderLoop update

  enum class PlayerInput: uint16_t {
    None,
    Jump,
    MoveLeft,
    MoveRight,
    MoveDown,
    Fire,
    Swing,
  };

  // input body from the client
  // read from message.body (byte array) using ByteReader
  // write from NetGameInput -> ByteWriter. pass the ByteWriterBuff as message.body
  struct NetGameInput { // can there be multiple inputs at a time?
    //movePlayer. left, right, up keys.
    //fireBullet. scancodeA
    //swingSword. scancodeS (to add)
    uint32_t playerID; // is msg.header.id already player id? no its the GameMsgHeaders
    uint32_t tick;
    PlayerInput move = PlayerInput::None;
    PlayerInput fireProjectile = PlayerInput::None; // PlayerInput::Fire
    PlayerInput swingWeapon = PlayerInput::None; // PlayerInput::Swing

    bool shouldSendMessage = false; // not serealized, only used to indicate whether message is ready to be sent
    std::vector<uint8_t> serealizeNetGameInput() const {
      net::ByteWriter bytes;

      bytes.write_u32(playerID);
      bytes.write_u32(tick);

      bytes.write_enum(move);
      bytes.write_enum(fireProjectile);
      bytes.write_enum(swingWeapon);
      // if (input.move != PlayerInput::None) {
      //   bytes.write_enum(input.move);
      // }
      // if (input.fireProjectile != PlayerInput::None) {
      //   bytes.write_enum(input.fireProjectile);
      // }
      // if (input.swingWeapon != PlayerInput::None) {
      //   bytes.write_enum(input.swingWeapon);
      // }

      return bytes.buff;
    };

    void deserealizeNetGameInput(const std::vector<uint8_t>& bytes) {

      net::ByteReader reader(bytes);

      playerID = reader.read_u32();
      tick = reader.read_u32();
      move = reader.read_enum<PlayerInput>();
      fireProjectile = reader.read_enum<PlayerInput>();
      swingWeapon = reader.read_enum<PlayerInput>();

    };
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
    glm::vec2 position, velocity, acceleration;
    uint32_t spriteFrame;
    uint32_t currentAnimation; // determined by the server
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

  struct NetGameStateSnapshot {
    uint64_t m_stateLastUpdatedAt; // when the gameState was last updated, by local or by server msg
    std::unordered_map<GameObjectKey, NetGameObjectSnapshot, GameObjectKeyHash> m_gameObjects;
    // std::vector<NetGameObjectSnapshot> m_gameObjects;
    // std::vector<NetGameObjectSnapshot> m_projectiles; // bullets
    std::vector<std::uint8_t> serealizeNetGameStateSnapshot() const {

      net::ByteWriter w;

      w.write_u16(VERSION);
      w.write_u16(MSG_SNAPSHOT);

      w.write_u64(m_stateLastUpdatedAt);

      // write the unordered_map
      w.write_u32(m_gameObjects.size());
      for (auto &[key, obj] : m_gameObjects) {

          w.write_u32(obj.id); // std::pair<ObjectType, uint32_t>;
          w.write_u32(obj.layer);
          w.write_enum<ObjectClass>(obj.type); // std::pair<ObjectType, uint32_t>;
          w.write_glm_vec2(obj.position);
          w.write_glm_vec2(obj.velocity);
          w.write_glm_vec2(obj.acceleration);
          w.write_u32(obj.spriteFrame);
          w.write_u32(obj.currentAnimation);
          w.write_float(obj.direction);
          w.write_float(obj.maxSpeedX);
          w.write_bool(obj.grounded);
          w.write_bool(obj.shouldFlash);


        // for ObjectData Union
        switch (obj.type) {
          case ObjectClass::Player: {
            w.write_enum<PlayerState>(obj.data.player.state);
            w.write_u32(static_cast<uint32_t>(obj.data.player.healthPoints));
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
            break;
          }
          case ObjectClass::Level: {
            w.write_sdl_frect(obj.data.level.src);
            w.write_sdl_frect(obj.data.level.dst);
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
      m_stateLastUpdatedAt = r.read_u64();

      size_t length = r.read_u32(); // how many NetGameObjectSnapshot there are

      for (std::uint32_t idx = 0; idx < length; idx++) {
        NetGameObjectSnapshot obj;

        obj.id = r.read_u32(); // std::pair<ObjectType, uint32_t>;
        obj.layer = r.read_u32();
        obj.type = r.read_enum<ObjectClass>();
        obj.position = r.read_glm_vec2();
        obj.velocity = r.read_glm_vec2();
        obj.acceleration = r.read_glm_vec2();
        obj.spriteFrame = r.read_u32();
        obj.currentAnimation = r.read_u32();
        obj.direction = r.read_float();
        obj.maxSpeedX = r.read_float();
        obj.grounded = r.read_bool();
        obj.shouldFlash = r.read_bool();

        switch (obj.type) {
          case ObjectClass::Player: {
            new (&obj.data.player) PlayerData{}; // set active member
            obj.data.player.state = r.read_enum<PlayerState>();
            obj.data.player.healthPoints = r.read_u32();
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
            break;
          }
          case ObjectClass::Level: {
            // already constructed as LevelData by default ctor; optional to reconstruct:
            new (&obj.data.level) LevelData{};
            obj.data.level.src = r.read_sdl_frect();
            obj.data.level.dst = r.read_sdl_frect();
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
    Game_PlayerInput
  };

}
