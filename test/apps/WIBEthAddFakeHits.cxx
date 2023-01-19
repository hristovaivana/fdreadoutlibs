/**
 * @file WIB2AddFakeHits.cxx: quick and dirty solution to modify an input binary file and add 
 * fake hits at a predefined frame and channel numbers 
 * Usage: ./WIB2EthAddFakeHits ./output_0_3.out 100
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2022.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "detdataformats/wibeth/WIBEthFrame.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <algorithm>


#include <cstring>
#include <immintrin.h>
#include <cstdio> // For printf
#include <array>
#include <chrono>
#include <stdio.h>
#include <stdint.h>


#include "hdf5libs/HDF5RawDataFile.hpp"
#include "hdf5libs/hdf5filelayout/Nljs.hpp"

#include "logging/Logging.hpp"

#include <iostream>
#include <sstream>
#include <string>

using namespace dunedaq::hdf5libs;
using namespace dunedaq::daqdataformats;


void
print_usage()
{
  TLOG() << "Usage: WIB2EthAddFakeHits <input_file_name> <input_channel>";
}


class FrameFile
{
public:

    FrameFile(const char* filename)
        : m_file(filename, std::ifstream::binary),
          m_buffer(new char[sizeof(dunedaq::detdataformats::wibeth::WIBEthFrame)])
    {
        if(m_file.bad() || m_file.fail() || !m_file.is_open()){
            throw std::runtime_error(std::string("Bad file ")+std::string(filename));
        }
        // Calculate the length of the file
        m_file.seekg(0, m_file.end);
        m_length = m_file.tellg();
        m_file.seekg(0, m_file.beg);
        if(m_length==0){
            throw std::runtime_error("Empty file");
        }
        if(m_length%sizeof(dunedaq::detdataformats::wibeth::WIBEthFrame)!=0){
            throw std::runtime_error("File does not contain an integer number of frames");
        }
        m_n_frames=m_length/sizeof(dunedaq::detdataformats::wibeth::WIBEthFrame);

	// Reinterpret the frame as WIB2Frame
	
	m_file.read(m_buffer, m_file.eof());
        m_wib2_frame = reinterpret_cast<dunedaq::detdataformats::wibeth::WIBEthFrame*>(m_buffer);
    }

    ~FrameFile()
    {
        m_file.close();
        delete[] m_buffer;
    }

    // Length of the file in bytes
    size_t length() const {return m_length;}
    // Number of frames in the file
    size_t num_frames() const { return m_n_frames; }

    dunedaq::detdataformats::wibeth::WIBEthFrame* get_wib2_frame() const { return m_wib2_frame; }

    dunedaq::detdataformats::wibeth::WIBEthFrame* frame(size_t i)
    {
        if(i>=num_frames()) return nullptr;
        // Seek to the right place in the file
        m_file.seekg(i*sizeof(dunedaq::detdataformats::wibeth::WIBEthFrame));
        // Check we didn't go past the end
        if(m_file.bad() || m_file.eof()) return nullptr;
        // Actually read the fragment into the buffer
        m_file.read(m_buffer,sizeof(dunedaq::detdataformats::wibeth::WIBEthFrame));
        if(m_file.bad() || m_file.eof()) return nullptr;
        return reinterpret_cast<dunedaq::detdataformats::wibeth::WIBEthFrame*>(m_buffer);
    }
    

    

protected:
    std::ifstream m_file;
    char* m_buffer;
    dunedaq::detdataformats::wibeth::WIBEthFrame* m_wib2_frame = nullptr;
    size_t m_length;
    size_t m_n_frames;
};




int main(int argc, char** argv)
{
 

  if (argc != 3) {
    print_usage();
    return 1;
  }

  const std::string ifile_name = std::string(argv[1]);

  const size_t input_ch = atoi(argv[2]);

  // Read file
  FrameFile input_file = FrameFile(ifile_name.c_str()); 

  std::cout << "Size of the input file " << input_file.length() << std::endl;
  std::cout << "Number of frames " << input_file.num_frames() << std::endl;
   
  size_t frame_number = 2050; // frame chosen randomly
  size_t ch = input_ch;

  
  dunedaq::detdataformats::wibeth::WIBEthFrame* frame = input_file.frame(frame_number);
  
  auto val_to_check = frame->get_adc(ch);
  std::cout << "Output adc value: " << val_to_check << " for frame " << frame_number << " and channel number " << ch << std::endl;

 
  // Write output file 
  // Modified two channels in the input
  std::fstream output_file;
  output_file.open("output_230117.bin", std::ios::app | std::ios::binary);

  dunedaq::detdataformats::wibeth::WIBEthFrame* output_frame; 
  for (int i=0; i<input_file.num_frames(); i++) {
     output_frame = input_file.frame(i);
     //if (i==frame_number || i==frame_number+1 || i==frame_number+2 ) {
     if ( i%5 == 0  ) {
       for (int j=0; j<5; j++) {
         //auto adc_val2 = output_frame->get_adc(12+j);
         //output_frame->set_adc(12+j, 5000);

         auto adc_val = output_frame->get_adc(ch+j);
	     std::cout << "Channel " << ch+j << " "; 
	     std::cout << adc_val << " \n" ;
         output_frame->set_adc(ch+j, 0, 5000);
	     std::cout << "AFTER\n";
         adc_val = output_frame->get_adc(ch+j);
	     std::cout << "Channel " << ch+j << " "; 
	     std::cout << adc_val << " \n" ;


       }
       std::cout << "FINISHED LOOP" <<std::endl;
     } 
     output_file.write(reinterpret_cast<char*>(output_frame), sizeof(dunedaq::detdataformats::wibeth::WIBEthFrame) );
  
  } 


}
