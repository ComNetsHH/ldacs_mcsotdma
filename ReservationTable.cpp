//
// Created by Sebastian Lindner on 06.10.20.
//

#include <stdexcept>
#include <algorithm>
#include <math.h>
#include <limits>
#include <sstream>
#include "ReservationTable.hpp"
#include "coutdebug.hpp"
#include "MCSOTDMA_Mac.hpp"
#include "SlotCalculator.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ReservationTable::ReservationTable(uint32_t planning_horizon)
		: planning_horizon(planning_horizon), slot_utilization_vec(std::vector<Reservation>(uint64_t(planning_horizon * 2 + 1))), last_updated(), num_idle_future_slots(planning_horizon + 1), default_reservation(SYMBOLIC_ID_UNSET, Reservation::IDLE) {
	// The planning horizon denotes how many slots we want to be able to look into future and past.
	// Since the current moment in time must also be represented, we need planning_horizon*2+1 values.
	// If we use UINT32_MAX, then we wouldn't be able to store 2*UINT32_MAX+1 in UINT64, so throw an exception if this is attempted.
	if (planning_horizon == UINT32_MAX)
		throw std::invalid_argument("Cannot instantiate a reservation table with a planning horizon of UINT32_MAX. It must be at least one slot less.");
	// In practice, allocating even much less results in a std::bad_alloc anyway...
}

ReservationTable::ReservationTable() : ReservationTable(512) {}

ReservationTable::ReservationTable(uint32_t planning_horizon, const Reservation& default_reservation) : ReservationTable(planning_horizon) {
	this->default_reservation = default_reservation;
	for (auto& it : slot_utilization_vec)
		it = default_reservation;
}

uint32_t ReservationTable::getPlanningHorizon() const {
	return this->planning_horizon;
}

Reservation* ReservationTable::mark(int32_t slot_offset, const Reservation& reservation) {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("ReservationTable::mark planning_horizon=" + std::to_string(planning_horizon) + " smaller than queried slot_offset=" + std::to_string(slot_offset) + "!");
	// If the exact same reservation already exists, just return it and be done with it.
	if (getReservation(slot_offset) == reservation)
		return &this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset));	
	// Ensure that linked hardware tables have capacity.
	if ((reservation.isAnyTx()) && transmitter_reservation_table != nullptr)
		if (!(transmitter_reservation_table->isIdle(slot_offset) || transmitter_reservation_table->isLocked(slot_offset)))
			throw no_tx_available_error("ReservationTable::mark(" + std::to_string(slot_offset) + ") can't forward TX reservation because the linked transmitter table is not idle.");
	if ((reservation.isAnyRx()) && !receiver_reservation_tables.empty()) {
		if (!std::any_of(receiver_reservation_tables.begin(), receiver_reservation_tables.end(), [slot_offset](ReservationTable* table) {
			return table->isIdle(slot_offset) || table->isLocked(slot_offset);
		})) {
			for (const auto& rx_table : receiver_reservation_tables)
				coutd << std::endl << "Problematic reservation: " << rx_table->getReservation(slot_offset) << std::endl;
			throw no_rx_available_error("ReservationTable::mark(" + std::to_string(slot_offset) + ") can't forward RX reservation because none out of " + std::to_string(receiver_reservation_tables.size()) + " linked receiver tables are idle.");
		}
	}	
	bool currently_idle = getReservation(slot_offset).isIdle();
	// check if the transmitter reservation table should be free'd
	bool can_free_transmitter;
	if (transmitter_reservation_table != nullptr && !currently_idle && reservation.isIdle() && getReservation(slot_offset).isAnyTx()) // if it goes TX->IDLE
		can_free_transmitter = true;
	else 
		can_free_transmitter = false;
	// same for receiver reservation tables
	bool can_free_receiver;
	if (!receiver_reservation_tables.empty() && !currently_idle && reservation.isIdle() && getReservation(slot_offset).isAnyRx())
		can_free_receiver = true;
	else
		can_free_receiver = false;
	this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)) = reservation;
	// Update the number of idle slots.
	if (currently_idle && !reservation.isIdle()) // idle -> non-idle
		num_idle_future_slots--;
	else if (!currently_idle && reservation.isIdle()) // non-idle -> idle
		num_idle_future_slots++;
	// If a transmitter table is linked, mark it there, too.
	if (transmitter_reservation_table != nullptr) {
		if (reservation.isAnyTx() || can_free_transmitter)
			transmitter_reservation_table->mark(slot_offset, reservation);	
	}	
	// Same for receiver tables
	if (!receiver_reservation_tables.empty()) {
		if (reservation.isAnyRx() || can_free_receiver) {
			for (ReservationTable* rx_table : receiver_reservation_tables) {
				if (rx_table->getReservation(slot_offset).isIdle()) {					
					rx_table->mark(slot_offset, reservation);
					break;
				}
			}	
		}	
	}			
	return &this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset));
}

bool ReservationTable::isUtilized(int32_t slot_offset) const {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("ReservationTable::isUtilized for planning horizon smaller than queried offset!");
	return !this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).isIdle();
}

bool ReservationTable::isLocked(int32_t slot_offset) const {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("ReservationTable::isLocked for planning horizon smaller than queried offset!");
	return this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).isLocked();
}

bool ReservationTable::anyTxReservations(int32_t slot_offset) const {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("ReservationTable::anyTxReservations for planning horizon smaller than queried offset!");
	const Reservation& res = this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset));
	return res.isAnyTx();
}

bool ReservationTable::anyTxReservations(int32_t start, uint32_t length) const {
	if (length == 1)
		return anyTxReservations(start);
	if (!this->isValid(start, length))
		throw std::invalid_argument("ReservationTable::anyTxReservations invalid slot range: start=" + std::to_string(start) + " length=" + std::to_string(length));
	// A slot range contains a TX or TX_CONT if any slot does
	for (int32_t slot = start; slot < start + int32_t(length); slot++)
		if (anyTxReservations(slot))
			return true; // so a single busy one fails the check
	return false;
}

bool ReservationTable::anyRxReservations(int32_t slot_offset) const {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("ReservationTable::anyRxReservations for planning horizon smaller than queried offset!");
	const Reservation& res = this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset));
	return res.isAnyRx();
}

bool ReservationTable::anyRxReservations(int32_t start, uint32_t length) const {
	if (length == 1)
		return anyRxReservations(start);
	if (!this->isValid(start, length))
		throw std::invalid_argument("ReservationTable::anyRxReservations invalid slot range: start=" + std::to_string(start) + " length=" + std::to_string(length));
	// A slot range contains a RX if any slot does
	for (int32_t slot = start; slot < start + int32_t(length); slot++)
		if (anyRxReservations(slot))
			return true; // so a single busy one fails the check
	return false;
}

bool ReservationTable::isIdle(int32_t slot_offset) const {
	return !this->isUtilized(slot_offset);
}

bool ReservationTable::isIdle(int32_t start, uint32_t length) const {
	if (length == 1)
		return this->isIdle(start);
	if (!this->isValid(start, length))
		throw std::invalid_argument("ReservationTable::isIdle invalid slot range: start=" + std::to_string(start) + " length=" + std::to_string(length));
	// A slot range is idle if ALL slots within are idle.
	for (int32_t slot = start; slot < start + int32_t(length); slot++)
		if (isUtilized(slot))
			return false; // so a single busy one fails the check
	return true;
}

bool ReservationTable::isUtilized(int32_t start, uint32_t length) const {
	// A slot range is utilized if any slot within is utilized.
	return !this->isIdle(start, length);
}

bool ReservationTable::isBurstValid(int start_slot, unsigned int burst_length, unsigned int burst_length_tx, bool rx_idle_during_first_slot, MCSOTDMA_Mac *mac) const {	
	// if (mac != nullptr) {
		// std::vector<std::vector<double>>& prediction = mac->getCurrentPrediction();
		// process prediction
	// }
	// Check if local table is idle...
	if (isIdle(start_slot, burst_length)) {		
		// ... check if the transmitter is idle for the first burst_length_tx slots...
		bool transmitter_idle = transmitter_reservation_table->isIdle(start_slot, burst_length_tx);		
		// ... check if a receiver is idle for the trailing burst_length_rx slots...
		unsigned int burst_length_rx = burst_length - burst_length_tx;		
		bool receiver_idle;
		if (burst_length_rx == 0)
			receiver_idle = true;
		else {
			unsigned int slot_rx = start_slot + burst_length_tx;			
			if (receiver_reservation_tables.empty())
				receiver_idle = true;
			else
				receiver_idle = std::any_of(receiver_reservation_tables.begin(), receiver_reservation_tables.end(), [slot_rx, burst_length_rx](ReservationTable* table) {					
					return table->isIdle(slot_rx, burst_length_rx);
				});
		}
		// ... if a receiver must also be available during the first slot...
		if (rx_idle_during_first_slot) {
			if (!receiver_reservation_tables.empty() && std::any_of(receiver_reservation_tables.begin(), receiver_reservation_tables.end(), [start_slot](ReservationTable* table) {return table->isIdle(start_slot);}))
				receiver_idle = receiver_idle && true;
		}

		return transmitter_idle && receiver_idle;
	} else
		return false;
}

bool ReservationTable::isTxValid(int slot) const {
	return isIdle(slot) && transmitter_reservation_table->isIdle(slot);
}

bool ReservationTable::isRxValid(int slot) const {
	return isIdle(slot) && std::any_of(receiver_reservation_tables.begin(), receiver_reservation_tables.end(), [slot](ReservationTable* table) {return table->isIdle(slot);});
}

unsigned int ReservationTable::findEarliestIdleSlotsPP(int start_offset, int num_forward_bursts, int num_reverse_bursts, int period, int timeout) const {	
	// start looking
	for (int t = (int) start_offset; t < planning_horizon; t++) {
		// save all starting slots of each burst
		const auto tx_rx_slots = SlotCalculator::calculateAlternatingBursts(t, num_forward_bursts, num_reverse_bursts, period, timeout);		
		const auto &tx_slots = tx_rx_slots.first;
		const auto &rx_slots = tx_rx_slots.second;
		if (tx_slots.empty())
			throw std::invalid_argument("ReservationTable::findEarliestIdleSlotsPP for no TX slots");
		if (rx_slots.empty())
			throw std::invalid_argument("ReservationTable::findEarliestIdleSlotsPP for no RX slots");
		// make sure that they are all valid
		if (std::all_of(tx_slots.begin(), tx_slots.end(), [this](int slot){return this->isTxValid(slot);}) && std::all_of(rx_slots.begin(), rx_slots.end(), [this](int slot){return this->isRxValid(slot);})) {
			return t; // if so, we've found the earliest starting slot that can be used to initiate a PP link
		}
	}
	// if going over the entire horizon didn't find something, 
	// then there are none -> throw an error
	throw std::range_error("cannot find an idle slot range");
}

unsigned int ReservationTable::findEarliestIdleSlotsBC(unsigned int start_offset) const {
	if (!isValid((int) start_offset))
		throw std::invalid_argument("exceeded planning horizon");
	if (transmitter_reservation_table == nullptr)
		throw std::runtime_error("ReservationTable::findEarliestIdleSlotsBC for unset transmitter table.");

	for (int t = (int) start_offset; t < planning_horizon; t++) {
		if (isBurstValid(t, 1, 1, false, nullptr))
			return t;
	}
	throw std::range_error("No idle slot range could be found.");
}

//int32_t ReservationTable::findEarliestIdleRange(int32_t start, uint32_t length, bool consider_transmitter) const {
//	if (consider_transmitter && !transmitter_reservation_table)
//		throw std::runtime_error("ReservationTable::findEarliestIdleRange that should consider the transmitter with an unset transmitter_reservation_table.");
//	if (!isValid(start, length))
//		throw std::invalid_argument("Invalid slot range!");
//	for (int32_t i = start; i < int32_t(this->planning_horizon); i++) {
//		// Is this table idle?
//		if (this->isIdle(i, length)) {
//			if (!consider_transmitter) // Don't care about an idle transmitter?
//				return i; // Just return the slot since this table is idle.
//			else if (!transmitter_reservation_table->anyTxReservations(i, length)) // Do care? Then check whether it is idle, too.
//				return i;
//		}
//	}
//	throw std::runtime_error("ReservationTable::findEarliestIdleRange found no idle slot range of specified length.");
//}

bool ReservationTable::isValid(int32_t slot_offset) const {
	return std::abs(slot_offset) <= this->planning_horizon; // can't move more than one horizon into either direction of time.
}

bool ReservationTable::isValid(int32_t start, uint32_t length) const {
	if (length == 1)
		return this->isValid(start);
	return isValid(start) && isValid(start + length - 1);
}

const Timestamp& ReservationTable::getCurrentSlot() const {
	return this->last_updated;
}

void ReservationTable::update(uint64_t num_slots) {
	// Count the number of busy slots that go out of scope on the time domain.
	uint64_t num_busy_slots = 0;
	// Start counting at offset zero (current time slot) as history doesn't matter.
	for (size_t t = 0; t < num_slots; t++)
		if (!getReservation(t).isIdle())
			num_busy_slots++;
	num_idle_future_slots += num_busy_slots; // As these go out of scope, we may have more idle slots now.

	// Shift all elements to the front, old ones are overwritten.
	std::move(this->slot_utilization_vec.begin() + num_slots, this->slot_utilization_vec.end(), this->slot_utilization_vec.begin());
	// All new elements are initialized as idle.
	for (auto it = slot_utilization_vec.end() - 1; it >= slot_utilization_vec.end() - num_slots; it--)
		*it = default_reservation;
	last_updated += num_slots;
}

const std::vector<Reservation>& ReservationTable::getVec() const {
	return this->slot_utilization_vec;
}

uint64_t ReservationTable::convertOffsetToIndex(int32_t slot_offset) const {
	// The vector has planning_horizon-many past slots, one current slot, and planning_horizon-many future slots.
	// So planning_horizon+0 indicates the current slot, which is the basis for this relative access.
	return planning_horizon + slot_offset;
}

void ReservationTable::setLastUpdated(const Timestamp& timestamp) {
	last_updated = timestamp;
}

uint64_t ReservationTable::getNumIdleSlots() const {
	return this->num_idle_future_slots;
}

std::vector<unsigned int> ReservationTable::findSHCandidates(unsigned int num_candidates, int min_offset) const {
	std::vector<unsigned int> start_slots;
	int last_offset = min_offset;
	for (size_t i = 0; i < num_candidates; i++) {
		try {
			auto start_slot = findEarliestIdleSlotsBC(last_offset);
			start_slots.push_back(start_slot);
			last_offset = start_slot + 1;
		} catch (const std::range_error& e) {
			// This is thrown if no idle range can be found.
			coutd << "cannot find anymore after t=" << last_offset << ": " << e.what() << " -> stopping at " << start_slots.size() << " candidates -> ";
 			break; // Stop if no more ranges can be found.
		} catch (const std::invalid_argument& e) {
			// This is thrown if the input is invalid (i.e. we are exceeding the planning horizon).
			coutd << "cannot find anymore after t=" << last_offset << ": " << e.what() << " -> stopping at " << start_slots.size() << " candidates -> ";
			break; // Stop if no more ranges can be found.
		} // all other exceptions should still end execution
	}
	return start_slots;
}

std::vector<unsigned int> ReservationTable::findPPCandidates(unsigned int num_proposal_slots, unsigned int min_offset, int num_forward_bursts, int num_reverse_bursts, int period, int timeout) const {
	std::vector<unsigned int> start_slot_offsets;
	unsigned int last_offset = min_offset;
	for (size_t i = 0; i < num_proposal_slots; i++) {		
		// Try to find another slot range.
		try {			
			// coutd << "checking last_offset=" << last_offset << " burst_length=" << burst_length << " burst_length_tx=" << burst_length_tx << " burst_offset=" << burst_offset << ": ";
			int32_t start_slot = (int) findEarliestIdleSlotsPP(last_offset, num_forward_bursts, num_reverse_bursts, period, timeout);			
			start_slot_offsets.push_back(start_slot);
			last_offset = start_slot + 1; // Next attempt, look later than current one.
			// coutd << "added -> ";
		} catch (const std::range_error& e) {
			// coutd << e.what() << " -> ";
			// This is thrown if no idle range can be found.
			break; // Stop if no more ranges can be found.
		} catch (const std::invalid_argument& e) {			
			// coutd << e.what() << " -> ";
			// This is thrown if the input is invalid (i.e. we are exceeding the planning horizon).
			break; // Stop if no more ranges can be found.
		} // all other exceptions should still end execution
	}
	return start_slot_offsets;
}

bool ReservationTable::lock(unsigned int slot_offset, const MacId& id) {
	// Nothing to do if it's already locked.
	MacId res_id = slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).getTarget();
	if (isLocked(slot_offset) && res_id == id)
		return false;
	if (isLocked(slot_offset) && res_id != id) {
		std::stringstream ss;
		ss << "ReservationTable::lock cannot lock resource in " << slot_offset << " slots for given ID '" << id << "' as it is already locked to '" << res_id << "'.";
		throw id_mismatch(ss.str());	
	}
	// Ensure that you *can* lock before actually doing so.
	if (!isIdle(slot_offset)) {
		std::stringstream ss;
		ss << "ReservationTable::lock for non-idle and non-locked slot: " << slot_utilization_vec.at(convertOffsetToIndex(slot_offset)) << ".";
		throw cannot_lock(ss.str());
	}
	// Then lock.
	slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).setAction(Reservation::LOCKED);
	slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).setTarget(id);		
	return true;
}

bool ReservationTable::lock_either_id(unsigned int slot_offset, const MacId& id1, const MacId& id2) {
	bool success = false;
	try {
		success = lock(slot_offset, id1);		
	} catch (const id_mismatch &e) {
		try {
			success = lock(slot_offset, id2);
		} catch (const id_mismatch &e) {
			throw id_mismatch("Couldn't lock to either ID: " + std::string(e.what()));
		}
	}
	return success;
}

void ReservationTable::unlock(unsigned int slot_offset, const MacId& id) {
	if (!isLocked(slot_offset) && !isIdle(slot_offset))
		throw std::invalid_argument("cannot unlock non-locked reservation");
	if (slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).getTarget() != id && slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).getTarget() != SYMBOLIC_ID_UNSET)	
		throw id_mismatch("cannot unlock locked reservation whose ID is " + std::to_string(slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).getTarget().getId()) + " and not " + std::to_string(id.getId()));
	slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).setAction(Reservation::IDLE);
	slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).setTarget(SYMBOLIC_ID_UNSET);	
}

void ReservationTable::unlock_either_id(unsigned int slot_offset, const MacId& id1, const MacId& id2) {
	try {
		unlock(slot_offset, id1);		
	} catch (const id_mismatch &e) {
		try {
			unlock(slot_offset, id2);
		} catch (const id_mismatch &e) {
			throw id_mismatch("Couldn't unlock, tried both IDs, error after trying the second ID: " + std::string(e.what()));
		}
	}
}

bool ReservationTable::canLock(unsigned int slot_offset) const {
	const Reservation& res = slot_utilization_vec.at(convertOffsetToIndex(slot_offset));
	return res.isIdle() || res.isLocked();
}

int32_t ReservationTable::findEarliestOffset(int32_t start_offset, const Reservation& reservation) const {
	for (uint32_t i = start_offset; i < planning_horizon; i++) {
		const Reservation& current_reservation = slot_utilization_vec.at(convertOffsetToIndex(i));
		if (reservation == current_reservation)
			return i;
	}
	throw std::runtime_error("ReservationTable::findEarliestOffset finds no scheduled reservation from present to future.");
}

void ReservationTable::linkFrequencyChannel(FrequencyChannel* channel) {
	this->freq_channel = channel;
}

const FrequencyChannel* ReservationTable::getLinkedChannel() const {
	return freq_channel;
}

const Reservation& ReservationTable::getReservation(int offset) const {
	return getVec().at(convertOffsetToIndex(offset));
}

unsigned long ReservationTable::countReservedTxSlots(const MacId& id) const {
	unsigned long counter = 0;
	for (const Reservation& reservation : slot_utilization_vec)
		if (reservation.getTarget() == id && reservation.isAnyTx())
			counter++;
	return counter;
}

ReservationTable* ReservationTable::getTxReservations(const MacId& id) const {
	auto* table = new ReservationTable(this->planning_horizon);
	for (size_t i = 0; i < slot_utilization_vec.size(); i++) {
		const Reservation& reservation = slot_utilization_vec.at(i);
		if (reservation.getTarget() == id && reservation.isAnyTx())
			table->slot_utilization_vec.at(i) = Reservation(reservation);
	}
	return table;
}

void ReservationTable::integrateTxReservations(const ReservationTable* other) {
	if (other->planning_horizon != this->planning_horizon)
		throw std::invalid_argument("ReservationTable::integrateTxReservations where other table doesn't have the same dimension!");
	for (size_t i = 0; i < slot_utilization_vec.size(); i++) {
		const Reservation& reservation = other->slot_utilization_vec.at(i);
		if (reservation.isAnyTx())
			slot_utilization_vec.at(i) = Reservation(reservation);
	}
}

bool ReservationTable::operator==(const ReservationTable& other) const {
	if (other.planning_horizon != this->planning_horizon || slot_utilization_vec.size() != other.slot_utilization_vec.size())
		return false;
	for (size_t i = 0; i < slot_utilization_vec.size(); i++)
		if (slot_utilization_vec.at(i) != other.slot_utilization_vec.at(i))
			return false;
	return true;
}

bool ReservationTable::operator!=(const ReservationTable& other) const {
	return !((*this) == other);
}

void ReservationTable::linkTransmitterReservationTable(ReservationTable* tx_table) {
	this->transmitter_reservation_table = tx_table;
}

void ReservationTable::linkReceiverReservationTable(ReservationTable* rx_table) {
	this->receiver_reservation_tables.push_back(rx_table);
}

ReservationTable::~ReservationTable() = default;
