//
// Created by Sebastian Lindner on 10.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP

#include "MacId.hpp"
#include <L2Packet.hpp>
#include "ReservationManager.hpp"
#include "MCSOTDMA_Mac.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/**
	 * A LinkManager is responsible for a single communication link.
	 * It is notified by a QueueManager of new packets, and utilizes a ReservationManager to make slot reservations.
	 */
	class LinkManager {
		public:
			class ProposalPayload : L2Packet::Payload {
				public:
					unsigned int getBits() const override;
				
				protected:
					std::vector<FrequencyChannel> proposed_channels;
					/** Pairs of <start_slot, num_slots>. */
					std::vector<std::pair<unsigned int, unsigned int>> proposed_slots;
			};
			
			enum Status {
				/** Everything is OK. */
				link_established,
				/** Link has not been established yet. */
				link_not_established,
				/** Link establishment request has been prepared and we're waiting for the reply. */
				awaiting_reply
			};
			
			LinkManager(const MacId& link_id, ReservationManager& reservation_manager, MCSOTDMA_Mac& mac);
			
			/**
			 * @return The link ID that is managed.
			 */
			const MacId& getLinkId() const;
			
			/**
			 * After the QueueManager has enqueued an upper-layer packet, the corresponding LinkManager is notified.
			 */
			void notifyOutgoing();
			
			/**
			 * When a packet on this link comes in, the LinkManager is notified.
			 */
			void notifyIncoming(unsigned int num_bits);
			
			/**
			 * @return The number of slots that should be reserved when a new link is established.
			 */
			unsigned int getNumSlotsToReserve() const;
			
			/**
			 * @param num_slots The number of slots that should be reserved when a new link is established.
			 */
			void setNumSlotsToReserver(const unsigned int& num_slots);
			
			void setProposalDimension(unsigned int num_channels, unsigned int num_slots);
			
			/**
			 * @return The number of frequency channels that will be proposed when a new link request is prepared.
			 */
			unsigned int getProposalNumChannels() const;
			/**
			 * @return The number of time slots that will be proposed when a new link request is prepared.
			 */
			unsigned int getProposalNumSlots() const;
			
			/**
			 * @return The current, computed traffic estimate from a moving average over some window of past values.
			 */
			double getCurrentTrafficEstimate() const;
			
			const unsigned int& getTrafficEstimateWindowSize() const;
		
		protected:
			L2Packet* prepareLinkEstablishmentRequest();
			
		protected:
			/** The link ID that is managed. */
			const MacId link_id;
			/** Points to the reservation table of this link. */
			ReservationManager& reservation_manager;
			/** Points to the MAC sublayer. */
			MCSOTDMA_Mac& mac;
			/** Link establishment status. */
			Status link_establishment_status;
			/** A link is assigned on one particular frequency channel. It may be nullptr unless the link_establishment status is `link_established`. */
			FrequencyChannel* current_channel = nullptr;
			/** Another component may define the number of slots that should be reserved for this link. This is based off the expected traffic load. */
			unsigned int num_slots_to_reserve = 1;
			/** The number of frequency channels that should be proposed when a new link request is prepared. */
			unsigned int num_proposed_channels = 2;
			/** The number of time slots that should be proposed when a new link request is prepared. */
			unsigned int num_proposed_slots = 3;
			/** The number of past values to consider for the traffic estimate. */
			const unsigned int traffic_estimate_num_values = 20;
			/** Whenever a LinkManager is notified of new data for its link, a number of values are saved so that a traffic estimate can be calculated. */
			std::vector<unsigned long long> traffic_estimate_queue_lengths;
			/** Keeps track of the index that should be updated next in the `traffic_estimate_queue_lengths`. */
			size_t traffic_estimate_index = 0;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP
