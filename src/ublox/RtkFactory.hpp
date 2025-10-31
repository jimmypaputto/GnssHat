/*
 * Jimmy Paputto 2025
 */

#ifndef JP_RTK_FACTORY_HPP_
#define JP_RTK_FACTORY_HPP_

#include "GnssConfig.hpp"
#include "Rtcm3Store.hpp"
#include "RTK.hpp"


namespace JimmyPaputto::RtkFactory
{

IRtk* create(Rtcm3Store& rtcm3Store, const GnssConfig& config);

}  // JimmyPaputto::RtkFactory

#endif  // JP_RTK_FACTORY_HPP_
