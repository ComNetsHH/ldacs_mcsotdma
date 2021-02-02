//
// Created by seba on 1/14/21.
//

#include <cassert>
#include "LinkManagementEntity.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "coutdebug.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

LinkManagementEntity::LinkManagementEntity(LinkManager* owner) : owner(owner) {}

std::vector<uint64_t> LinkManagementEntity::scheduleRequests(unsigned int timeout, unsigned int init_offset,
                                                             unsigned int burst_offset, unsigned int num_attempts) const {
	std::vector<uint64_t> slots;
	// For each transmission burst from last to first according to this reservation...
	for (long i = 0, offset = init_offset + (timeout - 1) * burst_offset; slots.size() < num_attempts && offset >= init_offset; offset -= burst_offset, i++) {
		// ... add every second burst
		if (i % 2 == 1) {
			slots.push_back(owner->mac->getCurrentSlot() + offset);
			coutd << "t=" << offset << " ";
		}
	}
	coutd << "-> ";
	return slots;
}

size_t LinkManagementEntity::clearPendingRequestReservations(const std::map<const FrequencyChannel*, std::vector<unsigned int>>& proposed_resources, uint64_t absolute_proposal_time, uint64_t current_time) {
	coutd << "removing reservations on proposed resources: ";
	// Remove all RX reservations for proposed resources that, since we are processing this reply, don't need to be listened to anymore.
	if (proposed_resources.empty())
		throw std::runtime_error("LinkManagementEntity::processLinkReply for unsaved last proposal.");
	size_t num_removed = 0;
	for (const auto& item : proposed_resources) {
		const FrequencyChannel* proposed_channel = item.first;
		const std::vector<unsigned int>& proposed_slots_in_this_channel = item.second;
		ReservationTable* table = owner->reservation_manager->getReservationTable(proposed_channel);
		for (unsigned int offset : proposed_slots_in_this_channel) {
			// Number of time slots that have passed since the proposal.
			uint64_t time_difference = current_time - absolute_proposal_time;
			// Ignore slots that have already passed.
			if (time_difference < offset) {
				// Normalize offsets to current time.
				unsigned long normalized_offset = offset - time_difference;
				const Reservation* reservation = &table->getReservation((int) normalized_offset);
				coutd << "f=" << *proposed_channel << ",t=" << normalized_offset << ":" << *reservation;
				if (!reservation->isRx() && !reservation->isLocked())
					throw std::runtime_error("LinkManagementEntity::processLinkReply should clear a pending reservation, but the action was " + std::to_string(reservation->getAction()) + ".");
				table->mark(normalized_offset, Reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE));
				coutd << "->idle ";
				num_removed++;
			}
		}
	}
	coutd << "-> ";
	return num_removed;
}

void LinkManagementEntity::processLinkReply(const L2HeaderLinkEstablishmentReply*& header, const ProposalPayload*& payload) {
	// Make sure we're expecting a reply.
	if (owner->link_establishment_status != owner->Status::awaiting_reply)
		throw std::runtime_error("LinkManager for ID '" + std::to_string(owner->link_id.getId()) + "' received a link reply but its state is '" + std::to_string(owner->link_establishment_status) + "'.");
	assert(payload->proposed_resources.size() == 1);

	// Clear all scheduled requests, as one apparently made it through.
	coutd << "clearing " << scheduled_requests.size() << " pending requests -> ";
	scheduled_requests.clear();
	size_t num_cleared_reservations = clearPendingRequestReservations(last_proposed_resources, last_proposal_absolute_time, owner->mac->getCurrentSlot());
	last_proposed_resources.clear();
	coutd << num_cleared_reservations << " cleared -> ";
	link_renewal_pending = false;

	// Differentiate between initial and renewal replies.
	if (owner->current_channel == nullptr)
		processInitialReply(header, payload);
	else
		processRenewalReply(header, payload);
}

void LinkManagementEntity::processInitialReply(const L2HeaderLinkEstablishmentReply*& header, const LinkManagementEntity::ProposalPayload*& payload) {
	coutd << "establishing link -> assigning channel -> ";
	const FrequencyChannel* channel = (*payload->proposed_resources.begin()).first;
	owner->assign(channel);
	tx_timeout = default_tx_timeout;
	coutd << "resetting timeout to " << tx_timeout << " -> marking TX reservations:";
	owner->markReservations(tx_timeout, 0, tx_offset, tx_burst_num_slots, owner->link_id, Reservation::TX);
	coutd << " -> configuring link renewal request slots -> ";
	// Schedule the absolute slots for sending requests.
	this->num_renewal_attempts = max_link_renewal_attempts;
	scheduled_requests = scheduleRequests(tx_timeout, 0, tx_offset, num_renewal_attempts);
	coutd << scheduled_requests.size() << " scheduled -> ";
	owner->link_establishment_status = owner->Status::link_established;
	owner->mac->notifyAboutNewLink(owner->link_id);
	coutd << "link is now established -> ";
}

void LinkManagementEntity::processRenewalReply(const L2HeaderLinkEstablishmentReply*& header, const LinkManagementEntity::ProposalPayload*& payload) {
	coutd << "renewing link -> ";
	const FrequencyChannel* channel = (*payload->proposed_resources.begin()).first;
	if (payload->proposed_resources.at(channel).size() != 1)
		throw std::invalid_argument("LinkManagementEntity::processRenewalReply for invalid number of slots.");
	unsigned int initial_slot = payload->proposed_resources.at(channel).at(0) - tx_offset;
	coutd << "initial_slot=" << initial_slot << std::endl;
	if (channel == owner->current_channel) {
		coutd << "no channel change -> increasing timeout: " << tx_timeout << "->";
		tx_timeout += default_tx_timeout;
		coutd << tx_timeout << " and marking TX reservations: ";
		owner->markReservations(tx_timeout, initial_slot, tx_offset, tx_burst_num_slots, owner->link_id, Reservation::TX);
		coutd << " -> configuring request slots -> ";
		// Schedule the absolute slots for sending requests.
		this->num_renewal_attempts = max_link_renewal_attempts;
		scheduled_requests = scheduleRequests(tx_timeout, 0, tx_offset, num_renewal_attempts);
		coutd << scheduled_requests.size() << " scheduled -> ";
		coutd << "link status update: " << owner->link_establishment_status;
		owner->link_establishment_status = owner->Status::link_established;
		coutd << "->" << owner->link_establishment_status;
	} else {
		coutd << "channel change -> saving new channel (" << *owner->current_channel << "->" << *channel << ") -> ";
		next_channel = channel;
		coutd << "and marking TX reservations on " << *next_channel << ": ";
		ReservationTable* table = owner->reservation_manager->getReservationTable(next_channel);
		owner->markReservations(table, default_tx_timeout, initial_slot, tx_offset, Reservation(owner->link_id, Reservation::TX, tx_burst_num_slots - 1));
		coutd << "link status update: " << owner->link_establishment_status;
		owner->link_establishment_status = owner->Status::link_renewal_complete;
		coutd << "->" << owner->link_establishment_status << " -> ";
	}
}

void LinkManagementEntity::onTransmissionBurst() {
	decrementTimeout();
}

void LinkManagementEntity::onReceptionSlot() {
	decrementTimeout();
}

void LinkManagementEntity::decrementTimeout() {
	// Don't update timeout if,
	// (1) the link is not established right now
	if (owner->link_establishment_status == LinkManager::link_not_established)
		return;
	// (2) we are in the process of establishment.
	if ((!link_renewal_pending && owner->link_establishment_status == LinkManager::awaiting_reply) || (!link_renewal_pending && owner->link_establishment_status == LinkManager::awaiting_data_tx))
		return;
	// (3) it has already been updated this slot.
	if (updated_timeout_this_slot)
		return;
	updated_timeout_this_slot = true;

	if (tx_timeout == 0)
		throw std::runtime_error("LinkManagementEntity::decrementTimeout attempted to decrement timeout past zero.");
	coutd << "timeout " << tx_timeout << "->";
	tx_timeout--;
	coutd << tx_timeout << " -> ";
	if (tx_timeout == 0) {
		coutd << "timeout reached -> ";
		if (owner->link_establishment_status == LinkManager::link_renewal_complete) {
			coutd << "applying renewal: " << *owner->current_channel << "->" << *next_channel;
			owner->reassign(next_channel);
			next_channel = nullptr;
			coutd << "; restoring timeout to " << default_tx_timeout;
			tx_timeout = default_tx_timeout;
			coutd << "; updating status: " << owner->link_establishment_status;
			owner->link_establishment_status = LinkManager::link_established;
			coutd << "->" << owner->link_establishment_status << " -> link renewal complete -> ";
		} else {
			coutd << "no pending renewal, changing status: " << owner->link_establishment_status << "->";
			owner->link_establishment_status = LinkManager::link_not_established;
			coutd << owner->link_establishment_status << " -> link reset -> ";
		}
	}
}

void LinkManagementEntity::processLinkRequest(const L2HeaderLinkEstablishmentRequest*& header,
                                              const ProposalPayload*& payload, const MacId& origin) {
	if (owner->link_establishment_status == LinkManager::link_not_established)
		processInitialRequest(header, payload, origin);
	else
		processRenewalRequest(header, payload, origin);

}

void LinkManagementEntity::processInitialRequest(const L2HeaderLinkEstablishmentRequest*& header, const LinkManagementEntity::ProposalPayload*& payload, const MacId& origin) {
	coutd << "processing initial link establishment request -> ";
	// It's an initial request, so we must *send* the reply at the selected candidate, hence take the transmitter utilization into account.
	bool consider_transmitter = true, consider_receiver = false;
	auto viable_candidates = findViableCandidatesInRequest((L2HeaderLinkEstablishmentRequest*&) header,(ProposalPayload*&) payload, consider_transmitter, consider_receiver);
	if (!viable_candidates.empty()) {
		// Choose a candidate out of the set.
		auto chosen_candidate = viable_candidates.at(owner->getRandomInt(0, viable_candidates.size()));
		coutd << "picked candidate (" << chosen_candidate.first->getCenterFrequency() << "kHz, offset " << chosen_candidate.second << ") -> ";
		// Prepare a link reply.
		L2Packet* reply = prepareReply(origin);
		// Populate the payload.
		const FrequencyChannel* reply_channel = chosen_candidate.first;
		assert(reply->getPayloads().size() == 2);
		auto* reply_payload = (ProposalPayload*) reply->getPayloads().at(1);
		int32_t slot_offset = chosen_candidate.second;
		reply_payload->proposed_resources[reply_channel].push_back(slot_offset);
		// Assign the channel directly.
		owner->assign(reply_channel);
		// And schedule a reply at the selected resource.
		scheduleInitialReply(reply, slot_offset);
	} else
		coutd << "no candidates viable. Doing nothing." << std::endl;
}

void LinkManagementEntity::processRenewalRequest(const L2HeaderLinkEstablishmentRequest*& header, const LinkManagementEntity::ProposalPayload*& payload, const MacId& origin) {
	coutd << "processing renewal request";
	// It's a renewal request, so we must *receive* at the selected candidate, hence take the receiver utilization into account.
	bool consider_transmitter = false, consider_receiver = true;
	auto viable_candidates = findViableCandidatesInRequest((L2HeaderLinkEstablishmentRequest*&) header,(ProposalPayload*&) payload, consider_transmitter, consider_receiver);
	if (!viable_candidates.empty()) {
		// Choose a candidate out of the set.
		auto chosen_candidate = viable_candidates.at(owner->getRandomInt(0, viable_candidates.size()));
		coutd << "picked candidate (" << chosen_candidate.first->getCenterFrequency() << "kHz, offset " << chosen_candidate.second << ") -> ";
		// Prepare a link reply.
		L2Packet* reply = prepareReply(origin);
		// Populate the payload.
		const FrequencyChannel* reply_channel = chosen_candidate.first;
		assert(reply->getPayloads().size() == 2);
		auto* reply_payload = (ProposalPayload*) reply->getPayloads().at(1);
		int32_t slot_offset = chosen_candidate.second;
		slot_offset -= tx_offset;
		reply_payload->proposed_resources[reply_channel].push_back(slot_offset);
		// Remember the channel to switch to after expiry.
		next_channel = reply_channel;
		// And schedule a reply in the next burst.
		scheduleRenewalReply(reply);
	} else
		coutd << "no candidates viable. Doing nothing." << std::endl;
}

std::vector<std::pair<const FrequencyChannel*, unsigned int>>
LinkManagementEntity::findViableCandidatesInRequest(L2HeaderLinkEstablishmentRequest*& header,
                                                    ProposalPayload*& payload, bool consider_transmitter, bool consider_receiver) const {
	assert(payload && "LinkManager::findViableCandidatesInRequest for nullptr ProposalPayload*");
	const MacId& dest_id = header->icao_dest_id;
	if (payload->proposed_resources.empty())
		throw std::invalid_argument("LinkManager::findViableCandidatesInRequest for an empty proposal.");

	// Go through all proposed channels...
	std::vector<std::pair<const FrequencyChannel*, unsigned int>> viable_candidates;
//        for (size_t i = 0; i < payload->proposed_channels.size(); i++) {
	for (const auto& item : payload->proposed_resources) {
		const FrequencyChannel* channel = item.first;
		coutd << " -> proposed channel " << channel->getCenterFrequency() << "kHz:";
		// ... and all slots proposed on this channel ...
		unsigned int num_candidates_on_this_channel = item.second.size();
		for (size_t j = 0; j < num_candidates_on_this_channel; j++) {
			unsigned int slot_offset = item.second.at(j);
			coutd << " @" << slot_offset;
			// ... and check if they're idle for us ...
			const ReservationTable* table = owner->reservation_manager->getReservationTable(channel);
			// ... if they are, then save them.
			bool viable = table->isIdle(slot_offset, payload->burst_length);
			if (consider_transmitter)
				viable = viable && owner->mac->isTransmitterIdle(slot_offset, payload->burst_length);
			if (consider_receiver)
				viable = viable && owner->mac->isAnyReceiverIdle(slot_offset, payload->burst_length);

			if (viable) {
				coutd << " (viable)";
				viable_candidates.emplace_back(channel, slot_offset);
			} else
				coutd << " (busy)";
		}
	}
	coutd << " -> ";
	return viable_candidates;
}

L2Packet* LinkManagementEntity::prepareRequest() const {
	auto* request = new L2Packet();
	// Query ARQ sublayer whether this link should be ARQ protected.
	bool link_should_be_arq_protected = owner->mac->shouldLinkBeArqProtected(owner->link_id);
	// Instantiate base header.
	auto* base_header = new L2HeaderBase(owner->mac->getMacId(), 0, 0, 0);
	request->addPayload(base_header, nullptr);
	// If the link is not yet established, the request must be sent on the broadcast channel.
	if (owner->link_establishment_status == LinkManager::link_not_established)
		request->addPayload(new L2HeaderBroadcast(), nullptr);
	// Instantiate request header.
	MacId dest_id = owner->link_id;
	auto* request_header = new L2HeaderLinkEstablishmentRequest(dest_id, link_should_be_arq_protected, 0, 0, 0);
	auto* body = new ProposalPayload(num_proposed_channels, num_proposed_slots);
	request->addPayload(request_header, body);
	request->addCallback(owner);
	return request;
}

L2Packet* LinkManagementEntity::prepareReply(const MacId& destination_id) const {
	auto* reply = new L2Packet();
	// Base header.
	auto* base_header = new L2HeaderBase(owner->mac->getMacId(), 0, 0, 0);
	reply->addPayload(base_header, nullptr);
	// Reply header.
	auto* reply_header = new L2HeaderLinkEstablishmentReply();
	reply_header->icao_dest_id = destination_id;
	// Reply payload will be populated later.
	auto* reply_payload = new ProposalPayload(1, 1);
	reply->addPayload(reply_header, reply_payload);
	return reply;
}


void LinkManagementEntity::establishLink() const {
	coutd << "establishing new link... ";
	if (owner->link_establishment_status == owner->link_not_established) {
		// Prepare a link request and inject it into the RLC sublayer above.
		L2Packet* request = prepareRequest();
		coutd << "prepared link establishment request... ";
		owner->mac->injectIntoUpper(request);
		coutd << "injected into upper layer... ";
		// We are now awaiting a reply.
		owner->link_establishment_status = LinkManager::Status::awaiting_reply;
		coutd << "updated status to 'awaiting_reply'." << std::endl;
	} else
		throw std::runtime_error("LinkManager::establishLink for link status: " + std::to_string(owner->link_establishment_status));
}

L2Packet* LinkManagementEntity::getControlMessage() {
	L2Packet* control_message = nullptr;
	if (hasPendingReply()) {
		auto it = scheduled_replies.find(owner->mac->getCurrentSlot());
		control_message = (*it).second;
		// Delete scheduled entry.
		scheduled_replies.erase(it);
		// Save chosen link transition.
		assert(control_message->getPayloads().size() == 2);
		assert(((ProposalPayload*) control_message->getPayloads().at(1))->proposed_resources.size() == 1);
		const FrequencyChannel* channel = (((ProposalPayload*) control_message->getPayloads().at(1))->proposed_resources.begin())->first;
		next_channel = channel;
	} else if (hasPendingRequest()) {
		control_message = prepareRequest(); // Sets the callback, s.t. the actual proposal is computed then.
		// Delete scheduled entry.
		for (auto it = scheduled_requests.begin(); it != scheduled_requests.end(); it++) { // it's a std::vector, so there's no find() and there may be multiples
			uint64_t current_slot = *it;
			if (current_slot == owner->mac->getCurrentSlot()) {
				scheduled_requests.erase(it);
				it--; // Update iterator as the vector has shrunk.
			}
		}
	}
	return control_message;
}

bool LinkManagementEntity::hasControlMessage() {
	return hasPendingRequest() || hasPendingReply();
}

bool LinkManagementEntity::hasPendingRequest() {
	for (auto it = scheduled_requests.begin(); it != scheduled_requests.end(); it++) {
		uint64_t current_slot = *it;
		if (current_slot == owner->mac->getCurrentSlot()) {
			if (owner->mac->isThereMoreData(owner->getLinkId())) {
				link_renewal_pending = true;
				return true;
			}
		} else if (current_slot < owner->mac->getCurrentSlot()) {
			if (owner->mac->isThereMoreData(owner->getLinkId()))
				throw std::invalid_argument("LinkManagementEntity::hasControlMessage has missed a scheduled request: " + std::to_string(current_slot) + " (current slot: " + std::to_string(owner->mac->getCurrentSlot()) + ").");
			else
				scheduled_requests.erase(it--);
		}
	}
	return false;
}

bool LinkManagementEntity::hasPendingReply() {
	return !scheduled_replies.empty() && scheduled_replies.find(owner->mac->getCurrentSlot()) != scheduled_replies.end();
}

void LinkManagementEntity::scheduleInitialReply(L2Packet* reply, int32_t slot_offset) {
	coutd << "schedule initial reply -> ";
	uint64_t absolute_slot = owner->mac->getCurrentSlot() + slot_offset;
	if (scheduled_replies.find(absolute_slot) != scheduled_replies.end())
		throw std::runtime_error("LinkManager::scheduleLinkReply wanted to schedule a link reply, but there's already one scheduled at slot " + std::to_string(absolute_slot) + ".");
	else {
		// Sanity check.
		if (reply->getPayloads().size() < 2)
			throw std::invalid_argument("LinkManagementEntity::scheduleLinkReply for proposal-less reply.");
		auto* proposal = (ProposalPayload*) reply->getPayloads().at(1);
		if (proposal->proposed_resources.empty())
			throw std::invalid_argument("LinkManagementEntity::scheduleLinkReply for proposal without a FrequencyChannel.");
		if ((*proposal->proposed_resources.begin()).second.empty())
			throw std::invalid_argument("LinkManagementEntity::scheduleLinkReply for proposal without a time slot.");

		// ... we send the reply on the selected channel.
		const FrequencyChannel* channel = proposal->proposed_resources.begin()->first;
		ReservationTable* table = owner->reservation_manager->getReservationTable(channel);

		// Make sure the selected slot is reserved for this link or idle (sanity check).
		const Reservation& current_reservation = table->getReservation(slot_offset);
		if (!current_reservation.isIdle() && current_reservation.getTarget() != owner->getLinkId()) {
			coutd << std::endl << "Reservation in question: " << current_reservation << std::endl;
			throw std::invalid_argument("LinkManager::scheduleLinkReply for an already reserved slot.");
		}

		// Mark the slot as TX to transmit the reply...
		table->mark(slot_offset, Reservation(reply->getDestination(), Reservation::Action::TX));
		scheduled_replies[absolute_slot] = reply;
		coutd << "-> scheduled reply in " << slot_offset << " slots on " << *channel << " -> ";

		// First data transmissions are expected one burst after the first slot of the selected resource (where the reply is sent).
		unsigned int expected_data_tx_slot = proposal->proposed_resources[channel].at(0) + tx_offset;
		table->mark(expected_data_tx_slot, Reservation(owner->link_id, Reservation::Action::RX));
		coutd << "marked first RX slot of chosen candidate (" << *channel << ", offset " << expected_data_tx_slot << ") -> ";

	}
}

void LinkManagementEntity::scheduleRenewalReply(L2Packet* reply) {
	coutd << "schedule renewal reply -> ";
	uint64_t absolute_slot = owner->mac->getCurrentSlot() + tx_offset;
	if (scheduled_replies.find(absolute_slot) != scheduled_replies.end())
		throw std::runtime_error("LinkManager::scheduleLinkReply wanted to schedule a link reply, but there's already one scheduled at slot " + std::to_string(absolute_slot) + ".");
	else {
		// Sanity check.
		if (reply->getPayloads().size() < 2)
			throw std::invalid_argument("LinkManagementEntity::scheduleLinkReply for proposal-less reply.");
		auto* proposal = (ProposalPayload*) reply->getPayloads().at(1);
		if (proposal->proposed_resources.empty())
			throw std::invalid_argument("LinkManagementEntity::scheduleLinkReply for proposal without a FrequencyChannel.");
		if ((*proposal->proposed_resources.begin()).second.empty())
			throw std::invalid_argument("LinkManagementEntity::scheduleLinkReply for proposal without a time slot.");

		// ... we send the reply on the current channel.
		ReservationTable* table = owner->current_reservation_table;
		const FrequencyChannel* channel = owner->current_reservation_table->getLinkedChannel();

		// Mark the slot as TX to transmit the reply...
		table->mark(tx_offset, Reservation(reply->getDestination(), Reservation::Action::TX));
		scheduled_replies[absolute_slot] = reply;
		coutd << "scheduled reply in " << tx_offset << " slots on " << *channel << " -> ";

		// First data transmissions are expected after this link has expired, on the new frequency channel for link renewals.
		const FrequencyChannel* selected_channel = proposal->proposed_resources.begin()->first;
		unsigned int expected_data_tx_slot = proposal->proposed_resources[selected_channel].at(0) + tx_offset;
		ReservationTable* selected_table = owner->reservation_manager->getReservationTable(selected_channel);
		coutd << "marking first RX slot of chosen candidate (" << *selected_channel << ", offset " << expected_data_tx_slot << ") -> ";
		owner->markReservations(selected_table, default_tx_timeout, expected_data_tx_slot - tx_offset, tx_offset, Reservation(owner->link_id, Reservation::Action::RX));
//		selected_table->mark(expected_data_tx_slot, Reservation(owner->link_id, Reservation::Action::RX));
	}
}

void LinkManagementEntity::setTxTimeout(unsigned int value) {
	updated_timeout_this_slot = true;
	tx_timeout = value;
}

void LinkManagementEntity::setTxOffset(unsigned int value) {
	tx_offset = value;
}

unsigned int LinkManagementEntity::getTxTimeout() const {
	return tx_timeout;
}

unsigned int LinkManagementEntity::getTxOffset() const {
	return tx_offset;
}

unsigned int LinkManagementEntity::getMinOffset() const {
	return min_offset_new_reservations;
}

LinkManagementEntity::ProposalPayload* LinkManagementEntity::p2pSlotSelection(const unsigned int burst_num_slots, const unsigned int num_channels, const unsigned int num_slots_per_channel, const unsigned int min_offset, bool consider_tx, bool consider_rx) {
	auto* proposal = new ProposalPayload(num_proposed_channels, num_proposed_slots);

	// Find resource proposals...
	// ... get the P2P reservation tables sorted by their numbers of idle slots ...
	auto table_priority_queue = owner->reservation_manager->getSortedP2PReservationTables();
	// ... until we have considered the target number of channels ...
	coutd << "p2pSlotSelection to reserve " << tx_burst_num_slots << " slots -> ";
	for (size_t num_channels_considered = 0; num_channels_considered < num_channels; num_channels_considered++) {
		if (table_priority_queue.empty()) // we could just stop here, but we're throwing an error to be aware when it happens
			throw std::runtime_error("LinkManager::prepareRequest has considered " + std::to_string(num_channels_considered) + " out of " + std::to_string(num_channels) + " and there are no more.");
		// ... get the next reservation table ...
		ReservationTable* table = table_priority_queue.top();
		table_priority_queue.pop();
		// ... and try to find candidate slots,
		std::vector<int32_t> candidate_slots = table->findCandidateSlots(min_offset, num_slots_per_channel, burst_num_slots, consider_tx, consider_rx);
		coutd << "found " << candidate_slots.size() << " slots on " << *table->getLinkedChannel() << ": ";
		for (int32_t slot : candidate_slots)
			coutd << slot << " ";
		coutd << " -> ";

		// ... and lock them s.t. future proposals don't consider them.
		if (!table->lock(candidate_slots, consider_tx, consider_rx))
			throw std::runtime_error("LME::p2pSlotSelection failed to lock resources.");
		else
			coutd << "locked -> ";

		// Fill proposal.
		proposal->burst_length = burst_num_slots;
		for (int32_t slot : candidate_slots) // The candidate slots.
			proposal->proposed_resources[table->getLinkedChannel()].push_back(slot);
	}
	return proposal;
}

unsigned int LinkManagementEntity::getTxBurstSlots() const {
	return tx_burst_num_slots;
}

void LinkManagementEntity::setTxBurstSlots(unsigned int value) {
	this->tx_burst_num_slots = value;
}

void LinkManagementEntity::populateRequest(L2Packet*& request) {
	int request_index = request->getRequestIndex();
	if (request_index == -1)
		throw std::invalid_argument("LinkManagementEntity::populateRequest for non-request packet.");
	auto* request_header = (L2HeaderLinkEstablishmentRequest*) request->getHeaders().at(request_index);
	// Set the destination ID (may be broadcast until now).
	request_header->icao_dest_id = owner->link_id;
	request_header->offset = tx_offset;
	request_header->timeout = tx_timeout;
	// Remember this request's number of slots.
	tx_burst_num_slots = owner->estimateCurrentNumSlots();
	request_header->length_next = tx_burst_num_slots;
	coutd << "populate link request: " << *request_header << " -> ";
	// Compute a current proposal.
	unsigned int min_offset;
	// For initial establishment...
	if (!link_renewal_pending) {
		min_offset = default_minimum_slot_offset_for_new_reservations; // have the minimum offset
		coutd << "initial request, offset=" << min_offset << " -> ";
	// For renewal...
	} else {
		min_offset = getExpiryOffset() + 1; // look for slots *after* this link has expired
		coutd << "renewal request, offset=" << min_offset << " -> ";
	}
	// First establishment => we receive during the selected slot. Renewal => we transmit during the selected slot.
	bool consider_tx = link_renewal_pending, consider_rx = !link_renewal_pending;
	request->getPayloads().at(request_index) = p2pSlotSelection(tx_burst_num_slots, num_proposed_channels, num_proposed_slots, min_offset, consider_tx, consider_rx);
	// Save current proposal.
	auto* proposal = (const ProposalPayload*) request->getPayloads().at(request_index);
	last_proposal_absolute_time = owner->mac->getCurrentSlot();
	last_proposed_resources = proposal->proposed_resources;

	// If this is not a renewal, mark all slots as RX to listen for replies.
	if (!link_renewal_pending) {
		for (const auto& item : proposal->proposed_resources) {
			const FrequencyChannel* channel = item.first;
			ReservationTable* table = owner->reservation_manager->getReservationTable(channel);
			std::vector<unsigned int> proposed_slots;
			// ... and each slot...
			for (int32_t offset : item.second) {
				try {
					// Even for multi-slot reservations, only the first slot should be marked, as the reply must fit within one slot.
					table->mark(offset, Reservation(owner->link_id, Reservation::Action::RX, 0));
				} catch (const std::exception& e) {
					throw std::runtime_error("LinkManager::packetBeingSentCallback couldn't mark RX slots: " + std::string(e.what()));
				}
			}
		}
	}
}

void LinkManagementEntity::onRequestTransmission() {
	// Upon a renewal request...
	if (owner->link_establishment_status != LinkManager::link_not_established) {
		// ... mark the next transmission burst as RX to receive the reply.
		owner->current_reservation_table->mark(tx_offset, Reservation(owner->getLinkId(), Reservation::Action::RX));
	// Upon initial requests...
	} else {
		// ... do nothing.
	}
}

unsigned int LinkManagementEntity::getExpiryOffset() const {
	return tx_timeout*tx_offset;
}

void LinkManagementEntity::update(uint64_t num_slots) {
	updated_timeout_this_slot = false;
}
