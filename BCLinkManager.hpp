//
// Created by Sebastian Lindner on 18.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP

#include "LinkManager.hpp"
#include "ContentionEstimator.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	class MCSOTDMA_Mac;
	
	/**
	 * The Broadcast Channel (BC) Link Manager.
	 */
	class BCLinkManager : public LinkManager {
			
		friend class BCLinkManagerTests;
			
		public:
			/**
			 *
			 * @param link_id ID this manager manages.
			 * @param reservation_manager
			 * @param mac
			 * @param num_slots_contention_estimate Number of slots to include in the neighbor contention estimate.
			 */
			BCLinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac, unsigned int num_slots_contention_estimate);
			BCLinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac);
			
			/**
			 * Applies broadcast slot selection.
			 * @param num_bits
			 */
			void notifyOutgoing(unsigned long num_bits) override;
			
			/** Schedules initial beacon reservation. */
			void startBeaconing();
			
			/**
			 * Handles both broadcasts and beacons.
			 * @param num_slots
			 * @return A packet to send.
			 */
			L2Packet* onTransmissionSlot(unsigned int num_slots) override;
			
			/**
			 * @return Number of neighbors that have been active on the broadcast channel according to their contention estimate.
			 */
			unsigned int getNumActiveNeighbors() const;
			
			void update(uint64_t num_slots);
		
		protected:
			/**
			 * @return A new beacon.
			 */
			L2Packet* prepareBeacon();
			
			void processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) override;
			
			void processIncomingBroadcast(const MacId& origin, L2HeaderBroadcast*& header) override;
			
			void setBeaconHeaderFields(L2HeaderBeacon* header) const override;
			void setBroadcastHeaderFields(L2HeaderBroadcast* header) const override;
			
			/**
			 * From neighbor observations, their broadcast activity is measured.
			 * This contention is used to parameterize a Binomial distribution.
			 * A random variable distributed according to this distribution is used to find its expectation value,
			 * which gives the expected number of accesses on the broadcast channel in the range [0, num_neighbors] (inclusive).
			 * From this, the number of idle slots is found, s.t. choosing one uniformly has the given probability of colliding with a neighbor transmission.
			 * @param target_collision_prob Target collision probability.
			 * @return Number of idle slots to consider to achieve the target collision probability.
			 */
			unsigned int getNumCandidateSlots(double target_collision_prob) const;
			unsigned long long nchoosek(unsigned long n, unsigned long k) const;
			
			/** Number of slots in-between beacons. */
			unsigned int beacon_slot_interval = 32,
						 current_beacon_slot_interval = beacon_slot_interval;
			
			/** For each neighbor, a moving average over past slots is kept, so that the contention by the neighbors is estimated. */
			ContentionEstimator contention_estimator;
			/** Based on the local contention estimate and this target collision probability, slot selection selects a number of slots. */
			double target_collision_probability = 0.1;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP
