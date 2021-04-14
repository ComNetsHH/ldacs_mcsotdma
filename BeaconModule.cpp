//
// Created by seba on 4/14/21.
//

#include <cmath>
#include <iostream>
#include "BeaconModule.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

BeaconModule::BeaconModule(ReservationTable* bc_table, unsigned int min_beacon_gap) : bc_table(bc_table), min_beacon_gap(min_beacon_gap) {}

BeaconModule::BeaconModule(ReservationTable* bc_table) : BeaconModule(bc_table, 1) {}

void BeaconModule::setBcReservationTable(ReservationTable* broadcast_reservation_table) {
	this->bc_table = broadcast_reservation_table;
}

bool BeaconModule::isConnected() const {
	return this->is_connected;
}

unsigned int BeaconModule::chooseNextBeaconSlot(bool random_choice) const {
	return 0;
}

unsigned int BeaconModule::computeBeaconInterval(double target_congestion, double avg_broadcast_rate, unsigned int num_active_neighbors) const {
	// Use same variable names as in the specification.
	double &n = target_congestion, &r = avg_broadcast_rate, m = (double) num_active_neighbors;
	// Find offset that meets congestion target.
	auto tau = (unsigned int) (std::ceil(m*(1+r)) / n);
	// Return within allowed bounds.
	return std::min(max_beacon_offset, std::max(min_beacon_offset, tau));
}
