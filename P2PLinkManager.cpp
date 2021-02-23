//
// Created by seba on 2/18/21.
//

#include <set>
#include <cassert>
#include "P2PLinkManager.hpp"
#include "coutdebug.hpp"
#include "BCLinkManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

P2PLinkManager::P2PLinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac, unsigned int default_timeout, unsigned int burst_offset)
	: LinkManager(link_id, reservation_manager, mac), default_timeout(default_timeout), burst_offset(burst_offset), outgoing_traffic_estimate(burst_offset) {}

P2PLinkManager::~P2PLinkManager() {
	delete current_link_state;
	delete next_link_state;
}

std::map<const FrequencyChannel*, std::vector<unsigned int>> P2PLinkManager::p2pSlotSelection(unsigned int num_channels, unsigned int num_slots, unsigned int min_offset, unsigned int burst_length, unsigned int burst_length_tx, bool is_init) {
	auto proposal_map = std::map<const FrequencyChannel*, std::vector<unsigned int>>();
	// ... get the P2P reservation tables sorted by their numbers of idle slots ...
	auto table_priority_queue = reservation_manager->getSortedP2PReservationTables();
	// ... until we have considered the target number of channels ...
	coutd << "p2pSlotSelection to reserve " << burst_length << " slots -> ";
	for (size_t num_channels_considered = 0; num_channels_considered < num_channels; num_channels_considered++) {
		if (table_priority_queue.empty())
			break;
		// ... get the next reservation table ...
		ReservationTable* table = table_priority_queue.top();
		table_priority_queue.pop();
		// ... check if the channel is blocked ...
		if (table->getLinkedChannel()->isBlocked()) {
			num_channels_considered--;
			continue;
		}
		// ... and try to find candidate slots,
		std::vector<unsigned int> candidate_slots = table->findCandidates(num_slots, min_offset, burst_length, burst_length_tx, is_init);
		coutd << "found " << candidate_slots.size() << " slots on " << *table->getLinkedChannel() << ": ";
		for (int32_t slot : candidate_slots)
			coutd << slot << ":" << slot + burst_length - 1 << " ";
		coutd << " -> ";

		// ... if this is used for an initial link request, then we need to reserve a receiver at each start slot.
		if (is_init) {
			for (unsigned int offset : candidate_slots) {
				bool could_lock_receiver = false;
				for (auto* rx_table : rx_tables)
					if (rx_table->canLock(offset)) {
						rx_table->lock(offset);
						could_lock_receiver = true;
						break;
					}
				if (!could_lock_receiver)
					throw std::range_error("P2PLinkManager::p2pSlotSelection cannot reserve any receiver for first slot of burst.");
			}
		}
		// ... and lock them s.t. future proposals don't consider them.
		lock(candidate_slots, burst_length, burst_length_tx, table);
		coutd << "locked -> ";

		// Fill proposal.
		for (unsigned int slot : candidate_slots)
			proposal_map[table->getLinkedChannel()].push_back(slot);
	}
	return proposal_map;
}

void P2PLinkManager::onReceptionBurstStart(unsigned int burst_length) {

}

void P2PLinkManager::onReceptionBurst(unsigned int remaining_burst_length) {

}

L2Packet* P2PLinkManager::onTransmissionBurstStart(unsigned int burst_length) {
	if (link_status == link_not_established)
		throw std::runtime_error("P2PLinkManager::onTransmissionBurst for unestablished link.");

	auto *packet = new L2Packet();
	// Add base header.
	auto *base_header = new L2HeaderBase(mac->getMacId(), 0, 0, 0, 0);
	packet->addMessage(base_header, nullptr);
	if (current_link_state != nullptr) {
		// Set base header fields.
		base_header->timeout = current_link_state->timeout;
		base_header->burst_length = current_link_state->burst_length;
		base_header->burst_length_tx = current_link_state->burst_length_tx;
		base_header->burst_offset = burst_offset;

		if (!current_link_state->scheduled_link_replies.empty()) {
			for (const auto &reply_reservation : current_link_state->scheduled_link_replies) {

			}
		}
	}
	return nullptr;
}

void P2PLinkManager::onTransmissionBurst(unsigned int remaining_burst_length) {

}

void P2PLinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << *this << "::notifyOutgoing(" << num_bits << ") -> ";
	// Update outgoing traffic estimate.
	outgoing_traffic_estimate.put(num_bits);

	if (link_status == link_not_established) {
		coutd << "link not established, triggering link establishment -> ";
		auto link_request_msg = prepareInitialRequest();
		((BCLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->sendLinkRequest(link_request_msg.first, link_request_msg.second);
		link_status = awaiting_reply;
	} else
		coutd << "link status is '" << link_status << "'; nothing to do." << std::endl;
}

void P2PLinkManager::onSlotStart(uint64_t num_slots) {
	if (current_link_state != nullptr) {
		for (auto& reservation : current_link_state->scheduled_link_requests)
			reservation.update(num_slots);
		for (auto& reservation : current_link_state->scheduled_link_replies)
			reservation.update(num_slots);
	}
	if (next_link_state != nullptr) {
		for (auto& reservation : next_link_state->scheduled_link_requests)
			reservation.update(num_slots);
		for (auto& reservation : next_link_state->scheduled_link_replies)
			reservation.update(num_slots);
	}
}

void P2PLinkManager::onSlotEnd() {

}

std::pair<L2HeaderLinkRequest*, LinkManager::LinkRequestPayload*> P2PLinkManager::prepareInitialRequest() {
	auto *header = new L2HeaderLinkRequest(link_id);
	auto *payload = new LinkRequestPayload();
	// Set this as the callback s.t. the payload can be populated just-in-time.
	payload->callback = this;
	payload->initial_request = true;
	return {header, payload};
}

void P2PLinkManager::populateLinkRequest(L2HeaderLinkRequest*& header, LinkManager::LinkRequestPayload*& payload) {
	coutd << "populating link request -> ";
	unsigned int min_offset;
	if (payload->initial_request)
		min_offset = 2;
	else
		throw std::runtime_error("not implemented");

	auto traffic_estimate = (unsigned int) this->outgoing_traffic_estimate.get(); // in bits.
	unsigned int datarate = mac->getCurrentDatarate(); // in bits/slot.
	unsigned int burst_length = std::max(uint32_t(1), traffic_estimate / datarate); // in slots.

	unsigned int burst_length_tx = burst_length;

	coutd << "min_offset=" << min_offset << ", burst_length=" << burst_length << ", burst_length_tx=" << burst_length_tx << " -> ";
	// Populate payload.
	payload->proposed_resources = p2pSlotSelection(num_p2p_channels_to_propose, num_slots_per_p2p_channel_to_propose, min_offset, burst_length, burst_length, payload->initial_request);
	// Populate header.
	header->timeout = default_timeout;
	header->burst_length = burst_length;
	header->burst_length_tx = burst_length_tx;
	header->burst_offset = burst_offset;

	coutd << "request populated -> ";
}

P2PLinkManager::LinkState* P2PLinkManager::processInitialRequest(const L2HeaderLinkRequest*& header, const LinkManager::LinkRequestPayload*& payload) {
	coutd << *this << "::processInitialRequest -> ";
	// Parse header fields.
	auto *state = new LinkState(header->timeout, header->burst_length, header->burst_length_tx);
	// Since this user is processing the request, they have not initiated the link.
	state->initiated_link = false;

	// Parse proposed resources.
	const auto &proposal = payload->proposed_resources;
	std::vector<const FrequencyChannel*> viable_resource_channel;
	std::vector<unsigned int> viable_resource_slot;
	// For each resource...
	for (const auto &resource : proposal) {
		const FrequencyChannel *channel = resource.first;
		const std::vector<unsigned int> &slots = resource.second;
		// ... get the channel's ReservationTable
		const ReservationTable *table = reservation_manager->getReservationTable(channel);
		// ... and check all proposed slot ranges, saving viable ones.
		coutd << "checking ";
		for (unsigned int slot : slots) {
			coutd << slot << "@" << *channel << " ";
			if (isViable(table, slot, header->burst_length, header->burst_length_tx)) {
				viable_resource_channel.push_back(channel);
				viable_resource_slot.push_back(slot);
				coutd << "(viable) ";
			} else
				coutd << "(busy) ";
		}
	}
	// Draw one resource from the viable ones randomly.
	if (!viable_resource_channel.empty()) {
		auto random_index = getRandomInt(0, viable_resource_channel.size());
		state->channel = viable_resource_channel.at(random_index);
		state->slot_offset = viable_resource_slot.at(random_index);
		coutd << "-> randomly chose " << *state->channel << "@" << state->slot_offset << " -> ";
	} else {
		state->channel = nullptr;
		state->slot_offset = 0;
		coutd << "-> no viable resources -> ";
	}

	return state;
}

bool P2PLinkManager::isViable(const ReservationTable* table, unsigned int burst_start, unsigned int burst_length, unsigned int burst_length_tx) const {
	// Entire slot range must be idle.
	bool viable = table->isIdle(burst_start, burst_length);
	// A receiver must be idle during the first slots.
	if (viable)
		viable = mac->isAnyReceiverIdle(burst_start, burst_length_tx);
	// And a transmitter during the latter slots.
	if (viable) {
		unsigned int burst_length_rx = burst_length - burst_length_tx;
		viable = mac->isTransmitterIdle(burst_start + burst_length_tx, burst_length_rx);
	}
	return viable;
}

void P2PLinkManager::processIncomingLinkRequest(const L2Header*& header, const L2Packet::Payload*& payload, const MacId& origin) {
	coutd << *this << "::processIncomingLinkRequest -> ";
	// If currently the link is unestablished, then this request must be an initial request.
	if (link_status == link_not_established) {
		LinkState *state = processInitialRequest((const L2HeaderLinkRequest*&) header, (const P2PLinkManager::LinkRequestPayload*&) payload);
		// If no viable resources were found, ...
		if (state->channel == nullptr) {
			// do nothing.
			delete state;
			coutd << "no viables resources; aborting." << std::endl;
		// If one was picked, then ...
		} else {
			// remember the choice,
			current_link_state = state;
			current_channel = current_link_state->channel;
			current_reservation_table = reservation_manager->getReservationTable(current_channel);
			// schedule a link reply,
			auto link_reply_message = prepareInitialReply(origin, current_link_state->channel, current_link_state->slot_offset, current_link_state->burst_length, current_link_state->burst_length_tx);
			current_link_state->scheduled_link_replies.emplace_back(state->slot_offset, link_reply_message.first, link_reply_message.second);
			// mark the slot as TX,
			current_reservation_table->mark(state->slot_offset, Reservation(origin, Reservation::TX));
			// and anticipate first data exchange one burst later,
			scheduleBurst(burst_offset + current_link_state->slot_offset, current_link_state->burst_length, current_link_state->burst_length_tx, origin, current_reservation_table, current_link_state->initiated_link);
			// and update the status.
			link_status = awaiting_data_tx;
		}
	// If the link is in any other status, this must be a renewal request.
	} else {
		throw std::runtime_error("Renewal request handling not yet implemented.");
	}
}

std::pair<L2HeaderLinkReply*, LinkManager::LinkRequestPayload*> P2PLinkManager::prepareInitialReply(const MacId& dest_id, const FrequencyChannel *channel, unsigned int slot_offset, unsigned int burst_length, unsigned int burst_length_tx) const {
	// The reply header just mirrors the request header values.
	auto *header = new L2HeaderLinkReply(dest_id);
	header->timeout = default_timeout;
	header->burst_offset = burst_offset;
	header->burst_length = burst_length;
	header->burst_length_tx = burst_length_tx;
	// The reply payload encodes the single, chosen resource.
	auto *payload = new LinkRequestPayload();
	payload->proposed_resources[channel].push_back(slot_offset);
	return {header, payload};
}

void P2PLinkManager::scheduleBurst(unsigned int burst_start_offset, unsigned int burst_length, unsigned int burst_length_tx, const MacId &dest_id, ReservationTable* table, bool link_initiator) {
	assert(table != nullptr);
	for (unsigned int t = 0; t < burst_length_tx; t++) {
		Reservation::Action action = t==0 ? (link_initiator ? Reservation::Action::TX : Reservation::Action::RX) : (link_initiator ? Reservation::Action::TX_CONT : Reservation::Action::RX_CONT);
		table->mark(burst_start_offset + t, Reservation(dest_id, action));
	}
	unsigned int burst_length_rx = burst_length - burst_length_tx;
	for (unsigned int t = 0; t < burst_length_rx; t++) {
		Reservation::Action action = t==0 ? (link_initiator ? Reservation::Action::RX : Reservation::Action::TX) : (link_initiator ? Reservation::Action::RX_CONT : Reservation::Action::TX_CONT);
		table->mark(burst_start_offset + burst_length_tx + t, Reservation(dest_id, action));
	}
}
