//
// Created by Sebastian Lindner on 18.11.20.
//

#include <cassert>
#include "BCLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

BCLinkManager::BCLinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac,
                             unsigned int num_slots_contention_estimate)
     : LinkManager(link_id, reservation_manager, mac), contention_estimator(num_slots_contention_estimate) {
	link_establishment_status = link_established;
}

BCLinkManager::BCLinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac)
	: BCLinkManager(link_id, reservation_manager, mac, 5000 /* past 60s for 12ms slots */) {}

L2Packet* BCLinkManager::prepareBeacon() {
	auto* beacon = new L2Packet();
	// Base header.
	auto* base_header = new L2HeaderBase(mac->getMacId(), 0, 0, 0);
	// Beacon header.
	CPRPosition pos = mac->getPosition(mac->getMacId());
	auto* beacon_header = new L2HeaderBeacon(pos, pos.odd, mac->getNumHopsToGS(), mac->getPositionQuality(mac->getMacId()));
	// Beacon payload.
	unsigned long max_bits = mac->getCurrentDatarate();
	max_bits -= (base_header->getBits() + beacon_header->getBits());
	auto* beacon_payload = computeBeaconPayload(max_bits);
	// Put it together.
	beacon->addPayload(base_header, nullptr);
	beacon->addPayload(beacon_header, beacon_payload);
	return beacon;
}

void BCLinkManager::processIncomingBroadcast(const MacId& origin, L2HeaderBroadcast*& header) {
	// Update the contention estimator.
	coutd << " -> process broadcast -> updated contention estimate";
	contention_estimator.reportBroadcast(origin);
}

void BCLinkManager::processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) {
	assert(payload && "LinkManager::processIncomingBeacon for nullptr BeaconPayload*");
	if (origin_id == SYMBOLIC_ID_UNSET)
		throw std::invalid_argument("LinkManager::processIncomingBeacon for an unset ID.");
	// Update the neighbor position.
	mac->updatePosition(origin_id, CPRPosition(header->position.latitude, header->position.longitude, header->position.altitude, header->is_cpr_odd), header->pos_quality);
	// Update neighbor's report of how many hops they need to the ground station.
	mac->reportNumHopsToGS(origin_id, header->num_hops_to_ground_station);
	// Parse the beacon payload to learn about this user's resource utilization.
	reservation_manager->updateTables(payload->local_reservations);
}

void BCLinkManager::setBeaconHeaderFields(L2HeaderBeacon* header) const {
	coutd << "-> setting beacon header fields:";
	header->num_hops_to_ground_station = mac->getNumHopsToGS();
	coutd << " num_hops=" << header->num_hops_to_ground_station;
	coutd << " ";
}

void BCLinkManager::setBroadcastHeaderFields(L2HeaderBroadcast* header) const {
	coutd << "-> setting broadcast header fields:";
	// no fields.
	coutd << " none ";
}

void BCLinkManager::notifyOutgoing(unsigned long num_bits) {
	LinkManager::notifyOutgoing(num_bits);
	// must implement broadcast slot selection
}

L2Packet* BCLinkManager::onTransmissionSlot(unsigned int num_slots) {
	return LinkManager::onTransmissionSlot(num_slots);
	// must differentiate between beacon and broadcast slots
	// and schedule new beacon slots if necessary
}

unsigned int BCLinkManager::getNumActiveNeighbors() const {
	return contention_estimator.getNumActiveNeighbors();
}

void BCLinkManager::update(uint64_t num_slots) {
	for (auto i = 0; i < num_slots; i++)
		contention_estimator.update();
}
