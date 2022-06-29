/*
 * @file TDEFrameProcessor.cpp TDE specific Task based raw processor
 * implementation
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#include "detdataformats/tde/TDE16Frame.hpp"
#include "fdreadoutlibs/tde/TDEFrameProcessor.hpp"

using dunedaq::readoutlibs::logging::TLVL_BOOKKEEPING;

namespace dunedaq {
namespace fdreadoutlibs {

void 
TDEFrameProcessor::conf(const nlohmann::json& args)
{
  TaskRawDataProcessorModel<types::TDE_AMC_STRUCT>::add_preprocess_task(
    std::bind(&TDEFrameProcessor::timestamp_check, this, std::placeholders::_1));
  // m_tasklist.push_back( std::bind(&TDEFrameProcessor::frame_error_check, this, std::placeholders::_1) );
  TaskRawDataProcessorModel<types::TDE_AMC_STRUCT>::conf(args);
}

/**
 * Pipeline Stage 1.: Check proper timestamp increments in TDE frames
 * */
void 
TDEFrameProcessor::timestamp_check(frameptr fp)
{
  // If EMU data, emulate perfectly incrementing timestamp
  if (inherited::m_emulator_mode) {         // emulate perfectly incrementing timestamp
    uint64_t ts_next = m_previous_ts + 1000; // NOLINT(build/unsigned)
    for (unsigned int i = 0; i < 64; ++i) { // NOLINT(build/unsigned)
      auto tdef = reinterpret_cast<dunedaq::detdataformats::tde::TDE16Frame*>(
        ((uint8_t*)fp) + i * sizeof(dunedaq::detdataformats::tde::TDE16Frame)); // NOLINT
      auto tdefh = tdef->get_tde_header(); // const_cast<dunedaq::detdataformats::tde::TDE16Frame::Header*>(tdef->get_wib_header());
      tdefh->set_timestamp(ts_next);
    }
  }

  // Acquire timestamp
  auto tdefptr = reinterpret_cast<dunedaq::detdataformats::tde::TDE16Frame*>(fp); // NOLINT
  m_current_ts = tdefptr->get_timestamp();

  // Check timestamp
  if (m_current_ts - m_previous_ts != 1000) {
    ++m_ts_error_ctr;
    m_error_registry->add_error("MISSING_FRAMES",
                                readoutlibs::FrameErrorRegistry::ErrorInterval(m_previous_ts + 1000, m_current_ts));
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
 * Pipeline Stage 2.: Check TDE headers for error flags
 * */
void 
TDEFrameProcessor::frame_error_check(frameptr /*fp*/)
{
  // check error fields
}

} // namespace fdreadoutlibs
} // namespace dunedaq
