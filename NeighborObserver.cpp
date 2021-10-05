/*
 * Created by Sebastian Lindner on Tue Oct 05 2021
 *
 * Copyright (c) 2021 Hamburg University of Technology
 */

#include "NeighborObserver.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

NeighborObserver::NeighborObserver(unsigned int max_time_slots_until_neighbor_not_active_anymore) : max_last_seen_val(max_time_slots_until_neighbor_not_active_anymore) {}

void NeighborObserver::reportActivity(const MacId& id) {
	auto it = active_neighbors.find(id);
	// if id does not exist
	if (it == active_neighbors.end())
		active_neighbors.insert({id, 0}); // add it
	else  // if it does
		(*it).second = 0;  // then set the last-seen value to zero slots	
}

void NeighborObserver::onSlotEnd() {
	// go through all active neighbors
	for (auto &pair : active_neighbors) {
		// increment its last-seen value
		pair.second++;
		// and remove it if it hasn't been reported for too long
		if (pair.second >= this->max_last_seen_val) 
			active_neighbors.erase(pair.first);
	}
}

size_t NeighborObserver::getNumActiveNeighbors() const {
	return active_neighbors.size();
}

bool NeighborObserver::isActive(const MacId& id) const {
	return active_neighbors.find(id) == active_neighbors.end();
}

std::vector<MacId> NeighborObserver::getActiveNeighbors() const {
	std::vector<MacId> ids;
	for (const auto& pair : active_neighbors)		
		ids.push_back(pair.first);
	return ids;
}