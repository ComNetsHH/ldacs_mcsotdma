#include <utility>
#include <vector>
#include "SlotCalculator.hpp"
#include <iostream>
#include <cmath>

using namespace TUHH_INTAIRNET_MCSOTDMA;

std::pair<std::vector<int>, std::vector<int>> SlotCalculator::calculateTxRxSlots(const int &start_slot_offset, const int &burst_length, const int &burst_length_tx, const int &burst_length_rx, const int &burst_offset, const int &timeout) {		
	auto tx_rx_slots = std::pair<std::vector<int>, std::vector<int>>();
	auto &tx_slots = tx_rx_slots.first;
	auto &rx_slots = tx_rx_slots.second;
	for (int burst = 0; burst < timeout; burst++) {
		for (int tx_slot = 0; tx_slot < burst_length_tx; tx_slot++) {
			int slot_offset = start_slot_offset + burst*burst_offset + tx_slot;
			if (slot_offset >= 0)
				tx_slots.push_back(slot_offset);
		}
		for (unsigned int rx_slot = 0; rx_slot < burst_length_rx; rx_slot++) {
			unsigned int slot_offset = start_slot_offset + burst*burst_offset + burst_length_tx + rx_slot;
			if (slot_offset >= 0)
				rx_slots.push_back(slot_offset);
		}
	}
	return tx_rx_slots;
}

std::pair<std::vector<int>, std::vector<int>> SlotCalculator::calculateAlternatingBursts(const int &start_slot_offset, const int &num_forward_bursts, const int &num_reverse_bursts, const int &period, const int &timeout) {
	auto tx_rx_slots = std::pair<std::vector<int>, std::vector<int>>();
	auto &tx_slots = tx_rx_slots.first;
	auto &rx_slots = tx_rx_slots.second;
	int increment = 5*std::pow(2, period);	
	int slot = start_slot_offset;
	for (int exchange = 0; exchange < timeout; exchange++) {				
		for (int fw_burst = 0; fw_burst < num_forward_bursts; fw_burst++) {
			tx_slots.push_back(slot);
			slot += increment;
		}
		for (int rv_burst = 0; rv_burst < num_reverse_bursts; rv_burst++) {
			rx_slots.push_back(slot);
			slot += increment;
		}
	}
	return tx_rx_slots;	
}