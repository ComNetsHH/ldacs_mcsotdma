//
// Created by Sebastian Lindner on 06.10.20.
//

#include <stdexcept>
#include <algorithm>
#include <math.h>
#include <limits>
#include <iostream>
#include "ReservationTable.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ReservationTable::ReservationTable(uint32_t planning_horizon)
	: planning_horizon(planning_horizon), slot_utilization_vec(std::vector<Reservation>(uint64_t(planning_horizon * 2 + 1))), last_updated(), num_idle_future_slots(planning_horizon + 1) {
	// The planning horizon denotes how many slots we want to be able to look into future and past.
	// Since the current moment in time must also be represented, we need planning_horizon*2+1 values.
	// If we use UINT32_MAX, then we wouldn't be able to store 2*UINT32_MAX+1 in UINT64, so throw an exception if this is attempted.
	if (planning_horizon == UINT32_MAX)
		throw std::invalid_argument("Cannot instantiate a reservation table with a planning horizon of UINT32_MAX. It must be at least one slot less.");
	// In practice, allocating even much less results in a std::bad_alloc anyway...
}

uint32_t ReservationTable::getPlanningHorizon() const {
	return this->planning_horizon;
}

Reservation* ReservationTable::mark(int32_t slot_offset, const Reservation& reservation) {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("Reservation table planning horizon smaller than queried slot offset!");
	bool currently_idle = this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).getAction() == Reservation::Action::IDLE;
	this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)) = reservation;
	// Update the number of idle slots.
	if (reservation.getAction() != Reservation::Action::IDLE)
		num_idle_future_slots--;
	else if (!currently_idle) // changing from non-idle to idle
		num_idle_future_slots++;
	// If this is a multi-slot transmission reservation, set the following ones, too.
	if (reservation.getNumRemainingTxSlots() > 0) {
		Reservation next_reservation = Reservation(reservation.getOwner(), Reservation::Action::TX_CONT, reservation.getNumRemainingTxSlots() - 1);
		mark(slot_offset + 1, next_reservation);
	}
	return &this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset));
}

bool ReservationTable::isUtilized(int32_t slot_offset) const {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("Reservation table planning horizon smaller than queried offset!");
	return this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)).getAction() != Reservation::Action::IDLE;
}

bool ReservationTable::isIdle(int32_t slot_offset) const {
	return !this->isUtilized(slot_offset);
}

bool ReservationTable::isIdle(int32_t start, uint32_t length) const {
	if (length == 1)
		return this->isIdle(start);
	if (!this->isValid(start, length))
		throw std::invalid_argument("Invalid slot range: start=" + std::to_string(start) + " length=" + std::to_string(length));
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

int32_t ReservationTable::findEarliestIdleRange(int32_t start, uint32_t length) const {
	if (!isValid(start, length))
		throw std::invalid_argument("Invalid slot range!");
	for (int32_t i = start; i < int32_t(this->planning_horizon); i++) {
		if (this->isIdle(i, length))
			return i;
	}
	throw std::runtime_error("No idle slot range of specified length found.");
}

bool ReservationTable::isValid(int32_t slot_offset) const {
	return abs(slot_offset) <= this->planning_horizon; // can't move more than one horizon into either direction of time.
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
	for (auto it = slot_utilization_vec.begin(); it < slot_utilization_vec.begin() + num_slots; it++) {
		Reservation reservation = *it;
		if (reservation.getAction() != Reservation::Action::IDLE)
			num_busy_slots++;
	}
	num_idle_future_slots += num_busy_slots; // As these go out of scope, we may have more idle slots now.
	
	// Shift all elements to the front, old ones are overwritten.
	std::move(this->slot_utilization_vec.begin() + num_slots, this->slot_utilization_vec.end(), this->slot_utilization_vec.begin());
	// All new elements are initialized as idle.
	for (auto it = slot_utilization_vec.end() - 1; it >= slot_utilization_vec.end() - num_slots; it--)
		*it = Reservation(SYMBOLIC_ID_UNSET, Reservation::Action::IDLE);
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

std::vector<int32_t> ReservationTable::findCandidateSlots(unsigned int min_offset, unsigned int num_candidates,
                                                          unsigned int range_length) const {
	std::vector<int32_t> start_slots;
	int32_t last_offset = min_offset;
	for (size_t i = 0; i < num_candidates; i++) {
		// Try to find another slot range.
		try {
			int32_t start_slot = findEarliestIdleRange(last_offset, range_length);
			start_slots.push_back(start_slot);
			last_offset = start_slot + 1; // Next attempt, look later than current one.
		} catch (const std::runtime_error& e) {
			// This is thrown if no idle range can be found.
			break; // Stop if no more ranges can be found.
		} catch (const std::invalid_argument& e) {
			// This is thrown if the input is invalid (i.e. we are exceeding the planning horizon).
			break; // Stop if no more ranges can be found.
		} // all other exceptions should still end execution
	}
	return start_slots;
}

int32_t ReservationTable::findEarliestOffset(const int32_t start_offset, const Reservation& reservation) const {
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

ReservationTable::~ReservationTable() = default;
