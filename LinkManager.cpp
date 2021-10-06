//
// Created by seba on 2/18/21.
//

#include <set>
#include <cassert>
#include <sstream>
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
		if (packet->getDestination() == mac->getMacId()) {
			coutd << " (us)' -> ";
			mac->statisticReportUnicastReceived();
		} else {
			coutd << "' -> ";
			if (packet->getDestination() == SYMBOLIC_LINK_ID_BROADCAST)
				mac->statisticReportBroadcastReceived();
			else if (packet->getDestination() == SYMBOLIC_LINK_ID_BEACON)
				mac->statisticReportBeaconReceived();
			else {
				std::stringstream ss;
				ss << *mac << "::" << *this << "::onPacketReception(unsupported destination): " << packet->getDestination();
				throw std::invalid_argument(ss.str());
			}
		}
	} else {
		std::stringstream ss;
		ss << *mac << "::" << *this << "::onPacketReception(unsupported destination): " << packet->getDestination();
		throw std::invalid_argument(ss.str());
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
				processBaseMessage(base_header);						
				break;
			}
			case L2Header::beacon: {
				coutd << "processing beacon -> ";
				processBeaconMessage(packet->getOrigin(), (L2HeaderBeacon*&) header, (BeaconPayload*&) payload);				
				break;
			}
			case L2Header::broadcast: {
				coutd << "processing broadcast -> ";
				processBroadcastMessage(packet->getOrigin(), (L2HeaderBroadcast*&) header);
				contains_data = true;
				break;
			}
			case L2Header::unicast: {
				coutd << "processing unicast -> ";
				processUnicastMessage((L2HeaderUnicast*&) header, payload);
				contains_data = true;
				break;
			}
			case L2Header::link_establishment_request: {
				coutd << "processing link establishment request -> ";
				processLinkRequestMessage((const L2Header*&) header, (const L2Packet::Payload*&) payload, packet->getOrigin());				
				break;
			}
			case L2Header::link_establishment_reply: {
				coutd << "processing link establishment reply -> ";
				processLinkReplyMessage((const L2HeaderLinkEstablishmentReply*&) header, (const L2Packet::Payload*&) payload);				
				break;
			}
			case L2Header::link_info: {
				coutd << "processing link info -> ";
				processLinkInfoMessage((const L2HeaderLinkInfo*&) header, (const LinkInfoPayload*&) payload);				
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

void LinkManager::processBeaconMessage(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) {
	coutd << *this << "::processBeaconMessage" << std::endl;
	throw std::runtime_error("not implemented");
}

void LinkManager::processBroadcastMessage(const MacId& origin, L2HeaderBroadcast*& header) {
	coutd << *this << "::processBroadcastMessage" << std::endl;
	throw std::runtime_error("not implemented");
}

void LinkManager::processUnicastMessage(L2HeaderUnicast*& header, L2Packet::Payload*& payload) {
	coutd << *this << "::processUnicastMessage" << std::endl;
	throw std::runtime_error("not implemented");
}

void LinkManager::processBaseMessage(L2HeaderBase*& header) {
	coutd << *this << "::processBaseMessage" << std::endl;
	throw std::runtime_error("not implemented");
}

void LinkManager::processLinkRequestMessage(const L2Header*& header, const L2Packet::Payload*& payload, const MacId& origin) {
	coutd << *this << "::processLinkRequestMessage" << std::endl;
	throw std::runtime_error("not implemented");
}

void LinkManager::processLinkReplyMessage(const L2HeaderLinkEstablishmentReply*& header, const L2Packet::Payload*& payload) {
	coutd << *this << "::processLinkReplyMessage" << std::endl;
	throw std::runtime_error("not implemented");
}

void LinkManager::processLinkInfoMessage(const L2HeaderLinkInfo*& header, const LinkInfoPayload*& payload) {
	coutd << *this << "::processLinkInfoMessage" << std::endl;
	throw std::runtime_error("not implemented");
}

void LinkManager::onSlotEnd() {}

unsigned int LinkManager::measureMacDelay() {	
	unsigned int now = mac->getCurrentSlot();	
	unsigned int mac_delay = now - time_since_last_channel_access;
	time_since_last_channel_access = now;		
	return mac_delay;
}