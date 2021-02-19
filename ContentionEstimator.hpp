//
// Created by Sebastian Lindner on 11.12.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_CONTENTIONESTIMATOR_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_CONTENTIONESTIMATOR_HPP

#include <MacId.hpp>
#include <map>
#include "MovingAverage.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	/** Keeps a moving average of the number of utilized slots per neighbor for some time frame. */
	class ContentionEstimator {
	public:
		ContentionEstimator();
		explicit ContentionEstimator(size_t horizon);
		ContentionEstimator(const ContentionEstimator& other);

		/**
		 * Report the reception of a broadcast during the current slot for the given 'id'.
		 * @param id
		 */
		void reportBroadcast(const MacId& id);

		/**
		 * Update the estimates. Should be called every slot.
		 */
		void update();

		/**
		 * @param id
		 * @return Current contention estimate 0<=x<=1 as avg_broadcasts/horizon.
		 */
		double getContentionEstimate(const MacId& id) const;

		/**
		 * @return The number of slots the estimate is computed over.
		 */
		size_t getHorizon() const;

		/**
		 * @return The number of neighbors that have a contention estimate larger than zero.
		 */
		unsigned int getNumActiveNeighbors() const;

		/**
		 * Calculates the average broadcast rate among *active* neighbors.
		 * Active means that their current estimate is larger than zero.
		 * @return Average active neighbor broadcast rate.
		 */
		double getAverageBroadcastRate() const;

	protected:
		std::map<MacId, MovingAverage> contention_estimates;
		std::map<MacId, bool> received_broadcast_on_this_slot;
		size_t horizon;
	};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_CONTENTIONESTIMATOR_HPP
