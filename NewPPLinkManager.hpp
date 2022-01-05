//
// Created by Sebastian Lindner on 12/21/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_PPLINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_PPLINKMANAGER_HPP

#include "LinkManager.hpp"
#include "MovingAverage.hpp"
#include <sstream>

namespace TUHH_INTAIRNET_MCSOTDMA {

class NewPPLinkManager : public LinkManager, public LinkManager::LinkEstablishmentPayload::Callback {

	friend class NewPPLinkManagerTests;	

	public:
		NewPPLinkManager(const MacId& link_id, ReservationManager *reservation_manager, MCSOTDMA_Mac *mac);
		
		void onReceptionBurstStart(unsigned int burst_length) override;
		void onReceptionBurst(unsigned int remaining_burst_length) override;
		L2Packet* onTransmissionBurstStart(unsigned int burst_length) override;
		void onTransmissionBurst(unsigned int remaining_burst_length) override;
		void notifyOutgoing(unsigned long num_bits) override;
		void onSlotStart(uint64_t num_slots) override;
		void onSlotEnd() override;
		void populateLinkRequest(L2HeaderLinkRequest*& header, LinkEstablishmentPayload*& payload) override;
		void processLinkRequestMessage(const L2Header*& header, const L2Packet::Payload*& payload, const MacId& origin) override;
		void processLinkReplyMessage(const L2HeaderLinkEstablishmentReply*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id) override;

	protected:
		/** Container that saves the resources that were locked or scheduled during link establishment. */
		class ReservationMap {
		public:
			ReservationMap() : num_slots_since_creation(0) {};
			
			/** Local resources that were locked. */
			std::vector<std::pair<ReservationTable*, unsigned int>> resource_reservations;
			/** Keep track of the number of time slots since creation, so that the slot offsets can be normalized to the current time. */
			unsigned int num_slots_since_creation;

			void merge(const ReservationMap& other) {
				for (const auto& pair : other.resource_reservations)
					resource_reservations.push_back(pair);				
			}

			size_t size() const {
				return resource_reservations.size();
			}			
			bool anyLocks() const {
				return size() > 0;
			}
			void clear() {				
				this->resource_reservations.clear();
			}

			/** 
			 * Assuming that all here-saved resources were locked, it first checks that they are, and then unlocks them.
			 * @throws std::invalid_argument if any resource was not locked
			 * */
			void unlock() {				
				for (const auto& pair : resource_reservations) {
					ReservationTable *table = pair.first;
					// skip SH reservations
					if (table->getLinkedChannel() != nullptr && table->getLinkedChannel()->isSH())
						continue;
					unsigned int slot_offset = pair.second - this->num_slots_since_creation;
					if (slot_offset > 0) {
						if (!table->getReservation(slot_offset).isLocked()) {							
							std::stringstream ss;
							ss << "ReservationMap::unlock cannot unlock reservation in " << slot_offset << " slots. Its status is: " << table->getReservation(slot_offset) << " when it should be locked.";
							throw std::invalid_argument(ss.str());
						} else 
							table->mark(slot_offset, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));
					}
				}
				// no need to manually unlock RX and TX tables
				// because the table->mark takes care of that auto-magically									
			}		

			/** 
			 * Assuming that all here-saved resources were scheduled, it first checks that they are, and then sets them to IDLE.
			 * @throws std::invalid_argument if any resource was not scheduled
			 * */
			void unschedule() {
				for (const auto& pair : resource_reservations) {
					ReservationTable *table = pair.first;
					unsigned int slot_offset = pair.second - this->num_slots_since_creation;
					if (slot_offset > 0) {
						if (!(table->getReservation(slot_offset).isTx() || table->getReservation(slot_offset).isRx())) {							
							std::stringstream ss;
							ss << "ReservationMap::unlock cannot unschedule reservation in " << slot_offset << " slots. Its status is: " << table->getReservation(slot_offset) << " when it should be TX or RX.";
							throw std::invalid_argument(ss.str());
						} else 
							table->mark(slot_offset, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));
					}
				}
			}	
		};

		/** Keeps track of the current link state values. */
		class LinkState {
		public:			
			LinkState(unsigned int timeout, unsigned int burst_offset, unsigned int burst_length, unsigned int burst_length_tx, unsigned int next_burst_in, bool is_link_initator, const FrequencyChannel *frequency_channel)
				: timeout(timeout), burst_offset(burst_offset), burst_length(burst_length), burst_length_tx(burst_length_tx), burst_length_rx(burst_length - burst_length_tx), next_burst_in(next_burst_in), is_link_initator(is_link_initator), reserved_resources(ReservationMap()), frequency_channel(frequency_channel) {}

			LinkState() : LinkState(0, 0, 0, 0, 0, false, nullptr) {}

			/** Remaining number of transmission bursts until link expiry. */
			unsigned int timeout;
			/** Number of slots in-between transmission bursts. */
			unsigned int burst_offset; 
			/** Number of slot a transmission burst occupies. */
			unsigned int burst_length;
			/** Number of initial slots within a transmission burst the link initiator claims for its transmission. */
			unsigned int burst_length_tx;
			/** Number of later slots within a transmission burst the link initiator listens for. */
			unsigned int burst_length_rx;
			/** Number of slots until the start of the next transmission burst. */
			unsigned int next_burst_in;
			/** When the reply is received, this is used to normalize the selected resource to the current time slot. */
			int reply_offset = -1;
			/** The link initiator is that user, that has sent the link request with which the link was established. */
			bool is_link_initator;
			ReservationMap reserved_resources;
			FrequencyChannel const *frequency_channel = nullptr;
		};
		
		/** Initiates link establishment by preparing a link request and its transmission via the SH. */
		void establishLink();

		/** Cancels all resource reservations that have been made. */
		void cancelLink();

		/**
		 * Processes an initial link request.
		 * @param header 
		 * @param payload 
		 */
		void processLinkRequestMessage_initial(const L2HeaderLinkRequest*& header, const LinkManager::LinkEstablishmentPayload*&);

		/**
		 * From a map of proposed resources, check each for validity and then choose one randomly.
		 * @param resources 
		 * @param burst_length 
		 * @param burst_length_tx 
		 * @return std::pair<const FrequencyChannel*, unsigned int> 
		 * @throws std::invalid_argument if no proposal is viable
		 */
		std::pair<const FrequencyChannel*, unsigned int> chooseRandomResource(const std::map<const FrequencyChannel*, std::vector<unsigned int>>& resources, unsigned int burst_length, unsigned int burst_length_tx);

		/**
		 * @brief Checks whether a proposed link is viable.
		 * @param table 
		 * @param burst_start 
		 * @param burst_length 
		 * @param burst_length_tx 
		 * @param burst_offset 
		 * @param timeout 
		 * @return true if viable
		 */
		bool isProposalViable(const ReservationTable *table, unsigned int burst_start, unsigned int burst_length, unsigned int burst_length_tx, unsigned int burst_offset, unsigned int timeout) const;		

		/**		 
		 * @param resource_req_me The number of slots that ideally should be used for transmission.
		 * @param resource_req_you The number of slots that ideally should be used for reception.
		 * @param burst_offset The number of slots in-between two TX/RX bursts.
		 * @return {no. of TX slots, no. of RX slots}
		 */
		std::pair<unsigned int, unsigned int> getTxRxSplit(unsigned int resource_req_me, unsigned int resource_req_you, unsigned int burst_offset) const;

		/**		 
		 * @return Number of time slots in-between two transmission bursts that should be proposed for new links.
		 */
		unsigned int getBurstOffset() const;

		/**		 
		 * @return Number of TX slots that should be proposed for new links.
		 */
		unsigned int getRequiredTxSlots() const;		

		/**		 
		 * @return Number of RX slots that should be proposed for new links.
		 */
		unsigned int getRequiredRxSlots() const;

		std::map<const FrequencyChannel*, std::vector<unsigned int>> slotSelection(unsigned int num_channels, unsigned int num_time_slots, unsigned int burst_length, unsigned int burst_length_tx) const;

		/**
		 * Locks all bursts in the respective ReservationTables.
		 * 
		 * @param start_slots 
		 * @param burst_length 
		 * @param burst_length_tx 
		 * @param timeout 
		 * @param is_link_initiator 
		 * @param table 
		 * @return Map of locked resources.
		 */
		ReservationMap lock_bursts(const std::vector<unsigned int>& start_slots, unsigned int burst_length, unsigned int burst_length_tx, unsigned int timeout, bool is_link_initiator, ReservationTable* table);		

		ReservationMap schedule_bursts(const FrequencyChannel *channel, const unsigned int timeout, const unsigned int selected_time_slot_offset, const unsigned int burst_length, const unsigned int burst_length_tx, const unsigned int burst_length_rx, bool is_link_initiator);

		/**		 
		 * @return Whether the timeout expires now.
		 */
		bool decrementTimeout();
		void onTimeoutExpiry();

		void processBaseMessage(L2HeaderBase*& header) override;
		void processUnicastMessage(L2HeaderUnicast*& header, L2Packet::Payload*& payload) override;		

		void setReportedDesiredTxSlots(unsigned int value);
		void setForceBidirectionalLinks(bool flag);

	protected:
		/** Number of transmission bursts until link expiry. */
		unsigned int timeout_before_link_expiry = 20;
		/** The number of slots in-between transmission bursts, often denoted as tau. */
		unsigned int burst_offset = 20;
		/** Number of slots in-between request and reply to give the receiver sufficient processing time. */
		const unsigned int min_offset_to_allow_processing = 2;
		/** Link requests should propose this many distinct frequency channels. */
		unsigned int proposal_num_frequency_channels = 3;
		/** Link requests should propose this many distinct time slot resources per frequency channel. */
		unsigned int proposal_num_time_slots = 3;
		/** Gives the average number of bits that should have been sent in-between two transmission bursts. */
		MovingAverage outgoing_traffic_estimate = MovingAverage(burst_offset);
		/** The communication partner can report how many resources it'd prefer. By default it is 1 to enable bidirectional communication. */
		unsigned int reported_resoure_requirement = 1;
		/** To measure the time until link establishment, the current slot number when the request is sent is saved here. */
		unsigned int time_when_request_was_generated = 0;
		unsigned int time_slots_until_reply = 0;
		bool force_bidirectional_links = true;
		/** Set to true if link establishment should be re-attempted next slot. */
		bool attempt_link_establishment_again = false;
		LinkState link_state = LinkState();	
		/** Whether the link has been established in the current time slot. */
		bool established_link_this_slot = false;
		/** Whether the timeout has already been updated in this time slot. */
		bool updated_timeout_this_slot = false;
		/** Whether communication has taken place during this time slot. */		
		bool communication_during_this_slot = false;
};

}

#endif //TUHH_INTAIRNET_MC_SOTDMA_PPLINKMANAGER_HPP