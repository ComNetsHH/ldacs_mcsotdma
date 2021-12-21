//
// Created by Sebastian Lindner on 12/21/21.
//

#include "NewPPLinkManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

NewPPLinkManager::NewPPLinkManager(const MacId& link_id, ReservationManager *reservation_manager, MCSOTDMA_Mac *mac) : LinkManager(link_id, reservation_manager, mac) {}

void NewPPLinkManager::onReceptionBurstStart(unsigned int burst_length) {

}

void NewPPLinkManager::onReceptionBurst(unsigned int remaining_burst_length) {

}

L2Packet* NewPPLinkManager::onTransmissionBurstStart(unsigned int burst_length) {
	return nullptr;
}

void NewPPLinkManager::onTransmissionBurst(unsigned int remaining_burst_length) {

}

void NewPPLinkManager::notifyOutgoing(unsigned long num_bits) {

}

void NewPPLinkManager::onSlotStart(uint64_t num_slots) {

}

void NewPPLinkManager::onSlotEnd() {

}

void NewPPLinkManager::populateLinkRequest(L2HeaderLinkRequest*& header, LinkRequestPayload*& payload) {

}