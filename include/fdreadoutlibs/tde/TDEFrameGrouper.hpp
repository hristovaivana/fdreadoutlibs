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

#include <vector>

namespace dunedaq {
namespace fdreadoutlibs {

class TDEFrameGrouper
{
public:
  void group(std::vector<std::vector<detdataformats::tde::TDE16Frame>>& v, detdataformats::tde::TDE16Frame* frames);

private:
};

} // namespace fdreadoutlibs
} // namespace dunedaq

#endif // FDREADOUTLIBS_INCLUDE_FDREADOUTLIBS_TDE_TDEFRAMEGROUPER_HPP_
