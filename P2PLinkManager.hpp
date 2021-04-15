//
// Created by seba on 2/18/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_P2PLINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_P2PLINKMANAGER_HPP

#include <stdint-gcc.h>
#include "NewLinkManager.hpp"
#include "MovingAverage.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

class P2PLinkManager : public LinkManager, public LinkManager::LinkRequestPayload::Callback {

		friend class P2PLinkManagerTests;
		friend class NewSystemTests;

	public:
		P2PLinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac, unsigned int default_timeout, unsigned int burst_offset);

		~P2PLinkManager() override;

		void onReceptionBurstStart(unsigned int burst_length) override;

		void onReceptionBurst(unsigned int remaining_burst_length) override;

		L2Packet* onTransmissionBurstStart(unsigned int remaining_burst_length) override;

		void onTransmissionBurst(unsigned int remaining_burst_length) override;

		void notifyOutgoing(unsigned long num_bits) override;

		void onSlotStart(uint64_t num_slots) override;

		void onSlotEnd() override;

		void populateLinkRequest(L2HeaderLinkRequest*& header, LinkRequestPayload*& payload) override;

		void processIncomingLinkRequest(const L2Header*& header, const L2Packet::Payload*& payload, const MacId& origin) override;

		void assign(const FrequencyChannel* channel) override;

	protected:

			/** Allows the scheduling of control messages at specific slots. */
			class ControlMessageReservation {
			public:
//				ControlMessageReservation(unsigned int slot_offset, L2Header *header, LinkRequestPayload *payload) : remaining_offset(slot_offset), header(header), payload(payload) {}
				ControlMessageReservation(unsigned int slot_offset, L2Header *&header, LinkRequestPayload *&payload) : remaining_offset(slot_offset), header(header), payload(payload) {}
				virtual ~ControlMessageReservation() = default;

				void update(unsigned int num_slots) {
					// Update counter until this control message is due.
					if (remaining_offset < num_slots)
						throw std::invalid_argument("ControlMessageReservation::onSlotEnd would decrement the remaining slots past zero - did we miss the corresponding slot?!");
					remaining_offset -= num_slots;

					// Update payload slot offsets to reflect a correct offset with respect to the current time.
					for (auto &pair : payload->proposed_resources) {
						for (unsigned int& i : pair.second) {
							if (i < num_slots)
								throw std::invalid_argument("ControlMessageReservation::onSlotEnd would decrement a slot offset past zero. Are we late with sending this reply?");
							i -= num_slots;
						}
					}
				}

				L2Header*& getHeader() {
					return header;
				}
				LinkRequestPayload*& getPayload() {
					return payload;
				}
				unsigned int getRemainingOffset() const {
					return remaining_offset;
				}
				void delete_mem() {
					delete header;
					delete payload;
				}

			protected:
				unsigned int remaining_offset;
				L2Header *header;
				LinkRequestPayload *payload;
			};

			class LinkState {
			public:
				LinkState(unsigned int timeout, unsigned int burst_length, unsigned int burst_length_tx) : timeout(timeout), burst_length(burst_length), burst_length_tx(burst_length_tx), next_burst_start(0) {}
				virtual ~LinkState() {
					delete last_proposed_renewal_resources;
					for (auto msg : scheduled_link_requests)
						msg.delete_mem();
					for (auto msg : scheduled_link_replies)
						msg.delete_mem();
				}

				void clearRequests() {
					for (auto msg : scheduled_link_requests)
						msg.delete_mem();
					scheduled_link_requests.clear();
				}

				void clearReplies() {
					for (auto msg : scheduled_link_replies)
						msg.delete_mem();
					scheduled_link_replies.clear();
				}

				/** Number of bursts that remain. It is decremented at the end of a burst, s.t. usually it means that the current timeout value *includes* the current burst.. */
				unsigned int timeout;
				/** Total number of slots reserved for this link. */
				const unsigned int burst_length;
				/** Number of slots reserved for transmission of the link initiator. If burst_length_tx=burst_length, then this is a unidirectional link. */
				const unsigned int burst_length_tx;
				/** Whether the local user has initiated this link, i.e. sends the link requests. */
				bool is_link_initiator = false;
				/** Whether this state results from an initial link establishment as opposed to a renewed one. */
				bool initial_setup = false;
				/** Whether a link renewal is due. */
				bool renewal_due = false;
				const FrequencyChannel *channel = nullptr;
				unsigned int next_burst_start;
				/** Link replies may be scheduled on specific slots. */
				std::vector<ControlMessageReservation> scheduled_link_replies;
				/** Link requests may be scheduled on specific slots. */
				std::vector<ControlMessageReservation> scheduled_link_requests;
				/** Initial link establishment makes these RX reservations to listen for replies. */
				std::vector<std::pair<const FrequencyChannel*, unsigned int>> scheduled_rx_slots;
				/** The last-proposed resources for link renewal are saved s.t. locked resources can be freed when an agreement is found. */
				LinkRequestPayload* last_proposed_renewal_resources = nullptr;
				unsigned int last_proposal_sent = 0;
			};

			/**
			 * Computes a map of proposed P2P channels and corresponding slot offsets.
			 * @param num_channels Target number of P2P channels that should be proposed.
			 * @param num_slots Target number of slot offsets per P2P channel that should be proposed.
			 * @param min_offset Minimum slot offset for the first proposed slot.
			 * @param burst_length Number of slots the burst must occupy.
			 * @param burst_length_tx Number of first slots that should be used for transmission.
			 * @param is_init Whether this slot selection is used for initial link establishment, i.e. does the receiver have to be idle during the first slot of each burst, s.t. a reply can be received.
			 * @return <Proposal map, locked map>
			 */
			std::pair<std::map<const FrequencyChannel*, std::vector<unsigned int>>, std::map<const FrequencyChannel*, std::vector<unsigned int>>> p2pSlotSelection(unsigned int num_channels, unsigned int num_slots, unsigned int min_offset, unsigned int burst_length, unsigned int burst_length_tx, bool is_init);

			std::pair<L2HeaderLinkRequest*, LinkManager::LinkRequestPayload*> prepareRequestMessage(bool initial_request);
			std::pair<L2HeaderLinkReply*, LinkManager::LinkRequestPayload*> prepareReply(const MacId& dest_id, const FrequencyChannel *channel, unsigned int slot_offset, unsigned int burst_length, unsigned int burst_length_tx) const;

			LinkState* processRequest(const L2HeaderLinkRequest*& header, const LinkManager::LinkRequestPayload*& payload);
			std::pair<const FrequencyChannel*, unsigned int> chooseRandomResource(const std::map<const FrequencyChannel*, std::vector<unsigned int>>& resources, unsigned int burst_length, unsigned int burst_length_tx);

			void processIncomingLinkReply(const L2HeaderLinkEstablishmentReply*& header, const L2Packet::Payload*& payload) override;
			void processInitialReply(const L2HeaderLinkReply*& header, const LinkManager::LinkRequestPayload*& payload);
			void processRenewalReply(const L2HeaderLinkReply*& header, const LinkManager::LinkRequestPayload*& payload);

			/**
			 * @param table
			 * @param burst_start
			 * @param burst_length
			 * @param burst_length_tx
			 * @return Whether the entire range is idle && a receiver is idle during the first burst_length_tx slots && a transmitter is idle during the remaining slots.
			 */
			bool isViable(const ReservationTable *table, unsigned int burst_start, unsigned int burst_length, unsigned int burst_length_tx) const;

			/**
			 * Helper function that schedules a communication burst with TX, TX_CONT, RX, RX_CONT reservations.
			 * @param burst_start_offset
			 * @param burst_length
			 * @param burst_length_tx
			 * @param dest_id
			 * @param table
			 * @param link_initiator Whether the first slots should be TX or RX.
			 */
			void scheduleBurst(unsigned int burst_start_offset, unsigned int burst_length, unsigned int burst_length_tx, const MacId &dest_id, ReservationTable *table, bool link_initiator);

			/**
			 * @param timeout
			 * @param init_offset First transmission burst offset.
			 * @param burst_offset
			 * @param num_attempts
			 * @return Slot offsets where link renewal requests should be sent.
			 */
			std::vector<unsigned int> scheduleRenewalRequestSlots(unsigned int timeout, unsigned int init_offset, unsigned int burst_offset, unsigned int num_attempts) const;

			void processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) override;
			void processIncomingBroadcast(const MacId& origin, L2HeaderBroadcast*& header) override;
			void processIncomingUnicast(L2HeaderUnicast*& header, L2Packet::Payload*& payload) override;
			void processIncomingBase(L2HeaderBase*& header) override;

			/**
			 * @return Whether the timeout has expired.
			 */
			bool decrementTimeout();
			void onTimeoutExpiry();

			void clearLockedResources(LinkRequestPayload*& proposal, unsigned int num_slot_since_proposal);

			unsigned int estimateCurrentNumSlots() const;
			/**
			 * @return Number of slots until timeout reaches value of 1 (just before expiry).
			 */
			unsigned int getExpiryOffset() const;

	protected:
			/** The default number of frames a newly established P2P link remains valid for. */
			const unsigned int default_timeout;
			/** The number of slots in-between bursts, i.e. the P2P frame length. */
			const unsigned int burst_offset;
			/** The number of P2P channels that should be proposed using link request. */
			unsigned int num_p2p_channels_to_propose = 2;
			/** The number of time slots per P2P channel that should be proposed using link request. */
			const unsigned int num_slots_per_p2p_channel_to_propose = 3;
			/** The number of renewal attempts that should be made. */
			const unsigned int num_renewal_attempts = 3;

			/** An estimate of this link's outgoing traffic estimate. */
			MovingAverage outgoing_traffic_estimate;
			/** The communication partner's report of the number of slots they desire for transmission. */
			unsigned int reported_desired_tx_slots = 0;

			/** The current link's state. */
			LinkState *current_link_state = nullptr;
			/** The next link's state, which may be applied upon link renewal. */
			LinkState *next_link_state = nullptr;

			size_t num_slots_since_last_burst_start = 0,
				   num_slots_since_last_burst_end = 0;
			/** Whether the current slot is the initial slot of a burst. */
			bool burst_start_during_this_slot = false;
			/** Whether the current slot is the end slot of a burst. */
			bool burst_end_during_this_slot = false;
			bool updated_timeout_this_slot = false;
			bool established_initial_link_this_slot = false;
		};
	}

#endif //TUHH_INTAIRNET_MC_SOTDMA_P2PLINKMANAGER_HPP
