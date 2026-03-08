#pragma once

#include "net/net_server.h"
#include "engine/net/game_net_common.h"



namespace game_engine {

  // forward declar
  struct GameState;
  struct Resources;

  class GameServer : public net::server_interface<GameMsgHeaders> {
    public:
      GameServer(uint16_t nPort, game_engine::GameState& gs, game_engine::Resources& res);

      std::unordered_map<uint32_t, NetGameObjectSnapshot> m_mapPlayerRoster;
      std::vector<uint32_t> m_vGarbageIDs;

      NetGameStateSnapshot m_currGameSnapshot;

      net::tsqueue<NetGameInput> m_playerInputQueue;

      GameState& m_currGameState;
      Resources& m_headlessResources;

    protected:
      bool OnClientConnect(std::shared_ptr<net::connection<GameMsgHeaders>> client) override;

      void OnClientValidated(std::shared_ptr<net::connection<GameMsgHeaders>> client) override;

      void OnClientDisconnect(std::shared_ptr<net::connection<GameMsgHeaders>> client) override;

      // OnMessage is an override that is called in .ProcessIncomingMessages on the server
      void OnMessage(std::shared_ptr<net::connection<GameMsgHeaders>> client, net::message<GameMsgHeaders>& msg) override;
      // {
      //     if (!m_vGarbageIDs.empty()) {
      //       for (auto pid : m_vGarbageIDs)
      //       {
      //         net::message<GameMsgHeaders> m;
      //         m.header.id = GameMsgHeaders::Game_RemovePlayer;
      //         m << pid;
      //         std::cout << "Removing " << pid << "\n";
      //         BroadcastToClients(m);
      //       }
      //       m_vGarbageIDs.clear();
      //     }



      //   switch (msg.header.id) {
      //     case GameMsgHeaders::Client_RegisterWithServer:
      //       {
      //         // NetGameObjectSnapshot desc;
      //         // msg >> desc;
      //         // desc.id = client->GetID();
      //         // m_mapPlayerRoster.insert_or_assign(desc.id, desc);

      //         // net::message<GameMsgHeaders> msgSendID;
      //         // msgSendID.header.id = GameMsgHeaders::Client_AssignID;
      //         // msgSendID << desc.id;
      //         // MessageClient(client, msgSendID);

      //         // net::message<GameMsgHeaders> msgAddPlayer;
      //         // msgAddPlayer.header.id = GameMsgHeaders::Game_AddPlayer;
      //         // msgAddPlayer << desc;
      //         // BroadcastToClients(msgAddPlayer);

      //         // for (const auto& player : m_mapPlayerRoster)
      //         // {
      //         //   net::message<GameMsgHeaders> msgAddOtherPlayers;
      //         //   msgAddOtherPlayers.header.id = GameMsgHeaders::Game_AddPlayer;
      //         //   msgAddOtherPlayers << player.second;
      //         //   MessageClient(client, msgAddOtherPlayers);
      //         // }

      //         break;
      //       }

      //     case GameMsgHeaders::Client_UnregisterWithServer:
      //       {
      //         break;
      //       }

      //     case GameMsgHeaders::Game_PlayerInput:
      //     {

      //       NetGameInput input;
      //       input.deserealizeNetGameInput(msg.body);
      //       // std::cout << "got input: " << input.playerID << " move: " << static_cast<int>(input.move) << " projectile: " << static_cast<int>(input.fireProjectile) << " swing: " << static_cast<int>(input.swingWeapon) << '\n';

      //       m_playerInputQueue.push_back(input);

      //       // OnMessage: keep this light—deserialize NetGameInput, tag it with client->GetID() (ignore playerID in the payload to prevent spoofing), push it into a per-player input queue, and return. Apply inputs during the server tick so physics/collision run once per frame with deterministic ordering, not per-message.

      //       break;
      //     }
      //     // case GameMsgHeaders::Game_UpdatePlayer:
      //     //   {
      //     //     // this should be the msg that is the user inputs

      //     //     // Simply bounce update to everyone except incoming client
      //     //     BroadcastToClients(msg, client);
      //     //     break;
      //     //   }

      //   }

      // }

    // void updateGameObject(NetGameInput& input, GameObject &obj, float deltaTime) {

    //     // TODO handle deltaTime
    //     if (obj.currentAnimation != -1) {
    //       obj.animations[obj.currentAnimation].step(deltaTime);
    //     }

    //     // gravity applied globally; downward y force when not grounded
    //     if (obj.dynamic && !obj.grounded) {
    //       // if (obj.type == ObjectType::enemy) {
    //       //     std::cout << "grounded" << obj.grounded << std::endl;
    //       // }
    //       // increase downward velocity = acc*deltaTime every frame
    //       obj.velocity += game_engine::Engine::GRAVITY * deltaTime;
    //     }

    //     float currDirection = 0;
    //     if (obj.type == ObjectType::Player) {

    //       // update direction
    //       if (input.move == PlayerInput::MoveLeft) {
    //        currDirection += -1;
    //       }
    //       if (input.move == PlayerInput::MoveRight) {
    //        currDirection += -1;
    //       }

    //       Timer &weaponTimer = obj.data.player.weaponTimer;
    //       weaponTimer.step(deltaTime);

    //       const auto handleShooting = [this, &obj, &weaponTimer, &currDirection, &input](
    //         SDL_Texture *tex, SDL_Texture *shootTex, int animIndex, int shootAnimIndex){
    //       // TODO use similar condition to prevent double jump
    //         if (input.fireProjectile == PlayerInput::Fire) {

    //           // set player texture during shooting anims
    //           obj.texture = shootTex;
    //           obj.currentAnimation = shootAnimIndex;
    //           if (weaponTimer.isTimedOut()) {
    //             weaponTimer.reset();
    //             // create bullets
    //             GameObject bullet(4, 4);
    //             bullet.data.bullet = BulletData();
    //             bullet.type = ObjectType::Bullet;
    //             bullet.direction = obj.direction;
    //             bullet.texture = m_resources.texBullet;
    //             bullet.currentAnimation = m_resources.ANIM_BULLET_MOVING;
    //             bullet.collider = SDL_FRect{
    //               .x = 0, .y = 0,
    //               .w = static_cast<float>(m_resources.texBullet->h),
    //               .h = static_cast<float>(m_resources.texBullet->h),
    //             };
    //             const int yJitter = 50;
    //             const float yVelocity = SDL_rand(yJitter) - yJitter / 2.0f;
    //             bullet.velocity = glm::vec2(
    //               obj.velocity.x + 600.0f,
    //               yVelocity
    //             ) * obj.direction;
    //             bullet.maxSpeedX = 1000.0f;
    //             bullet.animations = m_resources.bulletAnims;

    //             // adjust depending on direction faced; lerp
    //             const float left = 4;
    //             const float right = 24;
    //             const float t = (obj.direction + 1) / 2.0f; // 0 or 1 taking into account neg sign
    //             const float xOffset = left + right * t;
    //             bullet.position = glm::vec2(
    //               obj.position.x + xOffset,
    //               obj.position.y + obj.spritePixelH / 2 + 1
    //             );

    //             bool foundInactive = false;
    //             for (int i = 0; i < m_gameState.bullets.size() && !foundInactive; i++) {
    //               if (m_gameState.bullets[i].data.bullet.state == BulletState::inactive) {
    //                 foundInactive = true;
    //                 m_gameState.bullets[i] = bullet;
    //               }
    //             }

    //             // only add new if no inactive found
    //             if (!foundInactive) {
    //               this->m_gameState.bullets.push_back(bullet); // push bullets so we can draw them
    //             }
    //           }

    //           // MIX_SetTrackAudio(res.shootTrack, res.audioShoot); // or MIX_SetTrackAudioWithProperties
    //           // if (!MIX_PlayTrack(m_resources.shootTrack, 0)) {
    //           //     // SDL_Log("Play failed: %s", SDL_GetError());
    //           // };

    //         } else {
    //             obj.texture = tex;
    //             obj.currentAnimation = animIndex;
    //         }
    //       };

    //       // update animation state
    //       switch (obj.data.player.state) {
    //         case PlayerState::idle:
    //         {
    //           if (currDirection != 0) {
    //             obj.data.player.state = PlayerState::running;
    //             obj.texture = m_resources.texRun;
    //             obj.currentAnimation = m_resources.ANIM_PLAYER_RUN;
    //           } else {
    //             // decelerate faster than we speed up
    //             if (obj.velocity.x) {
    //               const float factor = obj.velocity.x > 0 ? -1.5f : 1.5f;
    //               float amount = factor * obj.acceleration.x * deltaTime;
    //               if (std::abs(obj.velocity.x) < std::abs(amount)) {
    //                 obj.velocity.x = 0;
    //               } else {
    //                 obj.velocity.x += amount;
    //               }
    //             }
    //           }

    //           handleShooting(m_resources.texIdle, m_resources.texShoot, m_resources.ANIM_PLAYER_IDLE, m_resources.ANIM_PLAYER_SHOOT);

    //           break;
    //         }
    //         case PlayerState::running:
    //         {
    //           if (currDirection == 0) {
    //             obj.data.player.state = PlayerState::idle;
    //           }

    //           // move in opposite dir of velocity, sliding
    //           if (obj.velocity.x * obj.direction < 0 && obj.grounded) {
    //             handleShooting(m_resources.texSlide, m_resources.texSlideShoot, m_resources.ANIM_PLAYER_SLIDE, m_resources.ANIM_PLAYER_SLIDE_SHOOT);
    //           } else {
    //             handleShooting(m_resources.texRun, m_resources.texRunShoot, m_resources.ANIM_PLAYER_RUN, m_resources.ANIM_PLAYER_RUN);
    //             // sprite sheets have same frames so we can seamlessly swap between the two sheets
    //           }

    //           break;
    //         }
    //         case PlayerState::jumping:
    //         {
    //           handleShooting(m_resources.texRun, m_resources.texRunShoot, m_resources.ANIM_PLAYER_RUN, m_resources.ANIM_PLAYER_RUN);
    //           // obj.texture = res.texRun;
    //           // obj.currentAnimation = res.ANIM_PLAYER_RUN;
    //           break;
    //         }
    //       }
    //     } else if (obj.type == ObjectType::Bullet) {

    //       switch (obj.data.bullet.state) {
    //         case BulletState::moving:
    //         {
    //           if (obj.position.x - m_gameState.mapViewport.x < 0 || obj.position.x - m_gameState.mapViewport.x > m_sdlState.logW ||
    //           obj.position.y - m_gameState.mapViewport.y < 0 ||
    //           obj.position.y - m_gameState.mapViewport.y > m_sdlState.logH) {
    //           obj.data.bullet.state = BulletState::inactive;
    //           }
    //           break;
    //         }
    //         case BulletState::colliding:
    //         {
    //           if (obj.animations[obj.currentAnimation].isDone()) {
    //             obj.data.bullet.state = BulletState::inactive;
    //           }
    //           break;
    //         }
    //       }
    //     } else if (obj.type == ObjectType::Enemy) {

    //       switch (obj.data.enemy.state) {
    //         case EnemyState::idle:
    //         {
    //           glm::vec2 distToPlayer = this->getPlayer().position - obj.position;
    //           if (glm::length(distToPlayer) < 100) {
    //             // face the enemy towards the player
    //             currDirection = 1;
    //             if (distToPlayer.x < 0) {
    //               currDirection = -1;
    //             };
    //             obj.acceleration = glm::vec2(30, 0);
    //             obj.texture = m_resources.texEnemyRun;
    //           } else {
    //             // stop them from moving when too far away
    //             obj.acceleration = glm::vec2(0);
    //             obj.velocity.x = 0;
    //             obj.texture = m_resources.texEnemy;
    //           }

    //           break;
    //         }
    //         case EnemyState::dying:
    //         {
    //           if (obj.data.enemy.damageTimer.step(deltaTime)) {
    //             obj.data.enemy.state = EnemyState::idle;
    //             obj.texture = m_resources.texEnemy;
    //             obj.currentAnimation = m_resources.ANIM_ENEMY;
    //             obj.data.enemy.damageTimer.reset();
    //           }
    //           break;
    //         }
    //         case EnemyState::dead:
    //         {
    //           obj.velocity.x = 0;
    //           if (obj.currentAnimation != -1 && obj.animations[obj.currentAnimation].isDone()) {
    //             // stop animations for dead enemy
    //             obj.currentAnimation = -1;
    //             obj.spriteFrame = 18; // TODO this is because enemy has 18 frames
    //           }
    //           break;
    //         }
    //       }
    //     }

    //     if (currDirection) {
    //       obj.direction = currDirection;
    //     }
    //     // update velocity based on currDirection (which way we're facing),
    //     // acceleration and deltaTime
    //     obj.velocity += currDirection * obj.acceleration * deltaTime;
    //     if (std::abs(obj.velocity.x) > obj.maxSpeedX) { // cap the max velocity
    //       obj.velocity.x = currDirection * obj.maxSpeedX;
    //     }
    //     // update position based on velocity
    //     obj.position += obj.velocity * deltaTime;

    //     // handle collision detection
    //     bool foundGround = false;
    //     for (auto &layer : m_gameState.layers) {
    //       for (GameObject &objB: layer){
    //         // if (obj.type == ObjectType::enemy) {
    //         //   std::cout << "found Ground" << foundGround << std::endl;
    //         // }
    //         if (&obj != &objB && objB.collider.h != 0 && objB.collider.w != 0) {
    //           this->handleCollision(obj, objB, deltaTime);

    //           // update ground sensor only when landing on level tiles
    //           if (objB.type == ObjectType::Level) {
    //             SDL_FRect sensor{
    //               .x = obj.position.x + obj.collider.x,
    //               .y = obj.position.y + obj.collider.y + obj.collider.h,
    //               .w = obj.collider.w, .h = 1
    //             };

    //             SDL_FRect rectB{
    //               .x = objB.position.x + objB.collider.x,
    //               .y = objB.position.y + objB.collider.y,
    //               .w = objB.collider.w, .h = objB.collider.h
    //             };

    //             SDL_FRect dummyRectC{0};

    //             if (SDL_GetRectIntersectionFloat(&sensor, &rectB, &dummyRectC)) {
    //               foundGround = true;
    //             }
    //           }

    //         }
    //       }
    //     }

    //     if (obj.grounded != foundGround) {
    //       // switching grounded state
    //       obj.grounded = foundGround;
    //       if (foundGround && obj.type == ObjectType::Player) {
    //         obj.data.player.state = PlayerState::running;
    //       }
    //     }
    //   }
    // void updateAllObjects(NetGameInput& input, float deltaTime) {

    //   // if singleplayer let is pass through normal logic
    //   for (auto &layer : m_currGameSnapshot.layers) {
    //     for (GameObject &obj : layer) { // for each obj in layer
    //       // optimization to avoid n*m comparisions
    //       if (obj.dynamic) {
    //         updateGameObject(input, obj, deltaTime);
    //       }
    //     }
    //   }

    //   // update bullet physics
    //   for (GameObject &bullet : m_gameState.bullets) {
    //     updateGameObject(input, bullet, deltaTime);
    //   }

    // };

    public:
      void applyPlayerInputs();

      void step();
  };

}
