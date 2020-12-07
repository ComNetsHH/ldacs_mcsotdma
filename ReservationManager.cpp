//
// Created by Sebastian Lindner on 14.10.20.
//

#include "ReservationManager.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ReservationManager::ReservationManager(uint32_t planning_horizon) : planning_horizon(planning_horizon), frequency_channels(), reservation_tables() {}

void ReservationManager::addFrequencyChannel(bool is_p2p, uint64_t center_frequency, uint64_t bandwidth) {
	auto* channel = new FrequencyChannel(is_p2p, center_frequency, bandwidth);
	auto* table = new ReservationTable(this->planning_horizon);
	table->linkFrequencyChannel(channel);
	if (is_p2p) {
		frequency_channels.push_back(channel);
		reservation_tables.push_back(table);
	} else {
		if (broadcast_frequency_channel == nullptr && broadcast_reservation_table == nullptr) {
			broadcast_frequency_channel = channel;
			broadcast_reservation_table = table;
		} else
			throw std::invalid_argument("ReservationManager::addFrequencyChannel called for broadcast channel, but there's already one configured.");
	}
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
    delete broadcast_reservation_table;
    delete broadcast_frequency_channel;
}

size_t ReservationManager::getNumEntries() const {
    return frequency_channels.size();
}

ReservationTable* ReservationManager::getLeastUtilizedP2PReservationTable() {
    // Keeping an up-to-date priority queue is less efficient than manually searching through all channels upon request,
    // because reservations are made very often, while finding the least utilized table is needed relatively rarely.
    ReservationTable* least_used_table = reservation_tables.at(0);
    for (auto it = reservation_tables.begin() + 1; it < reservation_tables.end(); it++)
        if (least_used_table->getNumIdleSlots() < (*it)->getNumIdleSlots())
            least_used_table = *it;
    return least_used_table;
}

std::priority_queue<ReservationTable*, std::vector<ReservationTable*>, ReservationManager::ReservationTableComparison>
ReservationManager::getSortedP2PReservationTables() const {
	auto queue = std::priority_queue<ReservationTable*, std::vector<ReservationTable*>, ReservationTableComparison>();
	for (auto it = reservation_tables.begin(); it < reservation_tables.end(); it++) {
		queue.push(*it);
	}
	return queue;
}

FrequencyChannel* ReservationManager::getBroadcastFreqChannel() {
	return this->broadcast_frequency_channel;
}

ReservationTable* ReservationManager::getBroadcastReservationTable() {
	return this->broadcast_reservation_table;
}

std::vector<std::pair<Reservation, const FrequencyChannel*>> ReservationManager::collectCurrentReservations() {
	std::vector<std::pair<Reservation, const FrequencyChannel*>> reservations;
	for (ReservationTable* table : reservation_tables) {
		auto reservation = std::pair<Reservation, const FrequencyChannel*>(table->getReservation(0), table->getLinkedChannel());
		reservations.push_back(reservation);
	}
	return reservations;
}
