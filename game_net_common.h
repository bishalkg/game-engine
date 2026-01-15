#pragma once

// #include <iostream>
// #include <vector>
#include <string>
// #include <format>s
// #include <array>
// #include <filesystem>



namespace game_engine {


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

  struct NetGameStateSnapshot {
    uint64_t m_stateLastUpdatedAt; // when the gameState was last updated, by local or by server msg
    std::vector<NetGameObjectSnapshot> m_gameObjects;
    std::vector<NetGameObjectSnapshot> m_projectiles; // bullets
  };

  enum class GameMsgHeaders : uint32_t {
    Server_GetStatus,
    Server_GetPing,

    Client_Accepted,
    Client_AssignID,
    Client_RegisterWithServer,
    Client_UnregisterWithServer,

    Game_AddPlayer,
    Game_RemovePlayer,
    Game_UpdatePlayer,
  };

}