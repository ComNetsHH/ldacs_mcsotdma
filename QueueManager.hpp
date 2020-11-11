//
// Created by Sebastian Lindner on 09.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_QUEUEMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_QUEUEMANAGER_HPP

#include <map>
#include <queue>
#include "IcaoId.hpp"
#include "L2Packet.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/**
	 * The queue manager accepts upper layer packets and sorts them into link-specific queues.
	 */
	class QueueManager {
		public:
			/** Outcome from pushing a packet to this manager. */
			enum Result {
				/** A new Point-to-Point queue was created. Indicates that a link should be set up. */
				enqueued_new_p2p,
				/** An existing Point-to-Point queue was added to. */
				enqueued_p2p,
				/** The Broadcast queue was added to. */
				enqueued_bc,
				/** The Beacon queue was added to. */
				enqueued_beacon
			};
			
			QueueManager();
			virtual ~QueueManager();
			
			Result push(L2Packet* packet);
			
			L2Packet* dequeue(const IcaoId& link_id);
		
		protected:
			std::map<IcaoId, std::queue<L2Packet*>*> queue_map;
	};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_QUEUEMANAGER_HPP
