//
// Created by seba on 4/14/21.
//

#include <cmath>
#include <iostream>
#include "BeaconModule.hpp"
#include "coutdebug.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

BeaconModule::BeaconModule(unsigned int min_beacon_gap, double congestion_goal) : min_beacon_gap(min_beacon_gap), BC_CONGESTION_GOAL(congestion_goal) {}

BeaconModule::BeaconModule() : BeaconModule(1, .45) {}

BeaconModule::~BeaconModule() {
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
	return viable_slots.at(getRandomInt(0, viable_slots.size()));
}

unsigned int BeaconModule::computeBeaconInterval(double target_congestion, double avg_broadcast_rate, unsigned int num_active_neighbors) const {	
	// Use same variable names as in the specification.
	double &n = target_congestion, &r = avg_broadcast_rate, m = (double) num_active_neighbors;
	// Find offset that meets congestion target.
	auto tau = (unsigned int) (std::ceil(m*(1 + r) / n));	
	// Return within allowed bounds.
	return std::min(max_beacon_offset, std::max(min_beacon_offset, tau));
}

bool BeaconModule::shouldSendBeaconThisSlot() const {
	return isEnabled() && next_beacon_in == 0;
}

void BeaconModule::onSlotEnd() {
	if (next_beacon_in > 0)
		next_beacon_in--;
}

unsigned int BeaconModule::scheduleNextBeacon(double avg_broadcast_rate, unsigned int num_active_neighbors, const ReservationTable *bc_table, const ReservationTable *tx_table) {
	this->beacon_offset = computeBeaconInterval(BC_CONGESTION_GOAL, avg_broadcast_rate, num_active_neighbors);
	next_beacon_in = chooseNextBeaconSlot(this->beacon_offset, this->b_beacon_slot_candidates, min_beacon_gap, bc_table, tx_table);
	return next_beacon_in;
}

unsigned int BeaconModule::getBeaconOffset() const {
	return this->beacon_offset;
}

void BeaconModule::setMinBeaconGap(unsigned int n) {
	this->min_beacon_gap = n;
}

std::pair<L2HeaderBeacon*, BeaconPayload*> BeaconModule::generateBeacon(const std::vector<ReservationTable*>& reservation_tables, const ReservationTable *bc_table, const SimulatorPosition simulatorPosition, size_t num_utilized_p2p_resources, size_t burst_offset) {
	auto *payload = new BeaconPayload();	
	// write resource utilization into the beacon payload
	if (this->write_resource_utilization_into_beacon) {
		payload->encode(bc_table->getLinkedChannel()->getCenterFrequency(), bc_table);
		if (!reservation_tables.empty()) {
			if (flip_p2p_table_encoding) {
				for (auto it = reservation_tables.end() - 1; it != reservation_tables.begin(); it--)
					payload->encode((*it)->getLinkedChannel()->getCenterFrequency(), *it);
				flip_p2p_table_encoding = false;
			} else {
				for (auto it = reservation_tables.begin(); it != reservation_tables.end(); it++)
					payload->encode((*it)->getLinkedChannel()->getCenterFrequency(), *it);
				flip_p2p_table_encoding = true;
			}
		}
	}

	auto position  = CPRPosition(simulatorPosition);
	L2HeaderBeacon::CongestionLevel congestion_level;	
	// assume bidirectional, ARQ-protected P2P links with two resources each	
	if (num_utilized_p2p_resources > burst_offset) {
		delete payload;
		throw std::invalid_argument("BeaconModule::generateBeacon was told there's more utilized resources than there are available.");
	}
	double congestion = ((double) num_utilized_p2p_resources) / ((double) burst_offset);
	if (congestion < .25)
		congestion_level = L2HeaderBeacon::CongestionLevel::uncongested;
	else if (congestion < .5)
		congestion_level = L2HeaderBeacon::CongestionLevel::slightly_congested;
	else if (congestion < .75)
		congestion_level = L2HeaderBeacon::CongestionLevel::moderately_congested;
	else
		congestion_level = L2HeaderBeacon::CongestionLevel::congested;		
	CPRPosition::PositionQuality pos_quality = CPRPosition::PositionQuality::low;
	L2HeaderBeacon *header = new L2HeaderBeacon(position, position.odd, congestion_level, pos_quality);
	return {header, payload};
}

std::pair<bool, bool> BeaconModule::parseBeacon(const MacId &sender_id, const BeaconPayload *&payload, ReservationManager* manager) const {
	bool must_reschedule_beacon = false;
	bool must_reschedule_broadcast = false;
	if (payload != nullptr) {
		// Go through all indicated reservations...
		for (const auto& item : payload->local_reservations) {
			// ... fetch frequency channel ...
			const uint64_t center_freq = item.first;
			const FrequencyChannel* channel = manager->getFreqChannelByCenterFreq(center_freq);
			ReservationTable* table = manager->getReservationTable(channel);
			coutd << "beacon indicates next transmission on f=" << *channel << " at ";
			// ... for every time slot ...
			for (const auto& pair : item.second) {
				int t = (int) pair.first;
				// ... mark it as RX_BEACON if the sender indicated it'll transmit a beacon, or as BUSY otherwise.
				const Reservation::Action action = pair.second == Reservation::TX_BEACON ? Reservation::RX_BEACON : Reservation::BUSY;
				coutd << "t=" << t << " ";
				const Reservation& res = table->getReservation(t);
				// ... mark it as BUSY if it's locally idle
				if (res.isIdle()) {
					table->mark(t, Reservation(sender_id, action));
					coutd << "marked t=" << t << " as " << action << " -> ";
				} else {
					coutd << "won't mark t=" << t << " which is already reserved for: " << res << " -> ";
					// We have to re-schedule our beacon transmission if this beacon tells us that another transmission is going to take place.
					if (channel->isBroadcastChannel() && res.getAction() == Reservation::TX_BEACON) {
						coutd << "re-scheduling own beacon transmission since it would collide -> ";
						must_reschedule_beacon = true;
					}
					// We have to re-schedule our broadcast transmission if this beacon tells us that another transmission is going to take place.
					if (channel->isBroadcastChannel() && res.isTx()) {
						coutd << "re-scheduling own broadcast transmission since it would collide -> ";
						must_reschedule_broadcast = true;
					}
				}
			}
		}
	} else
		coutd << "ignoring empty beacon payload -> ";
	coutd << "done parsing beacon -> ";
	return {must_reschedule_beacon, must_reschedule_broadcast};
}

unsigned int BeaconModule::getNextBeaconOffset() const {
	return next_beacon_in;
}

void BeaconModule::setEnabled(bool val) {
	this->enabled = val;
}

bool BeaconModule::isEnabled() const {
	return this->enabled;
}

void BeaconModule::reset() {
	next_beacon_in = 0;
}

void BeaconModule::setMinBeaconCandidateSlots(unsigned int value) {
	this->b_beacon_slot_candidates = value;
}

unsigned int BeaconModule::getMinBeaconCandidateSlots() const {
	return this->b_beacon_slot_candidates;
}

void BeaconModule::setMinBeaconInterval(unsigned int value) {
	this->min_beacon_offset = value;
}

void BeaconModule::setMaxBeaconInterval(unsigned int value) {
	this->max_beacon_offset = value;
}

unsigned int BeaconModule::getMinBeaconInterval() const {
	return this->min_beacon_offset;
}

unsigned int BeaconModule::getMaxBeaconInterval() const {
	return this->max_beacon_offset;
}

void BeaconModule::setWriteResourceUtilizationIntoBeacon(bool flag) {
	this->write_resource_utilization_into_beacon = flag;
}