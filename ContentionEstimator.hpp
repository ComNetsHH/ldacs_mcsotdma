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

		friend class ContentionEstimatorTests;

	public:
		ContentionEstimator();
		explicit ContentionEstimator(size_t horizon);
		ContentionEstimator(const ContentionEstimator& other);

		/**
		 * Report the reception of a broadcast during the current slot for the given 'id'.
		 * @param id: ID of the user whose broadcast was just received.
		 * @param current_slot: Absolute slot number of the current time slot.
		 */
		void reportNonBeaconBroadcast(const MacId& id, unsigned int current_slot);

		/**
		 * Update the estimates.
		 */
		void onSlotEnd(unsigned int current_slot);

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
		 * @return The number of neighbors that have been active within the contention window.
		 */
		unsigned int getNumActiveNeighbors() const;

		/**
		 * @return List of neighbors that have been active within the contention window.
		 */
		std::vector<MacId> getActiveNeighbors() const;

		/**
		 * Calculates the average broadcast rate among *active* neighbors.
		 * Active means that their current estimate is larger than zero.
		 * @return Average active neighbor broadcast rate.
		 */
		double getAverageNonBeaconBroadcastRate() const;

		double getChannelAccessProbability(const MacId& id, unsigned int current_slot) const;

	protected:
		/** Number of slots to aggregate for contention estimation on the broadcast channel. */
		const unsigned int DEFAULT_CONTENTION_WINDOW_SIZE = 5000;
		std::map<MacId, MovingAverage> avg_broadcast_rate_per_id;
		std::map<MacId, unsigned int> last_broadcast_per_id;
		std::map<MacId, unsigned int> broadcast_interval_per_id;
		MacId id_of_broadcast_this_slot = SYMBOLIC_ID_UNSET;
		size_t horizon;
	};
}

#endif //TUHH_INTAIRNET_MC_SOTDMA_CONTENTIONESTIMATOR_HPP
