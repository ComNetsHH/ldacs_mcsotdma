//
// Created by seba on 2/18/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_P2PLINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_P2PLINKMANAGER_HPP

#include <stdint-gcc.h>
#include "LinkManager.hpp"
#include "MovingAverage.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

class P2PLinkManager : public LinkManager, public LinkManager::LinkRequestPayload::Callback, public LinkInfoPayload::Callback {

		friend class P2PLinkManagerTests;
		friend class SystemTests;
		friend class ThreeUsersTests;

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
		LinkInfo getLinkInfo() override;
		void processIncomingLinkRequest(const L2Header*& header, const L2Packet::Payload*& payload, const MacId& origin) override;
		void processIncomingLinkRequest_Initial(const L2Header*& header, const L2Packet::Payload*& payload, const MacId& origin);
		void processIncomingLinkInfo(const L2HeaderLinkInfo*& header, const LinkInfoPayload*& payload) override;
		void assign(const FrequencyChannel* channel) override;

	protected:

			/** Allows the scheduling of control messages at specific slots. */
			class ControlMessageReservation {
			public:
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

			/** Container class of the state of a link. */
			class LinkState {
			public:
				LinkState(unsigned int timeout, unsigned int burst_length, unsigned int burst_length_tx) : timeout(timeout), burst_length(burst_length), burst_length_tx(burst_length_tx), next_burst_start(0) {}
				virtual ~LinkState() {
					for (auto msg : scheduled_link_replies)
						msg.delete_mem();
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
				const FrequencyChannel *channel = nullptr;
				unsigned int next_burst_start;
				/** Initial link establishment makes these RX reservations to listen for replies. */
				std::vector<std::pair<const FrequencyChannel*, unsigned int>> scheduled_rx_slots;
				unsigned int last_proposal_sent = 0;
				unsigned int latest_agreement_opportunity = 0;
				bool waiting_for_agreement = false;
				/** Link replies may be scheduled on specific slots. */
				std::vector<ControlMessageReservation> scheduled_link_replies;
			};

			/** Container class of the resources that were locked during link establishment. */
			class LockMap {
			public:
				LockMap() : num_slots_since_creation(0) {};

				/** Transmitter resources that were locked. */
				std::vector<std::pair<ReservationTable*, unsigned int>> locks_transmitter;
				/** Receiver resources that were locked. */
				std::vector<std::pair<ReservationTable*, unsigned int>> locks_receiver;
				/** Local resources that were locked. */
				std::vector<std::pair<ReservationTable*, unsigned int>> locks_local;

				size_t size_local() const {
					return locks_local.size();
				}
				size_t size_receiver() const {
					return locks_receiver.size();
				}
				size_t size_transmitter() const {
					return locks_transmitter.size();
				}
				bool anyLocks() const {
					return size_local() + size_receiver() + size_transmitter() > 0;
				}

				unsigned int num_slots_since_creation;
			};

			/**
			 * Locks given ReservationTable, as well as transmitter and receiver resources for the given candidate slots.
			 * @param start_slots Starting slot offsets.
			 * @param burst_length Number of first slots to lock the transmitter for.
			 * @param burst_length_tx Number of trailing slots to lock the receiver for
			 * @param table ReservationTable in which slots should be locked.
			 * @return Slot offsets that were locked.
			 */
			LockMap lock(const std::vector<unsigned int>& start_slots, unsigned int burst_length, unsigned int burst_length_tx, ReservationTable* table);

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
			std::pair<std::map<const FrequencyChannel*, std::vector<unsigned int>>, P2PLinkManager::LockMap> p2pSlotSelection(unsigned int num_channels, unsigned int num_slots, unsigned int min_offset, unsigned int burst_length, unsigned int burst_length_tx);

			std::pair<L2HeaderLinkRequest*, LinkManager::LinkRequestPayload*> prepareRequestMessage();
			std::pair<L2HeaderLinkReply*, LinkManager::LinkRequestPayload*> prepareReply(const MacId& dest_id, const FrequencyChannel *channel, unsigned int slot_offset, unsigned int burst_length, unsigned int burst_length_tx) const;

			/**
			 * Processes a link establishment request by parsing it and selecting a viable, proposed resource.
			 * @param header
			 * @param payload
			 * @return A LinkState with the selected resource saved.
			 * @throws std::invalid_argument If no resource was viable.
			 */
			LinkState* processRequest(const L2HeaderLinkRequest*& header, const LinkManager::LinkRequestPayload*& payload);

			/**
			 * Checks the provided resources for viable ones and selects from those one randomly.
			 * @param resources
			 * @param burst_length
			 * @param burst_length_tx
			 * @return Pair of (FrequencyChannel, time slot)
			 * @throws std::invalid_argument If no resources were viable.
			 */
			std::pair<const FrequencyChannel*, unsigned int> chooseRandomResource(const std::map<const FrequencyChannel*, std::vector<unsigned int>>& resources, unsigned int burst_length, unsigned int burst_length_tx);

			void processIncomingLinkReply(const L2HeaderLinkEstablishmentReply*& header, const L2Packet::Payload*& payload) override;

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

			void processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) override;
			void processIncomingBroadcast(const MacId& origin, L2HeaderBroadcast*& header) override;
			void processIncomingUnicast(L2HeaderUnicast*& header, L2Packet::Payload*& payload) override;
			void processIncomingBase(L2HeaderBase*& header) override;

			/**
			 * @return Whether the timeout has expired.
			 */
			bool decrementTimeout();
			void onTimeoutExpiry();

			void clearLockedResources(const LockMap& locked_resources);

			unsigned int estimateCurrentNumSlots() const;
			/**
			 * @return Number of slots until timeout reaches value of 1 (just before expiry).
			 */
			unsigned int getExpiryOffset() const;

			/**
			 * @param t
			 * @return Whether slot at t is part of a transmission burst of this link.
			 * @throws runtime_error If no reservation table is currently set.
			 */
			bool isSlotPartOfBurst(int t) const;

			/**
			 * @return Offset to the start of the *next* transmission burst of this link.
			 * @throws range_error If no next burst can be found.
			 * @throws runtime_error If no reservation table is currently set.
			 */
			int getNumSlotsUntilNextBurst() const;

			/**
			 * Clears pending RX reservations (to listen for link replies) and resets link status.
			 */
			void terminateLink();

	private:
		/**
		 * Helper function that clears all locks on the respective ReservationTable after normalizing by the given offset.
		 * For example, if in 'resources' an offset at 7 is saved and the normalization_offset is 2 (2 slots have passed since the lock came into effect), then now a resource at offset 7-2=5 will be freed.
		 * @param locked_resources
		 * @param normalization_offset Number of slots that have passed since 'resources' were locked.
		 */
		void clearLocks(const std::vector<std::pair<ReservationTable*, unsigned int>>& locked_resources, unsigned int normalization_offset);

	protected:
			/** The default number of frames a newly established P2P link remains valid for. */
			const unsigned int default_timeout;
			/** The number of slots in-between bursts, i.e. the P2P frame length. */
			const unsigned int burst_offset;
			/** The number of P2P channels that should be proposed using link request. */
			unsigned int num_p2p_channels_to_propose = 2;
			/** The number of time slots per P2P channel that should be proposed using link request. */
			const unsigned int num_slots_per_p2p_channel_to_propose = 3;
			const std::string str_statistic_num_links_established;
			unsigned long statistic_num_links_established = 0;

			/** An estimate of this link's outgoing traffic estimate. */
			MovingAverage outgoing_traffic_estimate;
			/** The communication partner's report of the number of slots they desire for transmission. */
			unsigned int reported_desired_tx_slots = 0;

			/** The current link's state. */
			LinkState *current_link_state = nullptr;

			/** Whether the current slot was used for communication. */
			bool communication_during_this_slot = false;
			bool updated_timeout_this_slot = false;
			/** Whether this slot a link was initially established. */
			bool established_initial_link_this_slot = false;
			/** Whether this slot a link was established, initial or renewed. */
			bool established_link_this_slot = false;
			/** Saves all locked resources. */
			LockMap lock_map;
		};

	}

#endif //TUHH_INTAIRNET_MC_SOTDMA_P2PLINKMANAGER_HPP
