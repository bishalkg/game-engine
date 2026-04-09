#include <cassert>
#include <cmath>
#include <iostream>

#include "engine/gameobject.h"
#include "engine/net/game_net_common.h"

namespace {

bool closeVec2(const glm::vec2& a, const glm::vec2& b, float eps = 1e-5f) {
  return std::fabs(a.x - b.x) < eps && std::fabs(a.y - b.y) < eps;
}

bool equalSnapshots(const game_engine::NetGameObjectSnapshot& a,
                    const game_engine::NetGameObjectSnapshot& b) {
  if (a.id != b.id || a.layer != b.layer || a.type != b.type || a.spriteType != b.spriteType) return false;
  if (!closeVec2(a.position, b.position) || !closeVec2(a.velocity, b.velocity) ||
      !closeVec2(a.acceleration, b.acceleration)) return false;
  if (a.spriteFrame != b.spriteFrame || a.currentAnimation != b.currentAnimation) return false;
  if (std::fabs(a.animElapsed - b.animElapsed) > 1e-5f) return false;
  if (std::fabs(a.direction - b.direction) > 1e-5f) return false;
  if (std::fabs(a.maxSpeedX - b.maxSpeedX) > 1e-5f) return false;
  if (a.grounded != b.grounded || a.shouldFlash != b.shouldFlash) return false;

  switch (a.type) {
    case ObjectClass::Player:
      return a.data.player.state == b.data.player.state &&
             a.data.player.healthPoints == b.data.player.healthPoints;
    case ObjectClass::Enemy:
      return a.data.enemy.state == b.data.enemy.state &&
             a.data.enemy.healthPoints == b.data.enemy.healthPoints &&
             a.data.enemy.srcH == b.data.enemy.srcH &&
             a.data.enemy.srcW == b.data.enemy.srcW;
    case ObjectClass::Projectile:
      return a.data.bullet.state == b.data.bullet.state;
    case ObjectClass::Level:
      return a.data.level.src.x == b.data.level.src.x &&
             a.data.level.src.y == b.data.level.src.y &&
             a.data.level.src.w == b.data.level.src.w &&
             a.data.level.src.h == b.data.level.src.h &&
             a.data.level.dst.x == b.data.level.dst.x &&
             a.data.level.dst.y == b.data.level.dst.y &&
             a.data.level.dst.w == b.data.level.dst.w &&
             a.data.level.dst.h == b.data.level.dst.h;
    case ObjectClass::Portal:
    case ObjectClass::Background:
      return true;
  }
  return false;
}

game_engine::NetGameStateSnapshot makeSnapshot() {
  using namespace game_engine;
  NetGameStateSnapshot snap{};
  snap.serverTick = 100;
  snap.levelId = LevelIndex::LEVEL_2;
  snap.m_stateLastUpdatedAt = 42;

  NetGameObjectSnapshot player{};
  player.id = 1;
  player.layer = 0;
  player.type = ObjectClass::Player;
  player.spriteType = SpriteType::Player_Mage;
  player.position = {1.f, 2.f};
  player.velocity = {0.5f, -0.25f};
  player.acceleration = {0.f, 0.1f};
  player.spriteFrame = 3;
  player.currentAnimation = 1;
  player.animElapsed = 0.33f;
  player.direction = 1.f;
  player.maxSpeedX = 10.f;
  player.grounded = true;
  player.shouldFlash = false;
  new (&player.data.player) PlayerData{};
  player.data.player.state = PlayerState::running;
  player.data.player.healthPoints = 88;
  snap.m_gameObjects[{player.type, player.id}] = player;

  NetGameObjectSnapshot enemy{};
  enemy.id = 2;
  enemy.layer = 1;
  enemy.type = ObjectClass::Enemy;
  enemy.spriteType = SpriteType::Red_Werewolf;
  enemy.position = {5.f, 6.f};
  enemy.velocity = {0.f, 0.f};
  enemy.acceleration = {0.f, 0.f};
  enemy.spriteFrame = 2;
  enemy.currentAnimation = 0;
  enemy.animElapsed = 0.12f;
  enemy.direction = -1.f;
  enemy.maxSpeedX = 4.f;
  enemy.grounded = false;
  enemy.shouldFlash = true;
  new (&enemy.data.enemy) EnemyData{};
  enemy.data.enemy.state = EnemyState::hurt;
  enemy.data.enemy.healthPoints = 50;
  enemy.data.enemy.srcH = 16;
  enemy.data.enemy.srcW = 24;
  snap.m_gameObjects[{enemy.type, enemy.id}] = enemy;

  NetGameObjectSnapshot bullet{};
  bullet.id = 3;
  bullet.layer = 1;
  bullet.type = ObjectClass::Projectile;
  bullet.spriteType = SpriteType::Player_Mage;
  bullet.position = {7.f, 8.f};
  bullet.velocity = {2.f, 0.f};
  bullet.acceleration = {0.f, 0.f};
  bullet.spriteFrame = 1;
  bullet.currentAnimation = 0;
  bullet.animElapsed = 0.45f;
  bullet.direction = 1.f;
  bullet.maxSpeedX = 20.f;
  bullet.grounded = false;
  bullet.shouldFlash = false;
  new (&bullet.data.bullet) BulletData{};
  bullet.data.bullet.state = BulletState::moving;
  snap.m_gameObjects[{bullet.type, bullet.id}] = bullet;

  NetGameObjectSnapshot level{};
  level.id = 4;
  level.layer = 0;
  level.type = ObjectClass::Level;
  level.spriteType = SpriteType::Player_Marie;
  level.position = {0.f, 0.f};
  level.velocity = {0.f, 0.f};
  level.acceleration = {0.f, 0.f};
  level.spriteFrame = 0;
  level.currentAnimation = 0;
  level.animElapsed = 0.0f;
  level.direction = 0.f;
  level.maxSpeedX = 0.f;
  level.grounded = true;
  level.shouldFlash = false;
  new (&level.data.level) LevelData{};
  level.data.level.src = SDL_FRect{1.f, 2.f, 3.f, 4.f};
  level.data.level.dst = SDL_FRect{5.f, 6.f, 7.f, 8.f};
  snap.m_gameObjects[{level.type, level.id}] = level;

  return snap;
}

void testNetGameInputRoundTrip() {
  using namespace game_engine;
  NetGameInput in{};
  in.playerID = 123;
  in.inputSeq = 456;
  in.leftHeld = true;
  in.rightHeld = false;
  in.fireHeld = true;
  in.jumpPressed = true;
  in.meleePressed = true;
  auto bytes = in.serealizeNetGameInput();

  NetGameInput out{};
  out.deserealizeNetGameInput(bytes);

  assert(out.playerID == in.playerID);
  assert(out.inputSeq == in.inputSeq);
  assert(out.leftHeld == in.leftHeld);
  assert(out.rightHeld == in.rightHeld);
  assert(out.fireHeld == in.fireHeld);
  assert(out.jumpPressed == in.jumpPressed);
  assert(out.meleePressed == in.meleePressed);
}

void testNetGameStateSnapshotRoundTrip() {
  using namespace game_engine;
  auto snap = makeSnapshot();
  auto bytes = snap.serealizeNetGameStateSnapshot();

  NetGameStateSnapshot decoded{};
  decoded.deserealizeNetGameStateSnapshot(bytes);

  assert(decoded.serverTick == snap.serverTick);
  assert(decoded.levelId == snap.levelId);
  assert(decoded.m_stateLastUpdatedAt == snap.m_stateLastUpdatedAt);
  assert(decoded.m_gameObjects.size() == snap.m_gameObjects.size());

  for (auto& [key, obj] : snap.m_gameObjects) {
    auto it = decoded.m_gameObjects.find(key);
    assert(it != decoded.m_gameObjects.end());
    assert(equalSnapshots(obj, it->second));
  }
}

} // namespace

int main(){
  testNetGameInputRoundTrip();
  testNetGameStateSnapshotRoundTrip();
  std::cout << "All net_common tests passed\n";
  return 0;
}


// You can run it either from VS Code’s CMake Tools UI or the terminal:

// In VS Code: open the CMake Tools side bar → set the build preset you use → pick net_common_tests from the “CMake: Set Build Target” dropdown → click “Build” (or run the CMake: Build command). Then run the built binary from the terminal: ./build/net_common_tests.

// From the terminal: cmake --build build --target net_common_tests and then ./build/net_common_tests (from the repo root).
// If you want it in the VS Code “Test” explorer, add a ctest entry by registering the test in CMake (add_test(NAME net_common_tests COMMAND net_common_tests)) and then run “CMake: Run Tests”.
