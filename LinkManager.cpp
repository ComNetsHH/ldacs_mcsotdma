//
// Created by seba on 2/18/21.
//

#include <set>
#include <cassert>
#include "LinkManager.hpp"
#include "MCSOTDMA_Mac.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

void LinkManager::assign(const FrequencyChannel* channel) {
	if (current_channel == nullptr && current_reservation_table == nullptr) {
		this->current_channel = channel;
		this->current_reservation_table = reservation_manager->getReservationTable(channel);
		coutd << "assigned channel ";
		if (channel == nullptr)
			coutd << "NONE";
		else
			coutd << *channel;
		coutd << " -> ";
	} else
		coutd << *this << "::assign, but channel or reservation table are already assigned; ignoring -> ";
}

void LinkManager::onPacketReception(L2Packet*& packet) {
	coutd << *mac << "::" << *this << "::onPacketReception... ";
	coutd << "a packet from '" << packet->getOrigin() << "' ";
	if (packet->getDestination() != SYMBOLIC_ID_UNSET) {
		coutd << "to '" << packet->getDestination() << "";
		if (packet->getDestination() == mac->getMacId())
			coutd << " (us)' -> ";
		else
			coutd << "' -> ";
	}

	assert(!packet->getHeaders().empty() && "LinkManager::onPacketReception(empty packet)");
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
				mac->statisticReportBeaconReceived();
				break;
			}
			case L2Header::broadcast: {
				coutd << "processing broadcast -> ";
				processIncomingBroadcast(packet->getOrigin(), (L2HeaderBroadcast*&) header);
				contains_data = true;
				break;
			}
			case L2Header::unicast: {
				coutd << "processing unicast -> ";
				processIncomingUnicast((L2HeaderUnicast*&) header, payload);
				contains_data = true;
				break;
			}
			case L2Header::link_establishment_request: {
				coutd << "processing link establishment request -> ";
				processIncomingLinkRequest((const L2Header*&) header, (const L2Packet::Payload*&) payload, packet->getOrigin());
//				delete header;
//				header = nullptr;
//				delete payload;
//				payload = nullptr;
				break;
			}
			case L2Header::link_establishment_reply: {
				coutd << "processing link establishment reply -> ";
				processIncomingLinkReply((const L2HeaderLinkEstablishmentReply*&) header, (const L2Packet::Payload*&) payload);
				// Delete and set to nullptr s.t. upper layers can easily ignore them.
//				delete header;
//				header = nullptr;
//				delete payload;
//				payload = nullptr;
				break;
			}
			case L2Header::link_info: {
				coutd << "processing link info -> ";
				processIncomingLinkInfo((const L2HeaderLinkInfo*&) header, (const LinkInfoPayload*&) payload);
				mac->statisticReportLinkReplyReceived();
				break;
			}
			default: {
				throw std::invalid_argument("LinkManager::onPacketReception for an unexpected header type.");
			}
		}
	}
	// After processing, the packet is passed to the upper layer.
	coutd << "processing done -> ";
	if (contains_data) {
		coutd << "passing to upper layer." << std::endl;
		mac->passToUpper(packet);
	} else {
		coutd << "deleting control packet." << std::endl;
		delete packet;
	}
}

void LinkManager::processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) {
	coutd << *this << "::processIncomingBeacon" << std::endl;
	throw std::runtime_error("not implemented");
}

void LinkManager::processIncomingBroadcast(const MacId& origin, L2HeaderBroadcast*& header) {
	coutd << *this << "::processIncomingBroadcast" << std::endl;
	throw std::runtime_error("not implemented");
}

void LinkManager::processIncomingUnicast(L2HeaderUnicast*& header, L2Packet::Payload*& payload) {
	coutd << *this << "::processIncomingUnicast" << std::endl;
	throw std::runtime_error("not implemented");
}

void LinkManager::processIncomingBase(L2HeaderBase*& header) {
	coutd << *this << "::processIncomingBase" << std::endl;
	throw std::runtime_error("not implemented");
}

void LinkManager::processIncomingLinkRequest(const L2Header*& header, const L2Packet::Payload*& payload, const MacId& origin) {
	coutd << *this << "::processIncomingLinkRequest" << std::endl;
	throw std::runtime_error("not implemented");
}

void LinkManager::processIncomingLinkReply(const L2HeaderLinkEstablishmentReply*& header, const L2Packet::Payload*& payload) {
	coutd << *this << "::processIncomingLinkReply" << std::endl;
	throw std::runtime_error("not implemented");
}

void LinkManager::processIncomingLinkInfo(const L2HeaderLinkInfo*& header, const LinkInfoPayload*& payload) {
	coutd << *this << "::processIncomingLinkInfo" << std::endl;
	throw std::runtime_error("not implemented");
}

void LinkManager::onSlotEnd() {}