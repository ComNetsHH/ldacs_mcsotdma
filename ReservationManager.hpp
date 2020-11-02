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
			explicit ReservationManager(uint32_t planning_horizon);
			
			/**
			 * Adds a frequency channel and corresponding reservation table.
			 * @param is_p2p
			 * @param center_frequency
			 * @param bandwidth
			 */
			void addFrequencyChannel(bool is_p2p, uint64_t center_frequency, uint64_t bandwidth);
			void removeFrequencyChannel(uint64_t center_frequency);
			
			FrequencyChannel& getFreqChannel(uint64_t center_frequency);
			ReservationTable& getReservationTable(uint64_t center_frequency);
		protected:
			/** Number of slots to remember both in the past and in the future. */
			uint32_t planning_horizon;
			/** Maps center frequency to FrequencyChannel object. */
			std::map<uint64_t, TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel> frequency_channels;
			/** Maps center frequency to reservation table. */
			std::map<uint64_t, TUHH_INTAIRNET_MCSOTDMA::ReservationTable> reservation_tables;
	};
	
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMANAGER_HPP
