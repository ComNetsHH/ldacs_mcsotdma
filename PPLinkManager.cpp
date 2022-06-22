#include "PPLinkManager.hpp"
#include "SHLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

PPLinkManager::PPLinkManager(const MacId& link_id, ReservationManager *reservation_manager, MCSOTDMA_Mac *mac) : LinkManager(link_id, reservation_manager, mac) {}

void PPLinkManager::onReceptionReservation() {

}

L2Packet* PPLinkManager::onTransmissionReservation() {		
	return nullptr;
}

void PPLinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << *mac << "::" << *this << "::notifyOutgoing(" << num_bits << ") -> ";
	// trigger link establishment
	if (link_status == link_not_established) {
		coutd << "link not established -> triggering establishment -> ";		
		establishLink();
	// unless it's already underway/established
	} else 
		coutd << "link status is '" << link_status << "' -> nothing to do." << std::endl;	
}

void PPLinkManager::establishLink() {	
	coutd << "starting link establishment -> ";	
	if (this->link_status == link_established) {
		coutd << "status is '" << this->link_status << "' -> no need to establish -> ";
		return;
	}	
	// send link request on SH
	((SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->sendLinkRequest(link_id);	
	// update status
	coutd << "changing link status '" << this->link_status << "->" << awaiting_request_generation << "' -> ";
	this->link_status = awaiting_request_generation;	

	// to be able to measure the link establishment time, save the current time slot
	this->stat_link_establishment_start = mac->getCurrentSlot();		
}

void PPLinkManager::onSlotStart(uint64_t num_slots) {

}

void PPLinkManager::onSlotEnd() {

}

void PPLinkManager::processUnicastMessage(L2HeaderPP*& header, L2Packet::Payload*& payload) {

}

double PPLinkManager::getNumTxPerTimeSlot() const {
	if (!isActive())
		throw std::runtime_error("cannot call PPLinkManager::getNumSlotsUntilExpiry for inactive link");
	return ((double) link_state.timeout) * ((double) link_state.burst_offset) / 2.0;
}
bool PPLinkManager::isActive() const {
	return !(link_status == LinkManager::Status::link_not_established || link_status == LinkManager::Status::awaiting_request_generation);
}
