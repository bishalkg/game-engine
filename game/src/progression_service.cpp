#include <vector>
#include <ctime>
#include "game/progression_service.h"


namespace game {


  static constexpr std::uint16_t SCHEMA_VERSION = 1;
  static constexpr std::uint32_t MAGIC = 65500;

  std::unique_ptr<game::ProgressionService> createDefaultProgressionService() {
    return std::make_unique<game::ProgressionService>();
  }

  const ProgressionProfile& ProgressionService::getProfile() const {
    return m_Profile;
  }


  LevelIndex ProgressionService::getLastCompletedLevel() const {
    LevelIndex lvl = LevelIndex::LEVEL_1;
    for (const LevelProgressRecord& lvlData : m_Profile.level_records) {
      if (lvlData.lvlid > lvl && lvlData.complete) {
        lvl = lvlData.lvlid;
      }
    }
    return lvl;
  }

  void ProgressionService::initLevelIfNotExists(LevelIndex lvlId) {
    bool exists = false;
    for (const LevelProgressRecord& lvlData : m_Profile.level_records) {
      if (lvlData.lvlid == lvlId) {
        exists = true;
        break;
      }
    }

    if (!exists) {
      m_Profile.level_records.push_back(LevelProgressRecord{lvlId, false});
    }

  };

  // call inside switchToLevel:
  // this can happen manually clicking the level
  // or can happen by entering the portal through progression
  void ProgressionService::markLevelComplete(LevelIndex lvlId) {
    initLevelIfNotExists(lvlId);
    for (LevelProgressRecord& lvlData : m_Profile.level_records) {
      if (lvlId == lvlData.lvlid) {
        std::cout << "mark level complete" << std::endl;
        lvlData.complete = true;
        break;
      }
    }
  };

  void ProgressionService::initCharIfNotExists(SpriteType spriteType) {
    bool exists = false;
    for (const CharacterProgressRecord& charRec : m_Profile.char_records) {
      if (charRec.spriteType == spriteType) {
        exists = true;
        break;
      }
    }

    if (!exists) {
      std::cout << "init char, did not exist" << std::endl;
      m_Profile.char_records.push_back(CharacterProgressRecord{spriteType, false, false});
    }

  };

  void ProgressionService::unlockUltimateForChar(SpriteType spriteType, uint32_t ultID) {
    initCharIfNotExists(spriteType);
    for (CharacterProgressRecord& charRec : m_Profile.char_records) {
      if (charRec.spriteType == spriteType) {
        switch (ultID) { // TODO make ULT into enum
          case 1:
            charRec.unlockedUltOne = true;
            std::cout << "unlocked ult for char" << std::endl;
            break;
          case 2:
            charRec.unlockedUltTwo = true;
            break;
          default:
        }
      }
    }
  }

  bool ProgressionService::isUltUnlockedForChar(SpriteType spriteType, uint32_t ultID) {
    for (CharacterProgressRecord& charRec : m_Profile.char_records) {
      if (charRec.spriteType == spriteType) {
        switch (ultID) {
          case 1:
            std::cout << "unlocked ult for char " << charRec.unlockedUltOne << std::endl;
            return charRec.unlockedUltOne;
          case 2:
            std::cout << "unlocked ult for char " << charRec.unlockedUltOne << std::endl;
            return charRec.unlockedUltTwo;
          default:
        }
      }
    }
    return false;
  }



  // SaveStorage
  // SaveResult
  // resolveSlotPath(slotName) -> path
  // readSlot(slotName) -> bytes/result
  // writeSlot(slotName, bytes) -> result
  // serialize the save state and send it to engine.writeSlot(slotName, bytes) -> bool for error or not
  std::vector<std::uint8_t> ProgressionService::serealizeSaveState() const
  {

    net::ByteWriter bytes;

    bytes.write_u16(SCHEMA_VERSION);
    bytes.write_u32(MAGIC);

    // TODO .lastSaveWrittenAt is last time we saved. can have another one for time each time we update the profile during gameplay
    bytes.write_u64(static_cast<std::uint64_t>(std::time(nullptr)));

    bytes.write_enum(ProfileChunkType::LevelProgress);
    bytes.write_u32(m_Profile.level_records.size());
    for (const LevelProgressRecord& lvlRec : m_Profile.level_records) {
      bytes.write_u32(static_cast<uint32_t>(lvlRec.lvlid));
      bytes.write_bool(lvlRec.complete);
    }

    bytes.write_enum(ProfileChunkType::CharacterProgress);
    bytes.write_u32(m_Profile.char_records.size());
    for (const CharacterProgressRecord& charRec : m_Profile.char_records) {
      bytes.write_u32(static_cast<uint32_t>(charRec.spriteType));
      bytes.write_bool(charRec.unlockedUltOne);
      bytes.write_bool(charRec.unlockedUltTwo);
    }


    bytes.write_enum(ProfileChunkType::InventoryProgress);
    bytes.write_u32(m_Profile.item_records.size());
    for (const InventoryItemRecord& invRec : m_Profile.item_records) {
      bytes.write_u32(invRec.id);
      bytes.write_u32(invRec.amount);
    }

    bytes.write_u32(MAGIC);
    // througout the game, mutate the saveState so that when user hits save we can serealize and have the engine save the new state without having to gather all the scattered data here
    return bytes.buff;
  }


  // TODO add chunk size for backwards compatibility
  // chunk size means:

  // the number of payload bytes in that chunk, not counting the chunk header itself.

  // Example chunk layout:

  // chunkType
  // chunkVersion
  // chunkSize
  // payload bytes...
  // So for your level chunk, chunkSize would be the total bytes for:

  // record count
  // all level records
  // Why it helps:
  // a loader can skip unknown chunks by doing:
  // “read chunkSize, jump forward that many bytes.”
  void ProgressionService::deserealizeSaveState(const std::vector<std::uint8_t>& bytes)
  {

      net::ByteReader r(bytes);

      auto schema = r.read_u16();
      if (schema != SCHEMA_VERSION) throw std::runtime_error("bad save file version!");
      auto magic = r.read_u32();
      if (magic != MAGIC) throw std::runtime_error("bad start magic number!");

      m_Profile.lastSaveWrittenAt = r.read_u64();

      auto chunkType = r.read_enum<ProfileChunkType>();
      if (chunkType != ProfileChunkType::LevelProgress) {
        throw std::runtime_error("not level progress chunk type!");
      }
      size_t levelRecordLen = r.read_u32();
      m_Profile.level_records.clear();
      m_Profile.level_records.resize(levelRecordLen);
      for (size_t i = 0; i < levelRecordLen; ++i) {
        m_Profile.level_records[i].lvlid = static_cast<LevelIndex>(r.read_u32());
        m_Profile.level_records[i].complete = r.read_bool();
      }

      chunkType = r.read_enum<ProfileChunkType>();
      if (chunkType != ProfileChunkType::CharacterProgress) {
        throw std::runtime_error("not character progress chunk type!");
      }
      size_t charRecordLen = r.read_u32();
      m_Profile.char_records.clear();
      m_Profile.char_records.resize(charRecordLen);
      for (size_t i = 0; i < charRecordLen; ++i) {
        m_Profile.char_records[i].spriteType = static_cast<SpriteType>(r.read_u32());
        m_Profile.char_records[i].unlockedUltOne = r.read_bool();
        m_Profile.char_records[i].unlockedUltTwo = r.read_bool();
      }


      chunkType = r.read_enum<ProfileChunkType>();
      if (chunkType != ProfileChunkType::InventoryProgress) {
        throw std::runtime_error("not inventory progress chunk type!");
      }
      size_t inventoryRecordLen = r.read_u32();
      m_Profile.item_records.clear();
      m_Profile.item_records.resize(inventoryRecordLen);
      for (size_t i = 0; i < inventoryRecordLen; ++i) {
        m_Profile.item_records[i].id = r.read_u32();
        m_Profile.item_records[i].amount = r.read_u32();
      }

      auto end_magic = r.read_u32();
      if (end_magic != MAGIC) throw std::runtime_error("bad end magic number!");
  }
  // void addItem();
  // void consumeItem();
  // void equiptItem();

}