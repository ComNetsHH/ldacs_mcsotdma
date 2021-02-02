//
// Created by seba on 1/14/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGEMENTENTITY_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGEMENTENTITY_HPP

#include <cmath>
#include <map>
#include "L2Packet.hpp"
#include "FrequencyChannel.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	class LinkManager;

	/**
	 * LinkManager module that handles the P2P link management, such as processing requests and replies.
	 */
	class LinkManagementEntity {

		friend class LinkManagementEntityTests;

		friend class LinkManagerTests;

		friend class BCLinkManagerTests;

		friend class SystemTests;

	public:

		/**
		 * Implements a link establishment payload that encodes proposed frequency channels and slots.
		 * Link requests may contain a number of channels and slots, while replies should contain just a single one.
		 */
		class ProposalPayload : public L2Packet::Payload {
		public:
			ProposalPayload(unsigned int num_freq_channels, unsigned int num_slots) : target_num_channels(num_freq_channels), target_num_slots(num_slots), burst_length(1) {
				if (target_num_slots > pow(2, 4))
					throw std::runtime_error("Cannot encode more than 16 candidate slots.");
			}

			/** Copy constructor. */
			ProposalPayload(const ProposalPayload& other)
					: proposed_resources(other.proposed_resources), target_num_channels(other.target_num_channels), target_num_slots(other.target_num_slots), burst_length(other.burst_length) {}

			unsigned int getBits() const override {
				return 8 * target_num_channels // 1B per frequency channel
				       + 8 * target_num_slots // 1B per candidate
				       + 4 * target_num_slots // number of actual candidates per channel
				       + 8; // 1B to denote candidate slot length
			}

			Payload* copy() const override {
				return new ProposalPayload(*this);
			}

			/** <channel, <start slots>>-map of proposed resources. */
			std::map<const FrequencyChannel*, std::vector<unsigned int>> proposed_resources;
			/** Target number of frequency channels to propose. */
			unsigned int target_num_channels;
			/** Target number of slots to propose. */
			unsigned int target_num_slots;
			/** Number of slots to reserve. */
			unsigned int burst_length;
		};


		explicit LinkManagementEntity(LinkManager* owner);

		/**
		 * @return Whether a link management control message should be sent.
		 */
		bool hasControlMessage();

		L2Packet* getControlMessage();

		/**
		 * When a LinkManager receives a link reply, it should forward it to this function.
		 * @param header
		 * @param payload
		 */
		virtual void processLinkReply(const L2HeaderLinkEstablishmentReply*& header, const ProposalPayload*& payload);

		/**
		 * When a LinkManager receoves a link request, it should forward it to this function.
		 * @param header
		 * @param payload
		 */
		virtual void processLinkRequest(const L2HeaderLinkEstablishmentRequest*& header, const ProposalPayload*& payload, const MacId& origin);

		/**
		 * @return Whether the timeout has expired.
		 */
		bool onTransmissionBurst();

		/**
		 * @return Whether the timeout has expired.
		 */
		bool onReceptionSlot();

		/**
		 * Prepares a link request and injects it into the upper layers.
		 */
		void establishLink() const;

		void populateRequest(L2Packet*& request);

		/**
		 * @param value Number of transmission bursts a reservation should be valid for.
		 */
		void setTxTimeout(unsigned int value);

		/**
		 * @param value Number of slots inbetween two transmission bursts.
		 */
		void setTxOffset(unsigned int value);

		/**
		 * @return Number of transmission bursts the reservation is still valid for.
		 */
		unsigned int getTxTimeout() const;

		/**
		 * @return Number of slots in-between two transmission bursts.
		 */
		unsigned int getTxOffset() const;

		/**
		 * @return Minimum offset for new reservations.
		 */
		unsigned int getMinOffset() const;

		/**
		 * @return Number of consecutive slots used per transmission burst.
		 */
		unsigned int getTxBurstSlots() const;

		void setTxBurstSlots(unsigned int value);

		void onRequestTransmission();

		void update(uint64_t num_slots);

		void onTimeoutExpiry();

	protected:
		std::vector<uint64_t> scheduleRequests(unsigned int timeout, unsigned int init_offset, unsigned int tx_offset, unsigned int num_attempts) const;

		std::vector<std::pair<const FrequencyChannel*, unsigned int>> findViableCandidatesInRequest(L2HeaderLinkEstablishmentRequest*& header, ProposalPayload*& payload, bool consider_transmitter, bool consider_receiver) const;

		L2Packet* prepareRequest() const;

		L2Packet* prepareReply(const MacId& destination_id) const;

		bool hasPendingRequest();

		bool hasPendingReply();

		/**
		 * @param burst_num_slots Number of consecutive slots per burst.
		 * @param num_channels Number of frequency channels to propose.
		 * @param num_slots_per_channel Number of time slots per frequency channel to propose.
		 * @param min_offset Minimum slot offset until the first time.
		 * @param consider_tx Whether the transmitter must be idle during selected slots.
		 * @param consider_rx Whether a receiver must be idle during selected slots.
		 * @return A payload that should accompany a link establishment request.
		 */
		ProposalPayload* p2pSlotSelection(unsigned int burst_num_slots, unsigned int num_channels, unsigned int num_slots_per_channel, unsigned int min_offset, bool consider_tx, bool consider_rx);

		/**
		 * @return Whether the timeout has expired.
		 */
		bool decrementTimeout();

		/**
		 * Clears all RX reservations in the proposed_resources map.
		 * Used to clear those RX reservations that were made when a request is sent, and when a reply has been received; future RX reservations of other candidate slots don't matter anymore.
		 * @param proposed_resources
		 * @param absolute_proposal_time
		 * @param current_time
		 * @return Number of cleared reservations
		 */
		size_t clearPendingRequestReservations(const std::map<const FrequencyChannel*, std::vector<unsigned int>>& proposed_resources, uint64_t absolute_proposal_time, uint64_t current_time);

		/**
		 * Processes a link establishment request when the link is not established.
		 * @param header
		 * @param payload
		 * @param origin
		 */
		void processInitialRequest(const L2HeaderLinkEstablishmentRequest*& header, const ProposalPayload*& payload, const MacId& origin);

		/**
		 * Processes a link establishment request when the link is already established.
		 * @param header
		 * @param payload
		 * @param origin
		 */
		void processRenewalRequest(const L2HeaderLinkEstablishmentRequest*& header, const ProposalPayload*& payload, const MacId& origin);

		/**
		 * Processes a link establishment reply when the link is not established.
		 * @param header
		 * @param payload
		 */
		void processInitialReply(const L2HeaderLinkEstablishmentReply*& header, const ProposalPayload*& payload);

		/**
		 * Processes a link establishment reply when the link is already established.
		 * @param header
		 * @param payload
		 */
		void processRenewalReply(const L2HeaderLinkEstablishmentReply*& header, const ProposalPayload*& payload);

		/**
		 * Schedules a link reply as response to an initial link establishment request.
		 * @param reply
		 * @param slot_offset
		 */
		void scheduleInitialReply(L2Packet* reply, int32_t slot_offset);

		/**
		 * Schedules a link reply as response to a link renewal request.
		 * It is always scheduled in the next transmission burst.
		 * @param reply
		 */
		void scheduleRenewalReply(L2Packet* reply);

		/**
		 * @return Slot offset until last reservation of this link.
		 */
		unsigned int getExpiryOffset() const;

	protected:
		/** Number of attempts to renew a link before giving up. */
		const unsigned int max_num_renewal_attempts = 3;
		/** A LinkManagementEntity is a module of a LinkManager. */
		LinkManager* owner = nullptr;
		/** The absolute points in time when requests should be sent. */
		std::vector<uint64_t> scheduled_requests;
		/** Link replies *must* be sent on specific slots. This container holds these bindings. */
		std::map<uint64_t, L2Packet*> scheduled_replies;
		const int32_t default_minimum_slot_offset_for_new_reservations = 2;
		/** The minimum number of slots a proposed slot should be in the future. */
		int32_t min_offset_new_reservations = default_minimum_slot_offset_for_new_reservations;
		/** The number of frequency channels that should be proposed when a new link request is prepared. */
		unsigned int num_proposed_channels = 2;
		/** The number of time slots that should be proposed when a new link request is prepared. */
		unsigned int num_proposed_slots = 3;
		/** Number of repetitions a reservation remains valid for. */
		const unsigned int default_tx_timeout = 10;
		unsigned int tx_timeout = default_tx_timeout;
		/** Number of slots occupied per transmission burst. */
		unsigned int tx_burst_num_slots = 1;
		/** Number of slots until the next transmission. Should be set to the P2P frame length, or dynamically for broadcast-type transmissions. */
		unsigned int tx_offset = 5;
		const FrequencyChannel* next_channel = nullptr;
		unsigned int next_link_first_slot = 0;
		/** Saves the last proposed (frequency channel, time slot)-pairs. */
		std::map<const FrequencyChannel*, std::vector<unsigned int>> last_proposed_resources;
		uint64_t last_proposal_absolute_time = 0;
		bool link_renewal_pending = false;
		/** Whether the tx_timeout field has been set from a packet reception in this slot. Prevents double-decrementing the tx_timeout counter. */
		bool updated_timeout_this_slot = false;
	};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGEMENTENTITY_HPP
