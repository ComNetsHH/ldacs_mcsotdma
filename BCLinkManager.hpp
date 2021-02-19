//
// Created by seba on 2/18/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP

#include "LinkManager.hpp"
#include "ContentionEstimator.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class BCLinkManager : public LinkManager {

		friend class BCLinkManagerTests;

	public:
		/**
		 * @param reservation_manager
		 * @param mac
		 * @param min_beacon_gap Minimum number of slots that should be kept idle when a beacon slot is selected.
		 */
		BCLinkManager(ReservationManager *reservation_manager, MCSOTDMA_Mac *mac, unsigned int min_beacon_gap);

		void onReceptionBurstStart(unsigned int burst_length) override;

		void onReceptionBurst(unsigned int remaining_burst_length) override;

		L2Packet* onTransmissionBurstStart(unsigned int burst_length) override;

		void onTransmissionBurst(unsigned int remaining_burst_length) override;

		void notifyOutgoing(unsigned long num_bits) override;

		void onSlotStart(uint64_t num_slots) override;

		void onSlotEnd() override;

		/**
		 * Called by P2PLinkManagers to send link requests on the broadcast channel.
		 * This call schedules a broadcast slot if necessary.
		 * @param header
		 * @param payload
		 */
		void sendLinkRequest(L2HeaderLinkRequest* header, LinkRequestPayload* payload);

	protected:
		unsigned int getNumCandidateSlots(double target_collision_prob) const;

		unsigned long long nchoosek(unsigned long n, unsigned long k) const;

		/**
		 * Applies Broadcast slot selection.
		 * @return Slot offset of the chosen slot.
		 */
		unsigned int broadcastSlotSelection();

		void scheduleBroadcastSlot();

	protected:
		/** Minimum number of slots that should be kept idle when a beacon slot is selected. */
		const unsigned int min_beacon_gap;
		/** Collection of link requests that should be broadcast as soon as possible. */
		std::vector<std::pair<L2HeaderLinkRequest*, LinkRequestPayload*>> link_requests;
		/** For each neighbor, a moving average over past slots is kept, so that the contention by the neighbors is estimated. */
		ContentionEstimator contention_estimator;
		/** Number of slots in-between beacons. */
		unsigned int beacon_offset = 2500;
		/** Default number of beacon transmissions until a new slot is sought. */
		const unsigned int default_beacon_timeout = 5;
		/** Number of beacon transmissions until a new slot is sought. */
		unsigned int beacon_timeout = default_beacon_timeout;
		/** Target collision probability for non-beacon broadcasts. */
		double bc_coll_prob = .05;
		/** Target collision probability for beacon broadcasts. */
		double beacon_coll_prob = .01;
		/** Whether the next broadcast slot has been scheduled. */
		bool next_broadcast_scheduled = false;
		unsigned int next_broadcast_slot = 0;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP
