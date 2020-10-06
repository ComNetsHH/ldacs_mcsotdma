//
// Created by Sebastian Lindner on 06.10.20.
//

#include <stdexcept>
#include <algorithm>
#include "ReservationTable.hpp"

TUHH_INTAIRNET_MCSOTDMA::ReservationTable::ReservationTable(uint32_t planning_horizon) : planning_horizon(planning_horizon), slot_utilization_vec(std::vector<bool>(planning_horizon)) {}

uint32_t TUHH_INTAIRNET_MCSOTDMA::ReservationTable::getPlanningHorizon() const {
	return this->planning_horizon;
}

bool TUHH_INTAIRNET_MCSOTDMA::ReservationTable::isIdle(uint32_t slot_offset) {
	if (slot_offset > getPlanningHorizon())
		throw std::invalid_argument("Reservation table planning horizon smaller than queried offset!");
	return this->slot_utilization_vec.at(slot_offset);
}

void TUHH_INTAIRNET_MCSOTDMA::ReservationTable::mark(uint32_t slot_offset, bool utilized) {
	if (slot_offset > getPlanningHorizon())
		throw std::invalid_argument("Reservation table planning horizon smaller than queried slot offset!");
	this->slot_utilization_vec.at(slot_offset) = utilized;
}

std::vector<bool> TUHH_INTAIRNET_MCSOTDMA::ReservationTable::isIdle(uint32_t start, uint32_t end) {
	if (start > getPlanningHorizon() || end > getPlanningHorizon())
		throw std::invalid_argument("Reservation table planning horizon smaller than queried slot offset!");
	return std::vector<bool>(this->slot_utilization_vec.begin() + start, this->slot_utilization_vec.begin() + end);
}

uint32_t TUHH_INTAIRNET_MCSOTDMA::ReservationTable::findEarliestIdleRange(uint32_t start, uint32_t length) {
	if (start > getPlanningHorizon() || length > getPlanningHorizon())
		throw std::invalid_argument("Reservation table planning horizon smaller than queried slot offset or length!");
	for (uint32_t i = start; i < this->planning_horizon - length; i++) {
		bool is_idle = std::all_of(this->slot_utilization_vec.begin() + i, this->slot_utilization_vec.begin() + i + length, [](bool is_utilized) {return !is_utilized;});
		if (is_idle)
			return i;
	}
	throw std::runtime_error("No idle slot range of specified length found.");
}
