//
// Created by Sebastian Lindner on 18.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP

#include "LinkManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	class MCSOTDMA_Mac;
	
	/**
	 * The Broadcast Channel (BC) Link Manager.
	 */
	class BCLinkManager : public LinkManager {
			
		friend class BCLinkManagerTests;
			
		public:
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
			
			
		
		protected:
			/**
			 * @return A new beacon.
			 */
			L2Packet* prepareBeacon();
			
			void processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) override;
			
			void processIncomingBroadcast(const MacId& origin, L2HeaderBroadcast*& header) override;
			
			void setBeaconHeaderFields(L2HeaderBeacon* header) const override;
			void setBroadcastHeaderFields(L2HeaderBroadcast* header) const override;
			
			/** Number of slots in-between beacons. */
			unsigned int beacon_slot_interval = 32,
						 current_beacon_slot_interval = beacon_slot_interval;
			
			/** For each neighbor, a moving average over past slots is kept, so that the contention by the neighbors is estimated. */
			std::map<MacId, MovingAverage> contention_estimates;
			std::map<MacId, bool> received_broadcast;
			/** Based on the local contention estimate and this target collision probability, slot selection selects a number of slots. */
			double target_collision_probability = 0.1;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP
