//
// Created by seba on 2/18/21.
//

#include <sstream>
#include "BCLinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "P2PLinkManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

BCLinkManager::BCLinkManager(ReservationManager* reservation_manager, MCSOTDMA_Mac* mac, unsigned int min_beacon_gap)
: LinkManager(SYMBOLIC_LINK_ID_BROADCAST, reservation_manager, mac), avg_num_slots_inbetween_packet_generations(100),
		contention_estimator(5000),
		beacon_module(),
		congestion_estimator(beacon_module.getMinBeaconInterval())
		{
	beacon_module.setMinBeaconGap(min_beacon_gap);
}

void BCLinkManager::onReceptionBurstStart(unsigned int burst_length) {

}

void BCLinkManager::onReceptionBurst(unsigned int remaining_burst_length) {

}

L2Packet* BCLinkManager::onTransmissionBurstStart(unsigned int remaining_burst_length) {
	coutd << *mac << "::" << *this << "::onTransmissionBurstStart -> ";
	if (remaining_burst_length != 0)
		throw std::invalid_argument("BCLinkManager::onTransmissionBurstStart for burst_length!=0.");

	auto *packet = new L2Packet();
	auto *base_header = new L2HeaderBase(mac->getMacId(), 0, 1, 1, 0);
	packet->addMessage(base_header, nullptr);
	unsigned long capacity = mac->getCurrentDatarate();

	// Beacon slots are exclusive to beacons.
	if (beacon_module.isEnabled() && beacon_module.shouldSendBeaconThisSlot()) {
		coutd << "broadcasting beacon -> ";
		// Schedule next beacon slot.
		scheduleBeacon(); // prints when the next beacon slot is scheduled
		coutd << "while non-beacon broadcast is " << (next_broadcast_scheduled ? "scheduled in " + std::to_string(next_broadcast_slot) + " slots -> " : "not scheduled -> ");

		// Generate beacon message.
		auto hostPosition = this->mac->getHostPosition();
		packet->addMessage(beacon_module.generateBeacon(reservation_manager->getP2PReservationTables(), reservation_manager->getBroadcastReservationTable(), hostPosition));
		mac->statisticReportBeaconSent();
		base_header->burst_offset = beacon_module.getNextBeaconOffset();
	// Non-beacon slots can be used for any other type of broadcast.
	} else {
		coutd << "broadcasting data -> ";
		mac->statisticReportBroadcastSent();
		mac->statisticReportBroadcastMacDelay(measureMacDelay());
		// Put a priority on link requests.
		std::vector<std::pair<L2HeaderLinkRequest*, LinkManager::LinkRequestPayload*>> requests_to_add;
		while (!link_requests.empty()) {
			// Fetch next link request.
			auto &pair = link_requests.at(0);
			// Compute payload.
			if (pair.second->callback == nullptr)
				throw std::invalid_argument("BCLinkManager::onTransmissionBurstStart has nullptr link request callback - can't populate the LinkRequest!");
			pair.second->callback->populateLinkRequest(pair.first, pair.second);
			// Add to the packet if it fits.
			if (pair.first->getBits() + pair.second->getBits() <= capacity) {
				requests_to_add.push_back(pair);
				link_requests.erase(link_requests.begin());
				capacity -= pair.first->getBits() + pair.second->getBits();
				coutd << "added link request for '" << pair.first->dest_id << "' to broadcast -> ";
				mac->statisticReportLinkRequestSent();
			} else
				break; // Stop if it doesn't fit anymore.
		}
		// Add broadcast data.
		int remaining_bits = capacity - packet->getBits() + base_header->getBits(); // The requested packet will have a base header, which we'll drop, so add it to the requested number of bits.
		if (remaining_bits > 0) {
			coutd << "adding " << remaining_bits << " bits from upper sublayer -> ";
			L2Packet* upper_layer_data = mac->requestSegment(remaining_bits, link_id);
			size_t num_bits_added = 0;
			for (size_t i = 0; i < upper_layer_data->getPayloads().size(); i++) {
				const auto &upper_layer_header = upper_layer_data->getHeaders().at(i);
				if (upper_layer_header->frame_type != L2Header::base) {
					// copy
					L2Header *header = upper_layer_header->copy();
					L2Packet::Payload *payload = upper_layer_data->getPayloads().at(i)->copy();
					// add
					packet->addMessage(header, payload);
					num_bits_added += header->getBits() + payload->getBits();
					coutd << "added '" << header->frame_type << "' message -> ";
					// report link info
					if (upper_layer_header->frame_type == L2Header::link_info) {
						mac->statisticReportLinkInfoSent();
					}
				}
			}
			coutd << "added " << num_bits_added << " bits -> ";
//			if (num_bits_added > remaining_bits) {
//				std::stringstream ss;
//				ss << *mac << "::" << *this << "::onTransmissionBurstStart error: " << num_bits_added << " bits were returned by upper sublayer instead of requested " << remaining_bits << "!";
//				throw std::runtime_error(ss.str());
//			}
			delete upper_layer_data;
		}
		for (const auto &pair : requests_to_add)
			packet->addMessage(pair.first, pair.second);
		if (packet->getLinkInfoIndex() != -1)
			((LinkInfoPayload*&) packet->getPayloads().at(packet->getLinkInfoIndex()))->populate();

		if (!always_schedule_next_slot) {
			// Schedule next broadcast if there's more data to send.
			if (!link_requests.empty() || mac->isThereMoreData(link_id)) {
				coutd << "there's more data, so scheduling next slot -> ";
				scheduleBroadcastSlot();
				coutd << next_broadcast_slot << " slots -> ";
				// Put it into the header.
				base_header->burst_offset = next_broadcast_slot;
			} else {
				next_broadcast_scheduled = false;
				next_broadcast_slot = 0;
				coutd << "no more broadcast data, not scheduling a next slot -> ";
			}
		} else {
			// Schedule next broadcast.
			coutd << "auto-schedule is on, scheduling next slot -> ";
			scheduleBroadcastSlot();
			coutd << next_broadcast_slot << " slots -> ";
			base_header->burst_offset = next_broadcast_slot;
		}
	}

	mac->statisticReportPacketSent();	
	return packet;
}

void BCLinkManager::onTransmissionBurst(unsigned int remaining_burst_length) {
	throw std::runtime_error("BCLinkManager::onTransmissionBurst, but the BCLinkManager should never have multi-slot transmissions.");
}

void BCLinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << *this << "::notifyOutgoing(" << num_bits << ") -> ";
	packet_generated_this_slot = true;
	if (!next_broadcast_scheduled) {
		coutd << "scheduling next broadcast -> ";
		scheduleBroadcastSlot();
	}
	coutd << "next broadcast scheduled in " << next_broadcast_slot << " slots -> ";
}

void BCLinkManager::onSlotStart(uint64_t num_slots) {
	// decrement next broadcast slot counter
	if (next_broadcast_scheduled) {
		if (next_broadcast_slot == 0)
			throw std::runtime_error("BCLinkManager(" + std::to_string(mac->getMacId().getId()) + ")::onSlotEnd would underflow next_broadcast_slot (was this transmission missed?)");
		next_broadcast_slot -= 1;
	} else
		next_broadcast_slot = 0;

	coutd << *mac << "::" << *this << "::onSlotStart(" << num_slots << ") -> " << (next_broadcast_scheduled ? "next broadcast in " + std::to_string(next_broadcast_slot) + " slots -> " : "");

	// broadcast link manager should always have a ReservationTable assigned
	if (current_reservation_table == nullptr)
		throw std::runtime_error("BCLinkManager::broadcastSlotSelection for unset ReservationTable.");

	// mark reception slot if there's nothing else to do
	const auto& current_reservation = current_reservation_table->getReservation(0);
	if (current_reservation.isIdle() || current_reservation.isBusy()) {
		coutd << "marking BC reception -> ";
		try {
			current_reservation_table->mark(0, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::RX));
		} catch (const std::exception& e) {
			throw std::runtime_error("BCLinkManager::onSlotStart(" + std::to_string(num_slots) + ") error trying to mark BC reception slot: " + e.what());
		}
	}
}

void BCLinkManager::onSlotEnd() {
	if (packet_generated_this_slot) {
		packet_generated_this_slot = false;
		avg_num_slots_inbetween_packet_generations.put(num_slots_since_last_packet_generation + 1);
		num_slots_since_last_packet_generation = 0;
	} else
		num_slots_since_last_packet_generation++;

	if (beacon_module.shouldSendBeaconThisSlot() || !next_beacon_scheduled) {
		// Schedule next beacon slot.
		scheduleBeacon();
	}

	// Update estimators.
	contention_estimator.onSlotEnd(mac->getCurrentSlot());
	congestion_estimator.onSlotEnd();
	beacon_module.onSlotEnd();
	mac->statisticReportCongestion(congestion_estimator.getCongestion());
	mac->statisticReportContention(contention_estimator.getAverageNonBeaconBroadcastRate());		

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
	unsigned int k;	
	// Estimate number of channel accesses from Binomial distribution.
	if (contention_method == ContentionMethod::binomial_estimate) {
		// get average broadcast rate
		double r = contention_estimator.getAverageNonBeaconBroadcastRate();
		// get no of active neighbors
		unsigned int m = contention_estimator.getNumActiveNeighbors();
		double num_candidates = 0;
		// if the broadcast rate is 100%, then assume that all m neighbors are active
		if (r == 1.0)
			num_candidates = (unsigned int) std::ceil(1.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / m)));
		// else find through Binomial distribution
		else {
			// For every number n of channel accesses from 0 to all neighbors...
			for (auto n = 0; n <= m; n++) {
				// Probability P(X=n) of n accesses.			
				double p = ((double) nchoosek(m, n)) * std::pow(r, n) * std::pow(1 - r, m - n);
				// Number of slots that should be chosen if n accesses occur (see IntAirNet Deliverable AP 2.2).
				unsigned int local_k = n == 0 ? 1 : (unsigned int) std::ceil(1.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / n)));
				num_candidates += p * local_k;
			}
		}
		k = (unsigned int) std::ceil(num_candidates);
		coutd << "channel access method: binomial estimate for " << m << " active neighbors with average broadcast rate " << r << " -> ";
	// Estimate number of channel accesses from Poisson Binomial distribution (each neighbor's channel access is an independent Bernoulli trial with its own probability)
	} else if (contention_method == ContentionMethod::poisson_binomial_estimate) {
		std::vector<MacId> active_neighbors = contention_estimator.getActiveNeighbors();
		// sum up each neighbor's channel access probability
		double expected_active_neighbors = 0.0;
		for (const MacId& id : active_neighbors)
			expected_active_neighbors += contention_estimator.getChannelAccessProbability(id, mac->getCurrentSlot());
		k = expected_active_neighbors == 0 ? 1 : (unsigned int) std::round(1.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / expected_active_neighbors)));
		coutd << "channel access method: poisson binomial estimate for " << expected_active_neighbors << " expected active neighbors (out of " << active_neighbors.size() << " recently active) with individual broadcast probabilities -> ";
	// Assume that every neighbor that has been active within the contention window will again be active.
	} else if (contention_method == ContentionMethod::all_active_again_assumption) {
		// Number of active neighbors.
		unsigned int m = contention_estimator.getNumActiveNeighbors();
		k = std::ceil(1.0 / (1.0 - std::pow(1.0 - target_collision_prob, 1.0 / m)));
		coutd << "channel access method: assume all " << m << " active neighbors are active again -> ";
	// Don't make use of contention estimation in any way. Just select something out of the next 100 idle slots.
	} else if (contention_method == ContentionMethod::naive_random_access) {
		k = 100;
		coutd << "channel access method: naive random access -> ";
	} else {
		throw std::invalid_argument("BCLinkManager::getNumCandidateSlots for unknown contention method: '" + std::to_string(contention_method) + "'.");
	}
	unsigned int final_candidates = std::max(MIN_CANDIDATES, k);
	coutd << "num_candidates=" << final_candidates << " -> ";
	return final_candidates;
}

unsigned long long BCLinkManager::nchoosek(unsigned long n, unsigned long k) const {
	if (k == 0)
		return 1;
	return (n * nchoosek(n - 1, k - 1)) / k;
}

unsigned int BCLinkManager::broadcastSlotSelection(unsigned int min_offset) {
	if (current_reservation_table == nullptr)
		throw std::runtime_error("BCLinkManager::broadcastSlotSelection for unset ReservationTable.");
	unsigned int num_candidates = getNumCandidateSlots(this->broadcast_target_collision_prob);
	mac->statisticReportBroadcastCandidateSlots((size_t) num_candidates);
	std::vector<unsigned int > candidate_slots = current_reservation_table->findCandidates(num_candidates, min_offset, 1, 1, 1, 0, false);
	if (candidate_slots.empty())
		throw std::runtime_error("BCLinkManager::broadcastSlotSelection found zero candidate slots.");
	unsigned int selected_slot = candidate_slots.at(getRandomInt(0, candidate_slots.size()));
	mac->statisticReportSelectedBroadcastCandidateSlots(selected_slot);
	return selected_slot;
}

void BCLinkManager::scheduleBroadcastSlot() {
	unscheduleBroadcastSlot();
	// By default, even the next slot could be chosen.
	unsigned int min_offset = 1;
	// Unless there's currently no data to send; then, schedule one when on average the next data packet should've been generated.
	if (!mac->isThereMoreData(link_id))
		min_offset = std::max(uint(1), getAvgNumSlotsInbetweenPacketGeneration());
	// Apply slot selection.
	next_broadcast_slot = broadcastSlotSelection(min_offset);
	next_broadcast_scheduled = true;
	current_reservation_table->mark(next_broadcast_slot, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::TX));
}

void BCLinkManager::unscheduleBroadcastSlot() {
	if (next_broadcast_slot > 0 && current_reservation_table->getReservation(next_broadcast_slot).isTx())
		current_reservation_table->mark(next_broadcast_slot, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));
}

void BCLinkManager::processBeaconMessage(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) {
	coutd << "parsing incoming beacon -> ";
	auto pair = beacon_module.parseBeacon(origin_id, (const BeaconPayload*&) payload, reservation_manager);
	if (pair.first) {
		coutd << "re-scheduling beacon from t=" << beacon_module.getNextBeaconOffset() << " to ";
		scheduleBeacon();
		coutd << "t=" << beacon_module.getNextBeaconOffset() << " -> ";
	} if (pair.second) {
		coutd << "re-scheduling broadcast from t=" << next_broadcast_slot << " to ";
		// remember current broadcast slot
		auto current_broadcast_slot = next_broadcast_slot;
		// unschedule it
		unscheduleBroadcastSlot();
		// mark it as BUSY so it won't be scheduled again
		current_reservation_table->mark(current_broadcast_slot, Reservation(origin_id, Reservation::BUSY));
		// find a new slot
		scheduleBroadcastSlot();
		coutd << "t=" << next_broadcast_slot << " -> ";
	}
	// pass it to the MAC layer
	mac->onBeaconReception(origin_id, L2HeaderBeacon(*header));
}

void BCLinkManager::processBroadcastMessage(const MacId& origin, L2HeaderBroadcast*& header) {
	mac->statisticReportBroadcastMessageDecoded();
}

void BCLinkManager::processUnicastMessage(L2HeaderUnicast*& header, L2Packet::Payload*& payload) {
	// TODO compare to local ID, discard or forward resp.
	LinkManager::processUnicastMessage(header, payload);
}

void BCLinkManager::processBaseMessage(L2HeaderBase*& header) {
	// Check indicated next broadcast slot.
	int next_broadcast = (int) header->burst_offset;
	if (next_broadcast > 0) { // If it has been set ...
		// ... check local reservation
		const Reservation& res = current_reservation_table->getReservation(next_broadcast);
		// if locally the slot is IDLE, then schedule listening to this broadcast
		if (res.isIdle()) {
			current_reservation_table->mark(next_broadcast, Reservation(header->src_id, Reservation::RX));
			coutd << "marked next broadcast in " << next_broadcast << " slots as RX -> ";
		// if locally, one's own transmission is scheduled...
		} else if (res.isTx() || res.isTxCont()) {
			coutd << "detected collision with own broadcast in " << next_broadcast << " slots -> ";
			// ... unschedule one's own transmission
			unscheduleBroadcastSlot();
			// ... mark the reception of the other broadcast
			current_reservation_table->mark(next_broadcast, Reservation(header->src_id, Reservation::RX));
			coutd << "marked next broadcast in " << next_broadcast << " slots as RX -> ";
			// ... and re-schedule one's own broadcast transmission
			scheduleBroadcastSlot();
			coutd << "re-scheduled own broadcast in " << next_broadcast_slot << " slots -> ";
		// if locally, one's own beacon is scheduled...
		} else if (res.isBeaconTx()) {
			coutd << "detected collision with own beacon in " << next_broadcast << " slots -> ";
			// ... unschedule one's own beacon
			unscheduleBeaconSlot();
			// ... mark the reception of the other broadcast
			current_reservation_table->mark(next_broadcast, Reservation(header->src_id, Reservation::RX));
			// ... and re-schedule one's own beacon
			scheduleBeacon();
			coutd << "re-scheduled own beacon in " << beacon_module.getNextBeaconOffset() << " slots -> ";
		} else {
			coutd << "indicated next broadcast in " << next_broadcast << " slots is locally reserved for " << res << " (not doing anything) -> ";
		}
	}
}

void BCLinkManager::processLinkRequestMessage(const L2Header*& header, const L2Packet::Payload*& payload, const MacId& origin) {
	MacId dest_id = ((const L2HeaderLinkRequest*&) header)->dest_id;
	if (dest_id == mac->getMacId()) {
		coutd << "forwarding link request to P2PLinkManager -> ";
		// do NOT report the received request to the MAC, as the P2PLinkManager will do that (otherwise it'll be counted twice)
		((P2PLinkManager*) mac->getLinkManager(origin))->processLinkRequestMessage(header, payload, origin);
	} else
		coutd << "discarding link request that is not destined to us -> ";
}

void BCLinkManager::processLinkReplyMessage(const L2HeaderLinkEstablishmentReply*& header, const L2Packet::Payload*& payload) {
	throw std::invalid_argument("BCLinkManager::processLinkReplyMessage called, but link replies shouldn't be received on the BC.");
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
	const MacId& id = packet->getOrigin();
	// report any activity to the MAC
	mac->reportNeighborActivity(id);
	// congestion is concerned with *any* received broadcast	
	congestion_estimator.reportBroadcast(id);		
	// contention is only concerned with non-beacon broadcasts
	if (packet->getBeaconIndex() == -1)
		contention_estimator.reportNonBeaconBroadcast(id, mac->getCurrentSlot());

	LinkManager::onPacketReception(packet);
}

void BCLinkManager::processLinkInfoMessage(const L2HeaderLinkInfo*& header, const LinkInfoPayload*& payload) {
	mac->statisticReportLinkInfoReceived();
	const LinkInfo &info = payload->getLinkInfo();
	const MacId &tx_id = info.getTxId(), &rx_id = info.getRxId();
	if (tx_id == mac->getMacId() || rx_id == mac->getMacId()) {
		coutd << "involves us; discarding -> ";
	} else {
		coutd << "passing on to " << tx_id  << " -> ";
		((P2PLinkManager*) mac->getLinkManager(tx_id))->processLinkInfoMessage(header, payload);
	}
}

void BCLinkManager::setTargetCollisionProb(double value) {
	this->broadcast_target_collision_prob = value;
}

void BCLinkManager::scheduleBeacon() {
	if (beacon_module.isEnabled()) {
		// un-schedule current beacon slot
		unscheduleBeaconSlot();
		// and schedule a new one
		int next_beacon_slot = (int) beacon_module.scheduleNextBeacon(congestion_estimator.getCongestion(), mac->getNeighborObserver().getNumActiveNeighbors(), current_reservation_table, reservation_manager->getTxTable());
		mac->statisticReportMinBeaconOffset((std::size_t) beacon_module.getBeaconOffset());
		if (!(current_reservation_table->isIdle(next_beacon_slot) || current_reservation_table->getReservation(next_beacon_slot).isBeaconTx())) {
			std::stringstream ss;
			ss << *mac << "::" << *this << "::scheduleBeacon scheduled a beacon slot at a non-idle resource: " << current_reservation_table->getReservation(next_beacon_slot) << "!";
			throw std::runtime_error(ss.str());
		}
		current_reservation_table->mark(next_beacon_slot, Reservation(SYMBOLIC_LINK_ID_BEACON, Reservation::TX_BEACON));
		next_beacon_scheduled = true;
		coutd << *mac << "::" << *this << "::scheduleBeacon scheduled next beacon slot in " << next_beacon_slot << " slots (" << beacon_module.getMinBeaconCandidateSlots() << " candidates) -> ";
		// Reset congestion estimator with new beacon interval.
		congestion_estimator.reset(beacon_module.getBeaconOffset());
	}
}

void BCLinkManager::unscheduleBeaconSlot() {
	if (beacon_module.isEnabled()) {
		if (beacon_module.getNextBeaconOffset() != 0 && next_beacon_scheduled) {
			assert(current_reservation_table != nullptr && current_reservation_table->getReservation(beacon_module.getNextBeaconOffset()) == Reservation(SYMBOLIC_LINK_ID_BEACON, Reservation::TX_BEACON));
			current_reservation_table->mark(beacon_module.getNextBeaconOffset(), Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));
		}
		next_beacon_scheduled = false;
		beacon_module.reset();
	}
}

void BCLinkManager::setMinNumCandidateSlots(int value) {
	MIN_CANDIDATES = value;
	beacon_module.setMinBeaconCandidateSlots(value);
}

void BCLinkManager::setAlwaysScheduleNextBroadcastSlot(bool value) {
	this->always_schedule_next_slot = value;
}

void BCLinkManager::setUseContentionMethod(ContentionMethod method) {
	contention_method = method;
}

unsigned int BCLinkManager::getAvgNumSlotsInbetweenPacketGeneration() const {
	return (unsigned int) std::ceil(avg_num_slots_inbetween_packet_generations.get());
}

void BCLinkManager::setMinBeaconInterval(unsigned int value) {
	this->beacon_module.setMinBeaconInterval(value);
}

void BCLinkManager::setMaxBeaconInterval(unsigned int value) {
	this->beacon_module.setMaxBeaconInterval(value);
}