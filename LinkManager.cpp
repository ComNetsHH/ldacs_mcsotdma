//
// Created by Sebastian Lindner on 10.11.20.
//

#include <cassert>
#include <random>
#include "LinkManager.hpp"
#include "coutdebug.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "LinkRenewalProcess.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

LinkManager::LinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac)
	: link_id(link_id), reservation_manager(reservation_manager),
	link_establishment_status((link_id == SYMBOLIC_LINK_ID_BROADCAST || link_id == SYMBOLIC_LINK_ID_BEACON) ? Status::link_established : Status::link_not_established) /* broadcast links are always established */,
	  mac(mac), traffic_estimate(20) {
	    link_renewal_process = new LinkRenewalProcess(this);
	}

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

void LinkManager::receiveFromLower(L2Packet*& packet, FrequencyChannel* channel) {
	coutd << "LinkManager(" << link_id << ")::receiveFromLower... ";
	coutd << "a packet from '" << packet->getOrigin() << "' ";
	if (packet->getDestination() != SYMBOLIC_ID_UNSET) {
		coutd << "to '" << packet->getDestination() << "";
		if (packet->getDestination() == mac->getMacId())
			coutd << " (us)' -> ";
		else
			coutd << "' -> ";
	}
	assert(!packet->getHeaders().empty() && "LinkManager::receiveFromLower(empty packet)");
	// Go through all header and payload pairs...
	for (size_t i = 0; i < packet->getHeaders().size(); i++) {
		auto*& header = (L2Header*&) packet->getHeaders().at(i);
		auto*& payload = (L2Packet::Payload*&) packet->getPayloads().at(i);
		switch (header->frame_type) {
			case L2Header::base: {
				coutd << "processing base header -> ";
				auto*& base_header = (L2HeaderBase*&) header;
				processIncomingBase(base_header);
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
					auto chosen_candidate = viable_candidates.at(getRandomInt(0, viable_candidates.size()));
					coutd << " -> picked candidate (" << chosen_candidate.first->getCenterFrequency() << "kHz, offset " << chosen_candidate.second << ") -> ";
					// Prepare a link reply.
					L2Packet* reply = prepareLinkEstablishmentReply(packet->getOrigin());
					const FrequencyChannel* reply_channel = chosen_candidate.first;
					int32_t slot_offset = chosen_candidate.second;
					// Pass it on to the corresponding LinkManager (this could've been received on the broadcast channel).
					coutd << "passing on to corresponding LinkManager -> ";
					unsigned int timeout = ((L2HeaderLinkEstablishmentRequest*&) header)->timeout,
								 offset = ((L2HeaderLinkEstablishmentRequest*&) header)->offset,
								 length = ((L2HeaderLinkEstablishmentRequest*&) header)->length_next;
					
					// The request may have been received by the broadcast link manager,
					// while the reply must be sent on a unicast channel,
					// so we have to forward the reply to the corresponding unicast link manager.
					mac->forwardLinkReply(reply, reply_channel, slot_offset, timeout, offset, length);
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
				processIncomingLinkEstablishmentReply((L2HeaderLinkEstablishmentReply*&) header, channel);
				// Delete and set to nullptr s.t. upper layers can easily ignore them.
				delete header;
				header = nullptr;
				delete payload;
				payload = nullptr;
				break;
			}
			default: {
				throw std::invalid_argument("LinkManager::receiveFromLower for an unexpected header type.");
			}
		}
	}
	// After processing, the packet is passed to the upper layer.
	coutd << " -> passing to upper layer." << std::endl;
	mac->passToUpper(packet);
}

void LinkManager::scheduleLinkReply(L2Packet* reply, int32_t slot_offset, unsigned int timeout, unsigned int offset, unsigned int length) {
	uint64_t absolute_slot = mac->getCurrentSlot() + slot_offset;
	auto it = scheduled_link_replies.find(absolute_slot);
	if (it != scheduled_link_replies.end())
		throw std::runtime_error("LinkManager::scheduleLinkReply wanted to schedule a link reply, but there's already one scheduled at slot " + std::to_string(absolute_slot) + ".");
	else {
		// ... schedule it.
		if (current_reservation_table->isUtilized(slot_offset))
			throw std::invalid_argument("LinkManager::scheduleLinkReply for an already reserved slot.");
		current_reservation_table->mark(slot_offset, Reservation(reply->getDestination(), Reservation::Action::TX));
		scheduled_link_replies[absolute_slot] = reply;
		coutd << "-> scheduled reply in " << slot_offset << " slots." << std::endl;
		// ... and mark reservations: we're sending a reply, so we're the receiver.
		markReservations(timeout, slot_offset, offset, length, reply->getDestination(), Reservation::Action::RX);
	}
}

double LinkManager::getCurrentTrafficEstimate() const {
	return traffic_estimate.get();
}

void LinkManager::requestNewLink() {
	coutd << "requesting new link... ";
	// An established link can just update its status, so that the next transmission slot sends a request.
	if (link_establishment_status == link_established) {
		link_establishment_status = link_expired;
		coutd << "set status to 'link_expired'." << std::endl;
	// An unestablished link must resort to injecting it into the upper layer as a broadcast.
	} else if (link_establishment_status == link_not_established) {
		// Prepare a link request and inject it into the RLC sublayer above.
		L2Packet* request = prepareLinkEstablishmentRequest();
		coutd << "prepared link establishment request... ";
		mac->injectIntoUpper(request);
		coutd << "injected into upper layer... ";
		// We are now awaiting a reply.
		this->link_establishment_status = Status::awaiting_reply;
		coutd << "updated status to 'awaiting_reply'." << std::endl;
	}
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
	MacId dest_id = link_establishment_status == link_not_established ? SYMBOLIC_LINK_ID_BROADCAST : link_id;
	auto* request_header = new L2HeaderLinkEstablishmentRequest(dest_id, link_should_be_arq_protected, 0, 0, 0);
	auto* body = new ProposalPayload(this->num_proposed_channels, this->num_proposed_slots);
	request->addPayload(request_header, body);
	request->addCallback(this);
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

unsigned int LinkManager::estimateCurrentNumSlots() const {
	unsigned int traffic_estimate = (unsigned int) this->traffic_estimate.get(); // in bits.
	unsigned int datarate = mac->getCurrentDatarate(); // in bits/slot.
	unsigned int num_slots = traffic_estimate / datarate; // in slots.
	return num_slots > 0 ? num_slots : 1; // in slots.
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

void LinkManager::packetBeingSentCallback(L2Packet* packet) {
	// This callback is used only for link requests.
	for (size_t i = 0; i < packet->getHeaders().size(); i++) {
		L2Header* header = packet->getHeaders().at(i);
		if (header->frame_type == L2Header::link_establishment_request) {
			// Set the destination ID (may be broadcast until now).
			((L2HeaderLinkEstablishmentRequest*) header)->icao_dest_id = link_id;
			((L2HeaderLinkEstablishmentRequest*) header)->offset = tx_offset;
			((L2HeaderLinkEstablishmentRequest*) header)->timeout = tx_timeout;
			((L2HeaderLinkEstablishmentRequest*) header)->length_next = tx_burst_num_slots;
			// Compute a current proposal.
			packet->getPayloads().at(i) = p2pSlotSelection();
			coutd << "-> computed link proposal -> ";
			break;
		}
	}
}

LinkManager::ProposalPayload* LinkManager::p2pSlotSelection() {
	auto* proposal = new ProposalPayload(num_proposed_channels, num_proposed_slots);
	
	// Find resource proposals...
	// ... get the P2P reservation tables sorted by their numbers of idle slots ...
	auto table_priority_queue = reservation_manager->getSortedP2PReservationTables();
	// ... until we have considered the target number of channels ...
	tx_burst_num_slots = estimateCurrentNumSlots();
	for (size_t num_channels_considered = 0; num_channels_considered < this->num_proposed_channels; num_channels_considered++) {
		if (table_priority_queue.empty()) // we could just stop here, but we're throwing an error to be aware when it happens
			throw std::runtime_error("LinkManager::prepareLinkEstablishmentRequest has considered " + std::to_string(num_channels_considered) + " out of " + std::to_string(num_proposed_channels) + " and there are no more.");
		// ... get the next reservation table ...
		ReservationTable* table = table_priority_queue.top();
		table_priority_queue.pop();
		// ... and try to find candidate slots ...
		std::vector<int32_t> candidate_slots = table->findCandidateSlots(this->minimum_slot_offset_for_new_slot_reservations, this->num_proposed_slots, tx_burst_num_slots, true);
		// ... and lock them s.t. future proposals don't consider them.
		table->lock(candidate_slots);
		
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
	coutd << "LinkManager(" << link_id << ")::onTransmissionSlot... ";
	L2Packet* segment;
	// Prioritize control messages.
	// Control message through scheduled link replies...
	if (!scheduled_link_replies.empty() && scheduled_link_replies.find(mac->getCurrentSlot()) != scheduled_link_replies.end()) {
		coutd << "sending control message." << std::endl;
		if (num_slots > 1) // Control messages should be sent during single slots.
			throw std::logic_error("LinkManager::onTransmissionSlot would send a control message, but num_slots>1.");
		auto it = scheduled_link_replies.find(mac->getCurrentSlot());
		segment = (*it).second;
		scheduled_link_replies.erase(mac->getCurrentSlot());
		assert(segment->getHeaders().size() == 2);
		if (segment->getHeaders().size() == 2) {
			const L2Header* header = segment->getHeaders().at(1);
			
			if (header->frame_type == L2Header::FrameType::link_establishment_request) {
				link_establishment_status = awaiting_reply;
			} else if (header->frame_type == L2Header::FrameType::link_establishment_reply) {
				link_establishment_status = reply_sent;
			} else
				throw std::logic_error("LinkManager::onTransmissionSlot for non-reply and non-request control message.");
		} else
			throw std::logic_error("LinkManager::onTransmissionSlot has a control message with too many or too few headers.");
		
		assert(segment->getHeaders().at(1)->frame_type == L2Header::FrameType::link_establishment_reply);
		// Link replies don't need a setting of their header fields.
		return segment;
	// Control message through link requests...
	} else if (link_establishment_status == link_expired || link_establishment_status == awaiting_reply) {
		segment = prepareLinkEstablishmentRequest(); // Sets the callback, s.t. the actual proposal is computed then.
		link_establishment_status = awaiting_reply;
	// Non-control messages...
	} else {
		// Non-control messages can only be sent on established links.
		if (link_establishment_status != Status::link_established)
			throw std::runtime_error("LinkManager::onTransmissionSlot for an unestablished link.");
		// Query PHY for the current datarate.
		unsigned long datarate = mac->getCurrentDatarate(); // bits/slot
		unsigned long num_bits = datarate * num_slots; // bits
		// Query ARQ for a new segment.
		coutd << "requesting " << num_bits << " bits." << std::endl;
		segment = mac->requestSegment(num_bits, getLinkId());
		tx_timeout--;
		if (tx_timeout == TIMEOUT_THRESHOLD_TRIGGER) {
			coutd << "Timeout threshold reached -> triggering new link request!" << std::endl;
			requestNewLink();
		}
	}
	// Set header fields.
	assert(segment->getHeaders().size() > 1 && "LinkManager::onTransmissionSlot received segment with <=1 headers.");
	for (L2Header* header : segment->getHeaders())
		setHeaderFields(header);
	
	return segment;
}

void LinkManager::setHeaderFields(L2Header* header) {
	switch (header->frame_type) {
		case L2Header::base: {
			setBaseHeaderFields((L2HeaderBase*) header);
			break;
		}
		case L2Header::beacon: {
			setBeaconHeaderFields((L2HeaderBeacon*) header);
			break;
		}
		case L2Header::broadcast: {
			setBroadcastHeaderFields((L2HeaderBroadcast*) header);
			break;
		}
		case L2Header::unicast: {
			setUnicastHeaderFields((L2HeaderUnicast*) header);
			break;
		}
		case L2Header::link_establishment_request: {
			setRequestHeaderFields((L2HeaderLinkEstablishmentRequest*) header);
			break;
		}
		default: {
			throw std::invalid_argument(
					"LinkManager::setHeaderFields for unsupported frame type: " + std::to_string(header->frame_type));
		}
	}
}

void LinkManager::setBaseHeaderFields(L2HeaderBase* header) {
	coutd << "setting base header fields:";
	header->icao_id = mac->getMacId();
	coutd << " icao_id=" << mac->getMacId();
	header->offset = this->tx_offset;
	coutd << " offset=" << this->tx_offset;
	if (this->tx_burst_num_slots == 0)
		throw std::runtime_error("LinkManager::setBaseHeaderFields attempted to set next_length to zero.");
	header->length_next = this->tx_burst_num_slots;
	coutd << " length_next=" << this->tx_burst_num_slots;
	header->timeout = this->tx_timeout;
	coutd << " timeout=" << this->tx_timeout;
	if (link_id != SYMBOLIC_LINK_ID_BROADCAST) {
		if (tx_timeout == 0)
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
	this->default_tx_timeout = reservation_timeout;
}

void LinkManager::setReservationOffset(unsigned int reservation_offset) {
	this->tx_offset = reservation_offset;
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

void LinkManager::processIncomingLinkEstablishmentReply(L2HeaderLinkEstablishmentReply*& header, FrequencyChannel*& channel) {
	// Make sure we're expecting a reply.
	if (link_establishment_status != Status::awaiting_reply)
		throw std::runtime_error("LinkManager for ID '" + std::to_string(link_id.getId()) + "' received a link reply but its state is '" + std::to_string(link_establishment_status) + "'.");
	// The link has now been established!
	// So update the status.
	link_establishment_status = Status::link_established;
	mac->notifyAboutNewLink(link_id);
	assign(channel);
	// And mark the reservations.
	// We've received a reply, so we have initiated this link, so we are the transmitter.
	tx_timeout = default_tx_timeout;
	markReservations(tx_timeout, 0, tx_offset, tx_burst_num_slots, link_id, Reservation::TX);
	// Refresh the link renewal process.
	this->link_renewal_process->configure(link_renewal_attempts, tx_timeout, 0, tx_offset);
	coutd << "link is now established";
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
	// ... and if we are ...
	} else {
		// ... update status if we've been expecting it.
		if (link_establishment_status == reply_sent) {
			coutd << "link is now established";
			link_establishment_status = link_established;
			mac->notifyAboutNewLink(link_id);
		} else if (link_establishment_status != link_established) {
			throw std::runtime_error("LinkManager::processIncomingUnicast for some status other than 'link_established' or 'reply_sent': " + std::to_string(link_establishment_status));
		}
	}
}

void LinkManager::processIncomingBase(L2HeaderBase*& header) {
	unsigned int timeout = header->timeout;
	unsigned int length_next = header->length_next;
	unsigned int offset = header->offset;
	coutd << "timeout=" << timeout << " length_next=" << length_next << " offset=" << offset << " -> ";
	if (timeout > 0) {
		coutd << " marking next " << timeout << " reservations:";
		// This is an incoming packet, so we must've been listening.
		// Mark future slots as RX slots, too.
		markReservations(timeout, 0, offset, length_next, header->icao_id, Reservation::RX);
		coutd << " -> ";
	}
}

void LinkManager::assign(const FrequencyChannel* channel) {
	if (current_channel != nullptr || current_reservation_table != nullptr)
		throw std::runtime_error("LinkManager reassignment not yet implemented!");
	this->current_channel = channel;
	this->current_reservation_table = reservation_manager->getReservationTable(channel);
}

size_t LinkManager::getRandomInt(size_t start, size_t end) {
	std::random_device random_device;
	std::mt19937 generator(random_device());
	std::uniform_int_distribution<> distribution(0, end - 1);
	return distribution(generator);
}

void LinkManager::markReservations(unsigned int timeout, unsigned int init_offset, unsigned int offset, unsigned int length, const MacId& target_id, Reservation::Action action) {
	if (current_reservation_table == nullptr)
		throw std::runtime_error("LinkManager::markReservations for unset ReservationTable.");
	coutd << " marking next " << timeout << " reservations (offset=" << offset << ", init_offset=" << init_offset << ", length=" << length << ", target_id=" << target_id << ", action=" << action << ")";
	unsigned int remaining_slots = length > 0 ? length - 1 : 0;
	Reservation reservation = Reservation(target_id, action, remaining_slots);
	for (size_t i = 0; i < timeout; i++) {
		int32_t current_offset = (i+1) * offset + init_offset;
		current_reservation_table->mark(current_offset, reservation);
		coutd << " @" << current_offset;
	}
	coutd << " -> ";
}

LinkManager::~LinkManager() {
    delete link_renewal_process;
}


