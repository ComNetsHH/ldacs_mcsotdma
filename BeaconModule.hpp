//
// Created by seba on 4/14/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_BEACONMODULE_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_BEACONMODULE_HPP


#include "ReservationTable.hpp"
#include "BeaconPayload.hpp"
#include <random>
#include <L2Header.hpp>

namespace TUHH_INTAIRNET_MCSOTDMA {
/**
 * Handles beacon-related tasks such as selecting appropriate slots and preparing beacons.
 */
	class BeaconModule {

		friend class BeaconModuleTests;

	public:
		/** Beacon interval minimum and maximum. */
		static const unsigned int MIN_BEACON_OFFSET,
								  MAX_BEACON_OFFSET;
	    /** Initial beacon offset at power-on. */
		static const unsigned int INITIAL_BEACON_OFFSET;

	public:
		BeaconModule(unsigned int min_beacon_gap, double congestion_goal);
		BeaconModule();
		virtual ~BeaconModule();

		/**
		 * @return Whether the node is currently connected to a LDACS A2A network, i.e. has performed network entry.
		 */
		bool isConnected() const;

		bool shouldSendBeaconThisSlot() const;

		void onSlotEnd();

		/**
		 * It does *not* mark the slot in the table.
		 * @param avg_broadcast_rate
		 * @param num_active_neighbors
		 * @param bc_table
		 * @return A suitable slot for the next beacon transmission.
		 */
		unsigned int scheduleNextBeacon(double avg_broadcast_rate, unsigned int num_active_neighbors, const ReservationTable *bc_table, const ReservationTable *tx_table);

		/**
		 * @return Current beacon offset.
		 */
		unsigned int getBeaconOffset() const;

		/**
		 * @param n Minimum number of non-beacon-reserved slots to keep when scheduling a new beacon slot.
		 */
		void setMinBeaconGap(unsigned int n);

		/**
		 * @param reservation_tables
		 * @return A new beacon message.
		 */
		std::pair<L2HeaderBeacon*, BeaconPayload*> generateBeacon(const std::vector<ReservationTable*>& reservation_tables) const;

	protected:
		/**
		 * @param random_choice Whether to choose randomly from a number of viable candidates.
		 * @return A time slot to use for the next beacon.
		 */
		unsigned int chooseNextBeaconSlot(unsigned int min_beacon_offset, unsigned int num_candidates, unsigned int min_gap_to_next_beacon, const ReservationTable *bc_table, const ReservationTable *tx_table);

		/**
		 *
		 * @param target_congestion A value 0<=r<=1 that specifies the percentage of time slots that *should* be idle between two beacon broadcasts.
		 * @param avg_broadcast_rate A value 0<=s<=1 that specifies the average likelihood of active neighbors broadcasting within the time of two beacon broadcasts.
		 * @param num_active_neighbors
		 * @return A value for the current beacon interval that aims to meet the congestion target.
		 */
		unsigned int computeBeaconInterval(double target_congestion, double avg_broadcast_rate, unsigned int num_active_neighbors) const;

		unsigned long getRandomInt(size_t start, size_t end);

	protected:
		/** When scheduling beacon slots, aim to keep this percentage of slots idle in-between two beacon broadcasts. */
		const double BC_CONGESTION_GOAL;
		/** Number of candidate slots that should be considered when an initial beacon slot is chosen. */
		const unsigned int N_BEACON_SLOT_CANDIDATES = 3;
		/** Minimum number of time slots to next beacon slot of any user. */
		unsigned int min_beacon_gap;

		/** The minimum interval in slots that should be kept in-between beacons. */
		unsigned int beacon_offset = MIN_BEACON_OFFSET;
		unsigned int next_beacon_in = beacon_offset;
		/** Whether this node has performed network entry. */
		bool is_connected = false;
		/** Target collision probability for beacon broadcasts. */
		double beacon_coll_prob = .01;

		std::random_device* random_device;
		std::mt19937 generator;
	};
}




#endif //TUHH_INTAIRNET_MC_SOTDMA_BEACONMODULE_HPP
