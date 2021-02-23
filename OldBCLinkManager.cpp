//
// Created by Sebastian Lindner on 18.11.20.
//

#include <cassert>
#include "OldBCLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"
#include "BCLinkManagementEntity.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

OldBCLinkManager::OldBCLinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac,
                                   unsigned int num_slots_contention_estimate)
		: OldLinkManager(link_id, reservation_manager, mac), contention_estimator(num_slots_contention_estimate) {
	if (link_id != SYMBOLIC_LINK_ID_BROADCAST)
		throw std::invalid_argument("BCLinkManager must have the broadcast ID.");
	delete lme;
	lme = new BCLinkManagementEntity(this);
	link_status = link_established;
	// Broadcast reservations don't remain valid.
	lme->setTxTimeout(0);
	// Offset to next broadcast will be dynamically chosen.
	lme->setTxOffset(0);
}

OldBCLinkManager::OldBCLinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac)
		: OldBCLinkManager(link_id, reservation_manager, mac, 5000 /* past 60s for 12ms slots */) {}


L2Packet* OldBCLinkManager::prepareBeacon() {
	auto* beacon = new L2Packet();
	// Base header.
	auto* base_header = new L2HeaderBase(mac->getMacId(), 0, 0, 0, 0);
	// Beacon header.
	CPRPosition pos = mac->getPosition(mac->getMacId());
	auto* beacon_header = new L2HeaderBeacon(pos, pos.odd, mac->getNumHopsToGS(), mac->getPositionQuality(mac->getMacId()));
	// Beacon payload.
	unsigned long max_bits = mac->getCurrentDatarate();
	max_bits -= (base_header->getBits() + beacon_header->getBits());
	auto* beacon_payload = computeBeaconPayload(max_bits);
	// Put it together.
	beacon->addMessage(base_header, nullptr);
	beacon->addMessage(beacon_header, beacon_payload);
	return beacon;
}

void OldBCLinkManager::processIncomingBroadcast(const MacId& origin, L2HeaderBroadcast*& header) {
	// Do nothing in particular.
}

void OldBCLinkManager::processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) {
	assert(payload && "OldLinkManager::processIncomingBeacon for nullptr BeaconPayload*");
	if (origin_id == SYMBOLIC_ID_UNSET)
		throw std::invalid_argument("OldLinkManager::processIncomingBeacon for an unset ID.");
	// Update the neighbor position.
	mac->updatePosition(origin_id, CPRPosition(header->position.latitude, header->position.longitude, header->position.altitude, header->is_cpr_odd), header->pos_quality);
	// Update neighbor's report of how many hops they need to the ground station.
	mac->reportNumHopsToGS(origin_id, header->num_hops_to_ground_station);
	// Parse the beacon payload to learn about this user's resource utilization.
	reservation_manager->updateTables(payload->local_reservations);
}

void OldBCLinkManager::setBeaconHeaderFields(L2HeaderBeacon*& header) const {
	coutd << "-> setting beacon header fields:";
	header->num_hops_to_ground_station = mac->getNumHopsToGS();
	coutd << " num_hops=" << header->num_hops_to_ground_station;
	coutd << " ";
}

void OldBCLinkManager::setBroadcastHeaderFields(L2HeaderBroadcast*& header) const {
	coutd << "-> setting broadcast header fields:";
	// no fields.
	coutd << " none ";
}

void OldBCLinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << "BCLinkManager(" << link_id << ")::notifyOutgoing(" << num_bits << " bits) -> ";
	if (!broadcast_slot_scheduled) {
		lme->setTxOffset(broadcastSlotSelection());
		assert(current_reservation_table && "BCLinkManager::notifyOutgoing for unset reservation table.");
		current_reservation_table->mark(lme->getTxOffset(), Reservation(link_id, Reservation::TX));
		coutd << "scheduled broadcast in " << lme->getTxOffset() << " slots." << std::endl;
		broadcast_slot_scheduled = true;
	} else
		coutd << "already have a broadcast slot scheduled." << std::endl;
}

L2Packet* OldBCLinkManager::onTransmissionBurstStart(unsigned int num_slots) {
	coutd << "BCLinkManager(" << link_id << ")::onTransmissionBurstStart -> ";
	if (num_slots != 1)
		throw std::invalid_argument("BCLinkManager::onTransmissionBurstStart cannot be used for more or less than 1 slot.");
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
			lme->setTxOffset(broadcastSlotSelection());
			current_reservation_table->mark(lme->getTxOffset(), Reservation(link_id, Reservation::TX));
			broadcast_slot_scheduled = true; // remains true
			coutd << "scheduled next broadcast in " << lme->getTxOffset() << " slots -> ";
		} else {
			coutd << "no next broadcast slot required -> ";
			broadcast_slot_scheduled = false;
			lme->setTxOffset(0);
		}
		// ... and set the header field.
		for (L2Header* header : packet->getHeaders())
			setHeaderFields(header);
	} else {
		// must send beacon
		// and schedule a new beacon slot
	}
	statistic_num_sent_packets++;
	return packet;
}

unsigned int OldBCLinkManager::getNumActiveNeighbors() const {
	return contention_estimator.getNumActiveNeighbors();
}

void OldBCLinkManager::onSlotStart(uint64_t num_slots) {
	if (!traffic_estimate.hasBeenUpdated())
		for (uint64_t t = 0; t < num_slots; t++)
			traffic_estimate.put(0);
	traffic_estimate.reset();

	for (uint64_t t = 0; t < num_slots; t++)
		contention_estimator.update();
	if (current_reservation_table->getReservation(0).isIdle()) {
		coutd << "marking BC reception: ";
		markReservations(1, 0, 0, 1, SYMBOLIC_LINK_ID_BROADCAST, Reservation::RX);
		coutd << std::endl;
	}
}

unsigned int OldBCLinkManager::getNumCandidateSlots(double target_collision_prob) const {
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
		double p = ((double) nchoosek(m, n)) * std::pow(r, n) * std::pow(1 - r, m - n);
		// Number of slots that should be chosen if n accesses occur (see IntAirNet Deliverable AP 2.2).
		unsigned int k = n == 0 ? 1 : std::ceil(1.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n)));
		expected_num_bc_accesses += p * k;
	}
	return std::ceil(expected_num_bc_accesses);
}

unsigned long long OldBCLinkManager::nchoosek(unsigned long n, unsigned long k) const {
	if (k == 0)
		return 1;
	return (n * nchoosek(n - 1, k - 1)) / k;
}

void OldBCLinkManager::setTargetCollisionProbability(double p) {
	if (p < 0.0 || p > 1.0)
		throw std::invalid_argument("BCLinkManager::setTargetCollisionProbability p must be between 0 and 1.");
	this->target_collision_probability = p;
}

unsigned int OldBCLinkManager::broadcastSlotSelection() {
	if (current_reservation_table == nullptr)
		throw std::runtime_error("BCLinkManager::broadcastSlotSelection for unset ReservationTable.");
	unsigned int num_candidates = getNumCandidateSlots(this->target_collision_probability);
	std::vector<int32_t> candidate_slots = current_reservation_table->findCandidateSlots(lme->getMinOffset(), num_candidates, 1, true, false);
	if (candidate_slots.empty())
		throw std::runtime_error("BCLinkManager::broadcastSlotSelection found zero candidate slots.");
	int32_t slot = candidate_slots.at(getRandomInt(0, candidate_slots.size()));
	if (slot < 0)
		throw std::runtime_error("BCLinkManager::broadcastSlotSelection chose a slot in the past.");
	return (unsigned int) slot;
}

void OldBCLinkManager::onReceptionBurstStart(unsigned int burst_length) {
	// Don't decrement timeout, i.e. don't call base function.
}

void OldBCLinkManager::processIncomingBase(L2HeaderBase*& header) {
	coutd << "updated contention estimate -> ";
	contention_estimator.reportBroadcast(header->src_id);
}

void OldBCLinkManager::onSlotEnd() {
	// Do nothing.
}