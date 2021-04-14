//
// Created by seba on 4/14/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_BEACONMODULE_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_BEACONMODULE_HPP


#include "ReservationTable.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
/**
 * Handles beacon-related tasks such as selecting appropriate slots and preparing beacons.
 */
	class BeaconModule {

		friend class BeaconModuleTests;

	public:
		BeaconModule(ReservationTable *bc_table, unsigned int min_beacon_gap);
		explicit BeaconModule(ReservationTable *bc_table);
		virtual ~BeaconModule() = default;

		void setBcReservationTable(ReservationTable *broadcast_reservation_table);

		/**
		 * @return Whether the node is currently connected to a LDACS A2A network, i.e. has performed network entry.
		 */
		bool isConnected() const;

	protected:
		/**
		 * @param random_choice Whether to choose randomly from a number of viable candidates.
		 * @return A time slot to use for the next beacon.
		 */
		unsigned int chooseNextBeaconSlot(bool random_choice) const;

		/**
		 *
		 * @param target_congestion A value 0<=r<=1 that specifies the percentage of time slots that *should* be idle between two beacon broadcasts.
		 * @param avg_broadcast_rate A value 0<=s<=1 that specifies the average likelihood of active neighbors broadcasting within the time of two beacon broadcasts.
		 * @param num_active_neighbors
		 * @return A value for the current beacon interval that aims to meet the congestion target.
		 */
		unsigned int computeBeaconInterval(double target_congestion, double avg_broadcast_rate, unsigned int num_active_neighbors) const;

	protected:
		/** Number of candidate slots that should be considered when an initial beacon slot is chosen. */
		const unsigned int n_beacon_slot_candidates = 3;
		/** The minimum interval in slots that should be kept in-between beacons. */
		unsigned int beacon_offset = min_beacon_offset;
		/** Beacon interval minimum and maximum. */
		const unsigned int min_beacon_offset = 80, /* 80*12ms=960ms */
						   max_beacon_offset = 25000; /* 5min */
	    /** Minimum number of time slots to next beacon slot of any user. */
	    const unsigned int min_beacon_gap;
		/** The broadcast channel ReservationTable. */
		ReservationTable *bc_table = nullptr;
		/** Whether this node has performed network entry. */
		bool is_connected = false;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_BEACONMODULE_HPP
