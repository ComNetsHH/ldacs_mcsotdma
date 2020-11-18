//
// Created by Sebastian Lindner on 14.10.20.
//

#include "ReservationManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ReservationManager::ReservationManager(uint32_t planning_horizon) : planning_horizon(planning_horizon), reservation_tables() {}

void ReservationManager::addFrequencyChannel(bool is_p2p, uint64_t center_frequency, uint64_t bandwidth) {
	FrequencyChannel* channel = new FrequencyChannel(is_p2p, center_frequency, bandwidth);
	frequency_channels.push_back(channel);
	ReservationTable* table = new ReservationTable(this->planning_horizon);
	reservation_tables.push_back(table);
}

FrequencyChannel* ReservationManager::getFreqChannel(size_t index) {
	return frequency_channels.at(index);
}

ReservationTable* ReservationManager::getReservationTable(size_t index) {
	return reservation_tables.at(index);
}

void ReservationManager::update(uint64_t num_slots) {
	for (ReservationTable* table : reservation_tables)
		table->update(num_slots);
}

ReservationManager::~ReservationManager() {
    for (FrequencyChannel* channel : frequency_channels)
        delete channel;
    for (ReservationTable* table : reservation_tables)
        delete table;
}

size_t ReservationManager::getNumEntries() const {
    return frequency_channels.size();
}

ReservationTable* ReservationManager::getLeastUtilizedReservationTable() {
    // Keeping an up-to-date priority queue is less efficient than manually searching through all channels upon request,
    // because reservations are made very often, while finding the least utilized table is needed relatively rarely.
    ReservationTable* least_used_table = reservation_tables.at(0);
    for (auto it = reservation_tables.begin() + 1; it < reservation_tables.end(); it++)
        if (least_used_table->getNumIdleSlots() < (*it)->getNumIdleSlots())
            least_used_table = *it;
    return least_used_table;
}

std::priority_queue<ReservationTable*, std::vector<ReservationTable*>, ReservationManager::ReservationTableComparison>
ReservationManager::getSortedReservationTables() const {
	auto queue = std::priority_queue<ReservationTable*, std::vector<ReservationTable*>, ReservationTableComparison>();
	for (auto it = reservation_tables.begin(); it < reservation_tables.end(); it++)
		queue.push(*it);
	return queue;
}
