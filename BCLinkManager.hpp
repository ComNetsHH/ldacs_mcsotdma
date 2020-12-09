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
			
		protected:
			/**
			 * @return A new beacon.
			 */
			L2Packet* prepareBeacon();
			
			void processIncomingBeacon(const MacId& origin_id, L2HeaderBeacon*& header, BeaconPayload*& payload) override;
			
			void setBeaconHeaderFields(L2HeaderBeacon* header) const override;
			void setBroadcastHeaderFields(L2HeaderBroadcast* header) const override;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_BCLINKMANAGER_HPP
