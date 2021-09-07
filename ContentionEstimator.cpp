//
// Created by Sebastian Lindner on 11.12.20.
//

#include <iostream>
#include "ContentionEstimator.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ContentionEstimator::ContentionEstimator(size_t horizon) : horizon(horizon) {}

ContentionEstimator::ContentionEstimator() : horizon(DEFAULT_CONTENTION_WINDOW_SIZE) {}

ContentionEstimator::ContentionEstimator(const ContentionEstimator& other) : ContentionEstimator(other.horizon) {
	avg_broadcast_rate_per_id = other.avg_broadcast_rate_per_id;
}

void ContentionEstimator::reportNonBeaconBroadcast(const MacId& id, unsigned int current_slot) {
	// Have to compute the beacon interval.
	unsigned int beacon_interval;
	// If the user has not been observed before...
	if (avg_broadcast_rate_per_id.find(id) == avg_broadcast_rate_per_id.end()) {
		// ... add a new moving average for its broadcast rate
		avg_broadcast_rate_per_id.emplace(id, MovingAverage(this->horizon));
		// ... and use the no. of slots since the beginning of time as its beacon interval
		beacon_interval = current_slot;
	// If it has ...
	} else {
		// ... then compute the beacon interval
		beacon_interval = current_slot - last_broadcast_per_id.find(id)->second;
	}
	avg_broadcast_rate_per_id.at(id).put(1);
	last_broadcast_per_id[id] = current_slot;
	broadcast_interval_per_id[id] = beacon_interval;
	id_of_broadcast_this_slot = id;
}

void ContentionEstimator::onSlotEnd(unsigned int current_slot) {
	// Put in zero's for all those users that didn't broadcast this slot.
	for (auto &it : avg_broadcast_rate_per_id) {
		const MacId &id = it.first;
		if (id != id_of_broadcast_this_slot)
			it.second.put(0);
	}
	// Erase users that haven't been active within the contention window.
	for (auto it = last_broadcast_per_id.begin(); it != last_broadcast_per_id.end();) {
		if (current_slot - it->second > horizon) {
			MacId id = it->first;
			it = last_broadcast_per_id.erase(it);
			broadcast_interval_per_id.erase(id);
		} else
			it++;
	}
	id_of_broadcast_this_slot = SYMBOLIC_ID_UNSET;
}

double ContentionEstimator::getContentionEstimate(const MacId& id) const {
	auto it = avg_broadcast_rate_per_id.find(id);
	if (it == avg_broadcast_rate_per_id.end())
		return 0.0;
	return it->second.get();
}

size_t ContentionEstimator::getHorizon() const {
	return this->horizon;
}

std::vector<MacId> ContentionEstimator::getActiveNeighbors() const {
	std::vector<MacId> ids;
	for (const auto& estimate : avg_broadcast_rate_per_id)
		if (estimate.second.get() > 0.0)
			ids.push_back(estimate.first);
	return ids;
}

unsigned int ContentionEstimator::getNumActiveNeighbors() const {
	return getActiveNeighbors().size();
}

double ContentionEstimator::getAverageNonBeaconBroadcastRate() const {
	double r = 0.0, n = 0.0;
	for (const auto& estimate : avg_broadcast_rate_per_id) {
		double neighbor_rate = estimate.second.get();
		if (neighbor_rate > 0.0) {
			r += neighbor_rate;
			n++;
		}
	}
	return n > 0.0 ? r / n : 0.0;
}

double ContentionEstimator::getChannelAccessProbability(const MacId& id, unsigned int current_slot) const {
	// If the particular user hasn't been observed, it won't be active.
	if (broadcast_interval_per_id.find(id) == broadcast_interval_per_id.end() || last_broadcast_per_id.find(id) == last_broadcast_per_id.end())
		return 0.0;
	// If it has, then estimate its channel access probability linearly with the number of slots since its last broadcast and the last-observed broadcast interval.
	auto broadcast_interval = (double) broadcast_interval_per_id.at(id);
	auto last_broadcast = (double) last_broadcast_per_id.at(id);
	return std::min(1.0, (current_slot - last_broadcast) / broadcast_interval);
}


