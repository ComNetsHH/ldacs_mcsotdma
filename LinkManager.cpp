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

void LinkManager::lock(const std::vector<unsigned int>& start_slots, unsigned int burst_length, unsigned int burst_length_tx, ReservationTable* table) {
	// Bursts can be overlapping, so while we check that we *can* lock them, save the unique slots to save some processing steps.
	std::set<unsigned int> unique_offsets_tx, unique_offsets_rx, unique_offsets_local;
	// For every burst start slot...
	for (unsigned int burst_start_offset : start_slots) {
		// the first burst_length_tx slots...
		for (unsigned int t = 0; t < burst_length_tx; t++) {
			unsigned int offset = burst_start_offset + t;
			// ... should be lockable locally
			if (!table->canLock(offset))
				throw std::range_error("LinkManager::lock cannot lock local ReservationTable.");
			// ... and at the transmitter
			if (!std::any_of(tx_tables.begin(), tx_tables.end(), [offset](ReservationTable *tx_table){return tx_table->canLock(offset);}))
				throw std::range_error("LinkManager::lock cannot lock TX ReservationTable.");
			unique_offsets_tx.emplace(offset);
			unique_offsets_local.emplace(offset);
		}
		// Latter burst_length_rx slots...
		for (unsigned int t = burst_length_tx; t < burst_length; t++) {
			unsigned int offset = burst_start_offset + t;
			// ... should be lockable locally
			if (!table->canLock(offset))
				throw std::range_error("LinkManager::lock cannot lock local ReservationTable.");
			// ... and at the receiver
			if (!std::any_of(rx_tables.begin(), rx_tables.end(), [offset](ReservationTable *rx_table){return rx_table->canLock(offset);}))
				throw std::range_error("LinkManager::lock cannot lock RX ReservationTable.");
			unique_offsets_rx.emplace(offset);
			unique_offsets_local.emplace(offset);
		}
	}

	// *All* slots should be locked in the local ReservationTable.
	for (unsigned int offset : unique_offsets_local)
		table->lock(offset);
	// Then lock transmitter resources.
	for (unsigned int offset : unique_offsets_tx) {
		for (auto* tx_table : tx_tables)
			if (tx_table->canLock(offset)) {
				tx_table->lock(offset);
				break;
			}
	}
	// Then receiver resources.
	for (unsigned int offset : unique_offsets_rx) {
		for (auto* rx_table : rx_tables)
			if (rx_table->canLock(offset)) {
				rx_table->lock(offset);
				break;
			}
	}
}

unsigned long LinkManager::getRandomInt(size_t start, size_t end) {
	if (start == end)
		return start;
	std::uniform_int_distribution<> distribution(start, end - 1);
	return distribution(generator);
}

void LinkManager::onPacketReception(L2Packet*& packet) {
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
