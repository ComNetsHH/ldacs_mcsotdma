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
	if ((current_channel == nullptr && current_reservation_table == nullptr) || channel == nullptr) {
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
			if (packet->getDestination() == SYMBOLIC_LINK_ID_BROADCAST || packet->getDestination() == SYMBOLIC_LINK_ID_BEACON)
				mac->statisticReportBroadcastReceived();			
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
			// case L2Header::base: {
			// 	coutd << "processing base header -> ";
			// 	auto*& base_header = (L2HeaderBase*&) header;
			// 	try {
			// 		processBaseMessage(base_header);
			// 	} catch (const std::exception &e) {
			// 		std::stringstream ss;
			// 		ss << *mac << "::" << *this << " error processing base message: " << e.what() << std::endl;					
			// 		throw std::runtime_error(ss.str());
			// 	}
			// 	break;
			// }
			// case L2Header::beacon: {
			// 	coutd << "processing beacon -> ";
			// 	try {
			// 		processBeaconMessage(packet->getOrigin(), (L2HeaderBeacon*&) header, (BeaconPayload*&) payload);				
			// 	} catch (const std::exception &e) {
			// 		std::stringstream ss;
			// 		ss << *mac << "::" << *this << " error processing beacon message: " << e.what() << std::endl;					
			// 		throw std::runtime_error(ss.str());
			// 	}
			// 	break;
			// }
			case L2Header::broadcast: {
				coutd << "processing broadcast -> ";
				try {
					processBroadcastMessage(packet->getOrigin(), (L2HeaderSH*&) header);
				} catch (const std::exception &e) {
					std::stringstream ss;
					ss << *mac << "::" << *this << " error processing broadcast message: " << e.what() << std::endl;					
					throw std::runtime_error(ss.str());
				}
				contains_data = true;
				break;
			}
			// case L2Header::unicast: {
			// 	coutd << "processing unicast -> ";
			// 	try {
			// 		processUnicastMessage((L2HeaderUnicast*&) header, payload);
			// 	} catch (const std::exception &e) {
			// 		std::stringstream ss;
			// 		ss << *mac << "::" << *this << " error processing unicast message: " << e.what() << std::endl;					
			// 		throw std::runtime_error(ss.str());
			// 	}
			// 	contains_data = true;
			// 	break;
			// }
			// case L2Header::link_establishment_request: {
			// 	coutd << "processing link establishment request -> ";
			// 	try {
			// 		processLinkRequestMessage((const L2HeaderLinkRequest*&) header, (const LinkManager::LinkEstablishmentPayload*&) payload, packet->getOrigin());				
			// 	} catch (const std::exception &e) {
			// 		std::stringstream ss;
			// 		ss << *mac << "::" << *this << " error processing link request message: " << e.what() << std::endl;			
			// 		throw std::runtime_error(ss.str());
			// 	}
			// 	break;
			// }
			// case L2Header::link_establishment_reply: {
			// 	coutd << "processing link establishment reply -> ";
			// 	try {
			// 		processLinkReplyMessage((const L2HeaderLinkReply*&) header, (const LinkManager::LinkEstablishmentPayload*&) payload, packet->getOrigin());				
			// 	} catch (const std::exception &e) {
			// 		std::stringstream ss;
			// 		ss << *mac << "::" << *this << " error processing link reply message: " << e.what() << std::endl;					
			// 		throw std::runtime_error(ss.str());
			// 	}
			// 	break;
			// }
			case L2Header::dme_request: {
				coutd << "discarding DME request -> ";
				break;
			}
			case L2Header::dme_response: {
				coutd << "discarding DME response -> ";
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
		coutd << "deleting packet." << std::endl;
		delete packet;
	}
}

// void LinkManager::processBeaconMessage(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) {
// 	coutd << *this << "::processBeaconMessage" << std::endl;
// 	throw std::runtime_error("not implemented");
// }

void LinkManager::processBroadcastMessage(const MacId& origin, L2HeaderSH*& header) {
	coutd << *this << "::processBroadcastMessage" << std::endl;
	throw std::runtime_error("not implemented");
}

void LinkManager::processUnicastMessage(L2HeaderPP*& header, L2Packet::Payload*& payload) {
	coutd << *this << "::processUnicastMessage" << std::endl;
	throw std::runtime_error("not implemented");
}

// void LinkManager::processBaseMessage(L2HeaderBase*& header) {
// 	coutd << *this << "::processBaseMessage" << std::endl;
// 	throw std::runtime_error("not implemented");
// }

// void LinkManager::processLinkRequestMessage(const L2HeaderLinkRequest*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id) {
// 	coutd << *this << "::processLinkRequestMessage" << std::endl;
// 	throw std::runtime_error("not implemented");
// }

// void LinkManager::processLinkReplyMessage(const L2HeaderLinkReply*& header, const LinkManager::LinkEstablishmentPayload*& payload, const MacId& origin_id) {
// 	coutd << *this << "::processLinkReplyMessage" << std::endl;
// 	throw std::runtime_error("not implemented");
// }

void LinkManager::onSlotEnd() {}

unsigned int LinkManager::measureMacDelay() {	
	unsigned int now = mac->getCurrentSlot();	
	unsigned int mac_delay = now - time_slot_of_last_channel_access;
	time_slot_of_last_channel_access = now;		
	return mac_delay;
}