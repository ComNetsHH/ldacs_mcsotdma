// The L-Band Digital Aeronautical Communications System (LDACS) Multi Channel Self-Organized TDMA (TDMA) Library provides an implementation of Multi Channel Self-Organized TDMA (MCSOTDMA) for the LDACS Air-Air Medium Access Control simulator.
// Copyright (C) 2023  Sebastian Lindner, Konrad Fuger, Musab Ahmed Eltayeb Ahmed, Andreas Timm-Giel, Institute of Communication Networks, Hamburg University of Technology, Hamburg, Germany
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
	if (values.size() == 0)
		throw std::runtime_error("MovingAverage has size zero, but put has been called.");
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

bool MovingAverage::hasReachedNumValues() const {
	return index >= values.size();
}
