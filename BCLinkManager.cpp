//
// Created by seba on 2/18/21.
//

#include "BCLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "P2PLinkManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

BCLinkManager::BCLinkManager(ReservationManager* reservation_manager, MCSOTDMA_Mac* mac, unsigned int min_beacon_gap)
	: LinkManager(SYMBOLIC_LINK_ID_BROADCAST, reservation_manager, mac), min_beacon_gap(min_beacon_gap) {
	contention_estimator = ContentionEstimator(beacon_offset);
}

void BCLinkManager::onReceptionBurstStart(unsigned int burst_length) {

}

void BCLinkManager::onReceptionBurst(unsigned int remaining_burst_length) {

}

L2Packet* BCLinkManager::onTransmissionBurstStart(unsigned int burst_length) {
	coutd << *this << "::onTransmissionBurstStart -> ";
	if (burst_length != 1)
		throw std::invalid_argument("BCLinkManager::onTransmissionBurstStart for burst_length!=1.");

	auto *packet = new L2Packet();
	auto *base_header = new L2HeaderBase(mac->getMacId(), 0, 1, 1, 0);
	packet->addMessage(base_header, nullptr);
	packet->addMessage(new L2HeaderBroadcast(), nullptr);
	unsigned long capacity = mac->getCurrentDatarate();
	// Put a priority on link requests.
	while (!link_requests.empty()) {
		// Fetch next link request.
		auto &pair = link_requests.at(0);
		// Compute payload.
		if (pair.second->callback == nullptr)
			throw std::invalid_argument("BCLinkManager::onTransmissionBurstStart has nullptr link request callback - can't populate the LinkRequest!");
		pair.second->callback->populateLinkRequest(pair.first, pair.second);
		// Add to the packet if it fits.
		if (packet->getBits() + pair.first->getBits() + pair.second->getBits() <= capacity) {
			packet->addMessage(pair.first, pair.second);
			link_requests.erase(link_requests.begin());
			coutd << "added link request for '" << pair.first->dest_id << "' to broadcast -> ";
			statistic_num_sent_requests++;
		} else
			break; // Stop if it doesn't fit anymore.
	}
	// Add broadcast payload
//	L2Packet *broadcast_data = mac->requestSegment(capacity - packet->getBits(), SYMBOLIC_LINK_ID_BROADCAST);

	// Schedule next broadcast if there's more data to send.
	if (!link_requests.empty() || mac->isThereMoreData(link_id)) {
		coutd << "scheduling next slot in ";
		scheduleBroadcastSlot();
		coutd << next_broadcast_slot << " slots -> ";
		// Put it into the header.
		base_header->burst_offset = next_broadcast_slot;
	} else {
		next_broadcast_scheduled = false;
		coutd << "no more broadcast data, not scheduling a next slot -> ";
	}

	statistic_num_sent_packets++;

	return packet;
}

void BCLinkManager::onTransmissionBurst(unsigned int remaining_burst_length) {
	throw std::runtime_error("BCLinkManager::onTransmissionBurst, but the BCLinkManager should never have multi-slot transmissions.");
}

void BCLinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << *this << "::notifyOutgoing(" << num_bits << ") -> ";
	if (!next_broadcast_scheduled) {
		coutd << "scheduling next broadcast -> ";
		scheduleBroadcastSlot();
	}
	coutd << "next broadcast scheduled in " << next_broadcast_slot << " slots." << std::endl;
}

void BCLinkManager::onSlotStart(uint64_t num_slots) {
	coutd << *this << "::onSlotStart(" << num_slots << ") -> ";

	for (uint64_t t = 0; t < num_slots; t++)
		contention_estimator.update();
	if (current_reservation_table->getReservation(0).isIdle()) {
		coutd << "marking BC reception" << std::endl;
		try {
			current_reservation_table->mark(0, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::RX));
		} catch (const std::exception& e) {
			throw std::runtime_error("BCLinkManager::onSlotStart(" + std::to_string(num_slots) + ") error trying to mark BC reception slot: " + e.what());
		}
	}
}

void BCLinkManager::onSlotEnd() {
	if (next_broadcast_scheduled) {
		if (next_broadcast_slot == 0)
			throw std::runtime_error("BCLinkManager::onSlotEnd would underflow next_broadcast_slot (was this transmission missed?)");
		next_broadcast_slot -= 1;
	}
}

void BCLinkManager::sendLinkRequest(L2HeaderLinkRequest* header, LinkManager::LinkRequestPayload* payload) {
	link_requests.emplace_back(header, payload);
	// Notify about outgoing data, which may schedule the next broadcast slot.
	notifyOutgoing(header->getBits() + payload->getBits());
}

unsigned int BCLinkManager::getNumCandidateSlots(double target_collision_prob) const {
	if (target_collision_prob < 0.0 || target_collision_prob > 1.0)
		throw std::invalid_argument("BCLinkManager::getNumCandidateSlots target collision probability not between 0 and 1.");
	// Average broadcast rate.
	double r = contention_estimator.getAverageBroadcastRate();
	// Number of active neighbors.
	unsigned int m = contention_estimator.getNumActiveNeighbors();
	double num_candidates = 0;
	// For every number n of channel accesses from 0 to all neighbors...
	for (auto n = 0; n <= m; n++) {
		// Probability P(X=n) of n accesses.
		double p = ((double) nchoosek(m, n)) * std::pow(r, n) * std::pow(1 - r, m - n);
		// Number of slots that should be chosen if n accesses occur (see IntAirNet Deliverable AP 2.2).
		unsigned int k = n == 0 ? 1 : std::ceil(1.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n)));
		num_candidates += p * k;
	}
	coutd << "avg_broadcast_rate=" << r << " num_active_neighbors=" << m << " -> num_candidates=" << std::ceil(num_candidates) << " -> ";
	return std::ceil(num_candidates);
}

unsigned long long BCLinkManager::nchoosek(unsigned long n, unsigned long k) const {
	if (k == 0)
		return 1;
	return (n * nchoosek(n - 1, k - 1)) / k;
}

unsigned int BCLinkManager::broadcastSlotSelection() {
	coutd << *this << "::broadcastSlotSelection -> ";
	if (current_reservation_table == nullptr)
		throw std::runtime_error("BCLinkManager::broadcastSlotSelection for unset ReservationTable.");
	unsigned int num_candidates = getNumCandidateSlots(this->bc_coll_prob);
	std::vector<unsigned int > candidate_slots = current_reservation_table->findCandidates(num_candidates, 1, 1, 1, false);
	if (candidate_slots.empty())
		throw std::runtime_error("BCLinkManager::broadcastSlotSelection found zero candidate slots.");
	return candidate_slots.at(getRandomInt(0, candidate_slots.size()));
}

void BCLinkManager::scheduleBroadcastSlot() {
	next_broadcast_slot = broadcastSlotSelection();
	next_broadcast_scheduled = true;
	current_reservation_table->mark(next_broadcast_slot, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX));
}

void BCLinkManager::processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) {
	LinkManager::processIncomingBeacon(origin_id, header, payload);
}

void BCLinkManager::processIncomingBroadcast(const MacId& origin, L2HeaderBroadcast*& header) {
	// TODO set next broadcast slot in reservation table
}

void BCLinkManager::processIncomingUnicast(L2HeaderUnicast*& header, L2Packet::Payload*& payload) {
	// TODO compare to local ID, discard or forward resp.
	LinkManager::processIncomingUnicast(header, payload);
}

void BCLinkManager::processIncomingBase(L2HeaderBase*& header) {
	MacId sender = header->src_id;
	contention_estimator.reportBroadcast(sender);
	coutd << "updated contention -> ";
}

void BCLinkManager::processIncomingLinkRequest(const L2Header*& header, const L2Packet::Payload*& payload, const MacId& origin) {
	MacId dest_id = ((const L2HeaderLinkRequest*&) header)->dest_id;
	if (dest_id == mac->getMacId()) {
		coutd << "forwarding link request to P2PLinkManager -> ";
		statistic_num_received_requests++;
		((P2PLinkManager*) mac->getLinkManager(origin))->processIncomingLinkRequest(header, payload, origin);
	} else
		coutd << "discarding link request that is not destined to us -> ";
}

void BCLinkManager::processIncomingLinkReply(const L2HeaderLinkEstablishmentReply*& header, const L2Packet::Payload*& payload) {
	throw std::invalid_argument("BCLinkManager::processIncomingLinkReply called, but link replies shouldn't be received on the BC.");
}
