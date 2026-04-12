#include <vector>
#include "game/progression_service.h"


namespace game {


  std::unique_ptr<game::ProgressionService> createDefaultProgressionService() {
    return std::make_unique<game::ProgressionService>();
  }


  void ProgressionService::initProfileFromBytes(std::vector<uint8_t> bytes) {

   deserealizeSaveState(bytes);

  }

  const ProgressionProfile& ProgressionService::getProfile() const {
    return m_Profile;
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
    // w.write_u16(VERSION);
    // w.write_u16(MSG_SNAPSHOT);

    // w.write_u64(serverTick);
    // w.write_enum<LevelIndex>(levelId);
    // w.write_u64(m_stateLastUpdatedAt);

    // througout the game, mutate the saveState so that when user hits save we can serealize and have the engine save the new state without having to gather all the scattered data here
    return bytes.buff;
  }


  void ProgressionService::deserealizeSaveState(const std::vector<std::uint8_t>& bytes)
  {

      net::ByteReader r(bytes);

      // read bytes onto ProgressionProfile
      // m_Profile =
  }


  void ProgressionService::markLevelComplete(int lvlId) {
    // TODO
  }
  void ProgressionService::unlockUltimateForChar(int charID, int ultID) {
    // TODO
  }
  // void addItem();
  // void consumeItem();
  // void equiptItem();

}