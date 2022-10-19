#ifndef FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_SW_PROTOWIB_TRIGGERPRIMITIVE_HPP_
#define FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_SW_PROTOWIB_TRIGGERPRIMITIVE_HPP_


#include "daqdataformats/FragmentHeader.hpp"
#include "daqdataformats/SourceID.hpp"
// #include "detdataformats/daphne/DAPHNEFrame.hpp"
// #include "detdataformats/ssp/SSPTypes.hpp"
// #include "detdataformats/wib/WIBFrame.hpp"
// #include "detdataformats/wib2/WIB2Frame.hpp"
// #include "detdataformats/tde/TDE16Frame.hpp"
// #include "detdataformats/fwtp/RawTp.hpp"
#include "triggeralgs/TriggerPrimitive.hpp"

#include <cstdint> // uint_t types
#include <memory>  // unique_ptr
#include <vector>
#include <cstring> // memcpy
#include <tuple> // tie

namespace dunedaq {
namespace fdreadoutlibs {
namespace types {

const constexpr std::size_t TP_SIZE = sizeof(triggeralgs::TriggerPrimitive);
struct SW_WIB_TRIGGERPRIMITIVE_STRUCT
{
  using FrameType = SW_WIB_TRIGGERPRIMITIVE_STRUCT;
  // data
  triggeralgs::TriggerPrimitive tp;
  // comparable based on start timestamp
  bool operator<(const SW_WIB_TRIGGERPRIMITIVE_STRUCT& other) const
  {
    return std::tie(this->tp.time_start, this->tp.channel) < std::tie(other.tp.time_start, other.tp.channel);
  }

  uint64_t get_first_timestamp() const // NOLINT(build/unsigned)
  {
    return tp.time_start;
  }

  void set_first_timestamp(uint64_t ts) // NOLINT(build/unsigned)
  {
    tp.time_start = ts;
  }

  uint64_t get_timestamp() const // NOLINT(build/unsigned)
  {
    return tp.time_start;
  }

  void fake_timestamps(uint64_t first_timestamp, uint64_t /*offset = 25*/) // NOLINT(build/unsigned)
  {
    tp.time_start = first_timestamp;
  }

  FrameType* begin() { return this; }

  FrameType* end() { return (this + 1); } // NOLINT

  size_t get_payload_size() { return TP_SIZE; }

  size_t get_num_frames() { return 1; }

  size_t get_frame_size() { return TP_SIZE; }

  static const constexpr daqdataformats::SourceID::Subsystem subsystem = daqdataformats::SourceID::Subsystem::kTrigger;
  static const constexpr daqdataformats::FragmentType fragment_type = daqdataformats::FragmentType::kTriggerPrimitive;
  static const constexpr uint64_t expected_tick_difference = 25; // NOLINT(build/unsigned)
};

static_assert(sizeof(struct SW_WIB_TRIGGERPRIMITIVE_STRUCT) == sizeof(triggeralgs::TriggerPrimitive),
              "Check your assumptions on SW_WIB_TRIGGERPRIMITIVE_STRUCT");

} // namespace types
} // namespace fdreadoutlibs
} // namespace dunedaq

#endif // FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_SW_PROTOWIB_TRIGGERPRIMITIVE_HPP_
