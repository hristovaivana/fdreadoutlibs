/**
 * @file TestSWTPGAlgorithms.cxx Main file for testing different swtpg algorithms 
 * @author Adam Abed Abud (adam.abed.abud@cern.ch)
 * 
 * This is part of the DUNE DAQ Application Framework, copyright 2022.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

// DUNE-DAQ
#include "readoutlibs/utils/FileSourceBuffer.hpp"
#include "readoutlibs/utils/BufferedFileWriter.hpp"
#include "fdreadoutlibs/DUNEWIBSuperChunkTypeAdapter.hpp"



// Local
#include "SwtpgTest/SwtpgBase.hpp"
#include "SwtpgTest/SwtpgNaive.hpp"
#include "SwtpgTest/SwtpgAvx.hpp"
//#include "SwtpgTest/RSNaive.hpp"
#include "SwtpgTest/RSAvx.hpp"


// system
#include "CLI/CLI.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <algorithm>


#include <cstring>
#include <immintrin.h>
#include <cstdio> 
#include <array>
#include <chrono>
#include <stdio.h>
#include <stdint.h>
#include <memory>




int 
main(int argc, char** argv)
{

    CLI::App app{ "Test SWTPG algorithms" };

    // Set default input frame file
    std::string frame_file_path = "/nfs/sw/work_dirs/aabedabu/daq_config/frames_wib2.bin";
    app.add_option("-f,--frame_file_path", frame_file_path, "Path to the input frame file");


    std::string select_algorithm;
    app.add_option("-a,--algorithm", select_algorithm, "SWTPG Algorithm (SWTPG / RS)");
  
    std::string select_implementation;
    app.add_option("-i,--implementation", select_implementation, "SWTPG implementation (AVX / NAIVE)");
  
    bool save_adc_data{false};
    app.add_option("--save_adc_data", save_adc_data, "Save ADC data (true/false)");

    bool save_hit_data{false};
    app.add_option("--save_hit_data", save_hit_data, "Save hit data (true/false)");


    CLI11_PARSE(app, argc, argv);

    std::unique_ptr<SwtpgBase> algo;

    if (select_algorithm == "SWTPG") {
      if (select_implementation == "NAIVE") {
        algo = std::make_unique<SwtpgNaive>(save_adc_data, save_hit_data);
        std::cout << "Created an instance of the " << select_algorithm << " algorithm ( " << select_implementation << " )" << std::endl;
      } else if (select_implementation == "AVX") {
        algo = std::make_unique<SwtpgAvx>(save_adc_data, save_hit_data);
        std::cout << "Created an instance of the " << select_algorithm << " algorithm ( " << select_implementation << " )" << std::endl;
      } else {
        std::cout << "Select a valid algorithm implementation. Use --help for further details." << std::endl;
        return 1;
      }
    } else if (select_algorithm == "RS") {
      if (select_implementation == "NAIVE") {
        //algo = std::make_unique<RSNaive>(save_adc_data, save_hit_data);
        std::cout << "Created an instance of the " << select_algorithm << " algorithm ( " << select_implementation << " )" << std::endl;
      } else if (select_implementation == "AVX") {
        algo = std::make_unique<RSAvx>(save_adc_data, save_hit_data);
        std::cout << "Created an instance of the " << select_algorithm << " algorithm ( " << select_implementation << " )" << std::endl;
      } else {
        std::cout << "Select a valid algorithm implementation. Use --help for further details." << std::endl;
        return 1;
      }
    } else {
      std::cout << "Select at least an algorithm. Use --help for further details." << std::endl;
      return 1;
    }


    
    // =================================================================
    //                       READ THE FRAMES.BIN FILE
    // =================================================================
    const std::string input_file = frame_file_path;
    std::unique_ptr<dunedaq::readoutlibs::FileSourceBuffer> m_source_buffer;
    m_source_buffer = std::make_unique<dunedaq::readoutlibs::FileSourceBuffer>(10485100, swtpg_wib2::SUPERCHUNK_FRAME_SIZE);
 
    m_source_buffer->read(input_file);
    auto& source = m_source_buffer->get(); 
    int num_superchunks = m_source_buffer->num_elements(); // file_ size/chunk_size = 180 

    std::cout << "Number of superchunks in the input file: " << num_superchunks << std::endl;

    // =================================================================
    //                       Process the DUNEWIB superchunks
    // =================================================================
    bool first_hit = true;
    int offset = 0;   


    // Loop over the DUNEWIB superchunks in the file
    //while (offset<num_superchunks){
    while (offset<50 ){

      // current superchunk
      auto fp = reinterpret_cast<dunedaq::fdreadoutlibs::types::DUNEWIBSuperChunkTypeAdapter*>(source.data() + offset*swtpg_wib2::SUPERCHUNK_FRAME_SIZE);

      // Reset the memory buffers
      algo->reset(first_hit);


      // Find the SWTPG hits
      algo->find_hits(fp, first_hit);


      first_hit = false;    
      ++offset;

      std::cout << "Executing superchunk number " << offset << " out of " << num_superchunks << std::endl;


    }

    std::cout << "\n\n===============================" << std::endl;
    std::cout << "Found in total " << algo->m_total_swtpg_hits << " hits." << std::endl;
    
    

    std::cout << "\n\nFinished testing." << std::endl;




}


