//
// Created by Sebastian Lindner on 10.12.20.
//

#include <iostream>
#include "MovingAverage.hpp"

using namespace TUHH_INTAIRNET_MCSOTDMA;

MovingAverage::MovingAverage(unsigned int num_values) : values(num_values), index(0) {}
MovingAverage::MovingAverage(const MovingAverage& old, unsigned int num_values) : values(num_values), index(0) {
	// copy as many values as fit
	for (size_t t = 0; t < std::min((size_t) num_values, (size_t) old.index); t++) {
		values.at(t) = old.values.at(t);
		index++;
	}
}


void MovingAverage::put(unsigned long value) {
	has_been_updated = true;
	// If the window hasn't been filled yet.
	if (index <= values.size() - 1) {
		values.at(index) = value;
		index++;
		// If it has, kick out an old value.
	} else {
		for (std::size_t i = 1; i < values.size(); i++)
			values.at(i - 1) = values.at(i);
		values.at(values.size() - 1) = value;
	}
}

double MovingAverage::get() const {
	if (index == 0)
		return 0.0; // No values were recorded yet.
	double moving_average = 0.0;
	for (auto it = values.begin(); it < values.end(); it++)
		moving_average += (*it);
	// Differentiate between a full window and a non-full window.
	return index < values.size() ? moving_average / ((double) index) : moving_average / ((double) values.size());
}

void MovingAverage::reset() {
	has_been_updated = false;
}

bool MovingAverage::hasBeenUpdated() const {
	return has_been_updated;
}
