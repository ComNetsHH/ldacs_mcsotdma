//
// Created by seba on 2/18/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP

#include <ContentionMethod.hpp>
#include "LinkManager.hpp"
#include "ContentionEstimator.hpp"
#include "CongestionEstimator.hpp"
#include "BeaconModule.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class SHLinkManager : public LinkManager {

		friend class SHLinkManagerTests;
		friend class PPLinkManagerTests;
		friend class SystemTests;
		friend class ThreeUsersTests;

	public:
		SHLinkManager(ReservationManager *reservation_manager, MCSOTDMA_Mac *mac, unsigned int min_beacon_gap);
		virtual ~SHLinkManager();

		void onReceptionReservation() override;		

		L2Packet* onTransmissionReservation() override;		

		void notifyOutgoing(unsigned long num_bits) override;

		void onSlotStart(uint64_t num_slots) override;

		void onSlotEnd() override;

		/**
		 * Called by P2PLinkManagers to send link requests on the broadcast channel.
		 * This call schedules a broadcast slot if necessary.
		 * @param header
		 * @param payload
		 */
		void sendLinkRequest(L2HeaderLinkRequest* header, LinkEstablishmentPayload* payload);

		/**
		 * Schedules a link reply at the specified time slot offset.
		 * 
		 * @param header 
		 * @param payload 
		 * @param time_slot_offset 
		 */
		void sendLinkReply(L2HeaderLinkReply* header, LinkEstablishmentPayload* payload, unsigned int time_slot_offset);

		/**
		 * Cancels all link requests towards 'id'.
		 * @param id
		 * @return Number of removed requests.
		 */
		size_t cancelLinkRequest(const MacId& id);

		size_t cancelLinkReply(const MacId& id);

		void assign(const FrequencyChannel* channel) override;

		void setTargetCollisionProb(double value);
		void setMinNumCandidateSlots(int value);
		void setMaxNumCandidateSlots(int value);

		/**
		 * Specify contention method used to find number of candidate slots.
		 * @param method
		 */
		void setUseContentionMethod(ContentionMethod method);

		/**
		 * If 'true': always schedule the next broadcast slot and advertise it in the header.
		 * If 'false: only schedule the next broadcast slot if there's more data queued up.
		 * @param value
		 */
		void setAlwaysScheduleNextBroadcastSlot(bool value);
		/**		 
		 * If 'true': advertise the next broadcast slot in the current header.		 
		 * @param flag 
		 */
		void setAdvertiseNextSlotInCurrentHeader(bool flag);

		void setMinBeaconInterval(unsigned int value);
		void setMaxBeaconInterval(unsigned int value);

		void setWriteResourceUtilizationIntoBeacon(bool flag);
		void setEnableBeacons(bool flag);		

		void onPacketReception(L2Packet*& packet) override;

		bool canSendLinkReply(unsigned int time_slot_offset) const;

		bool isNextBroadcastScheduled() const;
		unsigned int getNextBroadcastSlot() const;
		unsigned int getNextBeaconSlot() const;
		void reportCollisionWithScheduledBroadcast(const MacId& collider);		
		void reportThirdPartyExpectedLinkReply(int slot_offset, const MacId& sender_id);		

	protected:
		unsigned int getNumCandidateSlots(double target_collision_prob) const;

		unsigned long long nchoosek(unsigned long n, unsigned long k) const;

		/**
		 * Applies Broadcast slot selection.
		 * @param min_offset: Minimum number of slots before the next reservation.
		 * @return Slot offset of the chosen slot.
		 */
		unsigned int broadcastSlotSelection(unsigned int min_offset);

		void scheduleBroadcastSlot();

		void unscheduleBroadcastSlot();
		void unscheduleBeaconSlot();

		void scheduleBeacon();

		void processBeaconMessage(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) override;

		void processBroadcastMessage(const MacId& origin, L2HeaderBroadcast*& header) override;

		void processUnicastMessage(L2HeaderUnicast*& header, L2Packet::Payload*& payload) override;

		void processBaseMessage(L2HeaderBase*& header) override;

		void processLinkRequestMessage(const L2HeaderLinkRequest*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id) override;

		void processLinkReplyMessage(const L2HeaderLinkReply*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id) override;		

		/**
		 * @return Average number of slots inbetween broadcast packet generations as measured.
		 */
		unsigned int getAvgNumSlotsInbetweenPacketGeneration() const;		

	protected:
		/** Collection of link requests that should be broadcast as soon as possible. */
		std::vector<std::pair<L2HeaderLinkRequest*, LinkEstablishmentPayload*>> link_requests;
		/** Collection of link replies and corresponding time slots where they should be transmitted. */
		std::vector<std::pair<unsigned int, std::pair<L2HeaderLinkReply*, LinkEstablishmentPayload*>>> link_replies;
		/** Contention estimation is neighbor activity regarding non-beacon broadcasts. */
		ContentionEstimator contention_estimator;
		BeaconModule beacon_module;
		/** Congestion estimation is neighbor activity regarding all broadcasts. */
		CongestionEstimator congestion_estimator;
		/** Target collision probability for non-beacon broadcasts. */
		double broadcast_target_collision_prob = .626;
		/** Whether the next broadcast slot has been scheduled. */
		bool next_broadcast_scheduled = false;
		bool next_beacon_scheduled = false;
		/** If true, always schedule the next broadcast slot and advertise it in the header. If false, only do so if there's more data to send. */
		bool always_schedule_next_slot = false;
		/** If true, the next slot is advertised in the current header if possible. */
		bool advertise_slot_in_header = true;
		unsigned int next_broadcast_slot = 0;		
		/** Minimum number of slots to consider during slot selection. */
		unsigned int MIN_CANDIDATES = 3;
		/** Maximum number of slots to consider during slot selection. */
		unsigned int MAX_CANDIDATES = 10000;
		MovingAverage avg_num_slots_inbetween_packet_generations;
		unsigned int num_slots_since_last_packet_generation = 0;
		bool packet_generated_this_slot = false;
		ContentionMethod contention_method = randomized_slotted_aloha;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP
