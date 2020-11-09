//
// Created by Sebastian Lindner on 09.11.20.
//

#include "QueueManager.hpp"

TUHH_INTAIRNET_MCSOTDMA::QueueManager::QueueManager() : queue_map() {

}

TUHH_INTAIRNET_MCSOTDMA::QueueManager::Result
TUHH_INTAIRNET_MCSOTDMA::QueueManager::push(const TUHH_INTAIRNET_MCSOTDMA::L2Packet* packet) {
	const LinkId& destination_id = packet->getDestination();
	// Sanity check that the destination is set. Without it we cannot determine the corresponding queue.
	if (destination_id == LINK_ID_UNSET)
		throw std::runtime_error("QueueManager received a packet with an unset destination.");
	
	Result result;
	
	// Try to find the corresponding queue...
	auto iterator = queue_map.find(destination_id);
	std::queue<const L2Packet*>* queue;
	// ... if it has not been found, create a new one.
	if (iterator == queue_map.end()) {
		// Create a new queue. It'll be deleted through the destructor.
		queue = new std::queue<const L2Packet*>();
		queue_map[destination_id] = queue;
		if (destination_id == LINK_ID_BROADCAST)
			result = Result::enqueued_bc; // First broadcast packet. No special result for this.
		else
			result = Result::enqueued_new_p2p; // First P2P packet of a new link. Indicates that a link must be set up!
	// ... if it has been found, add to it.
	} else {
		queue = (*iterator).second;
		if (destination_id == LINK_ID_BROADCAST)
			result = Result::enqueued_bc;
		else
			result = Result::enqueued_p2p;
	}
	
	queue->push(packet);
	return result;
}

TUHH_INTAIRNET_MCSOTDMA::QueueManager::~QueueManager() {
	for (auto& it : queue_map)
		delete it.second;
}


