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
			friend class MCSOTDMA_MacTests;
			
			MCSOTDMA_Mac(const MacId& id, ReservationManager* reservation_manager);
			~MCSOTDMA_Mac() override;
			
			void notifyOutgoing(unsigned long num_bits, const MacId& mac_id) override;
			
			void passToLower(L2Packet* packet, unsigned int center_frequency) override;
			
			void receiveFromLower(L2Packet* packet, const MacId& id) override;
			
			/**
			 * @param id
			 * @return The LinkManager that manages the given 'id'.
			 */
			LinkManager* getLinkManager(const MacId& id);
			
			void passToUpper(L2Packet* packet) override;
			
			/** Notify this MAC that time has passed. */
			void update(uint64_t num_slots);
			
		protected:
			/**
			 * Define what happens when a particular FrequencyChannel should be listened on during this time slot.
			 * @param channel
			 */
			virtual void onReceptionSlot(const FrequencyChannel* channel) = 0;
			
			LinkManager* instantiateLinkManager(const MacId& id);
			
			/** Keeps track of transmission resource reservations. */
			ReservationManager* reservation_manager;
			/** Maps links to their link managers. */
			std::map<MacId, LinkManager*> link_managers;
			const size_t num_transmitters = 1, num_receivers = 2;
			/** Holds the current belief of neighbor positions. */
			std::map<MacId, CPRPosition> position_map;
	};
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_MAC_HPP
