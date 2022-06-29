/**
 * @file WIB2FrameProcessor.hpp WIB2 specific Task based raw processor
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_WIB2_WIB2FRAMEPROCESSOR_HPP_
#define FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_WIB2_WIB2FRAMEPROCESSOR_HPP_

#include "readoutlibs/ReadoutIssues.hpp"
#include "readoutlibs/models/TaskRawDataProcessorModel.hpp"

#include "detdataformats/wib2/WIB2Frame.hpp"
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

class WIB2FrameProcessor : public readoutlibs::TaskRawDataProcessorModel<types::WIB2_SUPERCHUNK_STRUCT>
{

public:
  using inherited = readoutlibs::TaskRawDataProcessorModel<types::WIB2_SUPERCHUNK_STRUCT>;
  using frameptr = types::WIB2_SUPERCHUNK_STRUCT*;
  using wib2frameptr = dunedaq::detdataformats::wib2::WIB2Frame*;
  using timestamp_t = std::uint64_t; // NOLINT(build/unsigned)

  // Constructor
  explicit WIB2FrameProcessor(std::unique_ptr<readoutlibs::FrameErrorRegistry>& error_registry)
    : TaskRawDataProcessorModel<types::WIB2_SUPERCHUNK_STRUCT>(error_registry)
  {}

  // Override specific configuration for pipeline setup
  void conf(const nlohmann::json& args) override;

protected:
  /**
   * Pipeline Stage 1.: Check proper timestamp increments in WIB frame
   * */
  void timestamp_check(frameptr fp);

  /**
   * Pipeline Stage 2.: Check WIB headers for error flags
   * */
  void frame_error_check(frameptr /*fp*/);

  // Internals
  timestamp_t m_previous_ts = 0;
  timestamp_t m_current_ts = 0;
  bool m_first_ts_missmatch = true;
  bool m_problem_reported = false;
  std::atomic<int> m_ts_error_ctr{ 0 };

private:
};

} // namespace fdreadoutlibs
} // namespace dunedaq

#endif // FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_WIB2_WIB2FRAMEPROCESSOR_HPP_
