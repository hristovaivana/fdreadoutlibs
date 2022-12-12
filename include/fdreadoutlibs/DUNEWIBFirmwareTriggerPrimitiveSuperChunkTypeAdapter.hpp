
#ifndef FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_DUNEWIBFIRMWARETRIGGERPRIMITIVESUPERCHUNKTYPEADAPTER_HPP_
#define FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_DUNEWIBFIRMWARETRIGGERPRIMITIVESUPERCHUNKTYPEADAPTER_HPP_

//#include "iomanager/Sender.hpp"
//#include "iomanager/Receiver.hpp"

#include "daqdataformats/FragmentHeader.hpp"
#include "daqdataformats/SourceID.hpp"
#include "detdataformats/fwtp/RawTp.hpp"
#include "triggeralgs/TriggerPrimitive.hpp"

#include <cstdint> // uint_t types
#include <cstring> // memcpy
#include <memory>  // unique_ptr
#include <tuple>   // tie
#include <vector>

namespace dunedaq {
namespace fdreadoutlibs {
namespace types {

// raw WIB TP
struct DUNEWIBFirmwareTriggerPrimitiveSuperChunkTypeAdapter
{
  DUNEWIBFirmwareTriggerPrimitiveSuperChunkTypeAdapter()
  {
    m_raw_tp_frame_chunksize = 0;
    m_first_timestamp = 0;
  }

  using FrameType = DUNEWIBFirmwareTriggerPrimitiveSuperChunkTypeAdapter;

  // latency buffer
  bool operator<(const DUNEWIBFirmwareTriggerPrimitiveSuperChunkTypeAdapter& other) const
  {
    // std::cout << "other header" << std::endl;
    // if(m_first_timestamp != other.m_first_timestamp)
    // {
    //   bool less = m_first_timestamp < other.m_first_timestamp;
    //   std::cout << "ts less operation is :" << less << std::endl;
    //   return less;
    // }
    // if(head.m_crate_no != ohead.m_crate_no)
    // {
    //   bool less = head.m_crate_no < ohead.m_crate_no;
    //   std::cout << "crate less operation is :" << less << std::endl;
    //   return less;
    // }
    // if(head.m_slot_no != ohead.m_slot_no)
    // {
    //   bool less = head.m_slot_no < ohead.m_slot_no;
    //   std::cout << "slot less operation is :" << less << std::endl;
    //   return less;
    // }
    // if(head.m_fiber_no != ohead.m_fiber_no)
    // {
    //   bool less = head.m_fiber_no < ohead.m_fiber_no;
    //   std::cout << "fiber less operation is :" << less << std::endl;
    //   return less;
    // }
    // if(head.m_wire_no != ohead.m_wire_no)
    // {
    //   bool less = head.m_wire_no < ohead.m_wire_no;
    //   std::cout << "wire less operation is :" << less << std::endl;
    //   return less;
    // }
    // return false;
    // return less;

    if (m_raw_tp_frame_chunksize == 0 or other.m_raw_tp_frame_chunksize == 0) {
      return m_raw_tp_frame_chunksize < other.m_raw_tp_frame_chunksize;
    }

    // std::cout << "Hey!" << std::endl;
    const detdataformats::fwtp::TpHeader* head = this->get_header();
    // std::cout << "this header" << std::endl;
    const detdataformats::fwtp::TpHeader* ohead = other.get_header();

    return (
      std::tie(m_first_timestamp, head->m_crate_no, head->m_slot_no, head->m_fiber_no, head->m_wire_no) <
      std::tie(other.m_first_timestamp, ohead->m_crate_no, ohead->m_slot_no, ohead->m_fiber_no, ohead->m_wire_no));
  }

  uint64_t get_timestamp() const // NOLINT(build/unsigned)
  {
    return get_first_timestamp();
  }
  uint64_t get_first_timestamp() const // NOLINT(build/unsigned)
  {
    return m_first_timestamp;
  }
  void set_first_timestamp(uint64_t ts) // NOLINT(build/unsigned)
  {
    m_first_timestamp = ts;
  }
  void fake_timestamps(uint64_t first_timestamp, uint64_t /*offset = 25*/) // NOLINT(build/unsigned)
  {
    m_first_timestamp = first_timestamp;
  }

  // FrameType* begin()
  //{
  //   return this;
  // }

  //
  // FrameType* end()
  //{
  //  return (this+1);
  //}

  FrameType* begin()
  {
    return reinterpret_cast<FrameType*>(m_raw_tp_frame_chunk.data()); // NOLINT
  }

  FrameType* end()
  {
    return reinterpret_cast<FrameType*>(m_raw_tp_frame_chunk.data() + m_raw_tp_frame_chunksize); // NOLINT
  }

  static const constexpr daqdataformats::SourceID::Subsystem subsystem = daqdataformats::SourceID::Subsystem::kTrigger;
  static const constexpr daqdataformats::FragmentType fragment_type =
    daqdataformats::FragmentType::kFW_TriggerPrimitive;
  static const constexpr uint64_t expected_tick_difference = 0; // 2 MHz@50MHz clock // NOLINT(build/unsigned)
  // static const constexpr size_t frame_size = TP_SIZE;
  // static const constexpr size_t element_size = TP_SIZE;
  static const constexpr uint64_t tick_dist = 25;        // 2 MHz@50MHz clock // NOLINT(build/unsigned)
  static const constexpr uint8_t frames_per_element = 1; // NOLINT(build/unsigned)

  // raw WIB TP frames are variable size
  size_t get_payload_size() { return m_raw_tp_frame_chunksize; }

  size_t get_num_frames() { return 1; }

  size_t get_frame_size() { return m_raw_tp_frame_chunksize; }

  void set_raw_tp_frame_chunk(std::vector<char>& source)
  {
    int bsize = source.capacity();
    m_raw_tp_frame_chunk.reserve(bsize);
    ::memcpy(static_cast<void*>(m_raw_tp_frame_chunk.data()), static_cast<void*>(source.data()), bsize);
    m_raw_tp_frame_chunksize = bsize;

    unpack_timestamp();
  }

  std::vector<std::uint8_t>& get_data() // NOLINT(build/unsigned)
  {
    return std::ref(m_raw_tp_frame_chunk);
  }

  void set_data_size(const int& bytes)
  {
    m_raw_tp_frame_chunksize = bytes;

    unpack_timestamp();
  }
  int get_raw_tp_frame_chunksize() { return m_raw_tp_frame_chunksize; }

private:
  void unpack_timestamp()
  {
    // std::unique_ptr<detdataformats::fwtp::RawTp> rwtp =
    //                 std::make_unique<detdataformats::fwtp::RawTp>();
    // ::memcpy(static_cast<void*>(&rwtp->m_head),
    //          static_cast<void*>(m_raw_tp_frame_chunk.data()),
    //          2*RAW_WIB_TP_SUBFRAME_SIZE);
    // m_first_timestamp = rwtp->m_head.get_timestamp();

    // m_first_timestamp =
    // reinterpret_cast<detdataformats::fwtp::RawTp*>(m_raw_tp_frame_chunk.data())->m_head.get_timestamp()
    m_first_timestamp = get_header()->get_timestamp();
  }

  const detdataformats::fwtp::TpHeader* get_header() const
  {
    return reinterpret_cast<const detdataformats::fwtp::TpHeader*>(m_raw_tp_frame_chunk.data());
  }

private:
  std::vector<std::uint8_t> m_raw_tp_frame_chunk; // NOLINT(build/unsigned)
  int m_raw_tp_frame_chunksize;

  uint64_t m_first_timestamp;
  static const constexpr std::size_t RAW_WIB_TP_SUBFRAME_SIZE = 12;
};

struct TpSubframe
{
  uint32_t word1; // NOLINT(build/unsigned)
  uint32_t word2; // NOLINT(build/unsigned)
  uint32_t word3; // NOLINT(build/unsigned)
};

} // namespace types
} // namespace fdreadoutlibs
} // namespace dunedaq

#endif // FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_DUNEWIBFIRMWARETRIGGERPRIMITIVESUPERCHUNKTYPEADAPTER_HPP_
