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

BeaconModule::BeaconModule(ReservationTable* bc_table, unsigned int min_beacon_gap, double congestion_goal) : bc_table(bc_table), min_beacon_gap(min_beacon_gap), BC_CONGESTION_GOAL(congestion_goal), random_device(new std::random_device), generator((*random_device)()) {}

BeaconModule::BeaconModule() : BeaconModule(nullptr, 1, .45) {}

BeaconModule::~BeaconModule() {
	delete random_device;
}

void BeaconModule::setBcReservationTable(ReservationTable* broadcast_reservation_table) {
	this->bc_table = broadcast_reservation_table;
}

bool BeaconModule::isConnected() const {
	return this->is_connected;
}

unsigned int BeaconModule::chooseNextBeaconSlot(unsigned int min_beacon_offset, unsigned int num_candidates, unsigned int min_gap_to_next_beacon) {
	std::vector<unsigned int> viable_slots;
	// Until we've found sufficiently many candidates...
	for (int t = (int) min_beacon_offset; viable_slots.size() < num_candidates && t < (int) bc_table->getPlanningHorizon(); t++) {
		// ... fetch the reservation at t
		const Reservation &res = bc_table->getReservation(t);
		// ... if it is idle,
		if (res.isIdle()) {
			// ... ensure that in both directions of time, at least the min of non-beacon slots is kept
			bool viable = true;
			// ... check past
			for (int t_past = 1; viable && t_past < (int) min_gap_to_next_beacon + 1; t_past++)
				viable = !bc_table->getReservation(t - t_past).isBeacon(); // checks for both own beacon tx and rx of other users' beacons
			// ... check future
			if (viable)
				for (int t_future = 1; viable && t_future < (int) min_gap_to_next_beacon + 1; t_future++)
					viable = !bc_table->getReservation(t + t_future).isBeacon();
			// ... and save the slot if all conditions are met.
			if (viable)
				viable_slots.push_back((unsigned int) t);
		}
	}

	if (viable_slots.empty())
		throw std::runtime_error("BeaconModule::chooseNextBeaconSlot couldn't find a single viable slot.");
	// Choose a random slot from the viable ones.
	unsigned int chosen_slot;
	if (num_candidates > 1)
		chosen_slot = viable_slots.at(getRandomInt(0, viable_slots.size()));
	else
		chosen_slot = viable_slots.at(0);
	return chosen_slot;
}

unsigned int BeaconModule::computeBeaconInterval(double target_congestion, double avg_broadcast_rate, unsigned int num_active_neighbors) const {
	// Use same variable names as in the specification.
	double &n = target_congestion, &r = avg_broadcast_rate, m = (double) num_active_neighbors;
	// Find offset that meets congestion target.
	auto tau = (unsigned int) (std::ceil(m*(1 + r) / n));
	// Return within allowed bounds.
	return std::min(MAX_BEACON_OFFSET, std::max(MIN_BEACON_OFFSET, tau));
}

bool BeaconModule::shouldSendBeaconThisSlot() const {
	return next_beacon_in == 0;
}

void BeaconModule::onSlotEnd() {
	next_beacon_in -= 1;
}

void BeaconModule::scheduleNextBeacon(double avg_broadcast_rate, unsigned int num_active_neighbors) {
	this->beacon_offset = computeBeaconInterval(BC_CONGESTION_GOAL, avg_broadcast_rate, num_active_neighbors);
	next_beacon_in = chooseNextBeaconSlot(this->beacon_offset, this->N_BEACON_SLOT_CANDIDATES, min_beacon_gap);
}

unsigned int BeaconModule::getBeaconOffset() const {
	return this->beacon_offset;
}

void BeaconModule::setMinBeaconGap(unsigned int n) {
	this->min_beacon_gap = n;
}

unsigned long BeaconModule::getRandomInt(size_t start, size_t end) {
	if (start == end)
		return start;
	std::uniform_int_distribution<> distribution(start, end - 1);
	return distribution(generator);
}


