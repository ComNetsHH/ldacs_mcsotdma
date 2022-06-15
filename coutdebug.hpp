//
// Created by Sebastian Lindner on 11.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_COUTD_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_COUTD_HPP

#include <iostream>

#if 1
	#define coutd std::cout
#else
	#define coutd 0 && std::cout
#endif

#endif //TUHH_INTAIRNET_MC_SOTDMA_COUTD_HPP
