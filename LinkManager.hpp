//
// Created by Sebastian Lindner on 10.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP

#include "IcaoId.hpp"
#include "L2Packet.hpp"
#include "QueueManager.hpp"
#include "ReservationManager.hpp"

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
			
			LinkManager(const IcaoId& link_id, ReservationManager& reservation_manager, QueueManager& queue_manager);
			
			/**
			 * @return The link ID that is managed.
			 */
			const IcaoId& getLinkId() const;
			
			/**
			 * After the QueueManager has enqueued an upper-layer packet, the corresponding LinkManager is notified.
			 */
			void notifyOutgoing();
			
			/**
			 * When a packet on this link comes in, the LinkManager is notified.
			 */
			void notifyIncoming(L2Packet* incoming_packet);
			
			bool isUsingArq() const;
			
			void setUseArq(bool value);
			
			/**
			 * @return The number of slots that should be reserved when a new link is established.
			 */
			unsigned int getNumSlotsToReserve() const;
			
			/**
			 * @param num_slots The number of slots that should be reserved when a new link is established.
			 */
			void setNumSlotsToReserver(const unsigned int& num_slots);
		
		protected:
			L2Packet* prepareLinkEstablishmentRequest();
			
		protected:
			/** The link ID that is managed. */
			const IcaoId link_id;
			/** Points to the reservation table of this link. */
			ReservationManager& reservation_manager;
			/** Points to the queue manager, so that packets can be dequeued when transmission comes up. */
			QueueManager& queue_manager;
			/** Link establishment status. */
			Status link_establishment_status;
			/** A link is assigned on one particular frequency channel. It may be nullptr until the link_establishment status is link_established. */
			FrequencyChannel* current_channel = nullptr;
			/** Whether to use ARQ if this is a Point-to-Point link. */
			bool use_arq = true;
			/** Another component may define the number of slots that should be reserved for this link. This is based off the expected traffic load. */
			unsigned int num_slots_to_reserve = 1;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP
