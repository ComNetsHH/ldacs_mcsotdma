//
// Created by Sebastian Lindner on 12/21/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_PPLINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_PPLINKMANAGER_HPP

#include "LinkManager.hpp"
#include "MovingAverage.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

class NewPPLinkManager : public LinkManager, public LinkManager::LinkRequestPayload::Callback {

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
		void populateLinkRequest(L2HeaderLinkRequest*& header, LinkRequestPayload*& payload) override;

	protected:
		class LockMap;
		
		void establishLink();

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
		LockMap lock_bursts(const std::vector<unsigned int>& start_slots, unsigned int burst_length, unsigned int burst_length_tx, unsigned int timeout, bool is_link_initiator, ReservationTable* table);

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

	protected:
		/** Container that saves the resources that were locked during link establishment. */
		class LockMap {
		public:
			LockMap() : num_slots_since_creation(0) {};

			/** Transmitter resources that were locked. */
			std::vector<std::pair<ReservationTable*, unsigned int>> locks_transmitter;
			/** Receiver resources that were locked. */
			std::vector<std::pair<ReservationTable*, unsigned int>> locks_receiver;
			/** Local resources that were locked. */
			std::vector<std::pair<ReservationTable*, unsigned int>> locks_local;

			void merge(const LockMap& other) {
				for (const auto& pair : other.locks_local)
					locks_local.push_back(pair);
				for (const auto& pair : other.locks_receiver)
					locks_receiver.push_back(pair);
				for (const auto& pair : other.locks_transmitter)
					locks_transmitter.push_back(pair);
			}

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
};

}

#endif //TUHH_INTAIRNET_MC_SOTDMA_PPLINKMANAGER_HPP