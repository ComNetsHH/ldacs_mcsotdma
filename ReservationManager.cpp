//
// Created by Sebastian Lindner on 14.10.20.
//

#include <cassert>
#include <iostream>
#include <algorithm>
#include "ReservationManager.hpp"
#include "coutdebug.hpp"
#include "SlotCalculator.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ReservationManager::ReservationManager(uint32_t planning_horizon) : planning_horizon(planning_horizon), p2p_frequency_channels(), p2p_reservation_tables() {}

void ReservationManager::addFrequencyChannel(bool is_p2p, uint64_t center_frequency, uint64_t bandwidth) {
	auto* table = new ReservationTable(planning_horizon);
	auto* channel = new FrequencyChannel(is_p2p, center_frequency, bandwidth);
	table->linkFrequencyChannel(channel);
	if (hardware_tx_table != nullptr)
		table->linkTransmitterReservationTable(this->hardware_tx_table);
	if (is_p2p) {
		p2p_frequency_channels.push_back(channel);
		p2p_reservation_tables.push_back(table);
		p2p_channel_map[*channel] = p2p_frequency_channels.size() - 1;
		p2p_table_map[table] = p2p_reservation_tables.size() - 1;
		for (ReservationTable* rx_table : hardware_rx_tables)
			table->linkReceiverReservationTable(rx_table);
	} else {
		if (broadcast_frequency_channel == nullptr && broadcast_reservation_table == nullptr) {
			broadcast_frequency_channel = channel;
			broadcast_reservation_table = table;
		} else
			throw std::invalid_argument("ReservationManager::addFrequencyChannel called for broadcast channel, but there's already one configured.");
	}
}

FrequencyChannel* ReservationManager::getFreqChannelByIndex(size_t index) {
	return p2p_frequency_channels.at(index);
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
	for (FrequencyChannel* channel : p2p_frequency_channels)
		delete channel;
	for (ReservationTable* table : p2p_reservation_tables)
		delete table;
	delete broadcast_reservation_table;
	delete broadcast_frequency_channel;
}

size_t ReservationManager::getNumEntries() const {
	return p2p_frequency_channels.size();
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

std::priority_queue<ReservationTable*, std::vector<ReservationTable*>, ReservationManager::ReservationTableComparison> ReservationManager::getSortedP2PReservationTables() const {
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

std::vector<std::pair<Reservation, const FrequencyChannel*>> ReservationManager::collectReservations(unsigned int slot_offset) const {
	std::vector<std::pair<Reservation, const FrequencyChannel*>> reservations;
	reservations.emplace_back(broadcast_reservation_table->getReservation(slot_offset), broadcast_frequency_channel);
	for (ReservationTable* table : p2p_reservation_tables)
		reservations.emplace_back(table->getReservation(slot_offset), table->getLinkedChannel());
	return reservations;
}

std::vector<std::pair<Reservation, const FrequencyChannel*>> ReservationManager::collectCurrentReservations() const {
	return collectReservations(0);
}

FrequencyChannel* ReservationManager::getFreqChannel(const ReservationTable* table) {
	FrequencyChannel* channel;
	if (table == broadcast_reservation_table)
		channel = broadcast_frequency_channel;
	else
		channel = p2p_frequency_channels.at(p2p_table_map.at(table));
	return channel;
}

ReservationTable* ReservationManager::getReservationTable(const FrequencyChannel* channel) {
	if (channel == nullptr)
		return nullptr;
	ReservationTable* table;
	if (broadcast_reservation_table != nullptr && *channel == *broadcast_frequency_channel)
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
	for (auto p2p_channel : p2p_frequency_channels) {
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
	for (FrequencyChannel* channel : p2p_frequency_channels)
		if (*channel == other)
			return channel;
	return nullptr;
}

void ReservationManager::setTransmitterReservationTable(ReservationTable* tx_table) {
	this->hardware_tx_table = tx_table;
}

FrequencyChannel* ReservationManager::getFreqChannelByCenterFreq(uint64_t center_frequency) {
	if (broadcast_frequency_channel->getCenterFrequency() == center_frequency)
		return broadcast_frequency_channel;
	for (auto* channel : p2p_frequency_channels)
		if (channel->getCenterFrequency() == center_frequency)
			return channel;
	return nullptr;
}

void ReservationManager::addReceiverReservationTable(ReservationTable*& rx_table) {
	this->hardware_rx_tables.push_back(rx_table);
}

std::vector<FrequencyChannel*>& ReservationManager::getP2PFreqChannels() {
	return p2p_frequency_channels;
}

const std::vector<ReservationTable*>& ReservationManager::getRxTables() const {
	return hardware_rx_tables;
}

ReservationTable* ReservationManager::getTxTable() const {
	return hardware_tx_table;
}

std::vector<ReservationTable*>& ReservationManager::getP2PReservationTables() {
	return this->p2p_reservation_tables;
}

ReservationMap ReservationManager::schedule_bursts(const FrequencyChannel *channel, const unsigned int timeout, const unsigned int first_burst_in, const unsigned int burst_length, const unsigned int burst_length_tx, const unsigned int burst_length_rx, const unsigned int burst_offset, const MacId& initiator_id, const MacId& recipient_id, bool is_link_initiator, bool is_third_party_link) {
	ReservationMap reservation_map;
	ReservationTable *tbl = getReservationTable(channel);	
	Reservation::Action action_1, action_2;
	MacId target_id_1, target_id_2;
	// if we're scheduling links for ourself
	if (!is_third_party_link) {
		// then we differentiate between being the link initiator or not
		action_1 = is_link_initiator ? Reservation::TX : Reservation::RX;
		action_2 = is_link_initiator ? Reservation::RX : Reservation::TX;
		// and the target is always the other user
		target_id_1 = recipient_id;
		target_id_2 = recipient_id;
	// if we're not
	} else {
		// then all actions are just BUSY
		action_1 = Reservation::BUSY;
		action_2 = Reservation::BUSY;
		// and we differentiate between the respective transmitters
		target_id_1 = initiator_id;
		target_id_2 = recipient_id;
	}
	
	auto tx_rx_slots = SlotCalculator::calculateTxRxSlots(first_burst_in, burst_length, burst_length_tx, burst_length_rx, burst_offset, timeout);
	// go over link initiator's TX slots
	for (int &slot_offset : tx_rx_slots.first) {
		// make sure that the reservation is idle locally
		if (!tbl->getReservation(slot_offset).isIdle()) {								
			std::stringstream s;
			s << *this << "::schedule_bursts couldn't schedule a " << action_1 << "@" << target_id_1 << " resource in " << slot_offset << " slots. It is " << tbl->getReservation(slot_offset) << ", when it should be idle.";
			throw std::runtime_error(s.str());
		}
		// make sure that hardware is available
		if (action_1 == Reservation::TX) {
			const auto &tx_reservation = getTxTable()->getReservation(slot_offset);
			bool transmitter_available = tx_reservation.isIdle();
			if (!transmitter_available) {
				std::stringstream s;
				s << *this << "::schedule_bursts couldn't schedule a " << action_1 << "@" << target_id_1 << " resource in " << slot_offset << " slots. The transmitter will be busy with " << tx_reservation << ", when it should be idle.";
				throw std::runtime_error(s.str());
			}
		} else if (action_1 == Reservation::RX) {
			bool receiver_available = std::any_of(getRxTables().begin(), getRxTables().end(), [slot_offset](const ReservationTable *rx_table){return rx_table->getReservation(slot_offset).isIdle();});
			if (!receiver_available) {
				std::stringstream s;
				s << *this << "::schedule_bursts couldn't schedule a " << action_1 << "@" << target_id_1 << " resource in " << slot_offset << " slots. The receivers will be busy with ";
				for (const auto *rx_table : getRxTables())
					s << rx_table->getReservation(slot_offset) << " ";
				s << "when at least one should be idle.";
				throw std::runtime_error(s.str());
			}
		}
		tbl->mark(slot_offset, Reservation(target_id_1, action_1));						
		reservation_map.add_scheduled_resource(tbl, slot_offset);
	}
	// go over the link initiator's RX slots
	for (int &slot_offset : tx_rx_slots.second) {
		// make sure that the reservation is idle locally
		if (!tbl->getReservation(slot_offset).isIdle()) {
			std::stringstream s;
			s << *this << "::schedule_bursts couldn't schedule a " << action_2 << "@" << target_id_2 << " resource in " << slot_offset << " slots. It is " << tbl->getReservation(slot_offset) << ", when it should be idle.";
			throw std::runtime_error(s.str());
		}
		// make sure that hardware is available
		if (action_2 == Reservation::TX) {
			const auto &tx_reservation = getTxTable()->getReservation(slot_offset);
			bool transmitter_available = tx_reservation.isIdle();
			if (!transmitter_available) {
				std::stringstream s;
				s << *this << "::schedule_bursts couldn't schedule a " << action_2 << "@" << target_id_2 <<   " resource in " << slot_offset << " slots. The transmitter will be busy with " << tx_reservation << ", when it should be idle.";
				throw std::runtime_error(s.str());
			}
		} else if (action_2 == Reservation::RX) {
			bool receiver_available = std::any_of(getRxTables().begin(), getRxTables().end(), [slot_offset](const ReservationTable *rx_table){return rx_table->getReservation(slot_offset).isIdle();});
			if (!receiver_available) {
				std::stringstream s;
				s << *this << "::schedule_bursts couldn't schedule a " << action_2 << "@" << target_id_2 <<  " resource in " << slot_offset << " slots. The receivers will be busy with ";
				for (const auto *rx_table : getRxTables())
					s << rx_table->getReservation(slot_offset) << " ";
				s << "when at least one should be idle.";
				throw std::runtime_error(s.str());
			}
		}
		tbl->mark(slot_offset, Reservation(target_id_2, action_2));		
		reservation_map.add_scheduled_resource(tbl, slot_offset);	
	}

	return reservation_map;
}