//
// Created by Sebastian Lindner on 10.11.20.
//

#include <cassert>
#include <random>
#include "LinkManager.hpp"
#include "coutdebug.hpp"
#include "MCSOTDMA_Mac.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

LinkManager::LinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac)
	: link_id(link_id), reservation_manager(reservation_manager),
	link_establishment_status((link_id == SYMBOLIC_LINK_ID_BROADCAST || link_id == SYMBOLIC_LINK_ID_BEACON) ? Status::link_established : Status::link_not_established) /* broadcast links are always established */,
	  mac(mac), traffic_estimate(20) {}

const MacId& LinkManager::getLinkId() const {
	return this->link_id;
}

void LinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << "LinkManager::notifyOutgoing(id='" << link_id << "')";
	
	// Update the moving average traffic estimate.
	updateTrafficEstimate(num_bits);
	
	// Check establishment status.
	// If the link is established...
	if (link_establishment_status == Status::link_established) {
		coutd << ": link already established";
	// If the link is not yet established...
	} else {
		// ... and we've created the request and are just waiting for a reply ...
		if (link_establishment_status == Status::awaiting_reply) {
			coutd << ": link is being established and currently awaiting reply. Doing nothing." << std::endl;
			// ... then do nothing.
			return;
		// ... and link establishment has not yet been started ...
		} else if (link_establishment_status == Status::link_not_established) {
			coutd << ": link is not established -> ";
			requestNewLink();
		} else {
			throw std::runtime_error("Unsupported LinkManager::Status: '" + std::to_string(link_establishment_status) + "'.");
		}
	}
}

void LinkManager::receiveFromLower(L2Packet* packet) {
	coutd << "LinkManager(" << link_id << ")::receiveFromLower... ";
	coutd << "a packet from '" << packet->getOrigin() << "' ";
	if (packet->getDestination() != SYMBOLIC_ID_UNSET) {
		coutd << "to '" << packet->getDestination() << " ";
		if (packet->getDestination() == mac->getMacId())
			coutd << "(us)' -> ";
		else
			coutd << "' -> ";
	}
	assert(!packet->getHeaders().empty() && "LinkManager::receiveFromLower(empty packet)");
	// Go through all header and payload pairs...
	for (size_t i = 0; i < packet->getHeaders().size(); i++) {
		L2Header* header = packet->getHeaders().at(i);
		L2Packet::Payload* payload = packet->getPayloads().at(i);
		switch (header->frame_type) {
			case L2Header::base: {
				coutd << "processing base header -> ";
				processIncomingBase((L2HeaderBase*&) header);
				break;
			}
			case L2Header::beacon: {
				coutd << "processing beacon -> ";
				processIncomingBeacon(packet->getOrigin(), (L2HeaderBeacon*&) header, (BeaconPayload*&) payload);
				// Delete and set to nullptr s.t. upper layers can easily ignore them.
				delete header;
				header = nullptr;
				delete payload;
				payload = nullptr;
				coutd << std::endl;
				break;
			}
			case L2Header::broadcast: {
				coutd << "processing broadcast -> ";
				processIncomingBroadcast(packet->getOrigin(), (L2HeaderBroadcast*&) header);
				break;
			}
			case L2Header::unicast: {
				coutd <<"processing unicast -> ";
				processIncomingUnicast((L2HeaderUnicast*&) header, payload);
				break;
			}
			case L2Header::link_establishment_request: {
				coutd << "processing link establishment request";
				auto viable_candidates = processIncomingLinkEstablishmentRequest(
						(L2HeaderLinkEstablishmentRequest*&) header,
						(ProposalPayload*&) payload);
				if (!viable_candidates.empty()) {
					// Choose a candidate out of the set.
					auto chosen_candidate = chooseCandidateSlot(viable_candidates);
					coutd << " -> picked candidate (" << chosen_candidate.first->getCenterFrequency() << "kHz, offset " << chosen_candidate.second << ") -> ";
					// Prepare a link reply.
					L2Packet* reply = prepareLinkEstablishmentReply(packet->getOrigin());
					const FrequencyChannel* reply_channel = chosen_candidate.first;
					int32_t slot_offset = chosen_candidate.second;
					// If this LinkManager manages the selected channel...
					if (reply_channel == current_channel) {
						scheduleLinkReply(reply, slot_offset);
						coutd << "scheduled reply in " << slot_offset << " slots." << std::endl;
					} else {
						coutd << "passing on to corresponding LinkManager -> ";
						mac->forwardLinkReply(reply, reply_channel, slot_offset);
					}
				} else
					coutd << "no candidates viable. Doing nothing." << std::endl;
				
				delete header;
				header = nullptr;
				delete payload;
				payload = nullptr;
				break;
			}
			case L2Header::link_establishment_reply: {
				coutd << "processing link establishment reply -> ";
				processIncomingLinkEstablishmentReply((L2HeaderLinkEstablishmentReply*&) header);
				// Delete and set to nullptr s.t. upper layers can easily ignore them.
				delete header;
				header = nullptr;
				delete payload;
				payload = nullptr;
				coutd << std::endl;
				break;
			}
			default: {
				throw std::invalid_argument("LinkManager::receiveFromLower for an unexpected header type.");
			}
		}
	}
	// After processing, the packet is passed to the upper layer.
	coutd << "passing to upper layer." << std::endl;
	mac->passToUpper(packet);
}

void LinkManager::scheduleLinkReply(L2Packet* reply, int32_t slot_offset) {
	uint64_t absolute_slot = mac->getCurrentSlot() + slot_offset;
	auto it = control_messages.find(absolute_slot);
	if (it != control_messages.end())
		throw std::runtime_error("LinkManager::scheduleLinkReply wanted to schedule a link reply, but there's already one scheduled at slot " + std::to_string(absolute_slot) + ".");
	else {
		// ... schedule it.
		if (current_reservation_table->isUtilized(slot_offset))
			throw std::invalid_argument("LinkManager::scheduleLinkReply for an already reserved slot.");
		current_reservation_table->mark(slot_offset, Reservation(reply->getDestination(), Reservation::Action::TX));
		control_messages[absolute_slot] = reply;
		coutd << "-> scheduled reply in " << slot_offset << " slots." << std::endl;
	}
}

double LinkManager::getCurrentTrafficEstimate() const {
	return traffic_estimate.get();
}

void LinkManager::requestNewLink() {
	coutd << "requesting new link... ";
	// Prepare a link request and inject it into the RLC sublayer above.
	L2Packet* request = prepareLinkEstablishmentRequest();
	coutd << "prepared link establishment request... ";
	mac->injectIntoUpper(request);
	coutd << "injected into upper layer... ";
	// We are now awaiting a reply.
	this->link_establishment_status = Status::awaiting_reply;
	coutd << "updated status." << std::endl;
}

L2Packet* LinkManager::prepareLinkEstablishmentRequest() {
	auto* request = new L2Packet();
	// Query ARQ sublayer whether this link should be ARQ protected.
	bool link_should_be_arq_protected = mac->shouldLinkBeArqProtected(this->link_id);
	// Instantiate base header.
	auto* base_header = new L2HeaderBase(mac->getMacId(), 0, 0, 0);
	request->addPayload(base_header, nullptr);
	// Instantiate request header.
	// If the link is not yet established, the request must be sent on the broadcast channel.
	MacId dest_id = link_establishment_status == link_established ? link_id : SYMBOLIC_LINK_ID_BROADCAST;
	auto* request_header = new L2HeaderLinkEstablishmentRequest(dest_id, link_should_be_arq_protected, 0, 0, 0);
	auto* body = new ProposalPayload(this->num_proposed_channels, this->num_proposed_slots);
	request->addPayload(request_header, body);
	return request;
}

L2Packet* LinkManager::prepareLinkEstablishmentReply(const MacId& destination_id) {
	auto* reply = new L2Packet();
	// Base header.
	auto* base_header = new L2HeaderBase(mac->getMacId(), 0, 0, 0);
	reply->addPayload(base_header, nullptr);
	// Reply.
	auto* reply_header = new L2HeaderLinkEstablishmentReply();
	reply_header->icao_dest_id = destination_id;
	reply->addPayload(reply_header, nullptr);
	return reply;
}

void LinkManager::setProposalDimension(unsigned int num_candidate_channels, unsigned int num_candidate_slots) {
	this->num_proposed_channels = num_candidate_channels;
	this->num_proposed_slots = num_candidate_slots;
}

unsigned long LinkManager::estimateCurrentNumSlots() const {
	unsigned long traffic_estimate = (unsigned long) this->traffic_estimate.get(); // in bits.
	unsigned long datarate = mac->getCurrentDatarate(); // in bits/slot.
	return traffic_estimate / datarate; // in slots.
}

unsigned int LinkManager::getNumPendingReservations() const {
	return this->num_pending_reservations;
}

void LinkManager::updateTrafficEstimate(unsigned long num_bits) {
	traffic_estimate.put(num_bits);
}

int32_t LinkManager::getEarliestReservationSlotOffset(int32_t start_slot, const Reservation& reservation) const {
	if (current_reservation_table == nullptr)
		throw std::runtime_error("LinkManager::getEarliestReservationSlotOffset has an unset reservation table.");
	return current_reservation_table->findEarliestOffset(start_slot, reservation);
}

void LinkManager::notifyPacketBeingSent(L2Packet* packet) {
	// This callback is used only for link requests, so ensure that this is a link request.
	for (size_t i = 0; i < packet->getHeaders().size(); i++) {
		L2Header* header = packet->getHeaders().at(i);
		if (header->frame_type == L2Header::link_establishment_request) {
			// Set the destination ID (may be broadcast until now).
			((L2HeaderLinkEstablishmentRequest*) header)->icao_dest_id = link_id;
			// Compute a current proposal.
			packet->getPayloads().at(i) = p2pSlotSelection();
			// Remember the proposal.
			if (this->last_proposal != nullptr)
				throw std::runtime_error("LinkManager::notifyPacketBeingSent called, proposal computed, but there's already a saved proposal which would now be overwritten.");
			this->last_proposal = new ProposalPayload(*((ProposalPayload*) packet->getPayloads().at(i)));
		}
	}
}

LinkManager::ProposalPayload* LinkManager::p2pSlotSelection() const {
	auto* proposal = new ProposalPayload(num_proposed_channels, num_proposed_slots);
	
	// Find resource proposals...
	// ... get the P2P reservation tables sorted by their numbers of idle slots ...
	auto table_priority_queue = reservation_manager->getSortedP2PReservationTables();
	// ... until we have considered the target number of channels ...
	unsigned long required_num_slots = estimateCurrentNumSlots();
	for (size_t num_channels_considered = 0; num_channels_considered < this->num_proposed_channels; num_channels_considered++) {
		if (table_priority_queue.empty()) // we could just stop here, but we're throwing an error to be aware when it happens
			throw std::runtime_error("LinkManager::prepareLinkEstablishmentRequest has considered " + std::to_string(num_channels_considered) + " out of " + std::to_string(num_proposed_channels) + " and there are no more.");
		// ... get the next reservation table ...
		ReservationTable* table = table_priority_queue.top();
		table_priority_queue.pop();
		// ... and try to find candidate slots.
		std::vector<int32_t> candidate_slots = table->findCandidateSlots(this->minimum_slot_offset_for_new_slot_reservations, this->num_proposed_slots, required_num_slots, true);
		
		// Fill proposal.
		proposal->proposed_channels.push_back(table->getLinkedChannel()); // Frequency channel.
		proposal->num_candidates.push_back(candidate_slots.size()); // Number of candidates (could be fewer than the target).
		for (int32_t slot : candidate_slots) // The candidate slots.
			proposal->proposed_slots.push_back(slot);
	}
	return proposal;
}

BeaconPayload* LinkManager::computeBeaconPayload(unsigned long max_bits) const {
	auto* payload = new BeaconPayload(mac->getMacId());
	// Fetch all local transmission reservations and copy it into the payload.
	payload->local_reservations = reservation_manager->getTxReservations(mac->getMacId());
	//!TODO kick out values until max_bits is met!
	if (payload->getBits() > max_bits)
		throw std::runtime_error("LinkManager::computeBeaconPayload doesn't kick out values, and we exceed the allowed number of bits.");
	return payload;
}

L2Packet* LinkManager::onTransmissionSlot(unsigned int num_slots) {
	coutd << "LinkManager::onTransmissionSlot... ";
	L2Packet* segment;
	// Prioritize link reply control messages.
	if (!control_messages.empty()) {
		auto it = control_messages.find(mac->getCurrentSlot());
		if (it != control_messages.end()) {
			if (num_slots > 1) // this shouldn't happen
				throw std::logic_error("LinkManager::onTransmissionSlot would send a control message, but num_slots>1.");
			coutd << "sending control message." << std::endl;
			segment = (*it).second;
			control_messages.erase(mac->getCurrentSlot());
			return segment;
		}
	}
	// Non-control messages can only be sent on established links.
	if (link_establishment_status != Status::link_established)
		throw std::runtime_error("LinkManager::onTransmissionSlot for an unestablished link.");
	// Query PHY for the current datarate.
	unsigned long datarate = mac->getCurrentDatarate(); // bits/slot
	unsigned long num_bits = datarate * num_slots; // bits
	// Query ARQ for a new segment.
	coutd << "requesting " << num_bits << " bits." << std::endl;
	segment = mac->requestSegment(num_bits, getLinkId());
	assert(segment->getHeaders().size() > 1 &&
	       "LinkManager::onTransmissionSlot received segment with <=1 headers.");
	for (L2Header* header : segment->getHeaders())
		setHeaderFields(header);
	if (current_reservation_timeout == TIMEOUT_THRESHOLD_TRIGGER) {
		coutd << "Timeout threshold reached -> triggering new link request!" << std::endl;
		requestNewLink();
	}
	
	return segment;
}

void LinkManager::setHeaderFields(L2Header* header) {
	switch (header->frame_type) {
		case L2Header::base:
			setBaseHeaderFields((L2HeaderBase*) header);
			break;
		case L2Header::beacon:
			setBeaconHeaderFields((L2HeaderBeacon*) header);
			break;
		case L2Header::broadcast:
			setBroadcastHeaderFields((L2HeaderBroadcast*) header);
			break;
		case L2Header::unicast:
			setUnicastHeaderFields((L2HeaderUnicast*) header);
			break;
		case L2Header::link_establishment_request:
			setRequestHeaderFields((L2HeaderLinkEstablishmentRequest*) header);
			break;
		default:
			throw std::invalid_argument("LinkManager::setHeaderFields for unsupported frame type: " + std::to_string(header->frame_type));
	}
}

void LinkManager::setBaseHeaderFields(L2HeaderBase* header) {
	coutd << "-> setting base header fields:";
	header->icao_id = mac->getMacId();
	coutd << " icao_id=" << this->link_id;
	header->offset = this->current_reservation_offset;
	coutd << " offset=" << this->current_reservation_offset;
	if (this->current_reservation_slot_length == 0)
		throw std::runtime_error("LinkManager::setBaseHeaderFields attempted to set next_length to zero.");
	header->length_next = this->current_reservation_slot_length;
	coutd << " length_next=" << this->current_reservation_slot_length;
	header->timeout = this->current_reservation_timeout;
	coutd << " timeout=" << this->current_reservation_timeout;
	if (link_id != SYMBOLIC_LINK_ID_BROADCAST) {
		if (current_reservation_timeout > 0)
			current_reservation_timeout = current_reservation_timeout - 1;
		else
			throw std::runtime_error("LinkManager::setBaseHeaderFields reached timeout of zero.");
	}
	coutd << " ";
}

void LinkManager::setBeaconHeaderFields(L2HeaderBeacon* header) const {
	throw std::runtime_error("P2P LinkManager shouldn't set beacon header fields.");
}

void LinkManager::setBroadcastHeaderFields(L2HeaderBroadcast* header) const {
	throw std::runtime_error("P2P LinkManager shouldn't set broadcast header fields.");
}

void LinkManager::setUnicastHeaderFields(L2HeaderUnicast* header) const {
	coutd << "-> setting unicast header fields:";
	coutd << " icao_dest_id=" << link_id;
	header->icao_dest_id = link_id;
	coutd << " ";
}

void LinkManager::setRequestHeaderFields(L2HeaderLinkEstablishmentRequest* header) const {
	coutd << "-> setting link establishment request header fields: ";
	coutd << " icao_dest_id=" << link_id;
	header->icao_dest_id = link_id;
	coutd << " ";
}

void LinkManager::setReservationTimeout(unsigned int reservation_timeout) {
	this->reservation_timeout = reservation_timeout;
}

void LinkManager::setReservationOffset(unsigned int reservation_offset) {
	this->current_reservation_offset = reservation_offset;
}

void LinkManager::processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) {
	throw std::runtime_error("Non-broadcast LinkManager got a beacon to process.");
}

std::vector<std::pair<const FrequencyChannel*, unsigned int>> LinkManager::processIncomingLinkEstablishmentRequest(L2HeaderLinkEstablishmentRequest*& header,
                                                                                                                   ProposalPayload*& payload) {
	assert(payload && "LinkManager::processIncomingLinkEstablishmentRequest for nullptr ProposalPayload*");
	const MacId& dest_id = header->icao_dest_id;
	if (payload->proposed_channels.empty())
		throw std::invalid_argument("LinkManager::processIncomingLinkEstablishmentRequest for an empty proposal.");
	
	// Go through all proposed channels...
	std::vector<std::pair<const FrequencyChannel*, unsigned int>> viable_candidates;
	for (size_t i = 0; i < payload->proposed_channels.size(); i++) {
		const FrequencyChannel* channel = payload->proposed_channels.at(i);
		coutd << " -> proposed channel " << channel->getCenterFrequency() << "kHz:";
		// ... and all slots proposed on this channel ...
		unsigned int num_candidates_on_this_channel = payload->num_candidates.at(i);
		for (size_t j = 0; j < num_candidates_on_this_channel; j++) {
			unsigned int slot_offset = payload->proposed_slots.at(j);
			coutd << " @" << slot_offset;
			// ... and check if they're idle for us ...
			const ReservationTable* table = reservation_manager->getReservationTable(channel);
			// ... if they are, then save them.
			if (table->isIdle(slot_offset, payload->num_slots_per_candidate) && mac->isTransmitterIdle(slot_offset, payload->num_slots_per_candidate)) {
				coutd << " (viable)";
				viable_candidates.emplace_back(channel, slot_offset);
			} else
				coutd << " (busy)";
		}
	}
	return viable_candidates;
}

void LinkManager::processIncomingLinkEstablishmentReply(L2HeaderLinkEstablishmentReply*& header) {
	// Make sure we're expecting a reply.
	if (link_establishment_status != Status::awaiting_reply) {
		std::string status = link_establishment_status == Status::link_not_established ? "link_not_established" : "link_established";
		throw std::runtime_error("LinkManager for ID '" + std::to_string(link_id.getId()) + "' received a link reply but its state is '" + status + "'.");
	}
	// The link has now been established!
	// So update the status.
	link_establishment_status = Status::link_established;
	// And mark the reservations.
	if (this->last_proposal == nullptr)
		throw std::runtime_error("LinkManager::processIncomingLinkEstablishmentReply for no remembered proposal.");
	current_reservation_slot_length = last_proposal->num_slots_per_candidate;
}

void LinkManager::processIncomingBroadcast(const MacId& origin, L2HeaderBroadcast*& header) {
	throw std::runtime_error("LinkManager::processIncomingBroadcast for P2P LinkManager.");
}

void LinkManager::processIncomingUnicast(L2HeaderUnicast*& header, L2Packet::Payload*& payload) {
	// Make sure we're the recipient.
	const MacId& recipient_id = header->icao_dest_id;
	// If we're not...
	if (recipient_id != mac->getMacId()) {
		coutd << "unicast not intended for us -> deleting it";
		// ... delete header and payload, s.t. upper layers don't attempt to process it.
		delete header;
		header = nullptr;
		delete payload;
		payload = nullptr;
	}
}

void LinkManager::processIncomingBase(L2HeaderBase*& header) {
	unsigned int timeout = header->timeout;
	unsigned int length_next = header->length_next;
	unsigned int offset = header->offset;
	coutd << "timeout=" << timeout << " length_next=" << length_next << " offset=" << offset << " -> ";
	if (current_reservation_table == nullptr) {
//		coutd << "unset reservation table -> ignore; ";
		throw std::runtime_error("LinkManager::processIncomingBase for unset ReservationTable.");
	} else if (timeout > 0) {
		coutd << " marking next " << timeout << " reservations:";
		unsigned int remaining_tx_slots = length_next - 1;
		Reservation reservation = Reservation(header->icao_id, Reservation::TX, remaining_tx_slots);
		for (size_t i = 0; i < timeout; i++) {
			int32_t current_offset = (i+1) * offset;
			current_reservation_table->mark(current_offset, reservation);
			coutd << " @" << current_offset;
		}
		coutd << " -> ";
	}
}

const std::pair<const FrequencyChannel*, unsigned int>& LinkManager::chooseCandidateSlot(
		const std::vector<std::pair<const FrequencyChannel*, unsigned int>>& candidates) const {
	if (candidates.empty())
		throw std::invalid_argument("LinkManager::chooseCandidateSlot on empty candidates.");
	// Choose randomly.
	std::random_device random_device;
	std::mt19937 generator(random_device());
	std::uniform_int_distribution<> distribution(0, candidates.size() - 1);
	return candidates.at(distribution(generator));
}

void LinkManager::assign(const FrequencyChannel* channel) {
	if (current_channel != nullptr || current_reservation_table != nullptr)
		throw std::runtime_error("LinkManager reassignment not yet implemented!");
	this->current_channel = channel;
	this->current_reservation_table = reservation_manager->getReservationTable(channel);
}


