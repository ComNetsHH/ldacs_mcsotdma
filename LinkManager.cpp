//
// Created by seba on 2/18/21.
//

#include <set>
#include "LinkManager.hpp"

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
