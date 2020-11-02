//
// Created by Sebastian Lindner on 14.10.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMANAGER_HPP

#include <cstdint>
#include <map>
#include "ReservationTable.hpp"
#include "FrequencyChannel.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/**
	 * For one user, the Reservation Manager provides a wrapper for managing reservation tables for each logical frequency channel.
	 */
	class ReservationManager {
		public:
			ReservationManager(uint32_t planning_horizon);
			
			void addFrequencyChannel(bool is_point_to_point_channel, uint64_t center_frequency, uint64_t bandwidth);
		protected:
			/** Number of slots to remember both in the past and in the future. */
			uint32_t planning_horizon;
			/** One reservation table per frequency channel. */
			std::map<FrequencyChannel, ReservationTable> reservation_tables;
	};
	
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMANAGER_HPP
