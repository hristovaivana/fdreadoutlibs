/*
 * @file TDEFrameGrouper.hpp Group TDE frames in together based on their slot number
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#ifndef FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_TDE_TDEFRAMEGROUPER_HPP_
#define FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_TDE_TDEFRAMEGROUPER_HPP_

#include "detdataformats/tde/TDE16Frame.hpp"
#include "fdreadoutlibs/FDReadoutTypes.hpp"

using dunedaq::readoutlibs::logging::TLVL_BOOKKEEPING;

namespace dunedaq {
namespace fdreadoutlibs {

class TDEFrameGrouper
{

public:

  void group(std::vector<std::vector<detdataformats::tde::TDE16Frame>>& v)
  {
    for (int i = 0; i < 12 * 64; i++) {
        v[frames[i].get_tde_header()->slot][frames[i].get_tde_header()->link] = frames[i];
    }
  }

private:
};

} // namespace fdreadoutlibs
} // namespace dunedaq

#endif // FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_TDE_TDEFRAMEGROUPER_HPP_


