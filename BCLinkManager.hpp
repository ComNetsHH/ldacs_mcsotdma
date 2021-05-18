//
// Created by seba on 2/18/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP

#include "LinkManager.hpp"
#include "ContentionEstimator.hpp"
#include "CongestionEstimator.hpp"
#include "BeaconModule.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class BCLinkManager : public LinkManager {

		friend class BCLinkManagerTests;
		friend class SystemTests;

	public:
		static constexpr unsigned int MIN_CANDIDATES = 3;

		BCLinkManager(ReservationManager *reservation_manager, MCSOTDMA_Mac *mac, unsigned int min_beacon_gap);
		virtual ~BCLinkManager();

		void onReceptionBurstStart(unsigned int burst_length) override;

		void onReceptionBurst(unsigned int remaining_burst_length) override;

		L2Packet* onTransmissionBurstStart(unsigned int remaining_burst_length) override;

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

		void assign(const FrequencyChannel* channel) override;

	protected:
		unsigned int getNumCandidateSlots(double target_collision_prob) const;

		unsigned long long nchoosek(unsigned long n, unsigned long k) const;

		/**
		 * Applies Broadcast slot selection.
		 * @return Slot offset of the chosen slot.
		 */
		unsigned int broadcastSlotSelection();

		void scheduleBroadcastSlot();

	public:
		void onPacketReception(L2Packet*& packet) override;

	protected:

		void processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) override;

		void processIncomingBroadcast(const MacId& origin, L2HeaderBroadcast*& header) override;

		void processIncomingUnicast(L2HeaderUnicast*& header, L2Packet::Payload*& payload) override;

		void processIncomingBase(L2HeaderBase*& header) override;

		void processIncomingLinkRequest(const L2Header*& header, const L2Packet::Payload*& payload, const MacId& origin) override;

		void processIncomingLinkReply(const L2HeaderLinkEstablishmentReply*& header, const L2Packet::Payload*& payload) override;

		void processIncomingLinkInfo(const L2HeaderLinkInfo*& header, const LinkInfoPayload*& payload) override;

	protected:
		/** Collection of link requests that should be broadcast as soon as possible. */
		std::vector<std::pair<L2HeaderLinkRequest*, LinkRequestPayload*>> link_requests;
		/** Contention estimation is neighbor activity regarding non-beacon broadcasts. */
		ContentionEstimator contention_estimator;
		/** Congestion estimation is neighbor activity regarding all broadcasts. */
		CongestionEstimator congestion_estimator;
		/** Target collision probability for non-beacon broadcasts. */
		double broadcast_target_collision_prob = .05;
		/** Whether the next broadcast slot has been scheduled. */
		bool next_broadcast_scheduled = false;
		unsigned int next_broadcast_slot = 0;
		BeaconModule beacon_module;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP
