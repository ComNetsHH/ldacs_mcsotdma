//
// Created by Sebastian Lindner on 09.11.20.
//

#include "QueueManager.hpp"
#include "LinkManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

QueueManager::QueueManager() : queue_map(), link_manager_map() {

}

QueueManager::Result QueueManager::push(::L2Packet* packet) {
	// Sanity check for a configured reservation manager.
	if (this->reservation_manager == nullptr)
		throw std::runtime_error("QueueManager cannot accept any packets until a ReservationManager has been assigned using setReservationManager(...).");
	
	const MacId& destination_id = packet->getDestination();
	// Sanity check that the destination is set. Without it we cannot determine the corresponding queue.
	if (destination_id == SYMBOLIC_ID_UNSET)
		throw std::runtime_error("QueueManager received a packet with an unset destination.");
	
	Result result;
	
	// Try to find the corresponding queue...
	auto iterator = queue_map.find(destination_id);
	std::queue<L2Packet*>* queue;
	LinkManager* link_manager = nullptr;
	// ... if it has not been found, create a new one.
	if (iterator == queue_map.end()) {
		// Create a new queue. It'll be deleted through the destructor.
		queue = new std::queue<L2Packet*>();
		std::pair<std::map<MacId, std::queue<L2Packet*>*>::iterator, bool> insertion_result = queue_map.insert(std::map<MacId, std::queue<L2Packet*>*>::value_type(destination_id, queue));
		if (!insertion_result.second)
			throw std::runtime_error("Attempted to insert a new queue, but there already was one.");
		if (destination_id == SYMBOLIC_LINK_ID_BROADCAST)
			result = Result::enqueued_bc; // First broadcast packet. No special result for this.
		else if (destination_id == SYMBOLIC_LINK_ID_BEACON)
			result = Result::enqueued_beacon; // First beacon packet. No special result for this.
		else
			result = Result::enqueued_new_p2p; // First P2P packet of a new link. Indicates that a link must be set up!
		// Also create a new link manager.
		link_manager = new LinkManager(destination_id, *reservation_manager, *this);
		std::pair<std::map<MacId, LinkManager*>::iterator, bool> other_insertion_result = link_manager_map.insert(std::map<MacId, LinkManager*>::value_type(destination_id, link_manager));
		if (!other_insertion_result.second)
			throw std::runtime_error("Attempted to insert a new link manager, but there already was one.");
		
	// ... if it has been found, add to it.
	} else {
		queue = (*iterator).second;
		if (destination_id == SYMBOLIC_LINK_ID_BROADCAST)
			result = Result::enqueued_bc;
		else if (destination_id == SYMBOLIC_LINK_ID_BEACON)
			result = Result::enqueued_beacon;
		else
			result = Result::enqueued_p2p;
		// Also find the corresponding link manager.
		link_manager = link_manager_map[destination_id];
	}
	
	// Push the packet into the queue...
	queue->push(packet);
	// ... and notify the corresponding link manager.
	link_manager->notifyOutgoing();
	return result;
}

QueueManager::~QueueManager() {
	for (auto& it : queue_map)
		delete it.second;
	for (auto& it : link_manager_map)
		delete it.second;
}

L2Packet* QueueManager::dequeue(const MacId& link_id) {
	auto iterator = queue_map.find(link_id);
	if (iterator == queue_map.end())
		throw std::invalid_argument("QueueManager::dequeue has no queue for link ID '" + std::to_string(link_id.getId()) + "'.");
	auto queue = (*iterator).second;
	if (queue->empty())
		throw std::runtime_error("QueueManager::dequeue on empty queue.");
	L2Packet* packet = queue->front();
	queue->pop();
	return packet;
}

void QueueManager::setReservationManager(ReservationManager* manager) {
	this->reservation_manager = manager;
}


