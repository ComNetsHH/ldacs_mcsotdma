//
// Created by Sebastian Lindner on 14.10.20.
//

#include "ReservationManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ReservationManager::ReservationManager(uint32_t planning_horizon) : planning_horizon(planning_horizon), reservation_tables() {

}

void ReservationManager::addFrequencyChannel(bool is_p2p, uint64_t center_frequency, uint64_t bandwidth) {
	auto table = std::map<uint64_t, ReservationTable>::value_type(center_frequency, ReservationTable(this->planning_horizon));
	reservation_tables.insert(table);
	auto channel = std::map<uint64_t, FrequencyChannel>::value_type(center_frequency, FrequencyChannel(is_p2p, center_frequency, bandwidth));
	frequency_channels.insert(channel);
}

FrequencyChannel& ReservationManager::getFreqChannel(uint64_t center_frequency) {
	return frequency_channels.at(center_frequency);
}

ReservationTable& ReservationManager::getReservationTable(uint64_t center_frequency) {
	return reservation_tables.at(center_frequency);
}

void ReservationManager::removeFrequencyChannel(uint64_t center_frequency) {
	reservation_tables.erase(center_frequency);
	frequency_channels.erase(center_frequency);
}

void ReservationManager::update(uint64_t num_slots) {
	for (auto& pair : reservation_tables)
		pair.second.update(num_slots);
}
