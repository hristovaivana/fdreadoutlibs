/**
 * @file RAWWIBTriggerPrimitiveProcessor.hpp WIB TP specific Task based raw processor
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_WIB_RAWWIBTRIGGERPRIMITIVEPROCESSOR_HPP_
#define FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_WIB_RAWWIBTRIGGERPRIMITIVEPROCESSOR_HPP_

#include "appfwk/DAQModuleHelper.hpp"
#include "readoutlibs/ReadoutIssues.hpp"
#include "readoutlibs/models/TaskRawDataProcessorModel.hpp"

#include "fdreadoutlibs/FDReadoutTypes.hpp"
#include "detdataformats/wib/RawWIBTp.hpp"
#include "logging/Logging.hpp"
#include "readoutlibs/FrameErrorRegistry.hpp"
#include "readoutlibs/ReadoutLogging.hpp"
#include "trigger/TPSet.hpp"
#include "triggeralgs/TriggerPrimitive.hpp"

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include <vector>
#include <utility>
#include <iostream>
#include <fstream>

using dunedaq::readoutlibs::logging::TLVL_BOOKKEEPING;

namespace dunedaq {
namespace fdreadoutlibs {

class RAWWIBTriggerPrimitiveProcessor
  : public readoutlibs::TaskRawDataProcessorModel<types::RAW_WIB_TRIGGERPRIMITIVE_STRUCT>
{

public:
  using inherited = readoutlibs::TaskRawDataProcessorModel<types::RAW_WIB_TRIGGERPRIMITIVE_STRUCT>;
  using frame_ptr = types::RAW_WIB_TRIGGERPRIMITIVE_STRUCT*;
  using rwtp_ptr = detdataformats::wib::RawWIBTp*;
  using timestamp_t = std::uint64_t; // NOLINT(build/unsigned)

  explicit RAWWIBTriggerPrimitiveProcessor(std::unique_ptr<readoutlibs::FrameErrorRegistry>& error_registry)
    : TaskRawDataProcessorModel<types::RAW_WIB_TRIGGERPRIMITIVE_STRUCT>(error_registry)
  {}

  void conf(const nlohmann::json& args) override
  {
    TaskRawDataProcessorModel<types::RAW_WIB_TRIGGERPRIMITIVE_STRUCT>::conf(args);
  }

  void init(const nlohmann::json& args) override
  {
    try {
      auto queue_index = appfwk::queue_index(args, {});
      if (queue_index.find("tp") != queue_index.end()) {
        m_tp_source.reset(new appfwk::DAQSource<types::RAW_WIB_TRIGGERPRIMITIVE_STRUCT>(queue_index["tp"].inst));
      }
    } catch (const ers::Issue& excpt) {
      // error
    }
  }

  void stop(const nlohmann::json& /*args*/) override
  {
  }

private:
  using source_t = appfwk::DAQSource<types::RAW_WIB_TRIGGERPRIMITIVE_STRUCT>;
  std::unique_ptr<source_t> m_tp_source;

  // info
  std::chrono::time_point<std::chrono::high_resolution_clock> m_t0;
};

} // namespace fdreadoutlibs
} // namespace dunedaq

#endif // FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_WIB_RAWWIBTRIGGERPRIMITIVEPROCESSOR_HPP_
