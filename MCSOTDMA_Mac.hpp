//
// Created by Sebastian Lindner on 16.11.20.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_MAC_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_MAC_HPP

#include <IMac.hpp>
#include <L2Packet.hpp>
#include <IArq.hpp>
#include "ReservationManager.hpp"
#include "LinkManager.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	
	/**
	 * Implements the MAC interface.
	 */
	class MCSOTDMA_Mac : public IMac {
		public:
			explicit MCSOTDMA_Mac(ReservationManager* reservation_manager);
			virtual ~MCSOTDMA_Mac();
			
			void notifyOutgoing(unsigned long num_bits, const MacId& mac_id) override;
			
			void passToLower(L2Packet* packet) override;
			
			/**
			 * Queries the ARQ sublayer above.
			 * @param mac_id
			 * @return Whether the specified link should be ARQ protected.
			 */
			bool shouldLinkBeArqProtected(const MacId& mac_id) const;
			
			/**
			 * Queres the PHY layer below.
			 * @return The current data rate in bits per slot.
			 */
			unsigned long getCurrentDatarate() const;
		
		protected:
			/** Keeps track of transmission resource reservations. */
			ReservationManager* reservation_manager;
			/** Maps links to their link managers. */
			std::map<MacId, LinkManager*> link_managers;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_MAC_HPP
