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
	if (link_id != SYMBOLIC_LINK_ID_BROADCAST)
		throw std::invalid_argument("BCLinkManager must have the broadcast ID.");
	link_establishment_status = link_established;
	// Broadcast reservations don't remain valid.
	current_reservation_timeout = 0;
	// Offset to next broadcast will be dynamically chosen.
	current_reservation_offset = 0;
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
	coutd << "updated contention estimate";
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
	coutd << "BCLinkManager(" << link_id << ")::notifyOutgoing(" << num_bits << " bits) -> ";
	if (!broadcast_slot_scheduled) {
		current_reservation_offset = broadcastSlotSelection();
		assert(current_reservation_table && "BCLinkManager::notifyOutgoing for unset reservation table.");
		current_reservation_table->mark(current_reservation_offset, Reservation(link_id, Reservation::TX));
		coutd << "scheduled broadcast next_broadcast_slot in " << current_reservation_offset << " slots." << std::endl;
		broadcast_slot_scheduled = true;
	} else
		coutd << "already have a broadcast slot scheduled." << std::endl;
}

L2Packet* BCLinkManager::onTransmissionSlot(unsigned int num_slots) {
	coutd << "BCLinkManager(" << link_id << ")::onTransmissionSlot -> ";
	if (num_slots != 1)
		throw std::invalid_argument("BCLinkManager::onTransmissionSlot cannot be used for more or less than 1 slot.");
	L2Packet* packet;
	bool is_beacon_slot = false; // TODO determine
	if (!is_beacon_slot) {
		// Prepare broadcast.
		unsigned long bits_per_slot = mac->getCurrentDatarate();
		coutd << "requesting " << bits_per_slot << " bits -> ";
		packet = mac->requestSegment(bits_per_slot, link_id);
		// Check if there's more data...
		if (mac->isThereMoreData(link_id)) {
			// ... if so, schedule a next slot
			current_reservation_offset = broadcastSlotSelection();
			current_reservation_table->mark(current_reservation_offset, Reservation(link_id, Reservation::TX));
			broadcast_slot_scheduled = true; // remains true
			coutd << "scheduled next broadcast in " << current_reservation_offset << " slots -> ";
		} else {
			coutd << "no next broadcast slot required -> ";
			broadcast_slot_scheduled = false;
			current_reservation_offset = 0;
		}
		// ... and set the header field.
		for (L2Header* header : packet->getHeaders())
			setHeaderFields(header);
	} else {
		// must send beacon
		// and schedule a new beacon slot
	}
	return packet;
}

unsigned int BCLinkManager::getNumActiveNeighbors() const {
	return contention_estimator.getNumActiveNeighbors();
}

void BCLinkManager::update(uint64_t num_slots) {
	for (uint64_t i = 0; i < num_slots; i++)
		contention_estimator.update();
}

unsigned int BCLinkManager::getNumCandidateSlots(double target_collision_prob) const {
	if (target_collision_prob < 0.0 || target_collision_prob > 1.0)
		throw std::invalid_argument("BCLinkManager::getNumCandidateSlots target collision probability not between 0 and 1.");
	// Average broadcast rate.
	double r = contention_estimator.getAverageBroadcastRate();
	// Number of active neighbors.
	unsigned int m = contention_estimator.getNumActiveNeighbors();
	double expected_num_bc_accesses = 0;
	// For every number n of channel accesses from 0 to all neighbors...
	for (auto n = 0; n <= m; n++) {
		// Probability P(X=n) of n accesses.
		double p = ((double) nchoosek(m, n)) * pow(r, n) * pow(1 - r, m - n);
		// Number of slots that should be chosen if n accesses occur (see IntAirNet Deliverable AP 2.2).
		unsigned int k = n == 0 ? 1 : ceil(1.0 / (1.0 - pow(1.0 - target_collision_prob, 1.0 / n)));
		expected_num_bc_accesses += p*k;
	}
	return ceil(expected_num_bc_accesses);
}

unsigned long long BCLinkManager::nchoosek(unsigned long n, unsigned long k) const {
	if (k == 0)
		return 1;
	return (n * nchoosek(n - 1, k - 1)) / k;
}

void BCLinkManager::setTargetCollisionProbability(double p) {
	if (p < 0.0 || p > 1.0)
		throw std::invalid_argument("BCLinkManager::setTargetCollisionProbability p must be between 0 and 1.");
	this->target_collision_probability = p;
}

unsigned int BCLinkManager::broadcastSlotSelection() const {
	if (current_reservation_table == nullptr)
		throw std::runtime_error("BCLinkManager::broadcastSlotSelection for unset ReservationTable.");
	unsigned int num_candidates = getNumCandidateSlots(this->target_collision_probability);
	std::vector<int32_t> candidate_slots = current_reservation_table->findCandidateSlots(this->minimum_slot_offset_for_new_slot_reservations, num_candidates, 1, true);
	int32_t slot = candidate_slots.at(getRandomInt(0, candidate_slots.size()));
	if (slot < 0)
		throw std::runtime_error("BCLinkManager::broadcastSlotSelection chose a slot in the past.");
	return (unsigned int) slot;
}
