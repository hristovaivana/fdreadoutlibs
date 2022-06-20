
 * @file TDEFrameProcessor.hpp TDE specific Task based raw processor
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_TDE_TDEFRAMEPROCESSOR_HPP_
#define FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_TDE_TDEFRAMEPROCESSOR_HPP_

#include "readoutlibs/ReadoutIssues.hpp"
#include "readoutlibs/models/TaskRawDataProcessorModel.hpp"

#include "detdataformats/tde/TDE16Frame.hpp"
#include "fdreadoutlibs/FDReadoutTypes.hpp"
#include "logging/Logging.hpp"
#include "readoutlibs/FrameErrorRegistry.hpp"
#include "readoutlibs/ReadoutLogging.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <string>

using dunedaq::readoutlibs::logging::TLVL_BOOKKEEPING;

namespace dunedaq {
namespace fdreadoutlibs {

class TDEFrameProcessor : public readoutlibs::TaskRawDataProcessorModel<types::TDE_AMC_STRUCT>
{

public:
  using inherited = readoutlibs::TaskRawDataProcessorModel<types::TDE_AMC_STRUCT>;
  using frameptr = types::TDE_AMC_STRUCT*;
  using tdeframeptr = dunedaq::detdataformats::tde::TDE16Frame*;
  using timestamp_t = std::uint64_t; // NOLINT(build/unsigned)

  explicit TDEFrameProcessor(std::unique_ptr<readoutlibs::FrameErrorRegistry>& error_registry)
    : TaskRawDataProcessorModel<types::TDE_AMC_STRUCT>(error_registry)
  {}

  void conf(const nlohmann::json& args) override
  {
    TaskRawDataProcessorModel<types::TDE_AMC_STRUCT>::add_preprocess_task(
      std::bind(&TDEFrameProcessor::timestamp_check, this, std::placeholders::_1));
    // m_tasklist.push_back( std::bind(&TDEFrameProcessor::frame_error_check, this, std::placeholders::_1) );
    TaskRawDataProcessorModel<types::TDE_AMC_STRUCT>::conf(args);
  }

protected:
  // Internals
  timestamp_t m_previous_ts = 0;
  timestamp_t m_current_ts = 0;
  bool m_first_ts_missmatch = true;
  bool m_problem_reported = false;
  std::atomic<int> m_ts_error_ctr{ 0 };

  /**
   * Pipeline Stage 1.: Check proper timestamp increments in TDE frames
   * */
  void timestamp_check(frameptr fp)
  {
    // If EMU data, emulate perfectly incrementing timestamp
    if (inherited::m_emulator_mode) {         // emulate perfectly incrementing timestamp
      uint64_t ts_next = m_previous_ts + 1000; // NOLINT(build/unsigned)
      for (unsigned int i = 0; i < 64; ++i) { // NOLINT(build/unsigned)
        auto tdef = reinterpret_cast<dunedaq::detdataformats::tde::TDE16Frame*>(((uint8_t*)fp) + i * sizeof(dunedaq::detdataformats::tde::TDE16Frame)); // NOLINT
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
  void frame_error_check(frameptr /*fp*/)
  {
    // check error fields
  }

private:
};

} // namespace fdreadoutlibs
} // namespace dunedaq

#endif // FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_TDE_TDEFRAMEPROCESSOR_HPP_
