//
// Created by Sebastian Lindner on 10.11.20.
//

#include <cassert>
#include <random>
#include "LinkManager.hpp"
#include "coutdebug.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "LinkManagementProcess.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

LinkManager::LinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac)
	: link_id(link_id), reservation_manager(reservation_manager),
	link_establishment_status((link_id == SYMBOLIC_LINK_ID_BROADCAST || link_id == SYMBOLIC_LINK_ID_BEACON) ? Status::link_established : Status::link_not_established) /* broadcast links are always established */,
	mac(mac), traffic_estimate(20) {
    link_management_process = new LinkManagementProcess(this);
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
            link_management_process->establishLink();
		} else {
			throw std::runtime_error("Unsupported LinkManager::notifyOutgoing with status: '" + std::to_string(link_establishment_status) + "'.");
		}
	}
}

void LinkManager::receiveFromLower(L2Packet*& packet) {
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
    assert(packet->getHeaders().size() == packet->getPayloads().size());
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
				link_management_process->processLinkRequest((const L2HeaderLinkEstablishmentRequest*&) header,
                                                            (const LinkManagementProcess::ProposalPayload*&) payload, packet->getOrigin());
				
				delete header;
				header = nullptr;
				delete payload;
				payload = nullptr;
				break;
			}
			case L2Header::link_establishment_reply: {
				coutd << "processing link establishment reply -> ";
				link_management_process->processLinkReply((const L2HeaderLinkEstablishmentReply*&) header, (const LinkManagementProcess::ProposalPayload*&) payload);
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
	link_management_process->scheduleLinkReply(reply, slot_offset, timeout, offset, length);
}

double LinkManager::getCurrentTrafficEstimate() const {
	return traffic_estimate.get();
}

unsigned int LinkManager::estimateCurrentNumSlots() const {
	unsigned int traffic_estimate = (unsigned int) this->traffic_estimate.get(); // in bits.
	unsigned int datarate = mac->getCurrentDatarate(); // in bits/slot.
	unsigned int num_slots = traffic_estimate / datarate; // in slots.
	return num_slots > 0 ? num_slots : 1; // in slots.
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
	link_management_process->populateRequest(packet);
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
	if (link_management_process->hasControlMessage()) {
        coutd << "fetching control message ";
        if (num_slots > 1) // Control messages should be sent during single slots.
            throw std::logic_error("LinkManager::onTransmissionSlot would send a control message, but num_slots>1.");
	    segment = link_management_process->getControlMessage();
        if (segment->getHeaders().size() == 2) {
            const L2Header* header = segment->getHeaders().at(1);
            if (header->frame_type == L2Header::FrameType::link_establishment_request) {
                coutd << "[request]... ";
                link_establishment_status = awaiting_reply;
            } else if (header->frame_type == L2Header::FrameType::link_establishment_reply) {
                coutd << "[reply]... ";
                link_establishment_status = reply_sent;
            } else
                throw std::logic_error("LinkManager::onTransmissionSlot for non-reply and non-request control message.");
        } else
            throw std::logic_error("LinkManager::onTransmissionSlot has a control message with too many or too few headers.");
    // If there are none, a new data packet can be sent.
	} else {
		// Non-control messages can only be sent on established links.
		if (link_establishment_status == Status::link_not_established)
			throw std::runtime_error("LinkManager::onTransmissionSlot for link status: " + std::to_string(link_establishment_status));
		// Query PHY for the current datarate.
		unsigned long datarate = mac->getCurrentDatarate(); // bits/slot
		unsigned long num_bits = datarate * num_slots; // bits
		// Query ARQ for a new segment.
		coutd << "requesting " << num_bits << " bits." << std::endl;
		segment = mac->requestSegment(num_bits, getLinkId());
	}
	// Update management process.
    link_management_process->onTransmissionSlot();
	// Set header fields.
	assert(segment->getHeaders().size() > 1 && "LinkManager::onTransmissionSlot received segment with <=1 headers.");
	for (L2Header* header : segment->getHeaders())
		setHeaderFields(header);
	
	return segment;
}

void LinkManager::setHeaderFields(L2Header* header) {
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
					"LinkManager::setHeaderFields for unsupported frame type: " + std::to_string(header->frame_type));
		}
	}
}

void LinkManager::setBaseHeaderFields(L2HeaderBase*& header) {
	header->icao_id = mac->getMacId();
	coutd << " icao_id=" << mac->getMacId();
	header->offset = link_management_process->getTxOffset();
	coutd << " offset=" << link_management_process->getTxOffset();
	if (link_management_process->getTxBurstSlots() == 0)
		throw std::runtime_error("LinkManager::setBaseHeaderFields attempted to set next_length to zero.");
	header->length_next = link_management_process->getTxBurstSlots();
	coutd << " length_next=" << link_management_process->getTxBurstSlots();
	header->timeout = link_management_process->getTxTimeout();
	coutd << " timeout=" << link_management_process->getTxTimeout();
	coutd << " ";
}

void LinkManager::setBeaconHeaderFields(L2HeaderBeacon*& header) const {
	throw std::runtime_error("P2P LinkManager shouldn't set beacon header fields.");
}

void LinkManager::setBroadcastHeaderFields(L2HeaderBroadcast*& header) const {
	throw std::runtime_error("P2P LinkManager shouldn't set broadcast header fields.");
}

void LinkManager::setUnicastHeaderFields(L2HeaderUnicast*& header) const {
	coutd << " icao_dest_id=" << link_id;
	header->icao_dest_id = link_id;
	coutd << " ";
}

void LinkManager::processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) {
	throw std::runtime_error("Non-broadcast LinkManager got a beacon to process.");
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
    delete link_management_process;
}


