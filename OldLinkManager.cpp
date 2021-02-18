//
// Created by Sebastian Lindner on 10.11.20.
//

#include <cassert>
#include "OldLinkManager.hpp"
#include "coutdebug.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "LinkManagementEntity.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

OldLinkManager::OldLinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac)
		: LinkManager(link_id, reservation_manager, mac),
		  traffic_estimate(20), random_device(new std::random_device), generator((*random_device)()) {
	lme = new LinkManagementEntity(this);
}

void OldLinkManager::notifyOutgoing(unsigned long num_bits) {
	coutd << *this << "::notifyOutgoing(id='" << link_id << "')";
	is_link_initiator = true;

	// Update the moving average traffic estimate.
	updateTrafficEstimate(num_bits);

	// Check establishment status.
	// If the link is established...
	if (link_establishment_status == Status::link_established || link_establishment_status == link_renewal_complete) {
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
			lme->establishLink();
		} else {
			throw std::runtime_error("Unsupported OldLinkManager::notifyOutgoing with status: '" + std::to_string(link_establishment_status) + "'.");
		}
	}
}

void OldLinkManager::onPacketReception(L2Packet*& packet) {
	coutd << *this << "::onPacketReception... ";
	coutd << "a packet from '" << packet->getOrigin() << "' ";
	if (packet->getDestination() != SYMBOLIC_ID_UNSET) {
		coutd << "to '" << packet->getDestination() << "";
		if (packet->getDestination() == mac->getMacId())
			coutd << " (us)' -> ";
		else
			coutd << "' -> ";
	}
	statistic_num_received_packets++;
	assert(!packet->getHeaders().empty() && "OldLinkManager::onPacketReception(empty packet)");
	assert(packet->getHeaders().size() == packet->getPayloads().size());
	// Go through all header and payload pairs...
	bool contains_data = false;
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
//				delete header;
//				header = nullptr;
//				delete payload;
//				payload = nullptr;
				coutd << std::endl;
				statistic_num_received_beacons++;
				break;
			}
			case L2Header::broadcast: {
				coutd << "processing broadcast -> ";
				processIncomingBroadcast(packet->getOrigin(), (L2HeaderBroadcast*&) header);
				statistic_num_received_broadcasts++;
				contains_data = true;
				break;
			}
			case L2Header::unicast: {
				coutd << "processing unicast -> ";
				processIncomingUnicast((L2HeaderUnicast*&) header, payload);
				statistic_num_received_unicasts++;
				contains_data = true;
				break;
			}
			case L2Header::link_establishment_request: {
				coutd << "processing link establishment request -> ";
				statistic_num_received_requests++;
				processIncomingLinkRequest((const L2HeaderLinkEstablishmentRequest*&) header, (const L2Packet::Payload*&) payload, packet->getOrigin());
//				delete header;
//				header = nullptr;
//				delete payload;
//				payload = nullptr;
				break;
			}
			case L2Header::link_establishment_reply: {
				coutd << "processing link establishment reply -> ";
				processIncomingLinkReply((const L2HeaderLinkEstablishmentReply*&) header, (const L2Packet::Payload*&) payload);
				statistic_num_received_replies++;
				// Delete and set to nullptr s.t. upper layers can easily ignore them.
//				delete header;
//				header = nullptr;
//				delete payload;
//				payload = nullptr;
				break;
			}
			default: {
				throw std::invalid_argument("OldLinkManager::onPacketReception for an unexpected header type.");
			}
		}
	}
	// After processing, the packet is passed to the upper layer.
	if (contains_data) {
		coutd << "passing to upper layer." << std::endl;
		mac->passToUpper(packet);
	} else {
		coutd << "deleting control packet." << std::endl;
		delete packet;
	}
}

double OldLinkManager::getCurrentTrafficEstimate() const {
	return traffic_estimate.get();
}

unsigned int OldLinkManager::estimateCurrentNumSlots() const {
	unsigned int traffic_estimate = (unsigned int) this->traffic_estimate.get(); // in bits.
	unsigned int datarate = mac->getCurrentDatarate(); // in bits/slot.
	unsigned int num_slots = traffic_estimate / datarate; // in slots.
	return num_slots > 0 ? num_slots : 1; // in slots.
}

void OldLinkManager::updateTrafficEstimate(unsigned long num_bits) {
	traffic_estimate.put(num_bits);
}

int32_t OldLinkManager::getEarliestReservationSlotOffset(int32_t start_slot, const Reservation& reservation) const {
	if (current_reservation_table == nullptr)
		throw std::runtime_error("OldLinkManager::getEarliestReservationSlotOffset has an unset reservation table.");
	return current_reservation_table->findEarliestOffset(start_slot, reservation);
}

void OldLinkManager::packetBeingSentCallback(L2Packet* packet) {
	// This callback is used only for link requests.
	// Populate the request with a proposal.
	lme->populateRequest(packet);
}

BeaconPayload* OldLinkManager::computeBeaconPayload(unsigned long max_bits) const {
	auto* payload = new BeaconPayload(mac->getMacId());
	// Fetch all local transmission reservations and copy it into the payload.
	payload->local_reservations = reservation_manager->getTxReservations(mac->getMacId());
	//!TODO kick out values until max_bits is met!
	if (payload->getBits() > max_bits)
		throw std::runtime_error("OldLinkManager::computeBeaconPayload doesn't kick out values, and we exceed the allowed number of bits.");
	return payload;
}

L2Packet* OldLinkManager::onTransmissionBurstStart(unsigned int burst_length) {
	coutd << *this << "::onTransmissionBurstStart(" << burst_length << " slots) -> ";
	L2Packet* segment = nullptr;
	// Prioritize control messages.
	bool sending_reply = false, has_control_message = lme->hasControlMessage();
	if (has_control_message) {
		coutd << "fetching control message ";
		segment = lme->getControlMessage();
		if (segment->getHeaders().size() == 2) {
			const L2Header* header = segment->getHeaders().at(1);
			if (header->frame_type == L2Header::FrameType::link_establishment_request) {
				coutd << "[request]... ";
				link_establishment_status = awaiting_reply;
				lme->onRequestTransmission();
			} else if (header->frame_type == L2Header::FrameType::link_establishment_reply) {
				coutd << "[reply]... ";
				link_establishment_status = (link_establishment_status == link_not_established ? awaiting_data_tx : link_renewal_complete);
				sending_reply = true;
			} else
				throw std::logic_error("OldLinkManager::onTransmissionBurstStart for non-reply and non-request control message.");
		} else
			throw std::logic_error("OldLinkManager::onTransmissionBurstStart has a control message with too many or too few headers.");
	} else {
		// Non-control messages can only be sent on established links.
		if (link_establishment_status == Status::link_not_established)
			return nullptr;
		// Query PHY for the current datarate.
		unsigned long datarate = mac->getCurrentDatarate(); // bits/slot
		unsigned long num_bits = datarate * burst_length; // bits
		// Query ARQ for a new segment.
		coutd << "requesting " << num_bits << " bits." << std::endl;
		segment = mac->requestSegment(num_bits, getLinkId());
	}

	// In some cases, application data can be appended to a control message, such as
	// (1) this is the link initiator AND the link is established
	// (2) this is the link initiator AND the link is being renewed
	// (3) this is the link initiator AND the link renewal has concluded
	if (is_link_initiator && has_control_message && (link_establishment_status == link_established || (link_establishment_status == awaiting_reply && lme->isLinkRenewalPending()) || link_establishment_status == link_renewal_complete)) {
		unsigned long datarate = mac->getCurrentDatarate(); // bits/slot
		unsigned long num_bits = datarate * burst_length - segment->getBits(); // bits
		coutd << "requesting additional " << num_bits << " bits from upper layer to append to control message." << std::endl;
		L2Packet* data_segment = mac->requestSegment(num_bits, getLinkId());
		for (size_t i = 1; i < data_segment->getHeaders().size(); i++)
			segment->addPayload(data_segment->getHeaders().at(i)->copy(), data_segment->getPayloads().at(i)->copy());
		delete data_segment;
	}

	assert(segment->getHeaders().size() > 1 && "OldLinkManager::onTransmissionBurstStart received segment with <=1 headers.");
	if (!sending_reply) {
		// Set header fields.
		for (L2Header* header : segment->getHeaders())
			setHeaderFields(header);
	}

	lme->onTransmissionSlot();

	statistic_num_sent_packets++;
	return segment;
}

void OldLinkManager::setHeaderFields(L2Header* header) {
	switch (header->frame_type) {
		case L2Header::base: {
			coutd << "setting base header fields:";
			setBaseHeaderFields((L2HeaderBase*&) header);
			break;
		}
		case L2Header::beacon: {
			coutd << "-> setting beacon header fields:";
			setBeaconHeaderFields((L2HeaderBeacon*&) header);
			break;
		}
		case L2Header::broadcast: {
			coutd << "-> setting broadcast header fields:";
			setBroadcastHeaderFields((L2HeaderBroadcast*&) header);
			break;
		}
		case L2Header::unicast: {
			coutd << "-> setting unicast header fields:";
			setUnicastHeaderFields((L2HeaderUnicast*&) header);
			break;
		}
		case L2Header::link_establishment_request: {
			coutd << "-> setting link establishment request header fields: ";
			setUnicastHeaderFields((L2HeaderUnicast*&) header);
			break;
		}
		case L2Header::link_establishment_reply: {
			coutd << "-> setting link establishment reply header fields: ";
			setUnicastHeaderFields((L2HeaderUnicast*&) header);
			break;
		}
		default: {
			throw std::invalid_argument(
					"OldLinkManager::setHeaderFields for unsupported frame type: " + std::to_string(header->frame_type));
		}
	}
	coutd << "-> ";
}

void OldLinkManager::setBaseHeaderFields(L2HeaderBase*& header) {
	header->icao_src_id = mac->getMacId();
	coutd << " icao_src_id=" << mac->getMacId();
	header->offset = lme->getTxOffset();
	coutd << " offset=" << lme->getTxOffset();
	if (lme->getTxBurstSlots() == 0)
		throw std::runtime_error("OldLinkManager::setBaseHeaderFields attempted to set next_length to zero.");
	header->length_next = lme->getTxBurstSlots();
	coutd << " length_next=" << lme->getTxBurstSlots();
	header->timeout = lme->getTxTimeout();
	coutd << " timeout=" << lme->getTxTimeout();
	coutd << " ";
}

void OldLinkManager::setBeaconHeaderFields(L2HeaderBeacon*& header) const {
	throw std::runtime_error("P2P OldLinkManager shouldn't set beacon header fields.");
}

void OldLinkManager::setBroadcastHeaderFields(L2HeaderBroadcast*& header) const {
	throw std::runtime_error("P2P OldLinkManager shouldn't set broadcast header fields.");
}

void OldLinkManager::setUnicastHeaderFields(L2HeaderUnicast*& header) const {
	coutd << " icao_dest_id=" << link_id;
	header->icao_dest_id = link_id;
	coutd << " ";
}

void OldLinkManager::processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) {
	throw std::runtime_error("OldLinkManager::processIncomingBeacon for P2P OldLinkManager.");
}

void OldLinkManager::processIncomingBroadcast(const MacId& origin, L2HeaderBroadcast*& header) {
	throw std::runtime_error("OldLinkManager::processIncomingBroadcast for P2P OldLinkManager.");
}

void OldLinkManager::processIncomingUnicast(L2HeaderUnicast*& header, L2Packet::Payload*& payload) {
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
		// ... onSlotStart status if we've been expecting it.
		if (link_establishment_status == awaiting_data_tx) {
			coutd << "link is now established -> ";
			link_establishment_status = link_established;
			mac->notifyAboutNewLink(link_id);
		} else if (link_establishment_status != link_established && link_establishment_status != link_renewal_complete) {
			throw std::runtime_error("OldLinkManager::processIncomingUnicast for some status other than 'link_established' or 'awaiting_data_tx' or 'link_renewal_complete': " + std::to_string(link_establishment_status));
		}
	}
}

void OldLinkManager::processIncomingBase(L2HeaderBase*& header) {
	unsigned int timeout = header->timeout;
	unsigned int offset = header->offset;
	coutd << "timeout=" << timeout << " offset=" << offset << " -> ";
	if (link_establishment_status == link_not_established && timeout == 0) {
		coutd << "unestablished link and zero timeout, so not processing this further -> ";
		return;
	}
	if (link_establishment_status == awaiting_reply) {
		coutd << "awaiting reply, so not processing this further -> ";
		return;
	}
	coutd << "updating link management parameters: ";
	coutd << "timeout:";
	if (lme->getTxTimeout() != timeout) {
		coutd << lme->getTxTimeout() << "->" << timeout << " ";
		lme->setTxTimeout(timeout);
	} else
		coutd << "(unchanged@" << lme->getTxTimeout() << ")";
	coutd << ", offset:";
	if (lme->getTxOffset() != offset) {
		coutd << lme->getTxOffset() << "->" << offset << " ";
		lme->setTxOffset(offset);
	} else
		coutd << "(unchanged@" << lme->getTxOffset() << ")";
	coutd << ", length_next:";
	coutd << "updating reservations: ";
	// This is an incoming packet, so we must've been listening.
	// Mark future slots as RX slots, too.
	markReservations(timeout - 1, 0, offset, lme->getTxBurstSlots(), header->icao_src_id, Reservation::RX);
	coutd << " -> ";
}

void OldLinkManager::processIncomingLinkRequest(const L2HeaderLinkEstablishmentRequest*& header, const L2Packet::Payload*& payload, const MacId& origin) {
	lme->processLinkRequest(header, (const LinkManagementEntity::ProposalPayload*&) payload, origin);
}

void OldLinkManager::processIncomingLinkReply(const L2HeaderLinkEstablishmentReply*& header, const L2Packet::Payload*& payload) {
	lme->processLinkReply(header, (const LinkManagementEntity::ProposalPayload*&) payload);
}

void OldLinkManager::reassign(const FrequencyChannel* channel) {
	this->current_channel = channel;
	this->current_reservation_table = reservation_manager->getReservationTable(channel);
}

size_t OldLinkManager::getRandomInt(size_t start, size_t end) {
	if (start == end)
		return start;
	std::uniform_int_distribution<> distribution(start, end - 1);
	return distribution(generator);
}

std::vector<unsigned int> OldLinkManager::markReservations(ReservationTable* table, unsigned int timeout, unsigned int init_offset, unsigned int offset, const Reservation& reservation) {
    std::vector<unsigned int> offsets;
	coutd << "marking next " << timeout << " " << reservation.getNumRemainingSlots() + 1 << "-slot-" << reservation.getAction() << " reservations:";
	for (size_t i = 0; i < timeout; i++) {
		int32_t current_offset = (i + 1) * offset + init_offset;
		const Reservation& current_reservation = table->getReservation(current_offset);
		if (current_reservation != reservation)
			table->mark(current_offset, reservation);
		if (current_reservation.getAction() != reservation.getAction())
			coutd << " t=" << current_offset << ":" << current_reservation << "->" << reservation;
		else
			coutd << " t=" << current_offset << ":" << reservation;
		offsets.push_back(current_offset);
	}
	return offsets;
}

void OldLinkManager::markReservations(unsigned int timeout, unsigned int init_offset, unsigned int offset, unsigned int length, const MacId& target_id, Reservation::Action action) {
	if (current_reservation_table == nullptr)
		throw std::runtime_error("OldLinkManager::markReservations for unset ReservationTable.");
	unsigned int remaining_slots = length > 0 ? length - 1 : 0;
	Reservation reservation = Reservation(target_id, action, remaining_slots);
	markReservations(current_reservation_table, timeout, init_offset, offset, reservation);
}

OldLinkManager::~OldLinkManager() {
	delete lme;
	delete random_device;
}

void OldLinkManager::onSlotStart(uint64_t num_slots) {
	if (!traffic_estimate.hasBeenUpdated())
		for (uint64_t t = 0; t < num_slots; t++) {
			traffic_estimate.put(0);
			traffic_estimate.reset();
		}
	traffic_estimate.reset();
	lme->update(num_slots);
}

void OldLinkManager::onReceptionBurstStart(unsigned int burst_length) {
    lme->onReceptionSlot();
}

void OldLinkManager::onSlotEnd() {
	lme->onSlotEnd();
}

void OldLinkManager::onReceptionBurst(unsigned int remaining_burst_length) {

}

void OldLinkManager::onTransmissionBurst(unsigned int remaining_burst_length) {

}


