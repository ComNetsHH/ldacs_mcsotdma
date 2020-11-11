//
// Created by Sebastian Lindner on 10.11.20.
//

#include "LinkManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

LinkManager::LinkManager(const IcaoId& link_id, ReservationManager& reservation_manager, QueueManager& queue_manager)
	: link_id(link_id), reservation_manager(reservation_manager),
	link_establishment_status((link_id == SYMBOLIC_LINK_ID_BROADCAST || link_id == SYMBOLIC_LINK_ID_BEACON) ? Status::link_established : Status::link_not_established) /* broadcast links are always established */,
	queue_manager(queue_manager) {}

const IcaoId& LinkManager::getLinkId() const {
	return this->link_id;
}

void LinkManager::notifyOutgoing() {
	// If the link is established...
	if (link_establishment_status == Status::link_established) {
	
	}
}

void LinkManager::notifyIncoming(L2Packet* incoming_packet) {
	throw std::runtime_error("not implemented");
}

