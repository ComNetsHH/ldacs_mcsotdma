//
// Created by Sebastian Lindner on 10.11.20.
//

#include "LinkManager.hpp"
#include "coutdebug.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

LinkManager::LinkManager(const MacId& link_id, ReservationManager& reservation_manager, MCSOTDMA_Mac& mac)
	: link_id(link_id), reservation_manager(reservation_manager),
	link_establishment_status((link_id == SYMBOLIC_LINK_ID_BROADCAST || link_id == SYMBOLIC_LINK_ID_BEACON) ? Status::link_established : Status::link_not_established) /* broadcast links are always established */,
	  mac(mac) {}

const MacId& LinkManager::getLinkId() const {
	return this->link_id;
}

void LinkManager::notifyOutgoing() {
	coutd << "LinkManager::notifyOutgoing on link '" << link_id.getId() << "'";
	// If the link is established...
	if (link_establishment_status == Status::link_established) {
		coutd << " is already established";
	// If the link is not yet established...
	} else {
		// ... and we've created the request and are just waiting for a reply ...
		if (link_establishment_status == Status::awaiting_reply) {
			coutd << " is awaiting reply. Doing nothing." << std::endl;
			// ... then do nothing.
			return;
		// ... and link establishment has not yet been started ...
		} else if (link_establishment_status == Status::link_not_established) {
			coutd << " starting link establishment" << std::endl;
			
		} else {
			throw std::runtime_error("Unsupported LinkManager::Status: '" + std::to_string(link_not_established) + "'.");
		}
	}
}

void LinkManager::notifyIncoming(L2Packet* incoming_packet) {
	throw std::runtime_error("not implemented");
}

L2Packet* LinkManager::prepareLinkEstablishmentRequest() {
	L2Packet* request;
//	L2HeaderLinkEstablishmentRequest* header = new L2HeaderLinkEstablishmentRequest(link_id, use_arq, 0, 0, 0); // TODO link to actual ARQ and set reasonable ARQ params here
//	ProposalPayload* payload = new ProposalPayload();
	return request;
}


unsigned int LinkManager::getNumSlotsToReserve() const {
	return this->num_slots_to_reserve;
}

void LinkManager::setNumSlotsToReserver(const unsigned  int& num_slots) {
	this->num_slots_to_reserve = num_slots;
}

unsigned int LinkManager::ProposalPayload::getBits() const {
	return 0;
}
