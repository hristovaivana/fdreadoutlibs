/*
 * @file TDEFrameGrouper.cpp Frame grouper implementation
 *
 * This is part of the DUNE DAQ , copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */
#include "fdreadoutlibs/tde/TDEFrameGrouper.hpp"

#include <vector>

using dunedaq::readoutlibs::logging::TLVL_BOOKKEEPING;

namespace dunedaq {
namespace fdreadoutlibs {

void 
TDEFrameGrouper::group(std::vector<std::vector<detdataformats::tde::TDE16Frame>>& v, 
                       detdataformats::tde::TDE16Frame* frames)
{
  for (int i = 0; i < 12 * 64; i++) {
    v[frames[i].get_tde_header()->slot][frames[i].get_tde_header()->link] = frames[i];
  }
}

} // namespace fdreadoutlibs
} // namespace dunedaq


