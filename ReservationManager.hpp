//
// Created by Sebastian Lindner on 14.10.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMANAGER_HPP

#include <cstdint>
#include <map>
#include <queue>
#include "ReservationTable.hpp"
#include "FrequencyChannel.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/**
	 * For one user, the Reservation Manager provides a wrapper for managing reservation tables for each logical frequency channel.
	 */
	class ReservationManager {
		public:
			
			/**
			 * Implements a comparison of ReservationTables. One table is 'smaller' than another if it has fewer idle slots.
			 * It is used to construct a priority_queue on the tables.
			 */
			class ReservationTableComparison {
				public:
					bool operator()(ReservationTable* tbl1, ReservationTable* tbl2) {
						return tbl1->getNumIdleSlots() < tbl2->getNumIdleSlots();
					}
			};
		public:
			explicit ReservationManager(uint32_t planning_horizon);
			virtual ~ReservationManager();
			
			/**
			 * Adds a frequency channel and corresponding reservation table.
			 * @param is_p2p
			 * @param center_frequency
			 * @param bandwidth
			 */
			void addFrequencyChannel(bool is_p2p, uint64_t center_frequency, uint64_t bandwidth);
			
			FrequencyChannel* getFreqChannel(size_t index);
			ReservationTable* getReservationTable(size_t index);
			
			/**
			 * Calls update() function on each reservation table.
			 * @param num_slots
			 */
			void update(uint64_t num_slots);

			/**
			 * @return Number of frequency channels and corresponding reservation tables that are managed.
			 */
			size_t getNumEntries() const;

			/**
			 * Looks through all reservation tables to find the one with most idle slots, so its complexity is O(n).
			 * @return A pointer to the least utilized reservation table according to its reported number of idle slots.
			 */
			ReservationTable* getLeastUtilizedReservationTable();
			
			std::priority_queue<ReservationTable*, std::vector<ReservationTable*>, ReservationManager::ReservationTableComparison> getSortedReservationTables() const;

		protected:
			/** Number of slots to remember both in the past and in the future. */
			uint32_t planning_horizon;
			/** Keeps frequency channels in the same order as reservation_tables. */
			std::vector<TUHH_INTAIRNET_MCSOTDMA::FrequencyChannel*> frequency_channels;
			/** Keeps reservation table in the same order as frequency_channels. */
			std::vector<TUHH_INTAIRNET_MCSOTDMA::ReservationTable*> reservation_tables;
	};
	
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_RESERVATIONMANAGER_HPP
