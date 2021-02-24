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

		// ... if this is used for an initial link request, then we need to reserve a receiver at each start slot and mark them as RX.
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
	burst_start_during_this_slot = true;
}

void P2PLinkManager::onReceptionBurst(unsigned int remaining_burst_length) {

}

L2Packet* P2PLinkManager::onTransmissionBurstStart(unsigned int burst_length) {
	burst_start_during_this_slot = true;
	coutd << *this << "::onTransmissionBurstStart(" << burst_length << " slots) -> ";
	if (link_status == link_not_established)
		throw std::runtime_error("P2PLinkManager::onTransmissionBurst for unestablished link.");

	auto *packet = new L2Packet();
	size_t capacity = mac->getCurrentDatarate() * burst_length;
	coutd << "filling packet with a capacity of " << capacity << " bits -> ";
	// Add base header.
	auto *base_header = new L2HeaderBase(mac->getMacId(), 0, 0, 0, 0);
	packet->addMessage(base_header, nullptr);
	if (current_link_state != nullptr) {
		// Set base header fields.
		base_header->timeout = current_link_state->timeout;
		base_header->burst_length = current_link_state->burst_length;
		base_header->burst_length_tx = current_link_state->burst_length_tx;
		base_header->burst_offset = burst_offset;

		// Put a priority on control messages:
		// 1) link replies
		if (!current_link_state->scheduled_link_replies.empty()) {
			for (auto it = current_link_state->scheduled_link_replies.begin(); it != current_link_state->scheduled_link_replies.end(); it++) {
				auto &reply_reservation = *it;
				// ... if due now, ...
				if (reply_reservation.getRemainingOffset() == 0) {
					size_t num_bits = reply_reservation.getHeader()->getBits() + reply_reservation.getPayload()->getBits();
					if (packet->getBits() + num_bits <= capacity) {
						// put it into the packet,
						packet->addMessage(reply_reservation.getHeader(), reply_reservation.getPayload());
						// and remove from scheduled replies.
						current_link_state->scheduled_link_replies.erase(it);
						it--;
						coutd << "added scheduled link reply -> ";
						statistic_num_sent_replies++;
					} else // Link replies must fit into single slots & have highest priority, so they should always fit. Throw an error if the unexpected happens.
						throw std::runtime_error("P2PLinkManager::onTransmissionBurstStart can't put link reply into packet because it wouldn't fit. This should never happen?!");
				}
			}
		}
		// 2) link requests
		if (!current_link_state->scheduled_link_requests.empty()) {
			for (auto it = current_link_state->scheduled_link_requests.begin(); it != current_link_state->scheduled_link_requests.end(); it++) {
				auto &request_reservation = *it;
				// ... if due now, ...
				if (request_reservation.getRemainingOffset() == 0) {
					size_t num_bits = request_reservation.getHeader()->getBits() + request_reservation.getPayload()->getBits();
					bool renewal_required = mac->isThereMoreData(link_id);
					// ... if a renewal is required ...
					if (renewal_required) {
						// ... compute payload ...
						request_reservation.getPayload()->callback->populateLinkRequest((L2HeaderLinkRequest*&) request_reservation.getHeader(), request_reservation.getPayload());
						// ... and if it fits ...
						if (packet->getBits() + num_bits <= capacity) {
							// ... put it into the packet,
							packet->addMessage(request_reservation.getHeader(), request_reservation.getPayload());
							coutd << "added scheduled link request -> ";
							statistic_num_sent_requests++;
						} else // Link requests must fit into single slots & have highest priority, so they should always fit. Throw an error if the unexpected happens.
							throw std::runtime_error("P2PLinkManager::onTransmissionBurstStart can't put link request into packet because it wouldn't fit. This should never happen?!");
					} else
						coutd << "removing link request (no more data to send) -> ";
					// Remove from scheduled replies.
					current_link_state->scheduled_link_requests.erase(it);
					it--;
				}
			}
		}
	}
	// Fill whatever capacity remains with upper-layer data.
	unsigned int remaining_bits = capacity - packet->getBits();
	coutd << "requesting " << remaining_bits << " bits from upper sublayer -> ";
	L2Packet *upper_layer_data = mac->requestSegment(remaining_bits, link_id);
	statistic_num_sent_packets++;
	for (size_t i = 0; i < upper_layer_data->getPayloads().size(); i++)
		if (upper_layer_data->getHeaders().at(i)->frame_type != L2Header::base)
			packet->addMessage(upper_layer_data->getHeaders().at(i)->copy(), upper_layer_data->getPayloads().at(i)->copy());
	delete upper_layer_data;
	return packet;
}

void P2PLinkManager::onTransmissionBurst(unsigned int remaining_burst_length) {

}

void P2PLinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << *this << "::notifyOutgoing(" << num_bits << ") -> ";
	// Update outgoing traffic estimate.
	outgoing_traffic_estimate.put(num_bits);

	if (link_status == link_not_established) {
		coutd << "link not established, triggering link establishment -> ";
		auto link_request_msg = prepareRequestMessage(true);
		((BCLinkManager*) mac->getLinkManager(SYMBOLIC_LINK_ID_BROADCAST))->sendLinkRequest(link_request_msg.first, link_request_msg.second);
		link_status = awaiting_reply;
	} else
		coutd << "link status is '" << link_status << "'; nothing to do." << std::endl;
}

void P2PLinkManager::onSlotStart(uint64_t num_slots) {
	burst_start_during_this_slot = false;
	updated_timeout_this_slot = false;
	established_initial_link_this_slot = false;

	if (current_link_state != nullptr) {
		for (auto &reservation : current_link_state->scheduled_link_requests)
			reservation.update(num_slots);
		for (auto &reservation : current_link_state->scheduled_link_replies)
			reservation.update(num_slots);
		// Update RX reservations to listen for replies.
		for (auto it = current_link_state->scheduled_rx_slots.begin(); it != current_link_state->scheduled_rx_slots.end(); it++) {
			// If it has passed, remove it from the remembered ones.
			if (it->second < num_slots) {
				current_link_state->scheduled_rx_slots.erase(it);
				it--;
			} else {
				it->second -= num_slots;
			}
		}
	}
	if (next_link_state != nullptr) {
		for (auto &reservation : next_link_state->scheduled_link_requests)
			reservation.update(num_slots);
		for (auto &reservation : next_link_state->scheduled_link_replies)
			reservation.update(num_slots);
		// Update RX reservations to listen for replies.
		for (auto it = next_link_state->scheduled_rx_slots.begin(); it != next_link_state->scheduled_rx_slots.end(); it++) {
			// If it has passed, remove it from the remembered ones.
			if (it->second < num_slots) {
				next_link_state->scheduled_rx_slots.erase(it);
				it--;
			} else {
				it->second -= num_slots;
			}
		}
	}
}

void P2PLinkManager::onSlotEnd() {
	if (burst_start_during_this_slot) {
		coutd << *mac << "::" << *this << "::onSlotEnd -> ";
		if (decrementTimeout())
			onTimeoutExpiry();
		coutd << std::endl;
	}
}

std::pair<L2HeaderLinkRequest*, LinkManager::LinkRequestPayload*> P2PLinkManager::prepareRequestMessage(bool initial_request) {
	auto *header = new L2HeaderLinkRequest(link_id);
	auto *payload = new LinkRequestPayload();
	// Set this as the callback s.t. the payload can be populated just-in-time.
	payload->callback = this;
	payload->initial_request = initial_request;
	return {header, payload};
}

void P2PLinkManager::populateLinkRequest(L2HeaderLinkRequest*& header, LinkManager::LinkRequestPayload*& payload) {
	coutd << "populating link request -> ";
	unsigned int min_offset;
	bool initial_setup = payload->initial_request;
	if (initial_setup)
		min_offset = 2;
	else
		min_offset = current_link_state->timeout*burst_offset + current_link_state->burst_length + 1; // Right after link expiry.

	auto traffic_estimate = (unsigned int) this->outgoing_traffic_estimate.get(); // in bits.
	unsigned int datarate = mac->getCurrentDatarate(); // in bits/slot.
	unsigned int burst_length_tx = std::max(uint32_t(1), traffic_estimate / datarate); // in slots.
	unsigned int burst_length = burst_length_tx + reported_desired_tx_slots;

	coutd << "min_offset=" << min_offset << ", burst_length=" << burst_length << ", burst_length_tx=" << burst_length_tx << " -> ";
	// Populate payload.
	payload->proposed_resources = p2pSlotSelection(num_p2p_channels_to_propose, num_slots_per_p2p_channel_to_propose, min_offset, burst_length, burst_length, initial_setup);
	// Populate header.
	header->timeout = default_timeout;
	header->burst_length = burst_length;
	header->burst_length_tx = burst_length_tx;
	header->burst_offset = burst_offset;
	// Save state.
	if (initial_setup) {
		delete current_link_state;
		current_link_state = new LinkState(default_timeout, burst_length, burst_length_tx);
		current_link_state->initial_setup = true;
		// We need to schedule RX slots at each candidate to be able to receive a reply there.
		for (const auto &pair : payload->proposed_resources) {
			const FrequencyChannel *channel = pair.first;
			const std::vector<unsigned int> &burst_start_offsets = pair.second;
			ReservationTable *table = reservation_manager->getReservationTable(channel);
			for (unsigned int offset : burst_start_offsets) {
				table->mark(offset, Reservation(link_id, Reservation::RX));
				// Remember them.
				current_link_state->scheduled_rx_slots.emplace_back(channel, offset);
			}
		}
	} else {
		delete next_link_state;
		next_link_state = new LinkState(default_timeout, burst_length, burst_length_tx);
		next_link_state->initial_setup = false;
		// We need to schedule one RX slot at the next burst to be able to receive a reply there.
		int slot_offset_for_last_slot_in_next_burst = burst_offset + current_link_state->burst_length - 1;
		const Reservation last_res_in_next_burst = current_reservation_table->getReservation(slot_offset_for_last_slot_in_next_burst);
		assert(last_res_in_next_burst.getTarget() == link_id);
		if (last_res_in_next_burst.isTx() || last_res_in_next_burst.isTxCont()) // possibly it already is RX, then do nothing.
			current_reservation_table->mark(slot_offset_for_last_slot_in_next_burst, Reservation(link_id, Reservation::RX));
	}

	coutd << "request populated -> ";
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
	statistic_num_received_requests++;
	// If currently the link is unestablished, then this request must be an initial request.
	if (link_status == link_not_established) {
		// Pick a random resource from those proposed.
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
			auto link_reply_message = prepareInitialReply(origin, current_link_state->channel, current_link_state->next_burst_start, current_link_state->burst_length, current_link_state->burst_length_tx);
			current_link_state->scheduled_link_replies.emplace_back(state->next_burst_start, link_reply_message.first, link_reply_message.second);
			// mark the slot as TX,
			current_reservation_table->mark(state->next_burst_start, Reservation(origin, Reservation::TX));
			coutd << "scheduled link reply at offset " << state->next_burst_start << " -> ";
			// and anticipate first data exchange one burst later,
			coutd << "scheduling slots for first transmission burst: ";
			scheduleBurst(burst_offset + current_link_state->next_burst_start, current_link_state->burst_length, current_link_state->burst_length_tx, origin, current_reservation_table, current_link_state->is_link_initiator);
			// and update status.
			link_status = awaiting_data_tx;
		}
	// If the link is in any other status, this must be a renewal request.
	} else {
		LinkState *state = processRenewalRequest((const L2HeaderLinkRequest*&) header, (const P2PLinkManager::LinkRequestPayload*&) payload);
	}
}

std::pair<const FrequencyChannel*, unsigned int> P2PLinkManager::chooseRandomResource(const std::map<const FrequencyChannel*, std::vector<unsigned int>>& resources, unsigned int burst_length, unsigned int burst_length_tx) {
	std::vector<const FrequencyChannel*> viable_resource_channel;
	std::vector<unsigned int> viable_resource_slot;
	// For each resource...
	for (const auto &resource : resources) {
		const FrequencyChannel *channel = resource.first;
		const std::vector<unsigned int> &slots = resource.second;
		// ... get the channel's ReservationTable
		const ReservationTable *table = reservation_manager->getReservationTable(channel);
		// ... and check all proposed slot ranges, saving viable ones.
		coutd << "checking ";
		for (unsigned int slot : slots) {
			coutd << slot << "@" << *channel << " ";
			if (isViable(table, slot, burst_length, burst_length_tx)) {
				viable_resource_channel.push_back(channel);
				viable_resource_slot.push_back(slot);
				coutd << "(viable) ";
			} else
				coutd << "(busy) ";
		}
	}
	if (viable_resource_channel.empty())
		return {nullptr, 0};
	else {
		auto random_index = getRandomInt(0, viable_resource_channel.size());
		return {viable_resource_channel.at(random_index), viable_resource_slot.at(random_index)};
	}
}

P2PLinkManager::LinkState* P2PLinkManager::processInitialRequest(const L2HeaderLinkRequest*& header, const LinkManager::LinkRequestPayload*& payload) {
	coutd << "initial request -> ";
	// Parse header fields.
	auto *state = new LinkState(header->timeout, header->burst_length, header->burst_length_tx);
	// Since this user is processing the request, they have not initiated the link.
	state->is_link_initiator = false;
	state->initial_setup = true;

	// Parse proposed resources.
	auto chosen_resource = chooseRandomResource(payload->proposed_resources, header->burst_length, header->burst_length_tx);

	// If a resource was successfully chosen...
	if (chosen_resource.first != nullptr) {
		// ... save it.
		state->channel = chosen_resource.first;
		state->next_burst_start = chosen_resource.second;
		coutd << "-> randomly chose " << *state->channel << "@" << state->next_burst_start << " -> ";
	} else {
		state->channel = nullptr;
		state->next_burst_start = 0;
		coutd << "-> no viable resources -> ";
	}

	return state;
}

P2PLinkManager::LinkState* P2PLinkManager::processRenewalRequest(const L2HeaderLinkRequest*& header, const LinkManager::LinkRequestPayload*& payload) {
	// Parse header fields.
	auto *state = new LinkState(header->timeout, header->burst_length, header->burst_length_tx);
	// Since this user is processing the request, they have not initiated the link.
	state->is_link_initiator = false;
	state->initial_setup = false;
	throw std::runtime_error("not implemented");
}

void P2PLinkManager::processIncomingLinkReply(const L2HeaderLinkEstablishmentReply*& header, const L2Packet::Payload*& payload) {
	coutd << *this << "::processIncomingLinkReply -> ";
	statistic_num_received_replies++;
	if (link_status != awaiting_reply) {
		coutd << "not awaiting reply; discarding -> ";
		return;
	}
	assert(current_link_state != nullptr && "P2PLinkManager::processIncomingLinkReply for unset current state.");

	if (current_link_state->initial_setup) {
		processInitialReply((const L2HeaderLinkReply*&) header, (const LinkManager::LinkRequestPayload*&) payload);
	} else
		throw std::runtime_error("not implemented");
}

void P2PLinkManager::processInitialReply(const L2HeaderLinkReply*& header, const LinkManager::LinkRequestPayload*& payload) {
	coutd << "initial reply -> ";
	// Reset timeout.
	current_link_state->timeout = default_timeout;
	// Parse resource.
	if (payload->proposed_resources.size() != 1)
		throw std::invalid_argument("P2PLinkManager::processInitialReply for payload with " + std::to_string(payload->proposed_resources.size()) + " resources.");
	const auto &resource = *payload->proposed_resources.begin();
	const auto *channel = resource.first;
	const auto &slots = resource.second;
	if (slots.size() != 1)
		throw std::invalid_argument("P2PLinkManager::processInitialReply for " + std::to_string(slots.size()) + " slots.");
	unsigned int slot_offset = slots.at(0);
	coutd << "received on " << *channel << "@" << slot_offset << " -> ";
	// Assign channel.
	assign(channel);
	// Make reservations.
	coutd << "scheduling transmission bursts: ";
	for (unsigned int burst = 1; burst < default_timeout + 1; burst++)  // Start with next P2P frame
		scheduleBurst(burst * burst_offset + slot_offset, current_link_state->burst_length, current_link_state->burst_length_tx, link_id, current_reservation_table, true);
	// Clear RX reservations made to receive this reply.
	for (auto &pair : current_link_state->scheduled_rx_slots) {
		ReservationTable *table = reservation_manager->getReservationTable(pair.first);
		table->mark(pair.second, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));
	}
	current_link_state->scheduled_rx_slots.clear();
	// Schedule link renewal request slots.
	coutd << "scheduling link renewal request slots: ";
	std::vector<unsigned int> link_renewal_request_slots = scheduleRenewalRequestSlots(default_timeout, burst_offset, burst_offset, num_renewal_attempst);
	for (unsigned int renewal_request_slot : link_renewal_request_slots) {
		auto request_msg = prepareRequestMessage(false);
		current_link_state->scheduled_link_requests.emplace_back(renewal_request_slot, request_msg.first, request_msg.second);
	}
	// Link is now established.
	coutd << "setting link status to '";
	link_status = link_established;
	established_initial_link_this_slot = true;
	coutd << link_status << "' -> ";
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
		Reservation res = Reservation(dest_id, action);
		table->mark(burst_start_offset + t, res);
		coutd << "t=" << burst_start_offset + t << ":" << res << " ";
	}
	unsigned int burst_length_rx = burst_length - burst_length_tx;
	for (unsigned int t = 0; t < burst_length_rx; t++) {
		Reservation::Action action = t==0 ? (link_initiator ? Reservation::Action::RX : Reservation::Action::TX) : (link_initiator ? Reservation::Action::RX_CONT : Reservation::Action::TX_CONT);
		Reservation res = Reservation(dest_id, action);
		table->mark(burst_start_offset + burst_length_tx + t, res);
		coutd << "t=" << burst_start_offset + burst_length_tx + t << ":" << res << " ";
	}
	coutd << "-> ";
}

std::vector<unsigned int> P2PLinkManager::scheduleRenewalRequestSlots(unsigned int timeout, unsigned int init_offset, unsigned int burst_offset, unsigned int num_attempts) const {
	std::vector<unsigned int> slots;
	// For each transmission burst from last to first according to this reservation...
	for (long i = 0, offset = init_offset + (timeout - 1) * burst_offset; slots.size() < num_attempts && offset >= init_offset; offset -= burst_offset, i++) {
		// ... add every second burst
		if (i % 2 == 1) {
			slots.push_back(offset);
			coutd << "@" << offset << " ";
		}
	}
	coutd << "-> ";
	return slots;
}

void P2PLinkManager::processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) {
	throw std::invalid_argument("P2PLinkManager::processIncomingBeacon called but beacons should not be received on P2P channels.");
}

void P2PLinkManager::processIncomingBroadcast(const MacId& origin, L2HeaderBroadcast*& header) {
	throw std::invalid_argument("P2PLinkManager::processIncomingBroadcast called but broadcasts should not be received on P2P channels.");
}

void P2PLinkManager::processIncomingUnicast(L2HeaderUnicast*& header, L2Packet::Payload*& payload) {
	MacId dest_id = header->dest_id;
	if (dest_id != mac->getMacId()) {
		coutd << "discarding unicast message not intended for us -> ";
		return;
	} else {
		if (link_status == awaiting_data_tx) {
			// Link is now established.
			link_status = link_established;
			coutd << "this transmission establishes the link, setting status to '" << link_status << "' -> informing upper layers -> ";
			// Inform upper sublayers.
			mac->notifyAboutNewLink(link_id);
			// Mark reservations.
			coutd << "reserving bursts: ";
			assert(current_link_state != nullptr);
			for (unsigned int burst = 1; burst < current_link_state->timeout; burst++)
				scheduleBurst(burst*burst_offset, current_link_state->burst_length, current_link_state->burst_length_tx, link_id, current_reservation_table, current_link_state->is_link_initiator);
		}
	}
}

void P2PLinkManager::processIncomingBase(L2HeaderBase*& header) {
	// Nothing to do.
}

bool P2PLinkManager::decrementTimeout() {
	// Don't decrement timeout if,
	// (1) the link is not established right now
	if (link_status == LinkManager::link_not_established || current_link_state == nullptr) {
		coutd << "link not established; not decrementing timeout -> ";
		return false;
	}
	// (2) we are in the process of initial establishment.
	if (current_link_state->initial_setup && (link_status == LinkManager::awaiting_reply || link_status == LinkManager::awaiting_data_tx)) {
		coutd << "link being established; not decrementing timeout -> ";
		return false;
	}
	// (3) it has already been updated this slot.
	if (updated_timeout_this_slot) {
		coutd << "already decremented timeout this slot; not decrementing timeout -> ";
		return current_link_state->timeout == 0;
	}
	// (4) the link was just now established.
	if (established_initial_link_this_slot) {
		coutd << "link was established in this slot; not decrementing timeout -> ";
		return current_link_state->timeout == 0;
	}

	updated_timeout_this_slot = true;

	if (current_link_state->timeout == 0)
		throw std::runtime_error("P2PLinkManager::decrementTimeout attempted to decrement timeout past zero.");
	coutd << "timeout " << current_link_state->timeout << "->";
	current_link_state->timeout--;
	coutd << current_link_state->timeout << " -> ";
	return current_link_state->timeout == 0;
}

void P2PLinkManager::onTimeoutExpiry() {
	coutd << "timeout reached -> ";
	if (link_status == LinkManager::link_renewal_complete) {
		throw std::runtime_error("link renewal not yet implemented");
//		coutd << "applying renewal: " << *owner->current_channel << "->" << *next_channel;
//		owner->reassign(next_channel);
//		// Only schedule request slots if we're the initiator, i.e. have sent requests before.
//		if (owner->is_link_initiator) {
//			coutd << "scheduling renewal requests at ";
//			scheduled_requests = scheduleRequests(tx_timeout, first_slot_of_next_link, tx_offset, max_num_renewal_attempts);
//			first_slot_of_next_link = 0;
//		}
//		coutd << "updating status: " << owner->link_status << "->" << OldLinkManager::link_established << " -> link renewal complete -> ";
//		owner->link_status = OldLinkManager::link_established;
	} else {
		coutd << "no pending renewal, updating status: " << link_status << "->" << LinkManager::link_not_established << " -> cleared associated channel -> ";
		current_channel = nullptr;
		current_reservation_table = nullptr;
		link_status = LinkManager::link_not_established;
		coutd << "clearing pending RX reservations: ";
		for (auto &pair : current_link_state->scheduled_rx_slots) {
			ReservationTable *table = reservation_manager->getReservationTable(pair.first);
			table->mark(pair.second, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));
			coutd << pair.second << "@" << *pair.first << " ";
		}
		delete current_link_state;
		current_link_state = nullptr;
		assert(next_link_state == nullptr);
		coutd << "-> link reset -> ";
	}
}
