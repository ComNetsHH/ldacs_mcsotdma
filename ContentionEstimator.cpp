//
// Created by Sebastian Lindner on 11.12.20.
//

#include <iostream>
#include "ContentionEstimator.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ContentionEstimator::ContentionEstimator(size_t horizon) : horizon(horizon) {}

ContentionEstimator::ContentionEstimator() : horizon(DEFAULT_CONTENTION_WINDOW_SIZE) {}

ContentionEstimator::ContentionEstimator(const ContentionEstimator& other) : ContentionEstimator(other.horizon) {
	contention_estimates = other.contention_estimates;
}

void ContentionEstimator::reportNonBeaconBroadcast(const MacId& id) {
	if (contention_estimates.find(id) == contention_estimates.end())
		contention_estimates.emplace(id, MovingAverage(this->horizon));
	contention_estimates.at(id).put(1);
	id_of_broadcast_this_slot = id;
}

void ContentionEstimator::update(size_t num_slots) {
	// Put in zero's for all those users that didn't broadcast this slot.
	for (auto &it : contention_estimates) {
		const MacId &id = it.first;
		if (id != id_of_broadcast_this_slot)
			it.second.put(0);
	}
	id_of_broadcast_this_slot = SYMBOLIC_ID_UNSET;
	if (num_slots > 1)
		update(num_slots - 1);
}

double ContentionEstimator::getContentionEstimate(const MacId& id) const {
	auto it = contention_estimates.find(id);
	if (it == contention_estimates.end())
		return 0.0;
	return it->second.get();
}

size_t ContentionEstimator::getHorizon() const {
	return this->horizon;
}

unsigned int ContentionEstimator::getNumActiveNeighbors() const {
	unsigned int n = 0;
	for (const auto& estimate : contention_estimates)
		if (estimate.second.get() > 0.0)
			n++;
	return n;
}

double ContentionEstimator::getAverageBroadcastRate() const {
	double r = 0.0, n = 0.0;
	for (const auto& estimate : contention_estimates) {
		double neighbor_rate = estimate.second.get();
		if (neighbor_rate > 0.0) {
			r += neighbor_rate;
			n++;
		}
	}
	return n > 0.0 ? r / n : 0.0;
}


