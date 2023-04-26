#include "detdataformats/wib2/WIB2Frame.hpp"
#include "detdataformats/fwtp/RawTp.hpp"

#include "fdreadoutlibs/DUNEWIBFirmwareTriggerPrimitiveSuperChunkTypeAdapter.hpp"
#include "readoutlibs/utils/FileSourceBuffer.hpp"

#include "detchannelmaps/TPCChannelMap.hpp"
#include "readoutlibs/ReadoutIssues.hpp"

// system
#include "CLI/CLI.hpp"

//#include <fstream>

using namespace dunedaq;
using namespace dunedaq::fdreadoutlibs;
using frame_ptr = types::DUNEWIBFirmwareTriggerPrimitiveSuperChunkTypeAdapter*;
using rwtp_ptr = detdataformats::fwtp::RawTp*;

struct TpSubframeOne
{
  uint32_t word1; // NOLINT(build/unsigned)
};

static const constexpr std::size_t RAW_WIB_TP_SUBFRAME_SIZE = 4; // 12=multiple of 3 4B/32b words

// stitching algorithm
  static double m_time_tick { 32.0 }; // 
  static int m_stitch_constant { 2048 }; // number of ticks between WIB-to-TP packets
  std::shared_ptr<detchannelmaps::TPCChannelMap> m_channel_map{dunedaq::detchannelmaps::make_map("VDColdboxChannelMap")};

  static std::vector<triggeralgs::TriggerPrimitive> m_A[256][10][10][10]; // keep track of TPs to stitch per channel/slot/fiber/ccrate
  static std::vector<uint64_t> m_T[256][10][10][10]; // NOLINT // keep track of last stitched start time
  static std::atomic<uint64_t> m_tps_stitched { 0 }; // NOLINT
  static std::atomic<uint64_t> m_tp_frames  { 0 }; // NOLINT
  static std::atomic<uint64_t> m_tp_frames_bad  { 0 }; // NOLINT
  static std::atomic<int> m_tp_hits { 0 };
  static std::vector<int> m_nhits { 0, 0, 0, 0, 0, 0, 0, 0};
  // interface to DS
  //std::unique_ptr<WIB2TPHandler> m_tphandler;
  std::atomic<uint64_t> m_tps_dropped{ 0 }; // NOLINT
  // info
  std::atomic<uint64_t> m_sent_tps{ 0 }; // NOLINT(build/unsigned)
  static std::atomic<int> m_total_hits_count{ 0 };
  // results
  std::vector<triggeralgs::TriggerPrimitive> m_tps;
    std::vector<triggeralgs::TriggerPrimitive> m_tps_unstitched;
    std::vector<triggeralgs::TriggerPrimitive> m_wibts;

void tp_save(std::vector<triggeralgs::TriggerPrimitive>& tps, const std::string& filename)
{
  std::ofstream fdo(filename);
  if (!fdo.is_open()) { fdo.close(); return void(); }
  fdo << ",start_time,peak_time,time_over_threshold,offline_ch,sum_adc,peak_adc,flag" << std::endl;
  int counter = 0;
  for (auto& t : tps) {
    fdo << counter << "," << t.time_start << "," << t.time_peak << "," << t.time_over_threshold << "," << t.channel << "," << t.adc_integral << "," << t.adc_peak << "," << 0 << std::endl;
    counter++;
  }
  fdo.close();
}

void tp_stitch(rwtp_ptr rwtp)
{
  m_tp_frames++;
  uint64_t ts_0 = rwtp->m_head.get_timestamp(); // NOLINT
  int nhits = rwtp->m_head.get_nhits(); // NOLINT
  TLOG() << "2: Frame has number of hits " << nhits;
  uint8_t m_channel_no = rwtp->m_head.m_wire_no; // NOLINT
  uint8_t m_fiber_no = rwtp->m_head.m_fiber_no; // NOLINT
  uint8_t m_crate_no = rwtp->m_head.m_crate_no; // NOLINT
  uint8_t m_slot_no = (rwtp->m_head.m_slot_no) & ((uint8_t) 0x7); // NOLINT
  uint offline_channel = m_channel_map->get_offline_channel_from_crate_slot_fiber_chan(m_crate_no, m_slot_no, m_fiber_no, m_channel_no); 
  TLOG() << "DBG offline channel for crate/slot/fiber/wire " << (int)offline_channel << " " << (int)m_crate_no << ", " << (int)m_slot_no << ", " << (int)m_fiber_no << ", " << (int)m_channel_no;

  if (nhits < 8) { 
    m_nhits[nhits] += 1;
  }
  m_total_hits_count += nhits;

  m_tp_hits += nhits;

  for (int i = 0; i < nhits; i++) {

    TLOG() << "DBG offline channel, hit " << (int)offline_channel << ", " << i << " : " << (int)rwtp->m_blocks[i].m_start_time << ", " << (int)rwtp->m_blocks[i].m_peak_time << ", " << (int)rwtp->m_blocks[i].m_end_time << ", " << (int)rwtp->m_blocks[i].m_sum_adc << ", " << (int)rwtp->m_blocks[i].m_peak_adc;
    triggeralgs::TriggerPrimitive trigprim;
    trigprim.time_start = ts_0 + rwtp->m_blocks[i].m_start_time * m_time_tick;
    trigprim.time_peak = ts_0 + rwtp->m_blocks[i].m_peak_time * m_time_tick;
    trigprim.time_over_threshold = (rwtp->m_blocks[i].m_end_time - rwtp->m_blocks[i].m_start_time) * m_time_tick;
    trigprim.channel = offline_channel; //offline_channel; // m_channel_no;
    trigprim.adc_integral = rwtp->m_blocks[i].m_sum_adc;
    trigprim.adc_peak = rwtp->m_blocks[i].m_peak_adc;
    trigprim.detid =
            m_fiber_no; // TODO: convert crate/slot/fiber to SourceID Roland Sipos rsipos@cern.ch July-22-2021
    trigprim.type = triggeralgs::TriggerPrimitive::Type::kTPC;
    trigprim.algorithm = triggeralgs::TriggerPrimitive::Algorithm::kTPCDefault;
    trigprim.version = 1;

    // unstitched 
    //m_tps.push_back(trigprim);
    m_wibts.push_back(ts_0);
    //continue;

    // stitch current hit to previous hit
    TLOG() << "DBG case 1: stitch " << m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no].size();
    TLOG() << "DBG case 1: stitch " << m_T[m_channel_no][m_slot_no][m_fiber_no][m_crate_no].size();
    if (m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no].size() == 1) {
      if (static_cast<int>(rwtp->m_blocks[i].m_start_time) == 0
          && (
          static_cast<int>(trigprim.time_start) - static_cast<int>(m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no][0].time_start)
             <= static_cast<int>(m_stitch_constant)
          || (static_cast<int>(trigprim.time_start) - static_cast<int>(m_T[m_channel_no][m_slot_no][m_fiber_no][m_crate_no][0])
             <= static_cast<int>(m_stitch_constant))
             )
          ) {
        // current hit is continuation of previous hit
        m_T[m_channel_no][m_slot_no][m_fiber_no][m_crate_no].clear();
        if (trigprim.adc_peak > m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no][0].adc_peak) {
          m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no][0].time_peak = trigprim.time_peak;
          m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no][0].adc_peak = trigprim.adc_peak;
        }
        m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no][0].time_over_threshold += trigprim.time_over_threshold;
        m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no][0].adc_integral += trigprim.adc_integral;
        m_T[m_channel_no][m_slot_no][m_fiber_no][m_crate_no].push_back(trigprim.time_start);

      } else {
        // current hit is not continuation of previous hit
        // add previous hit to TriggerPrimitives
        //if (m_tphandler != nullptr && !m_tphandler->add_tp(std::move(m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no][0]), ts_0)) {
        //  m_tps_dropped++;
        //}
        m_tps.push_back(m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no][0]);
        m_tps_stitched++;
        //if (m_tphandler != nullptr) { m_tphandler->try_sending_tpsets(ts_0); } 
        m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no].clear();
        m_T[m_channel_no][m_slot_no][m_fiber_no][m_crate_no].clear();
      }
    }

    // NB for TPSets: this assumes hits come ordered in time 
    // current hit (is, completes or starts) one TriggerPrimitive 
    uint8_t m_tp_continue = rwtp->m_blocks[i].m_hit_continue; // NOLINT
    uint8_t m_tp_end_time = rwtp->m_blocks[i].m_end_time; // NOLINT
 
    if (m_tp_continue == 0 && m_tp_end_time != 63) {
      if (m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no].size() == 1) {
        // the current hit completes one stitched TriggerPrimitive
        //if (m_tphandler != nullptr && !m_tphandler->add_tp(std::move(m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no][0]), ts_0)) {
        //  m_tps_dropped++;
        //}
        //if (m_tphandler != nullptr) { m_tphandler->try_sending_tpsets(ts_0); } 
        m_tps.push_back(m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no][0]);
        m_tps_stitched++;
        m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no].clear();
        m_T[m_channel_no][m_slot_no][m_fiber_no][m_crate_no].clear();
      } else {
        // the current hit is one TriggerTrimitive
        //if (m_tphandler != nullptr && !m_tphandler->add_tp(std::move(trigprim), ts_0)) {
        //  m_tps_dropped++;
        //}
        //if (m_tphandler != nullptr) { m_tphandler->try_sending_tpsets(ts_0); } 
        m_tps.push_back(trigprim);
        m_tps_stitched++;      
      }
    } else {
      // the current hit starts one TriggerPrimitive
      if (m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no].size() == 0) {
        m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no].push_back(trigprim);
        m_T[m_channel_no][m_slot_no][m_fiber_no][m_crate_no].push_back(trigprim.time_start);
      } else { // decide to add long TriggerPrimitive even when it doesn't end properly
               // this is rare case and can be removed for efficiency    
        // the current hit is "bad"
        // add one TriggerPrimitive from previous stitched hits except the current hit  
        if ( m_tp_continue == 0 && m_tp_end_time == 63 &&
             static_cast<int>(trigprim.time_start) - static_cast<int>(m_T[m_channel_no][m_slot_no][m_fiber_no][m_crate_no][0])
             <= static_cast<int>(m_stitch_constant)) {
          //if (m_tphandler != nullptr && !m_tphandler->add_tp(std::move(m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no][0]), ts_0)) {
          //  m_tps_dropped++;
          //}
          //if (m_tphandler != nullptr) { m_tphandler->try_sending_tpsets(ts_0); }
          m_tps.push_back(m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no][0]);
          m_tps_stitched++;      
          m_A[m_channel_no][m_slot_no][m_fiber_no][m_crate_no].clear();
          m_T[m_channel_no][m_slot_no][m_fiber_no][m_crate_no].clear();
        }
      }
    }
  }
} // NOLINT (exceeding 80 lines)

void tp_unpack(frame_ptr fr)  
{
  //std::size_t RAW_WIB_TP_BLOCK_SIZE = 12;

  auto& srcbuffer = fr->get_data();
  int num_elem = fr->get_raw_tp_frame_chunksize();

  if (num_elem == 0) {
    TLOG() << "No raw WIB TP elements to read from buffer! ";
    return;
  }
  if (num_elem % RAW_WIB_TP_SUBFRAME_SIZE != 0) {
    TLOG() << "Raw WIB TP elements not multiple of subframe size (3)! ";
    return;
  }
  TLOG() << "TP unpack number of elements:  " << num_elem;

  double dbg_frames = 0;
  double dbg_microseconds = 0;
  int offset = 0;
  while (offset <= num_elem) {

    if (offset == num_elem) break;
 
    // Count number of subframes in a TP frame
    int n;
    bool ped_found { false };
    for (n=0; offset + n*RAW_WIB_TP_SUBFRAME_SIZE<(size_t)num_elem; ++n) {
      //if (reinterpret_cast<types::TpSubframe*>(((uint8_t*)srcbuffer.data()) // NOLINT
      //     + offset + n*RAW_WIB_TP_SUBFRAME_SIZE)->word3 == 0xDEADBEEF) {
      //  ped_found = true;
      //  break; 
      //}
      //if (reinterpret_cast<uint32_t>(*((uint8_t*)(srcbuffer.data()) + 
      

      if (reinterpret_cast<TpSubframeOne*>(((uint8_t*)srcbuffer.data()) // NOLINT
        + offset + n*RAW_WIB_TP_SUBFRAME_SIZE)->word1 == 0xDEADBEEF) {
        ped_found = true;
        break;
      }
             
    }
    // Found no pedestal block
    if (!ped_found) {
      TLOG() << "Debug message: Raw WIB TP chunk contains no TP frames! Chunk size / offset / subframes is " << num_elem << " / " << offset << " / " << n;
      return;
    }

    TLOG() << "Pedestal block found  " << n << " at offset " << offset;
    if ( (n < 8) || ((n+1) % 3 != 0) ) {
      if (n < 8) {
        TLOG() << "Frame is too short ";
      }
      if ((n+1) % 3 != 0) {
        TLOG() << "Frame is not expected size " << n+1 << " at offset " << offset;
      }
      offset += (n+1)*RAW_WIB_TP_SUBFRAME_SIZE;
      m_tp_frames_bad++;
      continue;
    }

    int bsize = (n+1) * RAW_WIB_TP_SUBFRAME_SIZE;
    std::vector<char> tmpbuffer;
    tmpbuffer.reserve(bsize);
    int nhits = (n+1)/3 - 2;  //
    TLOG() << "1: Frame has number of hits " << nhits;
    if (nhits > 8) {
      TLOG() << "############################## TOO MANY HITS ##################### " << nhits;
      offset += (n+1)*RAW_WIB_TP_SUBFRAME_SIZE;
      m_tp_frames_bad++;
      continue;  
    }
    // add header block 
    ::memcpy(static_cast<void*>(tmpbuffer.data() + 0),
             static_cast<void*>(srcbuffer.data() + offset),
             3*RAW_WIB_TP_SUBFRAME_SIZE);

    // add pedinfo block 
    ::memcpy(static_cast<void*>(tmpbuffer.data() + 3*RAW_WIB_TP_SUBFRAME_SIZE),
             static_cast<void*>(srcbuffer.data() + offset + (nhits+1)*3*RAW_WIB_TP_SUBFRAME_SIZE),
             3*RAW_WIB_TP_SUBFRAME_SIZE);

    // add TP hits
    ::memcpy(static_cast<void*>(tmpbuffer.data() + 2*3*RAW_WIB_TP_SUBFRAME_SIZE),
             static_cast<void*>(srcbuffer.data() + offset + 3*RAW_WIB_TP_SUBFRAME_SIZE),
             nhits*3*RAW_WIB_TP_SUBFRAME_SIZE);

    auto heap_memory_block = malloc(
         sizeof(dunedaq::detdataformats::fwtp::TpHeader) + 
         nhits * sizeof(dunedaq::detdataformats::fwtp::TpData));
    rwtp_ptr rwtp =
         static_cast<dunedaq::detdataformats::fwtp::RawTp*>(heap_memory_block);

    ::memcpy(static_cast<void*>(&rwtp->m_head),
             static_cast<void*>(tmpbuffer.data() + 0),
             2*3*RAW_WIB_TP_SUBFRAME_SIZE);

    for (int i=0; i<nhits; i++) {
      ::memcpy(static_cast<void*>(&rwtp->m_blocks[i]),
               static_cast<void*>(tmpbuffer.data() + (2+i)*3*RAW_WIB_TP_SUBFRAME_SIZE),
               3*RAW_WIB_TP_SUBFRAME_SIZE);
    }
    dbg_frames++;
 
    // old format lacks number of hits
    rwtp->set_nhits(nhits); // explicitly set number of hits in new format
    TLOG() << "Frame timestamp is " << rwtp->get_timestamp();

    // stitch TP hits
    tp_stitch(rwtp);
    offset += (2+nhits)*3*RAW_WIB_TP_SUBFRAME_SIZE;
    free(heap_memory_block);
  }
}


int
main(int argc, char** argv)
{
  CLI::App app{ "Test WIB2TP processing" };
  /*
  std::vector<std::string> files{
    "/nfs/home/ivhristo/dune/rawhit/v320/tmp/350/bin/theshold350-00-180-230314-122133-1_1.bin"
    ,"/nfs/home/ivhristo/dune/rawhit/v320/tmp/350/bin/theshold350-00-1C0-230314-122133-1_1.bin"
    ,"/nfs/home/ivhristo/dune/rawhit/v320/tmp/350/bin/theshold350-10-180-230314-122133-1_1.bin"
    ,"/nfs/home/ivhristo/dune/rawhit/v320/tmp/350/bin/theshold350-10-1C0-230314-122133-1_1.bin"
  };*/
  
  std::vector<std::string> files{
    "/nfs/home/ivhristo/dune/rawhit/v320/tmp/350/bin/fcheck/theshold350-00-180-230314-122133-1_2.dat"
    ,"/nfs/home/ivhristo/dune/rawhit/v320/tmp/350/bin/fcheck/theshold350-00-1C0-230314-122133-1_2.dat"
    ,"/nfs/home/ivhristo/dune/rawhit/v320/tmp/350/bin/fcheck/theshold350-10-180-230314-122133-1_2.dat"
    ,"/nfs/home/ivhristo/dune/rawhit/v320/tmp/350/bin/fcheck/theshold350-10-1C0-230314-122133-1_2.dat"
  };

  int count = 0;
  for (auto& it : files) {
  std::string filename = it;

  m_A[256][10][10][10].clear(); // keep track of TPs to stitch per channel/slot/fiber/ccrate
  m_T[256][10][10][10].clear(); // NOLINT // keep track of last stitched start time
  m_tps_stitched = 0; // NOLINT
  m_tp_frames = 0; // NOLINT
  m_tp_frames_bad = 0; // NOLINT
  m_tp_hits = 0;
  m_nhits.clear();
  m_tps_dropped = 0; // NOLINT
  m_sent_tps = 0; // NOLINT(build/unsigned)
  m_total_hits_count = 0;
  m_tps.clear();



  types::DUNEWIBFirmwareTriggerPrimitiveSuperChunkTypeAdapter m_payload_wrapper;
  std::unique_ptr<readoutlibs::FileSourceBuffer> m_file_source;
  m_file_source = std::make_unique<readoutlibs::FileSourceBuffer>(10000000, RAW_WIB_TP_SUBFRAME_SIZE);

  try {
        m_file_source->read(filename);
      } catch (const ers::Issue& excpt) {
      throw readoutlibs::ResourceQueueError(ERS_HERE, "input file", "TestApp", excpt);
    }
  
  int offset = 0;
    auto& source = m_file_source->get();
    int num_elem = m_file_source->num_elements(); // bytes 
    TLOG() << "WIB2 TP elements to read from buffer " << num_elem;

    if (num_elem == 0) {
      TLOG() << "No raw WIB2 TP elements to read from buffer! Stopping...";
      return(0);
    }


  int bsize = num_elem * static_cast<int>(RAW_WIB_TP_SUBFRAME_SIZE);
      std::vector<char> tmpbuffer;
      tmpbuffer.reserve(bsize);
      ::memcpy(static_cast<void*>(tmpbuffer.data()),
               static_cast<void*>(source.data() + offset),
               bsize);
      m_payload_wrapper.set_raw_tp_frame_chunk(tmpbuffer);

  tp_unpack(&m_payload_wrapper);

  
  float frac = 0;
  frac = (float)((float)m_tp_frames_bad/(float)m_tp_frames)*100.0;
  std::cout << "Number of TP frames/bad " << m_tp_frames << "/" << m_tp_frames_bad << "/" << frac << "%" << std::endl;
  std::cout << "Number of hits " << m_tp_hits << "/" << m_total_hits_count << std::endl;
  std::cout << "Number of stitched TPs " << m_tps.size() << "/" << m_tps_stitched << std::endl;
  int cnt = 0;
  for (auto& t : m_nhits) {
    frac = (float)((float)t/(float)m_tp_frames)*100.0;
    std::cout << " -- Number of frames with " << cnt << " hit(s) " << t << "/" << frac << "%" << std::endl; 
    cnt++;
  }
  tp_save(m_tps, "fwtp_stitched_"+std::to_string(count)+".txt");
  count++;
  }

  std::cout << "\n\nFinished testing." << std::endl;


}
/*
/nfs/home/ivhristo/dune/rawhit/v320/tmp/350/bin/theshold350-00-1C0-230314-122133-1_2.bin  880640  
*/
