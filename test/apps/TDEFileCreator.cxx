/**
 * @file TDEFileCreator.cxx Create a binary file with TDE Frames for reading it from readout
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "detdataformats/tde/TDE16Frame.hpp"

#include <iostream>
#include <fstream>

using namespace dunedaq;

int main()
{
  int total_frames = 10 * 64;

  std::ofstream out("frames.bin", std::ios::out | std::ios::binary);

  for (int batch = 0; batch < total_frames / 64; batch++)
  {
    uint64_t timestamp = batch;
    for (int iframe = 0; iframe < 64; iframe++)
    {
      detdataformats::tde::TDE16Frame fr;
      // Timestamp
      fr.set_timestamp(timestamp);
      // Channel
      fr.get_tde_header()->crate = iframe;
      out.write(reinterpret_cast<char*>(&fr), sizeof(detdataformats::tde::TDE16Frame));
    }
  }
  out.close();

}
