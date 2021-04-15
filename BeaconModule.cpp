//
// Created by seba on 4/14/21.
//

#include <cmath>
#include <iostream>
#include "BeaconModule.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

const unsigned int BeaconModule::MIN_BEACON_OFFSET = 80; /* 80*12ms=960ms */
const unsigned int BeaconModule::MAX_BEACON_OFFSET = 25000; /* 5min */
const unsigned int BeaconModule::INITIAL_BEACON_OFFSET = MIN_BEACON_OFFSET;

BeaconModule::BeaconModule(ReservationTable* bc_table, unsigned int min_beacon_gap) : bc_table(bc_table), min_beacon_gap(min_beacon_gap) {}

BeaconModule::BeaconModule(ReservationTable* bc_table) : BeaconModule(bc_table, 1) {}

void BeaconModule::setBcReservationTable(ReservationTable* broadcast_reservation_table) {
	this->bc_table = broadcast_reservation_table;
}

bool BeaconModule::isConnected() const {
	return this->is_connected;
}

unsigned int BeaconModule::chooseNextBeaconSlot(bool random_choice) const {
	std::vector<unsigned int> viable_slots;
//	for (unsigned int t = beacon_offset)

	return beacon_offset;
}

unsigned int BeaconModule::computeBeaconInterval(double target_congestion, double avg_broadcast_rate, unsigned int num_active_neighbors) const {
	// Use same variable names as in the specification.
	double &n = target_congestion, &r = avg_broadcast_rate, m = (double) num_active_neighbors;
	// Find offset that meets congestion target.
	auto tau = (unsigned int) (std::ceil(m*(1+r)) / n);
	// Return within allowed bounds.
	return std::min(MAX_BEACON_OFFSET, std::max(MIN_BEACON_OFFSET, tau));
}

bool BeaconModule::shouldSendBeaconThisSlot() const {
	return next_beacon_in == 0;
}

void BeaconModule::update(size_t num_slots) {
	if (num_slots > next_beacon_in)
		throw std::invalid_argument("BeaconModule::onSlotEnd(" + std::to_string(num_slots) + ") misses next beacon slot!");
	next_beacon_in -= num_slots;
}

void BeaconModule::scheduleNextBeacon() {
	next_beacon_in = chooseNextBeaconSlot(false);
}

unsigned int BeaconModule::getBeaconOffset() const {
	return this->beacon_offset;
}


