//
// Created by Sebastian Lindner on 12/21/21.
//

#include "NewPPLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"
#include "SHLinkManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

NewPPLinkManager::NewPPLinkManager(const MacId& link_id, ReservationManager *reservation_manager, MCSOTDMA_Mac *mac) : LinkManager(link_id, reservation_manager, mac) {}

void NewPPLinkManager::onReceptionBurstStart(unsigned int burst_length) {
	throw std::runtime_error("not implemented");
}

void NewPPLinkManager::onReceptionBurst(unsigned int remaining_burst_length) {
	throw std::runtime_error("not implemented");
}

L2Packet* NewPPLinkManager::onTransmissionBurstStart(unsigned int burst_length) {
	throw std::runtime_error("not implemented");
	return nullptr;
}

void NewPPLinkManager::onTransmissionBurst(unsigned int remaining_burst_length) {
	throw std::runtime_error("not implemented");
}

void NewPPLinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << *mac << "::" << *this << "::notifyOutgoing(" << num_bits << ") -> ";
	if (link_status == link_not_established) {
		coutd << "link not established -> triggering establishment -> ";		
		establishLink();
	} else 
		coutd << "link status is '" << link_status << "' -> nothing to do." << std::endl;
}

void NewPPLinkManager::establishLink() {
	// create empty message
	auto *header = new L2HeaderLinkRequest(link_id);
	auto *payload = new LinkRequestPayload();
	// set callback s.t. the payload can be populated just-in-time.
	payload->callback = this;
	// pass to SH link manager
	((SHLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->sendLinkRequest(header, payload);
	// update status
	this->link_status = awaiting_reply;

	// to be able to measure the link establishment time, save the current time slot
	this->time_when_request_was_generated = mac->getCurrentSlot();
}

void NewPPLinkManager::onSlotStart(uint64_t num_slots) {
	throw std::runtime_error("not implemented");
}

void NewPPLinkManager::onSlotEnd() {
	throw std::runtime_error("not implemented");
}

void NewPPLinkManager::populateLinkRequest(L2HeaderLinkRequest*& header, LinkRequestPayload*& payload) {
	throw std::runtime_error("not implemented");
}