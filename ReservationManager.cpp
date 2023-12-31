// The L-Band Digital Aeronautical Communications System (LDACS) Multi Channel Self-Organized TDMA (TDMA) Library provides an implementation of Multi Channel Self-Organized TDMA (MCSOTDMA) for the LDACS Air-Air Medium Access Control simulator.
// Copyright (C) 2023  Sebastian Lindner, Konrad Fuger, Musab Ahmed Eltayeb Ahmed, Andreas Timm-Giel, Institute of Communication Networks, Hamburg University of Technology, Hamburg, Germany
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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

ReservationMap ReservationManager::scheduleBursts(const FrequencyChannel *channel, const int &start_slot_offset, const int &num_forward_bursts, const int &num_reverse_bursts, const int &period, const int &timeout, const MacId& initiator_id, const MacId& recipient_id, bool is_link_initiator) {	
	ReservationMap reservation_map;
	ReservationTable *tbl = getReservationTable(channel);	
	Reservation::Action action_1, action_2;	
	// we differentiate between being the link initiator or not
	action_1 = is_link_initiator ? Reservation::TX : Reservation::RX;
	action_2 = is_link_initiator ? Reservation::RX : Reservation::TX;	
	MacId target_id = is_link_initiator ? recipient_id : initiator_id; 	
	
	auto tx_rx_slots = SlotCalculator::calculateAlternatingBursts(start_slot_offset, num_forward_bursts, num_reverse_bursts, period, timeout);	
	size_t num_tx_scheduled = 0, num_rx_scheduled = 0;
	// go over link initiator's TX slots	
	coutd << (is_link_initiator ? "scheduling TX slots: " : "scheduling RX slots: ");
	for (int &slot_offset : tx_rx_slots.first) {
		bool can_write = false, can_overwrite = false;
		const auto &res = tbl->getReservation(slot_offset);
		// resource should be either idle 
		if (res.isIdle()) 
			can_write = true;
		// or it is our target that is already busy (which we know e.g. through its beacon)
		else if (res.isBusy() && res.getTarget() == target_id) 
			can_overwrite = true;		

		coutd << "t=" << slot_offset;
		// make sure that hardware is available
		if (can_write || can_overwrite) {			
			if (action_1 == Reservation::TX) {
				const auto &tx_reservation = getTxTable()->getReservation(slot_offset);
				bool transmitter_available = can_write ? tx_reservation.isIdle() : (tx_reservation.isIdle() || (tx_reservation.isBusy() && tx_reservation.getTarget() == target_id));
				if (!transmitter_available) {					
					coutd << "TX_NOT_AVAIL";
					can_write = false;
					can_overwrite = false;					
				}
			} else if (action_1 == Reservation::RX) {
				bool receiver_available;
				if (can_write)
					receiver_available = std::any_of(getRxTables().begin(), getRxTables().end(), [slot_offset](const ReservationTable *rx_table){return rx_table->getReservation(slot_offset).isIdle();});
				else
					receiver_available = std::any_of(getRxTables().begin(), getRxTables().end(), [slot_offset, target_id](const ReservationTable *rx_table){const auto &res = rx_table->getReservation(slot_offset); return res.isIdle() || (res.isBusy() && res.getTarget() == target_id);});
				if (!receiver_available) {
					coutd << "RX_NOT_AVAIL";
					can_write = false;
					can_overwrite = false;
				}
			}
		} 
				
		if (can_write || can_overwrite) {
			tbl->mark(slot_offset, Reservation(target_id, action_1));						
			reservation_map.add_scheduled_resource(tbl, slot_offset);
			coutd << ":" << tbl->getReservation(slot_offset);
			coutd << (can_write ? "" : " overwritten");
			action_1 == Reservation::TX ? num_tx_scheduled++ : num_rx_scheduled++;
		} 
		coutd << ", ";
	}
	coutd << (is_link_initiator ? "scheduling RX slots: " : "scheduling TX slots: ");
	// go over the link initiator's RX slots
	for (int &slot_offset : tx_rx_slots.second) {
		bool can_write = false, can_overwrite = false;
		const auto &res = tbl->getReservation(slot_offset);
		// resource should be either idle 
		if (res.isIdle()) 
			can_write = true;
		// or it is our target that is already busy (which we know e.g. through its beacon)
		else if (res.isBusy() && res.getTarget() == target_id) 
			can_overwrite = true;		

		coutd << "t=" << slot_offset;
		// make sure that hardware is available
		if (can_write || can_overwrite) {
			if (action_2 == Reservation::TX) {
				const auto &tx_reservation = getTxTable()->getReservation(slot_offset);
				bool transmitter_available = can_write ? tx_reservation.isIdle() : (tx_reservation.isIdle() || (tx_reservation.isBusy() && tx_reservation.getTarget() == target_id));
				if (!transmitter_available) {					
					coutd << "TX_NOT_AVAIL";
					can_write = false;
					can_overwrite = false;					
				}
			} else if (action_2 == Reservation::RX) {
				bool receiver_available;
				if (can_write)
					receiver_available = std::any_of(getRxTables().begin(), getRxTables().end(), [slot_offset](const ReservationTable *rx_table){return rx_table->getReservation(slot_offset).isIdle();});
				else
					receiver_available = std::any_of(getRxTables().begin(), getRxTables().end(), [slot_offset, target_id](const ReservationTable *rx_table){const auto &res = rx_table->getReservation(slot_offset); return res.isIdle() || (res.isBusy() && res.getTarget() == target_id);});
				if (!receiver_available) {
					coutd << "RX_NOT_AVAIL";
					can_write = false;
					can_overwrite = false;
				}
			}
		}
		if (can_write || can_overwrite) {
			tbl->mark(slot_offset, Reservation(target_id, action_2));		
			reservation_map.add_scheduled_resource(tbl, slot_offset);	
			coutd << ":" << tbl->getReservation(slot_offset);
			coutd << (can_write ? "" : " overwritten");
			action_2 == Reservation::TX ? num_tx_scheduled++ : num_rx_scheduled++;
		}
		coutd << ", ";
	}	
	coutd << "done -> ";
	if (num_tx_scheduled == 0 || num_rx_scheduled == 0) {		
		throw std::invalid_argument("ReservationManager::scheduleBursts could schedule " + std::to_string(num_tx_scheduled) + " TX reservations and " + std::to_string(num_rx_scheduled) + " RX reservations. Too many PP links, i.e. is the duty cycle exhausted?");
	}	
	return reservation_map;
}

uint32_t ReservationManager::getPlanningHorizon() const {
	return this->planning_horizon;
}