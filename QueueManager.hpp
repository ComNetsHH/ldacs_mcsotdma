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
			enum Result {
				/** A new P2P link should be set up. */
				link_required,
				/** An existing P2P queue was added to. */
				enqueued_p2p,
				/** The BC queue was added to. */
				enqueued_bc
			};
			
			QueueManager();
			virtual ~QueueManager();
			
			Result push(const L2Packet* packet);
		
		protected:
			std::map<IcaoId, std::queue<const L2Packet*>*> queue_map;
	};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_QUEUEMANAGER_HPP
