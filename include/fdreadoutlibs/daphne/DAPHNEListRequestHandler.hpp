/**
 * @file DAPHNEListRequestHandler.hpp Trigger matching mechanism for WIB frames.
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_DAPHNE_DAPHNELISTREQUESTHANDLER_HPP_
#define FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_DAPHNE_DAPHNELISTREQUESTHANDLER_HPP_

#include "logging/Logging.hpp"

#include "readoutlibs/FrameErrorRegistry.hpp"
#include "readoutlibs/ReadoutLogging.hpp"
#include "readoutlibs/ReadoutIssues.hpp"
#include "readoutlibs/models/DefaultRequestHandlerModel.hpp"
#include "readoutlibs/models/SkipListLatencyBufferModel.hpp"

#include "fdreadoutlibs/FDReadoutTypes.hpp"
#include "detdataformats/daphne/DAPHNEFrame.hpp"

#include <atomic>
#include <deque>
#include <functional>
#include <future>
#include <iomanip>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using dunedaq::readoutlibs::logging::TLVL_WORK_STEPS;

namespace dunedaq {
namespace fdreadoutlibs {

class DAPHNEListRequestHandler
  : public readoutlibs::DefaultRequestHandlerModel<types::DAPHNE_SUPERCHUNK_STRUCT,
                                      readoutlibs::SkipListLatencyBufferModel<types::DAPHNE_SUPERCHUNK_STRUCT>>
{
public:
  using inherited = readoutlibs::DefaultRequestHandlerModel<types::DAPHNE_SUPERCHUNK_STRUCT,
                                               readoutlibs::SkipListLatencyBufferModel<types::DAPHNE_SUPERCHUNK_STRUCT>>;
  using SkipListAcc = typename folly::ConcurrentSkipList<types::DAPHNE_SUPERCHUNK_STRUCT>::Accessor;
  using SkipListSkip = typename folly::ConcurrentSkipList<types::DAPHNE_SUPERCHUNK_STRUCT>::Skipper;

  DAPHNEListRequestHandler(std::unique_ptr<readoutlibs::SkipListLatencyBufferModel<types::DAPHNE_SUPERCHUNK_STRUCT>>& latency_buffer,
                           std::unique_ptr<readoutlibs::FrameErrorRegistry>& error_registry)
    : DefaultRequestHandlerModel<types::DAPHNE_SUPERCHUNK_STRUCT,
                                 readoutlibs::SkipListLatencyBufferModel<types::DAPHNE_SUPERCHUNK_STRUCT>>(latency_buffer,
                                                                                              error_registry)
  {
    TLOG_DEBUG(TLVL_WORK_STEPS) << "DAPHNEListRequestHandler created...";
  }

protected:
  void cleanup() override { daphne_cleanup_request(); }

  void daphne_cleanup_request()
  {
    // size_t occupancy_guess = m_latency_buffer->occupancy();
    size_t removed_ctr = 0;
    uint64_t tailts = 0; // oldest // NOLINT(build/unsigned)
    uint64_t headts = 0; // newest // NOLINT(build/unsigned)
    {
      SkipListAcc acc(m_latency_buffer->get_skip_list());
      auto tail = acc.last();
      auto head = acc.first();
      if (tail && head) {
        // auto tailptr = reinterpret_cast<const detdataformats::daphne::DAPHNEFrame*>(tail); // NOLINT
        // auto headptr = reinterpret_cast<const detdataformats::daphne::DAPHNEFrame*>(head); // NOLINT
        tailts = (*tail).get_first_timestamp(); // tailptr->get_timestamp();
        headts = (*head).get_first_timestamp(); // headptr->get_timestamp();
        TLOG_DEBUG(TLVL_WORK_STEPS) << "Cleanup REQUEST with "
                                    << "Oldest stored TS=" << headts << " "
                                    << "Newest stored TS=" << tailts;
        if (tailts - headts > m_max_ts_diff) { // ts differnce exceeds maximum
          ++(inherited::m_pop_reqs);
          uint64_t timediff = m_max_ts_diff; // NOLINT(build/unsigned)
          while (timediff >= m_max_ts_diff) {
            bool removed = acc.remove(*head);
            if (!removed) {
              TLOG_DEBUG(TLVL_WORK_STEPS) << "Unsuccesfull remove from SKL during cleanup: " << removed;
            } else {
              ++removed_ctr;
            }
            head = acc.first();
            // headptr = reinterpret_cast<const detdataformats::daphne::DAPHNEFrame*>(head);
            headts = (*head).get_first_timestamp(); // headptr->get_timestamp();
            timediff = tailts - headts;
          }
          inherited::m_pops_count += removed_ctr;
        }
      } else {
        TLOG_DEBUG(TLVL_WORK_STEPS) << "Didn't manage to get SKL head and tail!";
      }
    }
    m_num_buffer_cleanups++;
  }

private:
  // Constants
  static const constexpr uint64_t m_max_ts_diff = 10000000; // NOLINT(build/unsigned)

  // Stats
  std::atomic<int> m_found_requested_count{ 0 };
  std::atomic<int> m_bad_requested_count{ 0 };
};

} // namespace fdreadoutlibs
} // namespace dunedaq

#endif // FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_DAPHNE_DAPHNELISTREQUESTHANDLER_HPP_
