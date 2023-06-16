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

#ifndef TUHH_INTAIRNET_MC_SOTDMA_MOVINGAVERAGE_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_MOVINGAVERAGE_HPP


#include <vector>

namespace TUHH_INTAIRNET_MCSOTDMA {
	class MovingAverage {

		friend class LinkManagerTests;

	public:
		explicit MovingAverage(unsigned int num_values);
		MovingAverage(const MovingAverage& old, unsigned int num_values);

		void put(unsigned long value);

		double get() const;

		/**
		 * @return Whether a call to put() has been made since the last reset().
		 */
		bool hasBeenUpdated() const;

		/**
		 * Resets the 'has_been_updated' flag to false until the next call to put().
		 */
		void reset();

		/**		 		 
		 * @return Whether the initial num_values-many values have been put() yet.
		 */
		bool hasReachedNumValues() const;

	protected:
		std::vector<unsigned long long> values;
		std::size_t index;
		bool has_been_updated = false;
	};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_MOVINGAVERAGE_HPP
