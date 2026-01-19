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

#include "net/net_message.h"



namespace game_engine {


  // use std::ByteWriter, ByteReader to write and read GameStateSnapshot
  // transfer the GameStateSnapshot to the game_engines GameState during renderLoop update

  enum class PlayerInput: uint16_t {
    None,
    Up,
    Left,
    Right,
    Down,
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
  };

  inline std::vector<uint8_t> serealizeNetGameInput(const NetGameInput& input) {
    net::ByteWriter bytes;

    bytes.write_u32(input.playerID);
    bytes.write_u32(input.tick);

    bytes.write_enum(input.move);
    bytes.write_enum(input.fireProjectile);
    bytes.write_enum(input.swingWeapon);
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
  }

  inline NetGameInput deserealizeNetGameInput(const std::vector<uint8_t>& bytes) {

    net::ByteReader reader(bytes);
    NetGameInput input;

    input.playerID = reader.read_u32();
    input.tick = reader.read_u32();
    input.move = reader.read_enum<PlayerInput>();
    input.fireProjectile = reader.read_enum<PlayerInput>();
    input.swingWeapon = reader.read_enum<PlayerInput>();

    return input;
  }


  // output body from the server.
  // after server consumes the input we update the GameState, and then periodically
  // we read from GameState and populate this NetGameStateSnapshot. We write to msg.body using the ByteWriter byte by byte in a fixed order.
  // On the client side we read from the byte buffer in the same order using ByteReader
  // and populate client side NetGameSnapShot. And then use that snapshot to update the clients GameState.

  struct NetGameObjectSnapshot {
    uint32_t id = 0;
    uint32_t layer; // flattened so need? or since all updateable objects are in the same layer may not need..
    uint32_t type;   //  ObjectType type;
    glm::vec2 position, velocity, acceleration;
    float direction;
    float maxSpeedX;
    // std::vector<Animation> animations; // keep this on each client
    uint32_t currentAnimation; // determined by the server
    // SDL_Texture *texture; // keep on each client
    // bool dynamic; // static property
    bool grounded;
    // float spritePixelW; // static property
    // float spritePixelH; // static property
    // SDL_FRect collider; // if server is determining collisions, dont need to send obj to client
    // Timer flashTimer; // determined by server, sends shouldFlash
    bool shouldFlash;
    uint32_t spriteFrame;

    ObjectData data; // this is a union
  };

  using GameObjectKey = std::pair<ObjectType, uint32_t>;
  struct GameObjectKeyHash {
    size_t operator()(const GameObjectKey& k) const noexcept {
      using U = std::underlying_type_t<ObjectType>;
      const auto h1 = std::hash<U>{}(static_cast<U>(k.first));
      const auto h2 = std::hash<uint32_t>{}(k.second);
      return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
  };

  struct NetGameStateSnapshot {
    uint64_t m_stateLastUpdatedAt; // when the gameState was last updated, by local or by server msg
    std::unordered_map<GameObjectKey, NetGameObjectSnapshot, GameObjectKeyHash> gameObjects;
    // std::vector<NetGameObjectSnapshot> m_gameObjects;
    // std::vector<NetGameObjectSnapshot> m_projectiles; // bullets
  };

// NetGameStateSnapshot extractSnapshotFromGameState(const GameState& gs) {
//     NetGameStateSnapshot snapshot;
//     snapshot.m_stateLastUpdatedAt = gs.m_stateLastUpdatedAt;

//     for (size_t layerIdx = 0; layerIdx < gs.layers.size(); ++layerIdx) {
//         for (const auto& obj : gs.layers[layerIdx]) {
//             NetGameObjectSnapshot s{};
//             s.id = obj.id;
//             s.layer = static_cast<uint32_t>(layerIdx);
//             s.type = static_cast<uint32_t>(obj.type);
//             s.position = obj.position;
//             s.velocity = obj.velocity;
//             s.acceleration = obj.acceleration;
//             s.direction = obj.direction;
//             s.maxSpeedX = obj.maxSpeedX;
//             s.currentAnimation = static_cast<uint32_t>(obj.currentAnimation);
//             s.grounded = obj.grounded;
//             s.shouldFlash = obj.shouldFlash;
//             s.spriteFrame = static_cast<uint32_t>(obj.spriteFrame);
//             s.data = obj.data; // union to be handled in encodeNetGameStateSnapshot
//             snapshot.gameObjects[{obj.type, obj.id}] = s;
//         };
//     };

//     for (const auto& obj : gs.bullets) {
//         NetGameObjectSnapshot s{};
//         s.id = obj.id;
//         s.layer = static_cast<uint32_t>(gs.playerLayer);
//         s.type = static_cast<uint32_t>(obj.type);
//         s.position = obj.position;
//         s.velocity = obj.velocity;
//         s.acceleration = obj.acceleration;
//         s.direction = obj.direction;
//         s.maxSpeedX = obj.maxSpeedX;
//         s.currentAnimation = static_cast<uint32_t>(obj.currentAnimation);
//         s.grounded = obj.grounded;
//         s.shouldFlash = obj.shouldFlash;
//         s.spriteFrame = static_cast<uint32_t>(obj.spriteFrame);
//         snapshot.gameObjects[{obj.type, obj.id}] = s;
//     };

//     return snapshot;
// }


  static constexpr std::uint16_t VERSION = 1;
  static constexpr std::uint16_t MSG_SNAPSHOT = 1;

  inline std::vector<std::uint8_t> encodeNetGameStateSnapshot(const NetGameStateSnapshot &s) {

    net::ByteWriter w;

    w.write_u16(VERSION);
    w.write_u16(MSG_SNAPSHOT);

    w.write_u64(s.m_stateLastUpdatedAt);

    // write the unordered_map
    // for (auto &[key, obj] : s.gameObjects) {

    //   switch (obj.type) {
    //     case ObjectType::Player: {

    //     }

    //   }
    // }

    return w.buff;


  }

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
