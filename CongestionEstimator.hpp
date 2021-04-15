//
// Created by seba on 4/14/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_CONGESTIONESTIMATOR_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_CONGESTIONESTIMATOR_HPP


#include <MacId.hpp>
#include <set>
#include "MovingAverage.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class CongestionEstimator {
	public:
		explicit CongestionEstimator(size_t horizon);

		void reportBroadcast(const MacId& id);

		void onSlotEnd();

		void reset(size_t new_horizon);

		double getCongestion() const;

		unsigned int getNumActiveNeighbors() const;

		bool isActive(const MacId& id) const;

	protected:
		MovingAverage congestion_average;
		std::set<MacId> active_neighbors_list, last_active_neighbors_list;
		size_t horizon;
		size_t num_slots_so_far;
		bool broadcast_reported_this_slot = false;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_CONGESTIONESTIMATOR_HPP
