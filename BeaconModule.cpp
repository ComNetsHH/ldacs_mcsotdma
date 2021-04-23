//
// Created by seba on 4/14/21.
//

#include <cmath>
#include <iostream>
#include "BeaconModule.hpp"
#include "coutdebug.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

const unsigned int BeaconModule::MIN_BEACON_OFFSET = 80; /* 80*12ms=960ms */
const unsigned int BeaconModule::MAX_BEACON_OFFSET = 25000; /* 5min */
const unsigned int BeaconModule::INITIAL_BEACON_OFFSET = MIN_BEACON_OFFSET;

BeaconModule::BeaconModule(unsigned int min_beacon_gap, double congestion_goal) : min_beacon_gap(min_beacon_gap), BC_CONGESTION_GOAL(congestion_goal), random_device(new std::random_device), generator((*random_device)()) {}

BeaconModule::BeaconModule() : BeaconModule(1, .45) {}

BeaconModule::~BeaconModule() {
	delete random_device;
}

bool BeaconModule::isConnected() const {
	return this->is_connected;
}

unsigned int BeaconModule::chooseNextBeaconSlot(unsigned int min_beacon_offset, unsigned int num_candidates, unsigned int min_gap_to_next_beacon, const ReservationTable *bc_table, const ReservationTable *tx_table) {
	std::vector<unsigned int> viable_slots;
	// Until we've found sufficiently many candidates...
	for (int t = (int) min_beacon_offset; viable_slots.size() < num_candidates && t < (int) bc_table->getPlanningHorizon(); t++) {
		// ... if it is idle,
		if (bc_table->isIdle(t) && tx_table->isIdle(t)) {
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

unsigned int BeaconModule::scheduleNextBeacon(double avg_broadcast_rate, unsigned int num_active_neighbors, const ReservationTable *bc_table, const ReservationTable *tx_table) {
	this->beacon_offset = computeBeaconInterval(BC_CONGESTION_GOAL, avg_broadcast_rate, num_active_neighbors);
	next_beacon_in = chooseNextBeaconSlot(this->beacon_offset, this->N_BEACON_SLOT_CANDIDATES, min_beacon_gap, bc_table, tx_table);
	return next_beacon_in;
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

std::pair<L2HeaderBeacon*, BeaconPayload*> BeaconModule::generateBeacon(const std::vector<ReservationTable*>& reservation_tables, const ReservationTable *bc_table) const {
	auto *payload = new BeaconPayload(reservation_tables);
	payload->encode(bc_table->getLinkedChannel()->getCenterFrequency(), bc_table);
	return {new L2HeaderBeacon(), payload};
}

void BeaconModule::parseBeacon(const MacId &sender_id, const BeaconPayload *&payload, ReservationManager* manager) const {
	if (payload != nullptr) {
		// Go through all indicated reservations...
		for (const auto& pair : payload->local_reservations) {
			// ... fetch frequency channel ...
			const uint64_t center_freq = pair.first;
			const FrequencyChannel* channel = manager->getFreqChannelByCenterFreq(center_freq);
			ReservationTable* table = manager->getReservationTable(channel);
			coutd << "f=" << *channel << ": ";
			// ... for every time slot ...
			for (auto slot : pair.second) {
				int t = (int) slot;
				const Reservation& res = table->getReservation(t);
				// ... mark it as BUSY if it's locally idle
				if (res.isIdle()) {
					table->mark(t, Reservation(sender_id, Reservation::BUSY));
					coutd << "marked t=" << t << " as busy -> ";
				} else if (!res.isBusy()) // print error if it's neither IDLE nor BUSY
					coutd << "won't mark t=" << t << " which is already reserved for: " << res << " -> ";
			}
		}
	} else
		coutd << "ignoring empty beacon payload -> ";
	coutd << "done parsing beacon -> ";
}


