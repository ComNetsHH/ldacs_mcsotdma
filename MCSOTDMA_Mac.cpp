//
// Created by Sebastian Lindner on 16.11.20.
//

#include "MCSOTDMA_Mac.hpp"
#include <IPhy.hpp>

TUHH_INTAIRNET_MCSOTDMA::MCSOTDMA_Mac::~MCSOTDMA_Mac() {
	for (auto& pair : link_managers)
		delete pair.second;
}

void TUHH_INTAIRNET_MCSOTDMA::MCSOTDMA_Mac::notifyOutgoing(unsigned long num_bits,
                                                           const TUHH_INTAIRNET_MCSOTDMA::MacId& mac_id) {
	// Look for an existing link manager...
	auto it = link_managers.find(mac_id);
	LinkManager* link_manager;
	// ... if there's none ...
	if (it == link_managers.end()) {
		link_manager = new LinkManager(mac_id, reservation_manager, this);
		auto insertion_result = link_managers.insert(std::map<MacId, LinkManager*>::value_type(mac_id, link_manager));
		if (!insertion_result.second)
			throw std::runtime_error("Attempted to insert new LinkManager, but there already was one.");
	// ... if there already is one ...
	} else
		link_manager = (*it).second;
	
	// Tell the manager of new data.
	link_manager->notifyOutgoing(num_bits);
}

void TUHH_INTAIRNET_MCSOTDMA::MCSOTDMA_Mac::passToLower(TUHH_INTAIRNET_MCSOTDMA::L2Packet* packet) {

}

bool TUHH_INTAIRNET_MCSOTDMA::MCSOTDMA_Mac::shouldLinkBeArqProtected(const TUHH_INTAIRNET_MCSOTDMA::MacId& mac_id) const {
	if (this->upper_layer == nullptr)
		throw std::runtime_error("MCSOTDMA_Mac cannot query the ARQ sublayer because it has not been set yet (nullptr).");
	return this->upper_layer->shouldLinkBeArqProtected(mac_id);
}

TUHH_INTAIRNET_MCSOTDMA::MCSOTDMA_Mac::MCSOTDMA_Mac(TUHH_INTAIRNET_MCSOTDMA::ReservationManager* reservation_manager)
	: reservation_manager(reservation_manager) {
	
}

unsigned long TUHH_INTAIRNET_MCSOTDMA::MCSOTDMA_Mac::getCurrentDatarate() const {
	if (this->lower_layer == nullptr)
		throw std::runtime_error("MCSOTDMA_Mac cannot query the PHY layer because it has not been set yet (nullptr).");
	return lower_layer->getCurrentDatarate();
}

TUHH_INTAIRNET_MCSOTDMA::LinkManager*
TUHH_INTAIRNET_MCSOTDMA::MCSOTDMA_Mac::getLinkManager(const TUHH_INTAIRNET_MCSOTDMA::MacId& id) {
	return link_managers.at(id);
}
