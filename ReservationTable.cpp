//
// Created by Sebastian Lindner on 06.10.20.
//

#include <stdexcept>
#include <algorithm>
#include <math.h>
#include <limits>
#include <iostream>
#include "ReservationTable.hpp"

TUHH_INTAIRNET_MCSOTDMA::ReservationTable::ReservationTable(uint32_t planning_horizon) : planning_horizon(planning_horizon), slot_utilization_vec(std::vector<bool>(uint64_t(planning_horizon * 2 + 1))) {
	// The planning horizon denotes how many slots we want to be able to look into future and past.
	// Since the current moment in time must also be represented, we need planning_horizon*2+1 values.
	// If we use UINT32_MAX, then we wouldn't be able to store 2*UINT32_MAX+1 in UINT64, so throw an exception if this is attempted.
	if (planning_horizon == UINT32_MAX)
		throw std::invalid_argument("Cannot instantiate a reservation table with a planning horizon of UINT32_MAX. It must be one slot less.");
}

uint32_t TUHH_INTAIRNET_MCSOTDMA::ReservationTable::getPlanningHorizon() const {
	return this->planning_horizon;
}

void TUHH_INTAIRNET_MCSOTDMA::ReservationTable::mark(int32_t slot_offset, bool utilized) {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("Reservation table planning horizon smaller than queried slot offset!");
	// 0 to p-1 for p past slots
	// p is the current slot
	// p+1 to 2p+1 for p future slots
	// where p is the planning horizon
	this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset)) = utilized;
}

bool TUHH_INTAIRNET_MCSOTDMA::ReservationTable::isUtilized(int32_t slot_offset) {
	if (!this->isValid(slot_offset))
		throw std::invalid_argument("Reservation table planning horizon smaller than queried offset!");
	return this->slot_utilization_vec.at(convertOffsetToIndex(slot_offset));
}

bool TUHH_INTAIRNET_MCSOTDMA::ReservationTable::isIdle(int32_t slot_offset) {
	return !this->isUtilized(slot_offset);
}

bool TUHH_INTAIRNET_MCSOTDMA::ReservationTable::isIdle(int32_t start, uint32_t length) {
	if (length == 1)
		return this->isUtilized(start);
	if (!this->isValid(start, length))
		throw std::invalid_argument("Invalid slot range: start=" + std::to_string(start) + " length=" + std::to_string(length));
	// A slot range is idle if ALL slots within are idle.
	for (int32_t slot = start; slot < start + int32_t(length); slot++) {
 		if (isUtilized(slot)) {
		    return false;
	    }
	}
	return true;
	
//	return std::all_of(this->slot_utilization_vec.begin() + index_start, this->slot_utilization_vec.begin() + (index_start + length), [](bool is_utilized) {return is_utilized;});
}

bool TUHH_INTAIRNET_MCSOTDMA::ReservationTable::isUtilized(int32_t start, uint32_t length) {
	// A slot range is utilized if any slot within is utilized.
	return !this->isIdle(start, length);
}

int32_t TUHH_INTAIRNET_MCSOTDMA::ReservationTable::findEarliestIdleRange(int32_t start, uint32_t length) {
	if (!isValid(start, length))
		throw std::invalid_argument("Invalid slot range!");
	for (int32_t i = start; i < int32_t(this->planning_horizon); i++) {
		if (this->isIdle(start, length))
			return i;
	}
	throw std::runtime_error("No idle slot range of specified length found.");
}

bool TUHH_INTAIRNET_MCSOTDMA::ReservationTable::isValid(int32_t slot_offset) const {
	return abs(slot_offset) <= this->planning_horizon; // can't move more than one horizon into either direction of time.
}

bool TUHH_INTAIRNET_MCSOTDMA::ReservationTable::isValid(int32_t start, uint32_t length) const {
	if (length == 1)
		return this->isValid(start);
	return isValid(start) && int32_t(start + length) <= int32_t(planning_horizon);
}

uint64_t TUHH_INTAIRNET_MCSOTDMA::ReservationTable::getCurrentSlot() const {
	return this->last_updated;
}

void TUHH_INTAIRNET_MCSOTDMA::ReservationTable::update(uint64_t num_slots) {
	std::move(this->slot_utilization_vec.begin() + num_slots, this->slot_utilization_vec.end(), this->slot_utilization_vec.begin());
}

const std::vector<bool>& TUHH_INTAIRNET_MCSOTDMA::ReservationTable::getVec() const {
	return this->slot_utilization_vec;
}

uint64_t TUHH_INTAIRNET_MCSOTDMA::ReservationTable::convertOffsetToIndex(int32_t slot_offset) const {
	// The vector has planning_horizon-many past slots, one current slot, and planning_horizon-many future slots.
	// So planning_horizon+0 indicates the current slot, which is the basis for this relative access.
	return planning_horizon + slot_offset;
}
