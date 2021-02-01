//
// Created by Sebastian Lindner on 14.10.20.
//

#include <cassert>
#include <iostream>
#include "ReservationManager.hpp"
#include "coutdebug.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ReservationManager::ReservationManager(uint32_t planning_horizon) : planning_horizon(planning_horizon), frequency_channels(), p2p_reservation_tables() {}

void ReservationManager::addFrequencyChannel(bool is_p2p, uint64_t center_frequency, uint64_t bandwidth) {
//    ReservationTable* table = is_p2p ? new ReservationTable(this->planning_horizon) : new ReservationTable(this->planning_horizon, Reservation(SYMBOLIC_LINK_ID_BROADCAST, Reservation::RX));
	auto* table = new ReservationTable(planning_horizon);
	auto* channel = new FrequencyChannel(is_p2p, center_frequency, bandwidth);
	table->linkFrequencyChannel(channel);
	if (transmitter_table != nullptr)
		table->linkTransmitterReservationTable(this->transmitter_table);
	if (is_p2p) {
		for (ReservationTable* rx_table : receiver_reservation_tables)
			table->linkReceiverReservationTable(rx_table);
		frequency_channels.push_back(channel);
		p2p_reservation_tables.push_back(table);
		p2p_channel_map[*channel] = frequency_channels.size() - 1;
		p2p_table_map[table] = p2p_reservation_tables.size() - 1;
	} else {
		if (broadcast_frequency_channel == nullptr && broadcast_reservation_table == nullptr) {
			broadcast_frequency_channel = channel;
			broadcast_reservation_table = table;
		} else
			throw std::invalid_argument("ReservationManager::addFrequencyChannel called for broadcast channel, but there's already one configured.");
	}
}

FrequencyChannel* ReservationManager::getFreqChannelByIndex(size_t index) {
	return frequency_channels.at(index);
}

ReservationTable* ReservationManager::getReservationTableByIndex(size_t index) {
	return p2p_reservation_tables.at(index);
}

void ReservationManager::update(uint64_t num_slots) {
	if (broadcast_reservation_table != nullptr)
		broadcast_reservation_table->update(num_slots);
	for (ReservationTable* table : p2p_reservation_tables)
		table->update(num_slots);
}

ReservationManager::~ReservationManager() {
	for (FrequencyChannel* channel : frequency_channels)
		delete channel;
	for (ReservationTable* table : p2p_reservation_tables)
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
	ReservationTable* least_used_table = p2p_reservation_tables.at(0);
	for (auto it = p2p_reservation_tables.begin() + 1; it < p2p_reservation_tables.end(); it++)
		if (least_used_table->getNumIdleSlots() < (*it)->getNumIdleSlots())
			least_used_table = *it;
	return least_used_table;
}

std::priority_queue<ReservationTable*, std::vector<ReservationTable*>, ReservationManager::ReservationTableComparison>
ReservationManager::getSortedP2PReservationTables() const {
	auto queue = std::priority_queue<ReservationTable*, std::vector<ReservationTable*>, ReservationTableComparison>();
	for (auto it = p2p_reservation_tables.begin(); it < p2p_reservation_tables.end(); it++) {
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
	reservations.emplace_back(broadcast_reservation_table->getReservation(0), broadcast_frequency_channel);
	for (ReservationTable* table : p2p_reservation_tables)
		reservations.emplace_back(table->getReservation(0), table->getLinkedChannel());
	return reservations;
}

FrequencyChannel* ReservationManager::getFreqChannel(const ReservationTable* table) {
	FrequencyChannel* channel;
	if (table == broadcast_reservation_table)
		channel = broadcast_frequency_channel;
	else
		channel = frequency_channels.at(p2p_table_map.at(table));
	return channel;
}

ReservationTable* ReservationManager::getReservationTable(const FrequencyChannel* channel) {
	ReservationTable* table;
	if (*channel == *broadcast_frequency_channel)
		table = broadcast_reservation_table;
	else {
		auto it = p2p_channel_map.find(*channel);
		if (it == p2p_channel_map.end())
			throw std::invalid_argument("ReservationManager::getReservationTable couldn't find this channel's ReservationTable.");
		table = p2p_reservation_tables.at(it->second);
	}
	return table;
}

std::vector<std::pair<FrequencyChannel, ReservationTable*>>
ReservationManager::getTxReservations(const MacId& id) const {
	assert(broadcast_frequency_channel && broadcast_reservation_table && "ReservationManager::getTxReservations for unset broadcast channel / reservation table.");
	auto local_reservations = std::vector<std::pair<FrequencyChannel, ReservationTable*>>();
	local_reservations.emplace_back(FrequencyChannel(*broadcast_frequency_channel), broadcast_reservation_table->getTxReservations(id));
	for (auto p2p_channel : frequency_channels) {
		auto reservation_table = p2p_reservation_tables.at(p2p_channel_map.at(*p2p_channel));
		auto tx_reservation_table = reservation_table->getTxReservations(id);
		local_reservations.emplace_back(FrequencyChannel(*p2p_channel), tx_reservation_table);
	}
	return local_reservations;
}

void ReservationManager::updateTables(const std::vector<std::pair<FrequencyChannel, ReservationTable*>>& reservations) {
	for (const auto& pair : reservations) {
		// For every frequency channel encoded in 'reservations'...
		const FrequencyChannel& remote_channel = pair.first;
		// ... look for the local equivalent...
		FrequencyChannel* local_channel = matchFrequencyChannel(remote_channel);
		if (local_channel != nullptr) {
			// ... fetch the corresponding reservation table...
			ReservationTable* table = getReservationTable(local_channel);
			// ... and mark all slots as busy
			const ReservationTable* remote_table = pair.second;
			try {
				table->integrateTxReservations(remote_table);
			} catch (const std::exception& e) {
				throw std::invalid_argument("ReservationManager::updateTables couldn't integrate remote table: " + std::string(e.what()));
			}
		} else
			throw std::invalid_argument("ReservationManager::updateTables couldn't match remote channel @" + std::to_string(remote_channel.getCenterFrequency()) + "kHz to a local one.");
	}
}

FrequencyChannel* ReservationManager::matchFrequencyChannel(const FrequencyChannel& other) const {
	if (*broadcast_frequency_channel == other)
		return broadcast_frequency_channel;
	for (FrequencyChannel* channel : frequency_channels)
		if (*channel == other)
			return channel;
	return nullptr;
}

void ReservationManager::setTransmitterReservationTable(ReservationTable* tx_table) {
	this->transmitter_table = tx_table;
}

FrequencyChannel* ReservationManager::getFreqChannelByCenterFreq(uint64_t center_frequency) {
	if (broadcast_frequency_channel->getCenterFrequency() == center_frequency)
		return broadcast_frequency_channel;
	for (auto* channel : frequency_channels)
		if (channel->getCenterFrequency() == center_frequency)
			return channel;
	return nullptr;
}

void ReservationManager::addReceiverReservationTable(ReservationTable*& rx_table) {
	this->receiver_reservation_tables.push_back(rx_table);
}
