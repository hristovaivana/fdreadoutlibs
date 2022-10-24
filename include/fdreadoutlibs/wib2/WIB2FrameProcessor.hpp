/**
 * @file WIB2FrameProcessor.hpp WIB2 specific Task based raw processor
 * @author Adam Abed Abud (adam.abed.abud@cern.ch)
 *
 * This is part of the DUNE DAQ , copyright 2022.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_WIB2_WIBFRAMEPROCESSOR_HPP_
#define FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_WIB2_WIBFRAMEPROCESSOR_HPP_

#include "appfwk/DAQModuleHelper.hpp"
#include "iomanager/Sender.hpp"
#include "logging/Logging.hpp"

#include "readoutlibs/FrameErrorRegistry.hpp"
#include "readoutlibs/ReadoutIssues.hpp"
#include "readoutlibs/ReadoutLogging.hpp"
#include "readoutlibs/models/IterableQueueModel.hpp"
#include "readoutlibs/models/TaskRawDataProcessorModel.hpp"
#include "readoutlibs/readoutconfig/Nljs.hpp"
#include "readoutlibs/readoutinfo/InfoNljs.hpp"
#include "readoutlibs/utils/ReusableThread.hpp"

#include "detchannelmaps/TPCChannelMap.hpp"
#include "detdataformats/wib2/WIB2Frame.hpp"


#include "fdreadoutlibs/DUNEWIBSuperChunkTypeAdapter.hpp"
#include "fdreadoutlibs/TriggerPrimitiveTypeAdapter.hpp"

#include "fdreadoutlibs/wib2/WIB2TPHandler.hpp"
#include "rcif/cmd/Nljs.hpp"
#include "trigger/TPSet.hpp"
#include "triggeralgs/TriggerPrimitive.hpp"

#include "iomanager/FollyQueue.hpp"


#include "tpg/DesignFIR.hpp"
#include "tpg/FrameExpand.hpp"
#include "tpg/ProcessAVX2.hpp"
#include "tpg/ProcessingInfo.hpp"
#include "tpg/RegisterToChannelNumber.hpp"
#include "tpg/TPGConstants_wib2.hpp"

#include <atomic>
#include <bitset>
#include <functional>
#include <future>
#include <memory>
#include <pthread.h>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using dunedaq::readoutlibs::logging::TLVL_BOOKKEEPING;
using dunedaq::readoutlibs::logging::TLVL_TAKE_NOTE;

namespace dunedaq {
namespace fdreadoutlibs {



struct swtpg_output{
  uint16_t* output_location;
  uint64_t timestamp;
};

class WIB2FrameHandler {

public: 
  explicit WIB2FrameHandler(int register_selector_params) {
    m_register_selector = register_selector_params;
  }
  WIB2FrameHandler(const WIB2FrameHandler&) = delete;
  WIB2FrameHandler& operator=(const WIB2FrameHandler&) = delete;
  ~WIB2FrameHandler() {
    if (m_tpg_taps_p) {
      delete[] m_tpg_taps_p;
    }

    if (m_primfind_dest) {
      delete[] m_primfind_dest;
    }    
  }

  // Making the following public members to avoid copying the unique ptr
  //std::unique_ptr<WIB2TPHandler> tphandler;
  std::unique_ptr<swtpg_wib2::ProcessingInfo<swtpg_wib2::NUM_REGISTERS_PER_FRAME>> m_tpg_processing_info;
  bool first_hits = true;

  // Map from expanded AVX register position to offline channel number
  swtpg_wib2::RegisterChannelMap register_channel_map; 
                                                  
                                                  
  int get_registers_selector() {
    return m_register_selector;
  }

  void reset() {
    delete[] m_tpg_taps_p;
    m_tpg_taps_p = nullptr;
    delete[] m_primfind_dest;
    m_primfind_dest = nullptr;
  }
   

  void initialize() {
    m_tpg_taps = swtpg_wib2::firwin_int(7, 0.1, m_tpg_multiplier);
    m_tpg_taps.push_back(0);    


    if (m_tpg_taps_p == nullptr) {
      m_tpg_taps_p = new int16_t[m_tpg_taps.size()];
    }
    for (size_t i = 0; i < m_tpg_taps.size(); ++i) {
      m_tpg_taps_p[i] = m_tpg_taps[i];
    }
    if (m_primfind_dest == nullptr) {
      m_primfind_dest = new uint16_t[100000]; // NOLINT(build/unsigned)
    }

    m_tpg_processing_info = std::make_unique<swtpg_wib2::ProcessingInfo<swtpg_wib2::NUM_REGISTERS_PER_FRAME>>(
      nullptr,
      swtpg_wib2::FRAMES_PER_MSG,
      0,
      swtpg_wib2::NUM_REGISTERS_PER_FRAME,
      m_primfind_dest,
      m_tpg_taps_p,
      (uint8_t)m_tpg_taps.size(), // NOLINT(build/unsigned)
      m_tpg_tap_exponent,
      m_tpg_threshold,
      0,
      0
    );

  }

  uint16_t* get_primfind_dest() {
    return m_primfind_dest;
  }


private: 
  int m_register_selector;    
  uint16_t* m_primfind_dest = nullptr;  
  const uint16_t m_tpg_threshold = 5;                    // units of sigma // NOLINT(build/unsigned)
  const uint8_t m_tpg_tap_exponent = 6;                  // NOLINT(build/unsigned)
  const int m_tpg_multiplier = 1 << m_tpg_tap_exponent;  // 64
  std::vector<int16_t> m_tpg_taps;                       // firwin_int(7, 0.1, multiplier);
  int16_t* m_tpg_taps_p = nullptr;


};




class WIB2FrameProcessor : public readoutlibs::TaskRawDataProcessorModel<types::DUNEWIBSuperChunkTypeAdapter>
{

public:
  using inherited = readoutlibs::TaskRawDataProcessorModel<types::DUNEWIBSuperChunkTypeAdapter>;
  using frameptr = types::DUNEWIBSuperChunkTypeAdapter*;
  using constframeptr = const types::DUNEWIBSuperChunkTypeAdapter*;
  using wibframeptr = dunedaq::detdataformats::wib2::WIB2Frame*;
  using timestamp_t = std::uint64_t; // NOLINT(build/unsigned)

  // Channel map function type
  typedef int (*chan_map_fn_t)(int);

  explicit WIB2FrameProcessor(std::unique_ptr<readoutlibs::FrameErrorRegistry>& error_registry)
    : TaskRawDataProcessorModel<types::DUNEWIBSuperChunkTypeAdapter>(error_registry)
    , m_sw_tpg_enabled(false)
    , m_add_hits_tphandler_thread_should_run(false)    
  {}

  ~WIB2FrameProcessor()
  {
    m_wib2_frame_handler->reset();
    m_wib2_frame_handler_second_half->reset();

  }

  void start(const nlohmann::json& args) override
  {
    // Reset software TPG resources
    if (m_sw_tpg_enabled) {

      rcif::cmd::StartParams start_params = args.get<rcif::cmd::StartParams>();
      m_tphandler->set_run_number(start_params.run);
  
      m_tphandler->reset();

      m_tps_dropped = 0;

      m_wib2_frame_handler->initialize();
      m_wib2_frame_handler_second_half->initialize();

      


    } // end if(m_sw_tpg_enabled)

    // Reset timestamp check
    m_previous_ts = 0;
    m_current_ts = 0;
    m_first_ts_missmatch = true;
    m_problem_reported = false;
    m_ts_error_ctr = 0;

    // Reset stats
    m_first_hit = true;

    m_t0 = std::chrono::high_resolution_clock::now();
    m_new_hits = 0;
    m_new_tps = 0;
    m_swtpg_hits_count.exchange(0);
    m_frame_error_count = 0;
    m_frames_processed = 0;

    inherited::start(args);
  }

  void stop(const nlohmann::json& args) override
  {
    inherited::stop(args);
    if (m_sw_tpg_enabled) {
      // Make temp. buffers reusable on next start.
      m_wib2_frame_handler->reset();    
      m_wib2_frame_handler_second_half->reset();  
      
      auto runtime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_t0).count();
      //TLOG() << "Ran for " << runtime << "ms.";
    }

  }

  void init(const nlohmann::json& args) override
  {
    try {
      auto queue_index = appfwk::connection_index(args, {});
      if (queue_index.find("tp_out") != queue_index.end()) {
        m_tp_sink = get_iom_sender<types::TriggerPrimitiveTypeAdapter>(queue_index["tp_out"]);
      }
      if (queue_index.find("tpset_out") != queue_index.end()) {
        m_tpset_sink = get_iom_sender<trigger::TPSet>(queue_index["tpset_out"]);
      }
    } catch (const ers::Issue& excpt) {
      throw readoutlibs::ResourceQueueError(ERS_HERE, "tp queue", "DefaultRequestHandlerModel", excpt);
    }
  }

  void conf(const nlohmann::json& cfg) override
  {
    auto config = cfg["rawdataprocessorconf"].get<readoutlibs::readoutconfig::RawDataProcessorConf>();
    m_sourceid.id = config.source_id;
    m_sourceid.subsystem = types::DUNEWIBSuperChunkTypeAdapter::subsystem;
    m_error_counter_threshold = config.error_counter_threshold;
    m_error_reset_freq = config.error_reset_freq;

    if (config.enable_software_tpg) {
      m_sw_tpg_enabled = true;

      m_channel_map = dunedaq::detchannelmaps::make_map(config.channel_map_name);

      daqdataformats::SourceID tpset_sourceid;
      tpset_sourceid.id = config.tpset_sourceid;
      tpset_sourceid.subsystem = daqdataformats::SourceID::Subsystem::kTrigger;

      m_tphandler.reset(
        new WIB2TPHandler(*m_tp_sink, *m_tpset_sink, config.tp_timeout, config.tpset_window_size, tpset_sourceid, config.tpset_topic));

      TaskRawDataProcessorModel<types::DUNEWIBSuperChunkTypeAdapter>::add_postprocess_task(
        std::bind(&WIB2FrameProcessor::find_hits, this, std::placeholders::_1, m_wib2_frame_handler.get()));

      TaskRawDataProcessorModel<types::DUNEWIBSuperChunkTypeAdapter>::add_postprocess_task(
        std::bind(&WIB2FrameProcessor::find_hits, this, std::placeholders::_1, m_wib2_frame_handler_second_half.get()));

      // Launch the thread for adding hits to tphandler
      m_add_hits_tphandler_thread_should_run.store(true);
      m_add_hits_tphandler_thread = std::thread(&WIB2FrameProcessor::add_hits_to_tphandler, this);
      TLOG() << "Launched thread for adding hits to tphandler"; 
      
    }

    // Setup pre-processing pipeline
    TaskRawDataProcessorModel<types::DUNEWIBSuperChunkTypeAdapter>::add_preprocess_task(
      std::bind(&WIB2FrameProcessor::timestamp_check, this, std::placeholders::_1));

    TaskRawDataProcessorModel<types::DUNEWIBSuperChunkTypeAdapter>::conf(cfg);
  }

  void scrap(const nlohmann::json& args) override
  {
   if(m_sw_tpg_enabled) {	  
      TLOG() << "Waiting to join add_hits_tphandler_thread";
      m_add_hits_tphandler_thread_should_run.store(false);
      m_add_hits_tphandler_thread.join();
      TLOG() << "add_hits_tphandler_thread joined";
   }    
    m_tphandler.reset();

    TaskRawDataProcessorModel<types::DUNEWIBSuperChunkTypeAdapter>::scrap(args);
  }

  void get_info(opmonlib::InfoCollector& ci, int level)
  {
    readoutlibs::readoutinfo::RawDataProcessorInfo info;

    if (m_tphandler != nullptr) {
      info.num_tps_sent = m_tphandler->get_and_reset_num_sent_tps();
      info.num_tpsets_sent = m_tphandler->get_and_reset_num_sent_tpsets();
      info.num_tps_dropped = m_tps_dropped.exchange(0);
    }
    info.num_frame_errors = m_frame_error_count.exchange(0);

    auto now = std::chrono::high_resolution_clock::now();
    if (m_sw_tpg_enabled) {
      int new_hits = m_swtpg_hits_count.exchange(0);
      int new_tps = m_num_tps_pushed.exchange(0);
      double seconds = std::chrono::duration_cast<std::chrono::microseconds>(now - m_t0).count() / 1000000.;
      TLOG_DEBUG(TLVL_TAKE_NOTE) << "Hit rate: " << std::to_string(new_hits / seconds / 1000.) << " [kHz]";
      TLOG_DEBUG(TLVL_TAKE_NOTE) << "Total new hits: " << new_hits << " new pushes: " << new_tps;
      info.rate_tp_hits = new_hits / seconds / 1000.;
    }
    m_t0 = now;

    readoutlibs::TaskRawDataProcessorModel<types::DUNEWIBSuperChunkTypeAdapter>::get_info(ci, level);
    ci.add(info);
  }

protected:
  // Internals
  timestamp_t m_previous_ts = 0;
  timestamp_t m_current_ts = 0;
  bool m_first_ts_missmatch = true;
  bool m_problem_reported = false;
  std::atomic<int> m_ts_error_ctr{ 0 };



  void postprocess_example(const types::DUNEWIBSuperChunkTypeAdapter* fp)
  {
    TLOG() << "Postprocessing: " << fp->get_first_timestamp();
  }

  /**
   * Pipeline Stage 1.: Check proper timestamp increments in WIB frame
   * */
  void timestamp_check(frameptr fp)
  {

    uint16_t wib2_tick_difference = types::DUNEWIBSuperChunkTypeAdapter::expected_tick_difference;
    uint16_t wib2_superchunk_tick_difference = wib2_tick_difference * fp->get_num_frames();

    // If EMU data, emulate perfectly incrementing timestamp
    if (inherited::m_emulator_mode) {                           // emulate perfectly incrementing timestamp
      uint64_t ts_next = m_previous_ts + wib2_superchunk_tick_difference;                   // NOLINT(build/unsigned)
      auto wf = reinterpret_cast<wibframeptr>(((uint8_t*)fp));  // NOLINT
      for (unsigned int i = 0; i < fp->get_num_frames(); ++i) { // NOLINT(build/unsigned)
        //auto wfh = const_cast<dunedaq::detdataformats::wib2::WIB2Header*>(wf->get_wib_header());
        wf->set_timestamp(ts_next);
        ts_next += wib2_tick_difference;
        wf++;
      }
    }

    // Acquire timestamp
    auto wfptr = reinterpret_cast<dunedaq::detdataformats::wib2::WIB2Frame*>(fp); // NOLINT
    m_current_ts = wfptr->get_timestamp();

    // Check timestamp
    if (m_current_ts - m_previous_ts != wib2_superchunk_tick_difference) {
      ++m_ts_error_ctr;
      m_error_registry->add_error("MISSING_FRAMES",
                                  readoutlibs::FrameErrorRegistry::ErrorInterval(m_previous_ts + wib2_superchunk_tick_difference, m_current_ts));
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
  void frame_error_check(frameptr fp)
  {
    if (!fp)
      return;

    auto wf = reinterpret_cast<wibframeptr>(((uint8_t*)fp)); // NOLINT
    for (size_t i = 0; i < fp->get_num_frames(); ++i) {
      if (m_frames_processed % 10000 == 0) {
        for (int i = 0; i < m_num_frame_error_bits; ++i) {
          if (m_error_occurrence_counters[i])
            m_error_occurrence_counters[i]--;
        }
      }

      m_current_frame_pushed = false;


      wf++;
      m_frames_processed++;
    }
  }

  /**
   * Pipeline Stage 3.: Do software TPG
   * */
  void find_hits(constframeptr fp, WIB2FrameHandler* frame_handler)
  {
    if (!fp)
      return;
    auto wfptr = reinterpret_cast<dunedaq::detdataformats::wib2::WIB2Frame*>((uint8_t*)fp); // NOLINT
    uint64_t timestamp = wfptr->get_timestamp();                        // NOLINT(build/unsigned)

    // Frame expansion
    swtpg_wib2::MessageRegisters registers_array;
    int register_selection = frame_handler->get_registers_selector();    
    expand_wib2_adcs(fp, &registers_array, register_selection); 
      

    if (m_first_hit) {
      std::thread::id thread_id = std::this_thread::get_id();
      pid_t tid;
      tid = syscall(SYS_gettid);
      TLOG() << " Thread ID " << thread_id << " PID " << tid ;

      frame_handler->register_channel_map = swtpg_wib2::get_register_to_offline_channel_map_wib2(wfptr, m_channel_map, register_selection);

      frame_handler->m_tpg_processing_info->setState(registers_array);

      // Debugging statements 
      m_link = wfptr->header.link;
      m_crate_no = wfptr->header.crate;
      m_slot_no = wfptr->header.slot;
      TLOG() << "Got first item, link/crate/slot=" << m_link << "/" << m_crate_no << "/" << m_slot_no;      

      std::stringstream ss;
      ss << " Channels are:\n";
      for(size_t i=0; i<swtpg_wib2::NUM_REGISTERS_PER_FRAME*swtpg_wib2::SAMPLES_PER_REGISTER; ++i){
        ss << i << "\t" << m_register_channel_map.channel[i] << "\n";
      }
      TLOG_DEBUG(2) << ss.str();      

    } // end if (m_first_hit)
    
    // Execute the SWTPG algorithm
    frame_handler->m_tpg_processing_info->input = &registers_array;
    *frame_handler->get_primfind_dest() = swtpg_wib2::MAGIC;
    swtpg_wib2::process_window_avx2(*frame_handler->m_tpg_processing_info);
    
    // Insert output of the AVX processing into the swtpg_output 
    swtpg_output swtpg_processing_result = {frame_handler->get_primfind_dest(), timestamp};

    // Push to the MPMC tphandler queue. Used the default timeout of 100 ms. 
    m_tphandler_queue.push(std::move(swtpg_processing_result), std::chrono::milliseconds(100));

     

    if (m_first_hit) {
      TLOG() << "Processed the first superchunk ";//;      
      m_first_hit = false;
    }
        
  }



  unsigned int process_swtpg_hits(uint16_t* primfind_it, timestamp_t timestamp)
  {

    constexpr int clocksPerTPCTick = 32;

    uint16_t chan[16], hit_end[16], hit_charge[16], hit_tover[16]; // NOLINT(build/unsigned)
    unsigned int nhits = 0;

    while (*primfind_it != swtpg_wib2::MAGIC) {
      // First, get all of the register values (including those with no hit) into local variables
      for (int i = 0; i < 16; ++i) {
        chan[i] = *primfind_it++; // NOLINT(runtime/increment_decrement)
      }
      for (int i = 0; i < 16; ++i) {
        hit_end[i] = *primfind_it++; // NOLINT(runtime/increment_decrement)
      }
      for (int i = 0; i < 16; ++i) {
        hit_charge[i] = *primfind_it++; // NOLINT(runtime/increment_decrement)
      }
      for (int i = 0; i < 16; ++i) {
        hit_tover[i] = *primfind_it++; // NOLINT(runtime/increment_decrement)
      }

      // Now that we have all the register values in local
      // variables, loop over the register index (ie, channel) and
      // find the channels which actually had a hit, as indicated by
      // nonzero value of hit_charge
      for (int i = 0; i < 16; ++i) {
        if (hit_charge[i] && chan[i] != swtpg_wib2::MAGIC) {
          // This channel had a hit ending here, so we can create and output the hit here
          const uint16_t offline_channel = m_register_channel_map.channel[chan[i]];
          uint64_t tp_t_begin =                                                        // NOLINT(build/unsigned)
            timestamp + clocksPerTPCTick * (int64_t(hit_end[i]) - hit_tover[i]);       // NOLINT(build/unsigned)
          uint64_t tp_t_end = timestamp + clocksPerTPCTick * int64_t(hit_end[i]);      // NOLINT(build/unsigned)

          // May be needed for TPSet:
          // uint64_t tspan = clocksPerTPCTick * hit_tover[i]; // is/will be this needed?
          //

          // For quick n' dirty debugging: print out time/channel of hits.
          // Can then make a text file suitable for numpy plotting with, eg:
          //
          // sed -n -e 's/.*Hit: \(.*\) \(.*\).*/\1 \2/p' log.txt  > hits.txt
          //
          //TLOG_DEBUG(0) << "Hit: " << tp_t_begin << " " << offline_channel;

          triggeralgs::TriggerPrimitive trigprim;
          trigprim.time_start = tp_t_begin;
          trigprim.time_peak = (tp_t_begin + tp_t_end) / 2;
          trigprim.time_over_threshold = hit_tover[i] * clocksPerTPCTick;
          trigprim.channel = offline_channel;
          trigprim.adc_integral = hit_charge[i];
          trigprim.adc_peak = hit_charge[i] / 20;
          trigprim.detid =
            m_link; // TODO: convert crate/slot/link to SourceID Roland Sipos rsipos@cern.ch July-22-2021
          trigprim.type = triggeralgs::TriggerPrimitive::Type::kTPC;
          trigprim.algorithm = triggeralgs::TriggerPrimitive::Algorithm::kTPCDefault;
          trigprim.version = 1;

          if (m_first_hit) {
            TLOG() << "TP makes sense? -> hit_t_begin:" << tp_t_begin << " hit_t_end:" << tp_t_end
                   << " time_peak:" << (tp_t_begin + tp_t_end) / 2;
          }

          if (!m_tphandler->add_tp(trigprim, timestamp)) {
            m_tps_dropped++;
          }

          m_new_tps++;
          ++nhits;
        }
      }
    }
    return nhits;
  }









  void add_hits_to_tphandler() {

    std::stringstream thread_name;
    thread_name << "tphandler-" << m_sourceid.id;
    pthread_setname_np(pthread_self(), thread_name.str().c_str());    

    while (m_add_hits_tphandler_thread_should_run.load()) {
      swtpg_output result_from_swtpg; 
     
      bool try_pop_from_tphandler_queue = m_tphandler_queue.try_pop(result_from_swtpg, std::chrono::milliseconds(100));
      if (try_pop_from_tphandler_queue) {
        // Process the trigger primitve
        unsigned int nhits = process_swtpg_hits(result_from_swtpg.output_location, result_from_swtpg.timestamp);

        // Debugging statement for TPHandler
        //if (nhits > 0) {
        //   TLOG_DEBUG(0) << "Non null hits: " << nhits << " for ts: " << result_from_swtpg.timestamp;
        //}
    
        m_swtpg_hits_count += nhits;

        m_tphandler->try_sending_tpsets(result_from_swtpg.timestamp);

      } 
      
    } // m_add_hits_tphandler_thread_should_run

  }






private:
  bool m_sw_tpg_enabled;

  size_t m_num_msg = 0;
  size_t m_num_push_fail = 0;

  std::atomic<int> m_swtpg_hits_count{ 0 };

  std::atomic<int> m_num_tps_pushed{ 0 };

  bool m_first_hit = true;

  std::atomic<bool> m_add_hits_tphandler_thread_should_run;

  uint8_t m_link; // NOLINT(build/unsigned)
  uint8_t m_slot_no;  // NOLINT(build/unsigned)
  uint8_t m_crate_no; // NOLINT(build/unsigned)

  std::shared_ptr<detchannelmaps::TPCChannelMap> m_channel_map;

  // Map from expanded AVX register position to offline channel number
  swtpg_wib2::RegisterChannelMap m_register_channel_map; 
                                                    

  // Frame error check
  bool m_current_frame_pushed = false;
  int m_error_counter_threshold;
  const int m_num_frame_error_bits = 16;
  int m_error_occurrence_counters[16] = { 0 };
  int m_error_reset_freq;


  std::shared_ptr<iomanager::SenderConcept<types::TriggerPrimitiveTypeAdapter>> m_tp_sink;
  std::shared_ptr<iomanager::SenderConcept<trigger::TPSet>> m_tpset_sink;
  std::shared_ptr<iomanager::SenderConcept<detdataformats::wib2::WIB2Frame>> m_err_frame_sink;

  std::unique_ptr<WIB2TPHandler> m_tphandler;

  // Select the registers where to process the frame expansion
  // E.g.: {0} --> divide registers by 2 (= 16/2) and select the first half
  // E.g.: {1} --> divide registers by 2 (= 16/2) and select the second half
  int selection_of_register = 0; 
  std::unique_ptr<WIB2FrameHandler> m_wib2_frame_handler = std::make_unique<WIB2FrameHandler>(selection_of_register);

  int selection_of_register_second_half = 1; 
  std::unique_ptr<WIB2FrameHandler> m_wib2_frame_handler_second_half = std::make_unique<WIB2FrameHandler>(selection_of_register_second_half);

  // AAA: TODO: make selection of the initial capacity of the queue configurable
  size_t m_initial_capacity_mpmc_queue = 100000; 
  // The boolean argument is `MayBlock`, where "block" appears to mean
  // "make a system call". With `MayBlock` set to false, the queue
  // just spin-waits, so we want true
  //std::unique_ptr<folly::DMPMCQueue<WIB2FrameHandler*, true>> m_tphandler_queue = std::make_unique<folly::DMPMCQueue<WIB2FrameHandler*, true>>(m_initial_capacity_mpmc_queue);
  //folly::DMPMCQueue<std::unique_ptr<WIB2FrameHandler>, true> m_tphandler_queue = folly::DMPMCQueue<std::unique_ptr<WIB2FrameHandler>, true>(m_initial_capacity_mpmc_queue);
  //iomanager::FollyMPMCQueue<std::unique_ptr<WIB2FrameHandler>> m_tphandler_queue{"tphandler_queue", 100};
  iomanager::FollyMPMCQueue<swtpg_output> m_tphandler_queue{"tphandler_queue", m_initial_capacity_mpmc_queue};



  std::thread m_add_hits_tphandler_thread;

  std::atomic<uint64_t> m_frame_error_count{ 0 }; // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_frames_processed{ 0 };  // NOLINT(build/unsigned)
  daqdataformats::SourceID m_sourceid;

  std::atomic<uint64_t> m_new_hits{ 0 }; // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_new_tps{ 0 };  // NOLINT(build/unsigned)
  std::atomic<uint64_t> m_tps_dropped{ 0 };

  std::chrono::time_point<std::chrono::high_resolution_clock> m_t0;
};

} // namespace fdreadoutlibs
} // namespace dunedaq

#endif // FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_WIB2_WIBFRAMEPROCESSOR_HPP_
