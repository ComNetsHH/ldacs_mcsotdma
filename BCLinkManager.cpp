//
// Created by seba on 2/18/21.
//

#include <sstream>
#include "BCLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "P2PLinkManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

BCLinkManager::BCLinkManager(ReservationManager* reservation_manager, MCSOTDMA_Mac* mac, unsigned int min_beacon_gap)
	: LinkManager(SYMBOLIC_LINK_ID_BROADCAST, reservation_manager, mac),
		contention_estimator(),
		congestion_estimator(BeaconModule::INITIAL_BEACON_OFFSET),
		beacon_module() {
	beacon_module.setMinBeaconGap(min_beacon_gap);
}

void BCLinkManager::onReceptionBurstStart(unsigned int burst_length) {

}

void BCLinkManager::onReceptionBurst(unsigned int remaining_burst_length) {

}

L2Packet* BCLinkManager::onTransmissionBurstStart(unsigned int remaining_burst_length) {
	coutd << *this << "::onTransmissionBurstStart -> ";
	if (remaining_burst_length != 0)
		throw std::invalid_argument("BCLinkManager::onTransmissionBurstStart for burst_length!=0.");

	auto *packet = new L2Packet();
	auto *base_header = new L2HeaderBase(mac->getMacId(), 0, 1, 1, 0);
	packet->addMessage(base_header, nullptr);
	unsigned long capacity = mac->getCurrentDatarate();

	// Put highest priority on beacons.
	if (beacon_module.shouldSendBeaconThisSlot()) {
		coutd << "broadcasting beacon -> ";
		packet->addMessage(beacon_module.generateBeacon(reservation_manager->getP2PReservationTables(), reservation_manager->getBroadcastReservationTable()));
		statistic_num_sent_beacons++;
	} else {
		coutd << "broadcasting data -> ";
		packet->addMessage(new L2HeaderBroadcast(), nullptr);
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
		// Add broadcast data.
		unsigned int remaining_bits = capacity - packet->getBits() + base_header->getBits(); // The requested packet will have a base header, which we'll drop, so add it to the requested number of bits.
		coutd << "adding " << remaining_bits << " bits from upper sublayer -> ";
		L2Packet *upper_layer_data = mac->requestSegment(remaining_bits, link_id);
		for (size_t i = 0; i < upper_layer_data->getPayloads().size(); i++)
			if (upper_layer_data->getHeaders().at(i)->frame_type != L2Header::base)
				packet->addMessage(upper_layer_data->getHeaders().at(i)->copy(), upper_layer_data->getPayloads().at(i)->copy());
		delete upper_layer_data;
		if (packet->getLinkInfoIndex() != -1)
			((LinkInfoPayload*&) packet->getPayloads().at(packet->getLinkInfoIndex()))->populate();
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
	coutd << "next broadcast scheduled in " << next_broadcast_slot << " slots -> ";
}

void BCLinkManager::onSlotStart(uint64_t num_slots) {
	coutd << *mac << "::" << *this << "::onSlotStart(" << num_slots << ") -> " << (next_broadcast_scheduled ? "next broadcast in " + std::to_string(next_broadcast_slot) + " slots -> " : "");
	// Mark reception slot if there's nothing else to do.
	if (current_reservation_table->getReservation(0).isIdle()) {
		coutd << "marking BC reception -> ";
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
			throw std::runtime_error("BCLinkManager(" + std::to_string(mac->getMacId().getId()) + ")::onSlotEnd would underflow next_broadcast_slot (was this transmission missed?)");
		next_broadcast_slot -= 1;
	}
	if (beacon_module.shouldSendBeaconThisSlot()) {
		// Schedule next beacon slot.
		int next_beacon_slot = (int) beacon_module.scheduleNextBeacon(contention_estimator.getAverageNonBeaconBroadcastRate(), contention_estimator.getNumActiveNeighbors(), current_reservation_table, reservation_manager->getTxTable());
		mac->emit(str_statistic_min_beacon_offset, (std::size_t) beacon_module.getBeaconOffset());
		if (!(current_reservation_table->isIdle(next_beacon_slot) || current_reservation_table->getReservation(next_beacon_slot).isBeaconTx())) {
			std::stringstream ss;
			ss << *mac << "::" << *this << "::onSlotEnd scheduled a beacon slot at a non-idle resource: " << current_reservation_table->getReservation(next_beacon_slot) << "!";
			throw std::runtime_error(ss.str());
		}
		current_reservation_table->mark(next_beacon_slot, Reservation(mac->getMacId(), Reservation::TX_BEACON));
		coutd << *mac << "::" << *this << "::onSlotEnd scheduled next beacon slot in " << next_beacon_slot << " slots -> ";
		// Reset congestion estimator with new beacon interval.
		congestion_estimator.reset(beacon_module.getBeaconOffset());
	}
	// Update estimators.
	contention_estimator.onSlotEnd();
	congestion_estimator.onSlotEnd();
	beacon_module.onSlotEnd();
	mac->emit(str_statistic_congestion, congestion_estimator.getCongestion());

	LinkManager::onSlotEnd();
}

void BCLinkManager::sendLinkRequest(L2HeaderLinkRequest* header, LinkManager::LinkRequestPayload* payload) {
	link_requests.emplace_back(header, payload);
	// Notify about outgoing data, which may schedule the next broadcast slot.
	notifyOutgoing(header->getBits() + payload->getBits());
}

size_t BCLinkManager::cancelLinkRequest(const MacId& id) {
	size_t num_removed = 0;
	for (auto it = link_requests.begin(); it != link_requests.end(); it++) {
		const auto* header = it->first;
		if (header->getDestId() == id) {
			link_requests.erase(it--);
			num_removed++;
		}
	}
	return num_removed;
}

unsigned int BCLinkManager::getNumCandidateSlots(double target_collision_prob) const {
	if (target_collision_prob < 0.0 || target_collision_prob > 1.0)
		throw std::invalid_argument("BCLinkManager::getNumCandidateSlots target collision probability not between 0 and 1.");
	// Average broadcast rate.
	double r = contention_estimator.getAverageNonBeaconBroadcastRate();
	mac->emit(str_statistic_contention, r);
	// Number of active neighbors.
	unsigned int m = contention_estimator.getNumActiveNeighbors();
	mac->emit(str_statistic_num_active_neighbors, (size_t) m);
	double num_candidates = 0;
	// For every number n of channel accesses from 0 to all neighbors...
	for (auto n = 0; n <= m; n++) {
		// Probability P(X=n) of n accesses.
		double p = ((double) nchoosek(m, n)) * std::pow(r, n) * std::pow(1 - r, m - n);
		// Number of slots that should be chosen if n accesses occur (see IntAirNet Deliverable AP 2.2).
		unsigned int k = n == 0 ? 1 : std::ceil(1.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n)));
		num_candidates += p * k;
	}
	unsigned int final_candidates = std::max(MIN_CANDIDATES, (unsigned int) std::ceil(num_candidates));
	coutd << "avg_broadcast_rate=" << r << " num_active_neighbors=" << m << " -> num_candidates=" << final_candidates << " -> ";
	return final_candidates;
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
	unsigned int num_candidates = getNumCandidateSlots(this->broadcast_target_collision_prob);
	statistic_num_broadcast_candidate_slots = num_candidates;
	mac->emit(str_statistic_num_broadcast_candidate_slots, statistic_num_broadcast_candidate_slots);
	std::vector<unsigned int > candidate_slots = current_reservation_table->findCandidates(num_candidates, 1, 1, 1, 1, 0, false);
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
	coutd << "parsing incoming beacon -> ";
	beacon_module.parseBeacon(origin_id, (const BeaconPayload*&) payload, reservation_manager);
}

void BCLinkManager::processIncomingBroadcast(const MacId& origin, L2HeaderBroadcast*& header) {
	// TODO set next broadcast slot in reservation table
}

void BCLinkManager::processIncomingUnicast(L2HeaderUnicast*& header, L2Packet::Payload*& payload) {
	// TODO compare to local ID, discard or forward resp.
	LinkManager::processIncomingUnicast(header, payload);
}

void BCLinkManager::processIncomingBase(L2HeaderBase*& header) {
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

BCLinkManager::~BCLinkManager() {
	for (auto pair : link_requests) {
		delete pair.first;
		delete pair.second;
	}
}

void BCLinkManager::assign(const FrequencyChannel* channel) {
	LinkManager::assign(channel);
}

void BCLinkManager::onPacketReception(L2Packet*& packet) {
	// Congestion is concerned with *any* received broadcast
	congestion_estimator.reportBroadcast(packet->getOrigin());
	// Contention is only concerned with non-beacon broadcasts
	if (packet->getBeaconIndex() == -1)
		contention_estimator.reportNonBeaconBroadcast(packet->getOrigin());

	LinkManager::onPacketReception(packet);
}

void BCLinkManager::processIncomingLinkInfo(const L2HeaderLinkInfo*& header, const LinkInfoPayload*& payload) {
	const LinkInfo &info = payload->getLinkInfo();
	const MacId &tx_id = info.getTxId(), &rx_id = info.getRxId();
	if (tx_id == mac->getMacId() || rx_id == mac->getMacId()) {
		coutd << "involves us; discarding -> ";
	} else {
		coutd << "passing on to " << tx_id  << " -> ";
		((P2PLinkManager*) mac->getLinkManager(tx_id))->processIncomingLinkInfo(header, payload);
	}
}
