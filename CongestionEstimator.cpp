//
// Created by seba on 4/14/21.
//

#include "CongestionEstimator.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

CongestionEstimator::CongestionEstimator(size_t horizon) : horizon(horizon), congestion_average(horizon), num_slots_so_far(0) {}

void CongestionEstimator::reportBroadcast(const MacId& id) {
	congestion_average.put(1);
	active_neighbors_list.insert(id);
	broadcast_reported_this_slot = true;
}

void CongestionEstimator::update(size_t num_slots) {
	if (!broadcast_reported_this_slot)
		congestion_average.put(0);
	broadcast_reported_this_slot = false;
	num_slots_so_far += num_slots;
	if (num_slots_so_far > horizon)
		throw std::runtime_error("CongestionEstimation has exceeded its horizon!");
}

void CongestionEstimator::reset(size_t new_horizon) {
	// Start a new average.
	this->horizon = new_horizon;
	congestion_average = MovingAverage(new_horizon);
	// Copy last-active neighbors.
	last_active_neighbors_list = active_neighbors_list;
	// And clear the current ones.
	active_neighbors_list.clear();
	num_slots_so_far = 0;
}

double CongestionEstimator::getCongestion() const {
	return congestion_average.get();
}

unsigned int CongestionEstimator::getNumActiveNeighbors() const {
	std::set<MacId> all_active_neighbors = std::set<MacId>(last_active_neighbors_list.begin(), last_active_neighbors_list.end());
	all_active_neighbors.insert(active_neighbors_list.begin(), active_neighbors_list.end());
	return all_active_neighbors.size();
}

bool CongestionEstimator::isActive(const MacId& id) const {
	std::set<MacId> all_active_neighbors = std::set<MacId>(last_active_neighbors_list.begin(), last_active_neighbors_list.end());
	all_active_neighbors.insert(active_neighbors_list.begin(), active_neighbors_list.end());
	return all_active_neighbors.find(id) != all_active_neighbors.end();
}
