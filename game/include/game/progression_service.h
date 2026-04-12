#pragma once

#include <vector>
#include <memory>
#include "net/net_message.h"


namespace game {

  enum class ProfileChunkType : std::uint32_t {
    LevelProgress,
    CharacterProgress,
    InventoryProgress
};

  struct LevelProgressRecord {
    uint32_t id;
    bool complete;
  };

  struct CharacterProgressRecord {
    uint32_t id;
    bool unlockedUltOne;
    bool unlockedUltTwo;
  };

  struct InventoryItemRecord {
    uint32_t id;
    uint32_t amount;
  };


  struct ProgressionProfile {
    // Plain ol Data: use struct

    // todo will be inefficient to update them like this
    // use a map of ID -> Record instead?
    uint64_t lastSaveWrittenAt;
    std::vector<LevelProgressRecord> level_records;
    std::vector<CharacterProgressRecord> char_records;
    std::vector<InventoryItemRecord> item_records;

    // either compute along the way to use length at save
    // int32_t num_levels;
    // int32_t num_chars;
    // int32_t num_items;

        // format reccomendation:
    // file header (magic, schema, fileType, endianess)
    // repeated typed chunks
    // chunks include:
    // chunkType, chunkVersion, payloadSize, payloadBytes
    // then chunks can be:
    //  META(profile, save timestamp)
    // LEVELS(count,progress record for each level)
    // CHARS(count, repeated progress records for each char)
    // ITEMS (count, repeated item records)

    // for the above, create a struct hierarchy that lives on PersistanceSaveState that will then be translated into byte by byte .writes like below:

  };

  class ProgressionService {


    public:
      ProgressionService() = default;

      std::vector<std::uint8_t> serealizeSaveState() const;

      void deserealizeSaveState(const std::vector<std::uint8_t>& bytes);

      // bool ProgressionService::saveProfile();

      const ProgressionProfile& getProfile() const;


    private:

      ProgressionProfile m_Profile;
      // what do we want to persist?
      // what levels have been completed -> then after we can build level selection
      // what ultimates have been unlocked -> user can switch between which ultimate to use
      // what items the player owns
      //

    private:



      //     Engine-Level API Shape

      // I would aim for something conceptually like this, without overcomplicating it:

      // SaveStorage
      // SaveResult
      // readSlot(slotName) -> bytes/result
      // writeSlot(slotName, bytes) -> result
      // resolveSlotPath(slotName) -> path
      // serialize the save state and send it to engine.writeSlot(slotName, bytes) -> bool for error or not
      // {

      //   //       net::ByteWriter w;

      //   // format reccomendation:
      //   // file header (magic, schema, fileType, endianess)
      //   // repeated typed chunks
      //   // chunks include:
      //   // chunkType, chunkVersion, payloadSize, payloadBytes
      //   // then chunks can be:
      //   //  META(profile, save timestamp)
      //   // LEVELS(count,progress record for each level)
      //   // CHARS(count, repeated progress records for each char)
      //   // ITEMS (count, repeated item records)

      //   // for the above, create a struct hierarchy that lives on PersistanceSaveState that will then be translated into byte by byte .writes like below:
      //   // w.write_u16(VERSION);
      //   // w.write_u16(MSG_SNAPSHOT);

      //   // w.write_u64(serverTick);
      //   // w.write_enum<LevelIndex>(levelId);
      //   // w.write_u64(m_stateLastUpdatedAt);

      //   // througout the game, mutate the saveState so that when user hits save we can serealize and have the engine save the new state without having to gather all the scattered data here



      // }
      // {

      //     net::ByteReader r(bytes);

      //     // read bytes onto ProgressionProfile
      // }


      void markLevelComplete(int lvlId);
      void unlockUltimateForChar(int charID, int ultID);
      // void addItem();
      // void consumeItem();
      // void equiptItem();


  };


  std::unique_ptr<ProgressionService> createDefaultProgressionService();

}