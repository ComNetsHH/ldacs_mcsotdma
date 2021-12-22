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
	throw std::runtime_error("onReceptionBurstStart not implemented");
}

void NewPPLinkManager::onReceptionBurst(unsigned int remaining_burst_length) {
	throw std::runtime_error("onReceptionBurst not implemented");
}

L2Packet* NewPPLinkManager::onTransmissionBurstStart(unsigned int burst_length) {
	throw std::runtime_error("onTransmissionBurstStart not implemented");
	return nullptr;
}

void NewPPLinkManager::onTransmissionBurst(unsigned int remaining_burst_length) {
	throw std::runtime_error("onTransmissionBurst not implemented");
}

void NewPPLinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << *mac << "::" << *this << "::notifyOutgoing(" << num_bits << ") -> ";
	// trigger link establishment
	if (link_status == link_not_established) {
		coutd << "link not established -> triggering establishment -> ";		
		establishLink();
	// unless it's already underway/established
	} else 
		coutd << "link status is '" << link_status << "' -> nothing to do." << std::endl;
	// update the traffic estimate	
	outgoing_traffic_estimate.put(num_bits);
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
	coutd << *mac << "::" << *this << "::onSlotStart(" << num_slots << ") -> ";		
}

void NewPPLinkManager::onSlotEnd() {
	coutd << *mac << "::" << *this << "::onSlotEnd -> ";
	// update the outgoing traffic estimate if it hasn't been
	if (!outgoing_traffic_estimate.hasBeenUpdated())
		outgoing_traffic_estimate.put(0);
	// mark the outgoing traffic estimate as unset
	outgoing_traffic_estimate.reset();
}

void NewPPLinkManager::populateLinkRequest(L2HeaderLinkRequest*& header, LinkRequestPayload*& payload) {
	coutd << "populating link request -> ";

	// determine number of slots in-between transmission bursts
	burst_offset = getBurstOffset();
	// determine number of TX and RX slots
	std::pair<unsigned int, unsigned int> tx_rx_split = this->getTxRxSplit(getRequiredTxSlots(), getRequiredRxSlots(), burst_offset);

	
}

std::pair<unsigned int, unsigned int> NewPPLinkManager::getTxRxSplit(unsigned int resource_req_me, unsigned int resource_req_you, unsigned int burst_offset) const {
	unsigned int burst_length = resource_req_me + resource_req_you;
	if (burst_length > burst_offset) {
		double tx_fraction = ((double) resource_req_me) / ((double) burst_length);
		resource_req_me = (unsigned int) (tx_fraction * burst_offset);
		resource_req_you = burst_offset - resource_req_me;
	}
	return {resource_req_me, resource_req_you};
}

unsigned int NewPPLinkManager::getBurstOffset() const {
	// TODO have upper layer set a delay target?
	// or some other way of choosing a burst offset?
	return 20;
}

unsigned int NewPPLinkManager::getRequiredTxSlots() const {
	if (!this->force_bidirectional_links && !mac->isThereMoreData(link_id))
		return 0;
	// bits
	unsigned int num_bits_per_burst = (unsigned int) outgoing_traffic_estimate.get();
	// bits/slot
	unsigned int datarate = mac->getCurrentDatarate();
	// slots
	return std::max(this->force_bidirectional_links ? uint(1) : uint(0), num_bits_per_burst / datarate);
}

unsigned int NewPPLinkManager::getRequiredRxSlots() const {
	return this->force_bidirectional_links ? std::max(uint(1), reported_resoure_requirement) : reported_resoure_requirement;
}