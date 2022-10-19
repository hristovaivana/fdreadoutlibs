#ifndef FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_DUNEWIBSUPERCHUNK_HPP_
#define FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_DUNEWIBSUPERCHUNK_HPP_

#include "daqdataformats/FragmentHeader.hpp"
#include "daqdataformats/SourceID.hpp"
// #include "detdataformats/daphne/DAPHNEFrame.hpp"
// #include "detdataformats/ssp/SSPTypes.hpp"
// #include "detdataformats/wib/WIBFrame.hpp"
#include "detdataformats/wib2/WIB2Frame.hpp"
// #include "detdataformats/tde/TDE16Frame.hpp"
// #include "detdataformats/fwtp/RawTp.hpp"
// #include "triggeralgs/TriggerPrimitive.hpp"

#include <cstdint> // uint_t types
#include <memory>  // unique_ptr
#include <vector>
#include <cstring> // memcpy
#include <tuple> // tie

namespace dunedaq {
namespace fdreadoutlibs {
namespace types {

/**
 * @brief For WIB2 the numbers are different.
 * 12[WIB2 frames] x 472[Bytes] = 5664[Bytes]
 * */
const constexpr std::size_t WIB2_SUPERCHUNK_SIZE = 5664; // for 12: 5664
struct WIB2_SUPERCHUNK_STRUCT
{
  using FrameType = dunedaq::detdataformats::wib2::WIB2Frame;
  // data
  char data[WIB2_SUPERCHUNK_SIZE];
  // comparable based on first timestamp
  bool operator<(const WIB2_SUPERCHUNK_STRUCT& other) const
  {
    auto thisptr = reinterpret_cast<const dunedaq::detdataformats::wib2::WIB2Frame*>(&data);        // NOLINT
    auto otherptr = reinterpret_cast<const dunedaq::detdataformats::wib2::WIB2Frame*>(&other.data); // NOLINT
    return thisptr->get_timestamp() < otherptr->get_timestamp() ? true : false;
  }

  uint64_t get_first_timestamp() const // NOLINT(build/unsigned)
  {
    return reinterpret_cast<const dunedaq::detdataformats::wib2::WIB2Frame*>(&data)->get_timestamp(); // NOLINT
  }

  void set_first_timestamp(uint64_t ts) // NOLINT(build/unsigned)
  {
    auto frame = reinterpret_cast<dunedaq::detdataformats::wib2::WIB2Frame*>(&data); // NOLINT
    frame->header.timestamp_1 = ts;
    frame->header.timestamp_2 = ts >> 32;
  }

  void fake_timestamps(uint64_t first_timestamp, uint64_t offset = 32) // NOLINT(build/unsigned)
  {
    uint64_t ts_next = first_timestamp; // NOLINT(build/unsigned)
    for (unsigned int i = 0; i < 12; ++i) {
      auto w2f = reinterpret_cast<dunedaq::detdataformats::wib2::WIB2Frame*>(((uint8_t*)(&data)) + i * 472); // NOLINT
      w2f->header.timestamp_1 = ts_next;
      w2f->header.timestamp_2 = ts_next >> 32;
      ts_next += offset;
    }
  }

  void fake_frame_errors(std::vector<uint16_t>* /*fake_errors*/) // NOLINT
  {
    // Set error bits in header
  }

  FrameType* begin()
  {
    return reinterpret_cast<FrameType*>(&data[0]); // NOLINT
  }

  FrameType* end()
  {
    return reinterpret_cast<FrameType*>(data + WIB2_SUPERCHUNK_SIZE); // NOLINT
  }

  size_t get_payload_size() { return 5664; }

  size_t get_num_frames() { return 12; }

  size_t get_frame_size() { return 472; }

  static const constexpr daqdataformats::SourceID::Subsystem subsystem = daqdataformats::SourceID::Subsystem::kDetectorReadout;
  static const constexpr daqdataformats::FragmentType fragment_type = daqdataformats::FragmentType::kWIB;
  static const constexpr uint64_t expected_tick_difference = 32; // NOLINT(build/unsigned)
};

static_assert(sizeof(struct dunedaq::detdataformats::wib2::WIB2Frame)*12 == WIB2_SUPERCHUNK_SIZE,
              "Check your assumptions on WIB2_SUPERCHUNK_STRUCT");


} // namespace types
} // namespace fdreadoutlibs
} // namespace dunedaq

#endif /* FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_DUNEWIBSUPERCHUNK_HPP_ */