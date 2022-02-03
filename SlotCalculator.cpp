#include <utility>
#include <vector>
#include "SlotCalculator.hpp"

std::pair<std::vector<int>, std::vector<int>> TUHH_INTAIRNET_MCSOTDMA::SlotCalculator::calculateTxRxSlots(const int &start_slot_offset, const int &burst_length, const int &burst_length_tx, const int &burst_length_rx, const int &burst_offset, const int &timeout) {
	auto tx_rx_slots = std::pair<std::vector<int>, std::vector<int>>();
	auto &tx_slots = tx_rx_slots.first;
	auto &rx_slots = tx_rx_slots.second;
	for (int burst = 0; burst < timeout; burst++) {
		for (int tx_slot = 0; tx_slot < burst_length_tx; tx_slot++) {
			int slot_offset = start_slot_offset + burst*burst_offset + tx_slot;
			tx_slots.push_back(slot_offset);
		}
		for (unsigned int rx_slot = 0; rx_slot < burst_length_rx; rx_slot++) {
			unsigned int slot_offset = start_slot_offset + burst*burst_offset + burst_length_tx + rx_slot;
			rx_slots.push_back(slot_offset);
		}
	}
	return tx_rx_slots;
}