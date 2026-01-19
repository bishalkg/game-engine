#pragma once

// #include <iostream>
// #include <vector>
#include <string>
#include <vector>
// #include <format>
// #include <array>
// #include <filesystem>

#include "net/net_message.h"



namespace game_engine {


  // use std::ByteWriter, ByteReader to write and read GameStateSnapshot
  // transfer the GameStateSnapshot to the game_engines GameState during renderLoop update


    struct NetGameObjectSnapshot {
      uint32_t id;
      uint8_t layer; // flattened so need? or since all updateable objects are in the same layer may not need..
      uint8_t type;   //  ObjectType type;

      float vPos;



      //   ObjectType type;
      // ObjectData data; // by making this a union, the different object types can have different fields in their structs
      // glm::vec2 position, velocity, acceleration; // we have x and y positions/velocities/accelerations
      // float direction;
      // float maxSpeedX;
      // std::vector<Animation> animations;
      // int currentAnimation;
      // SDL_Texture *texture;
      // bool dynamic;
      // bool grounded;
      // float spritePixelW;
      // float spritePixelH;
      // SDL_FRect collider;
      // Timer flashTimer;
      // bool shouldFlash;
      // int spriteFrame;

  };

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
  struct NetGameStateSnapshot {
    uint64_t m_stateLastUpdatedAt; // when the gameState was last updated, by local or by server msg
    std::vector<NetGameObjectSnapshot> m_gameObjects;
    std::vector<NetGameObjectSnapshot> m_projectiles; // bullets
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
