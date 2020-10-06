//
// Created by Sebastian Lindner on 06.10.20.
//

#include <stdexcept>
#include "ReservationTable.hpp"

TUHH_INTAIRNET_MCSOTDMA::ReservationTable::ReservationTable(uint32_t planning_horizon) : planning_horizon(planning_horizon) {
	this->slot_utilization_vec = std::vector<bool>(planning_horizon);
}

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
		throw std::invalid_argument("Reservation table planning horizon smaller than queried slot_offset!");
	this->slot_utilization_vec.at(slot_offset) = utilized;
}
