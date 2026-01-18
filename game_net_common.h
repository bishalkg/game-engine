#pragma once

// #include <iostream>
// #include <vector>
#include <string>
// #include <format>
// #include <array>
// #include <filesystem>



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

  enum PlayerInput {
    None,
    Up,
    Left,
    Right,
    Down
  };

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
    Game_MovePlayer
  };

}