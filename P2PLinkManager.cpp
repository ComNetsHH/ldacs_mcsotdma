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

std::pair<std::map<const FrequencyChannel*, std::vector<unsigned int>>, std::map<const FrequencyChannel*, std::vector<unsigned int>>> P2PLinkManager::p2pSlotSelection(unsigned int num_channels, unsigned int num_slots, unsigned int min_offset, unsigned int burst_length, unsigned int burst_length_tx, bool is_init) {
	auto proposal_map = std::map<const FrequencyChannel*, std::vector<unsigned int>>();
	auto locked_map = std::map<const FrequencyChannel*, std::vector<unsigned int>>();
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
		std::vector<unsigned int> locked_offsets = lock(candidate_slots, burst_length, burst_length_tx, table);
		for (unsigned int slot : locked_offsets)
			locked_map[table->getLinkedChannel()].push_back(slot);
		coutd << "locked -> ";

		// Fill proposal.
		for (unsigned int slot : candidate_slots)
			proposal_map[table->getLinkedChannel()].push_back(slot);
	}
	return {proposal_map, locked_map};
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
	coutd << "added " << base_header->getBits() << "-bit base header -> ";
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
						coutd << "added " << reply_reservation.getHeader()->getBits() + reply_reservation.getPayload()->getBits() << "-bit scheduled link reply to renew on " << *reply_reservation.getPayload()->proposed_resources.begin()->first << "@" << reply_reservation.getPayload()->proposed_resources.begin()->second.at(0) << " -> ";
						if (next_link_state != nullptr)
							coutd << "my belief is " << next_link_state->next_burst_start << " -> ";
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
						current_link_state->renewal_due = true;
						// ... clear previously locked resources
						if (current_link_state->last_proposed_renewal_resources != nullptr) {
							coutd << "clear locked resources: ";
							clearLockedResources(current_link_state->last_proposed_renewal_resources, current_link_state->last_proposal_sent);
							delete current_link_state->last_proposed_renewal_resources;
							coutd << "-> ";
						}
						// ... compute payload ...
						request_reservation.getPayload()->callback->populateLinkRequest((L2HeaderLinkRequest*&) request_reservation.getHeader(), request_reservation.getPayload());
						// ... remember it ...
						current_link_state->last_proposed_renewal_resources = (LinkRequestPayload*) request_reservation.getPayload()->copy();
						current_link_state->last_proposal_sent = 0;
						// ... and if it fits ...
						if (packet->getBits() + num_bits <= capacity) {
							// ... put it into the packet,
							packet->addMessage(request_reservation.getHeader(), request_reservation.getPayload());
							coutd << "added scheduled link request -> ";
							statistic_num_sent_requests++;
							link_status = awaiting_reply;
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
	unsigned int remaining_bits = capacity - packet->getBits() + base_header->getBits(); // The requested packet will have a base header, which we'll drop, so add it to the requested number of bits.
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
	coutd << *this << "::onSlotStart(" << num_slots << ") -> ";
	burst_start_during_this_slot = false;
	updated_timeout_this_slot = false;
	established_initial_link_this_slot = false;

	// TODO properly test this (not sure if incrementing time by this many slots works as intended right now)
	if (num_slots > burst_offset) {
		std::cerr << "incrementing time by this many slots is untested; I'm not stopping, just warning." << std::endl;
		int num_passed_bursts = num_slots / burst_offset;
		for (int i = 0; i < num_passed_bursts; i++) {
			if (decrementTimeout())
				onTimeoutExpiry();
			if (i < num_passed_bursts - 1)
				updated_timeout_this_slot = false;
		}
	}

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
		if (current_link_state->last_proposed_renewal_resources != nullptr)
			current_link_state->last_proposal_sent += num_slots;
		if (current_link_state->next_burst_start > 0)
			current_link_state->next_burst_start -= num_slots % burst_offset;

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
		if (next_link_state->last_proposed_renewal_resources != nullptr)
			next_link_state->last_proposal_sent += num_slots;
		if (next_link_state->next_burst_start > 0)
			next_link_state->next_burst_start -= num_slots % burst_offset;

	}
}

void P2PLinkManager::onSlotEnd() {
	if (burst_start_during_this_slot) {
		coutd << *mac << "::" << *this << "::onSlotEnd -> ";
		if (decrementTimeout())
			onTimeoutExpiry();
		coutd << std::endl;
	} else
		coutd << *mac << "::" << *this << "::onSlotEnd NOT UPDATING";
	if (current_link_state != nullptr) {
		if (current_link_state->next_burst_start == 0)
			current_link_state->next_burst_start = burst_offset;
	}
	if (next_link_state != nullptr) {
		if (next_link_state->next_burst_start == 0)
			next_link_state->next_burst_start = burst_offset;
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
		min_offset = getExpiryOffset() + 1; // Right after link expiry.

	unsigned int burst_length_tx = estimateCurrentNumSlots(); // in slots.
	unsigned int burst_length = burst_length_tx + reported_desired_tx_slots;

	coutd << "min_offset=" << min_offset << ", burst_length=" << burst_length << ", burst_length_tx=" << burst_length_tx << " -> ";
	// Populate payload.
	const auto &proposed_locked_pair = p2pSlotSelection(num_p2p_channels_to_propose, num_slots_per_p2p_channel_to_propose, min_offset, burst_length, burst_length_tx, initial_setup);
	payload->proposed_resources = proposed_locked_pair.first;
	payload->locked_resources = proposed_locked_pair.second;
	// Populate header.
	header->timeout = default_timeout;
	header->burst_length = burst_length;
	header->burst_length_tx = burst_length_tx;
	header->burst_offset = burst_offset;
	// Save state.
	if (initial_setup) {
		delete current_link_state;
		current_link_state = new LinkState(default_timeout, burst_length, burst_length_tx);
		current_link_state->is_link_initiator = true;
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
		next_link_state->is_link_initiator = true;
		// We need to schedule one RX slot at the next burst to be able to receive a reply there.
		int slot_offset_for_last_slot_in_next_burst = burst_offset + current_link_state->burst_length - 1;
		const Reservation last_res_in_next_burst = current_reservation_table->getReservation(slot_offset_for_last_slot_in_next_burst);
		assert(last_res_in_next_burst.getTarget() == link_id);
		if (last_res_in_next_burst.isTx() || last_res_in_next_burst.isTxCont()) { // possibly it already is RX, then do nothing.
			const Reservation& overwrite_res = current_reservation_table->getReservation(slot_offset_for_last_slot_in_next_burst);
			coutd << "using the t=" << slot_offset_for_last_slot_in_next_burst << ":" <<  overwrite_res << " for a RX slot -> ";
			current_reservation_table->mark(slot_offset_for_last_slot_in_next_burst, Reservation(link_id, Reservation::RX));
		}
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
		coutd << "initial request -> ";
		// Pick a random resource from those proposed.
		LinkState *state = processRequest((const L2HeaderLinkRequest*&) header, (const P2PLinkManager::LinkRequestPayload*&) payload);
		state->initial_setup = true;
		// If no viable resources were found, ...
		if (state->channel == nullptr) {
			// do nothing.
			delete state;
			coutd << "no viables resources; aborting -> ";
		// If one was picked, then ...
		} else {
			// remember the choice,
			delete current_link_state;
			current_link_state = state;
			current_channel = current_link_state->channel;
			current_reservation_table = reservation_manager->getReservationTable(current_channel);
			coutd << "randomly chose " << *current_channel << "@" << current_link_state->next_burst_start << " -> ";
			// schedule a link reply,
			auto link_reply_message = prepareReply(origin, current_link_state->channel, current_link_state->next_burst_start, current_link_state->burst_length, current_link_state->burst_length_tx);
			current_link_state->scheduled_link_replies.emplace_back(state->next_burst_start, link_reply_message.first, link_reply_message.second);
			// mark the slot as TX,
			current_reservation_table->mark(state->next_burst_start, Reservation(origin, Reservation::TX));
			coutd << "scheduled link reply at offset " << state->next_burst_start << " -> ";
			// and anticipate first data exchange one burst later,
			coutd << "scheduling slots for first transmission burst: ";
			scheduleBurst(burst_offset + current_link_state->next_burst_start, current_link_state->burst_length, current_link_state->burst_length_tx, origin, current_reservation_table, current_link_state->is_link_initiator);
			// and update status.
			coutd << "changing status " << link_status << "->" << awaiting_data_tx << " -> ";
			link_status = awaiting_data_tx;
		}
	// If the link is of any other status, this must be a renewal request.
	} else {
		coutd << "renewal request -> ";
		current_link_state->renewal_due = true;
		LinkState *state = processRequest((const L2HeaderLinkRequest*&) header, (const P2PLinkManager::LinkRequestPayload*&) payload);
		state->initial_setup = false;
		// If no viable resources were found, ...
		if (state->channel == nullptr) {
			// do nothing.
			delete state;
			coutd << "no viables resources; aborting -> ";
		// If one was picked, then ...
		} else {
			// if an earlier link had been agreed upon, free its resources
			if (next_link_state != nullptr) {
				coutd << "clearing earlier-made slot reservations: ";
				ReservationTable *table = reservation_manager->getReservationTable(next_link_state->channel);
				for (unsigned int burst = 0; burst < default_timeout; burst++) {
					for (unsigned t = 0; t < next_link_state->burst_length; t++) {
						unsigned int offset = next_link_state->next_burst_start + burst*burst_offset + t;
						const Reservation &res = table->getReservation(offset);
						if (res.getTarget() == link_id) {
							coutd << "t=" << offset << ":" << res << " ";
							table->mark(offset, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));
						}
					}
				}
			}
			// remember the choice,
			delete next_link_state;
			next_link_state = state;
			coutd << "randomly chose " << *next_link_state->channel << "@" << next_link_state->next_burst_start << " -> ";
			// schedule a link reply,
			auto link_reply_message = prepareReply(origin, next_link_state->channel, next_link_state->next_burst_start, next_link_state->burst_length, next_link_state->burst_length_tx);
			// during the next transmission burst,
			// if there are no slots scheduled for this user's transmission, ...
			if (current_link_state->burst_length_tx == current_link_state->burst_length) {
				// ... then for once schedule the last slot
				int last_slot_next_burst = burst_offset + current_link_state->burst_length - 1;
				coutd << "scheduling link reply at last slot of next burst (hijacking the t=" << last_slot_next_burst << ":" << current_reservation_table->getReservation(last_slot_next_burst) << " slot for a TX slot) -> ";
				assert(current_reservation_table->getReservation(last_slot_next_burst).isRx() || current_reservation_table->getReservation(last_slot_next_burst).isRxCont());
				current_reservation_table->mark(last_slot_next_burst, Reservation(link_id, Reservation::TX));
				current_link_state->scheduled_link_replies.emplace_back(last_slot_next_burst, link_reply_message.first, link_reply_message.second);
			// if there *are* slots scheduled for this user's transmission, ...
			} else {
				// ... then schedule the reply there
				coutd << "scheduling link reply for next burst -> ";
				int tx_slot_next_burst = burst_offset + current_link_state->burst_length_tx;
				assert(current_reservation_table->getReservation(tx_slot_next_burst).isTx());
				current_link_state->scheduled_link_replies.emplace_back(tx_slot_next_burst, link_reply_message.first, link_reply_message.second);
			}
			// Mark slots of the new link.
			ReservationTable *table = reservation_manager->getReservationTable(next_link_state->channel);
			coutd << "-> mark new slot reservations: ";
			for (unsigned int t = 0; t < default_timeout; t++)
				scheduleBurst(next_link_state->next_burst_start + t*burst_offset, next_link_state->burst_length, next_link_state->burst_length_tx, link_id, table, false);
			// and update status.
			coutd << "changing status " << link_status << "->" << link_renewal_complete << " -> ";
			link_status = link_renewal_complete;
		}
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

P2PLinkManager::LinkState* P2PLinkManager::processRequest(const L2HeaderLinkRequest*& header, const LinkManager::LinkRequestPayload*& payload) {
	// Parse header fields.
	auto *state = new LinkState(header->timeout, header->burst_length, header->burst_length_tx);
	// Since this user is processing the request, they have not initiated the link.
	state->is_link_initiator = false;

	// Parse proposed resources.
	auto chosen_resource = chooseRandomResource(payload->proposed_resources, header->burst_length, header->burst_length_tx);
	// ... and save it.
	state->channel = chosen_resource.first;
	state->next_burst_start = chosen_resource.second;
	return state;
}

void P2PLinkManager::processIncomingLinkReply(const L2HeaderLinkEstablishmentReply*& header, const L2Packet::Payload*& payload) {
	coutd << *this << "::processIncomingLinkReply -> ";
	statistic_num_received_replies++;
	if (link_status != awaiting_reply) {
		coutd << "not awaiting reply; discarding -> ";
		return;
	}
	assert(current_link_state != nullptr && "P2PLinkManager::processIncomingLinkReply for unset current state.");

	if (!current_link_state->renewal_due) {
		processInitialReply((const L2HeaderLinkReply*&) header, (const LinkManager::LinkRequestPayload*&) payload);
	} else
		processRenewalReply((const L2HeaderLinkReply*&) header, (const LinkManager::LinkRequestPayload*&) payload);
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
	std::vector<unsigned int> link_renewal_request_slots = scheduleRenewalRequestSlots(default_timeout, burst_offset, burst_offset, num_renewal_attempts);
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

void P2PLinkManager::processRenewalReply(const L2HeaderLinkReply*& header, const LinkManager::LinkRequestPayload*& payload) {
	coutd << "renewal reply -> ";
	// Parse resource.
	if (payload->proposed_resources.size() != 1)
		throw std::invalid_argument("P2PLinkManager::processRenewalReply for payload with " + std::to_string(payload->proposed_resources.size()) + " resources.");
	const auto &resource = *payload->proposed_resources.begin();
	const auto *channel = resource.first;
	const auto &slots = resource.second;
	if (slots.size() != 1)
		throw std::invalid_argument("P2PLinkManager::processRenewalReply for " + std::to_string(slots.size()) + " slots.");
	unsigned int slot_offset = slots.at(0);
	// Remember choice.
	assert(next_link_state != nullptr);
	// if an earlier link had been agreed upon, free its resources
	ReservationTable* table = reservation_manager->getReservationTable(channel);
	if (link_status == link_renewal_complete) {
		coutd << "clearing earlier-made slot reservations: ";
		for (unsigned int burst = 0; burst < default_timeout; burst++) {
			for (unsigned t = 0; t < next_link_state->burst_length; t++) {
				unsigned int offset = next_link_state->next_burst_start + burst * burst_offset + t;
				const Reservation& res = table->getReservation(offset);
				if (res.getTarget() == link_id) {
					coutd << "t=" << offset << ":" << res << " ";
					table->mark(offset, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));
				}
			}
		}
	}
	coutd << "will renew link on " << *channel << "@" << slot_offset << " -> ";
	next_link_state->channel = channel;
	next_link_state->next_burst_start = slot_offset;
	// Clear pending requests.
	coutd << "clear " << current_link_state->scheduled_link_requests.size() << " pending renewal requests -> ";
	current_link_state->scheduled_link_requests.clear();
	// Clear locked resources.
	coutd << "unlock resources: ";
	clearLockedResources(current_link_state->last_proposed_renewal_resources, current_link_state->last_proposal_sent);
	delete current_link_state->last_proposed_renewal_resources;
	current_link_state->last_proposal_sent = 0;
	// Mark slot reservations of the new link.
	coutd << "-> mark new slot reservations: ";
	for (unsigned int t = 0; t < default_timeout; t++)
		scheduleBurst(next_link_state->next_burst_start + t*burst_offset, next_link_state->burst_length, next_link_state->burst_length_tx, link_id, table, true);
	// Update status.
	coutd << "-> changing status '" << link_status << "->" << LinkManager::link_renewal_complete << "' -> ";
	link_status = link_renewal_complete;
}

std::pair<L2HeaderLinkReply*, LinkManager::LinkRequestPayload*> P2PLinkManager::prepareReply(const MacId& dest_id, const FrequencyChannel *channel, unsigned int slot_offset, unsigned int burst_length, unsigned int burst_length_tx) const {
	// The reply header.
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
		Reservation res = Reservation(dest_id, action, burst_length > 0 ? burst_length - 1 : 0);
		table->mark(burst_start_offset + t, res);
		coutd << "t=" << burst_start_offset + t << ":" << res << " ";
	}
	unsigned int burst_length_rx = burst_length - burst_length_tx;
	for (unsigned int t = 0; t < burst_length_rx; t++) {
		Reservation::Action action = t==0 ? (link_initiator ? Reservation::Action::RX : Reservation::Action::TX) : (link_initiator ? Reservation::Action::RX_CONT : Reservation::Action::TX_CONT);
		Reservation res = Reservation(dest_id, action, burst_length > 0 ? burst_length - 1 : 0);
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
	if (!current_link_state->renewal_due && (link_status == LinkManager::awaiting_reply || link_status == LinkManager::awaiting_data_tx)) {
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
		assert(current_link_state != nullptr);
		assert(current_link_state->channel != nullptr);
		assert(next_link_state != nullptr);
		assert(next_link_state->channel != nullptr);
		coutd << "applying renewal: " << *current_link_state->channel << "->" << *next_link_state->channel << " starting in " << next_link_state->next_burst_start << " slots -> ";
		current_channel = next_link_state->channel;
		current_reservation_table = reservation_manager->getReservationTable(current_channel);
		// The link initiator...
		if (current_link_state->is_link_initiator) {
			next_link_state->is_link_initiator = true;
			// schedules renewal request slots...
			coutd << "scheduling link renewal request slots: ";
			std::vector<unsigned int> link_renewal_request_slots = scheduleRenewalRequestSlots(default_timeout, next_link_state->next_burst_start, burst_offset, num_renewal_attempts);
			for (unsigned int renewal_request_slot : link_renewal_request_slots) {
				auto request_msg = prepareRequestMessage(false);
				next_link_state->scheduled_link_requests.emplace_back(renewal_request_slot, request_msg.first, request_msg.second);
			}
		}
		delete current_link_state;
		current_link_state = next_link_state;
		next_link_state = nullptr;
		coutd << "updating status: " << link_status << "->" << LinkManager::link_established << " -> link renewal complete." << std::endl;
		link_status = link_established;
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
		if (current_link_state->last_proposed_renewal_resources != nullptr)
			clearLockedResources(current_link_state->last_proposed_renewal_resources, current_link_state->last_proposal_sent);
		delete current_link_state;
		current_link_state = nullptr;
		delete next_link_state;
		next_link_state = nullptr;
		coutd << "-> link reset -> ";
		// Check if there's more data,
		if (mac->isThereMoreData(link_id)) // and re-establish the link if there is.
			notifyOutgoing((unsigned long) outgoing_traffic_estimate.get());
	}
}

void P2PLinkManager::clearLockedResources(LinkManager::LinkRequestPayload*& proposal, unsigned int num_slot_since_proposal) {
	for (const auto& item : proposal->locked_resources) {
		const FrequencyChannel *channel = item.first;
		const std::vector<unsigned int> &slots = item.second;
		ReservationTable *table = reservation_manager->getReservationTable(channel);
		for (unsigned int slot : slots) {
			if (slot < num_slot_since_proposal)
				continue; // Skip those that have already passed.
			unsigned int normalized_offset = slot - num_slot_since_proposal;
			if (!table->getReservation(normalized_offset).isLocked()) {
				std::cout << "Conflict: t=" << normalized_offset << " " << table->getReservation(normalized_offset) << std::endl;
				assert(table->getReservation(normalized_offset).isLocked());
			}
			table->mark(normalized_offset, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));
			coutd << *channel << "@" << normalized_offset << " ";
		}
	}
}

void P2PLinkManager::assign(const FrequencyChannel* channel) {
	// Base class call might set current_channel, but only if it's nullptr, so we do the same.
	if (current_channel == nullptr && current_link_state != nullptr)
		current_link_state->channel = channel;
	LinkManager::assign(channel);
}

unsigned int P2PLinkManager::estimateCurrentNumSlots() const {
	unsigned int traffic_estimate = (unsigned int) outgoing_traffic_estimate.get(); // in bits.
	unsigned int datarate = mac->getCurrentDatarate(); // in bits/slot.
	return std::max(uint32_t(1), traffic_estimate / datarate); // in slots.
}

unsigned int P2PLinkManager::getExpiryOffset() const {
	if (current_link_state == nullptr)
		return 0;
	else
		return (current_link_state->timeout - 1)*burst_offset + current_link_state->burst_length;
}
