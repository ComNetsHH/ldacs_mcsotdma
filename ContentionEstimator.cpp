//
// Created by Sebastian Lindner on 11.12.20.
//

#include <iostream>
#include "ContentionEstimator.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

ContentionEstimator::ContentionEstimator(size_t horizon) : horizon(horizon) {}

void ContentionEstimator::reportBroadcast(const MacId& id) {
	received_broadcast_on_this_slot[id] = true;
	contention_estimates.emplace(id, MovingAverage(this->horizon));
}

void ContentionEstimator::update() {
	// For every neighbor that was reported...
	for (auto& it : received_broadcast_on_this_slot) {
		const MacId& id = it.first;
		bool received_broadcast = it.second;
		// ... put the number of broadcasts in this slot into the corresponding MovingAverage...
		contention_estimates.at(id).put(received_broadcast ? 1 : 0);
		// ... and set the reporting to false for the next slot.
		it.second = false;
	}
}

double ContentionEstimator::getContentionEstimate(const MacId& id) const {
	auto it = contention_estimates.find(id);
	if (it == contention_estimates.end())
		throw std::out_of_range("ContentionEstimator::getContentionEstimate doesn't keep ID '" + std::to_string(id.getId()) + "'.");
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
	return n > 0.0 ? r/n : 0.0;
}


