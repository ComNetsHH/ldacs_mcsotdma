//
// Created by Sebastian Lindner on 10.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP

#include "ReservationManager.hpp"
#include "IcaoId.hpp"
#include "L2Packet.hpp"
#include "QueueManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/**
	 * A LinkManager is responsible for a single communication link.
	 * It is notified by a QueueManager of new packets, and utilizes a ReservationManager to make slot reservations.
	 */
	class LinkManager {
		public:
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
			
		protected:
			/** The link ID that is managed. */
			const IcaoId link_id;
			/** Points to the reservation manager, so that reservations can be made. */
			ReservationManager& reservation_manager;
			/** Points to the queue manager, so that packets can be dequeued when transmission comes up. */
			QueueManager& queue_manager;
			/** Link establishment status. */
			Status link_establishment_status;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_LINKMANAGER_HPP
