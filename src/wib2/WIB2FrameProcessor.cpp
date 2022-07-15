/**
 * @file WIB2FrameProcessor.cpp WIB2 specific Task based raw processor
 * implementation
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#include "detdataformats/wib2/WIB2Frame.hpp"
#include "fdreadoutlibs/wib2/WIB2FrameProcessor.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <string>

using dunedaq::readoutlibs::logging::TLVL_BOOKKEEPING;

namespace dunedaq {
namespace fdreadoutlibs {

void 
WIB2FrameProcessor::conf(const nlohmann::json& args)
{
  TaskRawDataProcessorModel<types::WIB2_SUPERCHUNK_STRUCT>::add_preprocess_task(
    std::bind(&WIB2FrameProcessor::timestamp_check, this, std::placeholders::_1));
  // m_tasklist.push_back( std::bind(&WIB2FrameProcessor::frame_error_check, this, std::placeholders::_1) );
  TaskRawDataProcessorModel<types::WIB2_SUPERCHUNK_STRUCT>::conf(args);
}

/**
 * Pipeline Stage 1.: Check proper timestamp increments in WIB frame
 * */
void 
WIB2FrameProcessor::timestamp_check(frameptr fp)
{
  // If EMU data, emulate perfectly incrementing timestamp
  if (inherited::m_emulator_mode) {         // emulate perfectly incrementing timestamp
    uint64_t ts_next = m_previous_ts + 384; // NOLINT(build/unsigned)
    for (unsigned int i = 0; i < 12; ++i) { // NOLINT(build/unsigned)
      auto wf = reinterpret_cast<dunedaq::detdataformats::wib2::WIB2Frame*>(((uint8_t*)fp) + i * 468); // NOLINT
      auto& wfh = wf->header; // const_cast<dunedaq::detdataformats::wib2::WIB2Frame::Header*>(wf->get_wib_header());
      // wfh->set_timestamp(ts_next);
      wfh.timestamp_1 = ts_next;
      wfh.timestamp_2 = ts_next >> 32;
      ts_next += 32;
    }
  }

  // Acquire timestamp
  auto wfptr = reinterpret_cast<dunedaq::detdataformats::wib2::WIB2Frame*>(fp); // NOLINT
  m_current_ts = wfptr->get_timestamp();

  // Check timestamp
  if (m_current_ts - m_previous_ts != 384) {
    ++m_ts_error_ctr;
    m_error_registry->add_error("MISSING_FRAMES",
                                readoutlibs::FrameErrorRegistry::ErrorInterval(m_previous_ts + 384, m_current_ts));
    if (m_first_ts_missmatch) { // log once
      TLOG_DEBUG(TLVL_BOOKKEEPING) << "First timestamp MISSMATCH! -> | previous: " << std::to_string(m_previous_ts)
                                   << " current: " + std::to_string(m_current_ts);
      m_first_ts_missmatch = false;
    }
  }

  if (m_ts_error_ctr > 1000) {
    if (!m_problem_reported) {
      TLOG() << "*** Data Integrity ERROR *** Timestamp continuity is completely broken! "
             << "Something is wrong with the FE source or with the configuration!";
      m_problem_reported = true;
    }
  }

  m_previous_ts = m_current_ts;
  m_last_processed_daq_ts = m_current_ts;
}

/**
 * Pipeline Stage 2.: Check WIB headers for error flags
 * */
void 
WIB2FrameProcessor::frame_error_check(frameptr /*fp*/)
{
  // check error fields
}

} // namespace fdreadoutlibs
} // namespace dunedaq
