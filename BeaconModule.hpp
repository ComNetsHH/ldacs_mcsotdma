//
// Created by seba on 4/14/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_BEACONMODULE_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_BEACONMODULE_HPP


#include "ReservationTable.hpp"
#include "BeaconPayload.hpp"
#include "ReservationManager.hpp"
#include <random>
#include <L2Header.hpp>
#include <SimulatorPosition.hpp>
#include <RngProvider.hpp>

namespace TUHH_INTAIRNET_MCSOTDMA {
/**
 * Handles beacon-related tasks such as selecting appropriate slots and preparing beacons.
 */
	class BeaconModule : public IRng {

		friend class BeaconModuleTests;
		friend class SHLinkManagerTests;
	
	public:
		BeaconModule(unsigned int min_beacon_gap);
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
		 * @param num_candidate_slots
		 * @param num_active_neighbors
		 * @param bc_table
		 * @return A suitable slot for the next beacon transmission.
		 */
		unsigned int scheduleNextBeacon(unsigned int num_candidate_slots, unsigned int num_active_neighbors, const ReservationTable *bc_table, const ReservationTable *tx_table);

		unsigned int getNextBeaconSlot() const;

		/** Resets next_beacon_in slot counter. */
		void reset();

		/**
		 * @return Current value for the minimum interval in slots that should be kept in-between beacons.
		 * The actual beacon slot may differ from this through random selection.
		 */
		unsigned int getBeaconOffset() const;

		/**
		 * @param n Minimum number of non-beacon-reserved slots to keep when scheduling a new beacon slot.
		 */
		void setMinBeaconGap(unsigned int n);

		void setMinBeaconInterval(unsigned int value);
		void setMaxBeaconInterval(unsigned int value);
		unsigned int getMinBeaconInterval() const;
		unsigned int getMaxBeaconInterval() const;

		/**
		 * @param reservation_tables
		 * @return A new beacon message.
		 */
		std::pair<L2HeaderBeacon*, BeaconPayload*> generateBeacon(const std::vector<ReservationTable*>& reservation_tables, const ReservationTable *bc_table, const SimulatorPosition simulatorPosition, size_t num_active_p2p_links, size_t burst_offset);

		/**
		 * @param sender_id
		 * @param payload
		 * @param manager
		 * @return <must reschedule beacon, must reschedule broadcast>
		 */
		std::pair<bool, bool> parseBeacon(const MacId &sender_id, const BeaconPayload *&payload, ReservationManager *manager) const;

		void setEnabled(bool val);

		bool isEnabled() const;

		void setMinBeaconCandidateSlots(unsigned int value);
		unsigned int getMinBeaconCandidateSlots() const;

		void setWriteResourceUtilizationIntoBeacon(bool flag);

	protected:
		/**
		 * @param random_choice Whether to choose randomly from a number of viable candidates.
		 * @return A time slot to use for the next beacon.
		 */
		unsigned int chooseNextBeaconSlot(unsigned int min_beacon_offset, unsigned int num_candidates, unsigned int min_gap_to_next_beacon, const ReservationTable *bc_table, const ReservationTable *tx_table);

		/**
		 *		 
		 * @param num_active_neighbors
		 * @return A value for the current beacon interval that aims to meet the congestion target.
		 */
		unsigned int computeBeaconInterval(unsigned int num_active_neighbors) const;

	protected:		
		/** Number of candidate slots that should be considered when an initial beacon slot is chosen. */
		unsigned int b_beacon_slot_candidates = 3;
		unsigned int min_beacon_offset = 80; /* 80*12ms=960ms */
		unsigned int max_beacon_offset = 25000; /* 25000*12ms=5min */
		/** Minimum number of time slots to next beacon slot of any user. */
		unsigned int min_beacon_gap;

		/** The minimum interval in slots that should be kept in-between beacons. */
		unsigned int beacon_offset = min_beacon_offset;
		unsigned int next_beacon_in = beacon_offset;
		/** Whether this node has performed network entry. */
		bool is_connected = false;
		/** Target collision probability for beacon broadcasts. */
		double beacon_coll_prob = .01;
		bool flip_p2p_table_encoding = false;
		bool enabled = true;
		bool write_resource_utilization_into_beacon = true;
	};
}




#endif //TUHH_INTAIRNET_MC_SOTDMA_BEACONMODULE_HPP
