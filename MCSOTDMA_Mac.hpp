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
			friend class LinkManagementProcessTests;
			
			MCSOTDMA_Mac(const MacId& id, uint32_t planning_horizon);
			~MCSOTDMA_Mac() override;
			
			void notifyOutgoing(unsigned long num_bits, const MacId& mac_id) override;
			
			void passToLower(L2Packet* packet, unsigned int center_frequency) override;
			
			void receiveFromLower(L2Packet* packet) override;
			
			/**
			 * @param id
			 * @return The LinkManager that manages the given 'id'.
			 */
			LinkManager* getLinkManager(const MacId& id);
			
			void passToUpper(L2Packet* packet) override;
			
			/** Notify this MAC that time has passed. */
			void update(int64_t num_slots) override;
			
			/**
			 * Execute reservations valid in the current time slot.
			 * All users should have been updated before calling their executes s.t. time is synchronized.
			 * @return A pair of (num_transmissions, num_receptions) that were executed.
			 * */
			std::pair<size_t, size_t> execute();
			
			/**
			 * When a LinkManager computes a link reply in response to a request, it may have to be sent on a different FrequencyChannel.
			 * For example, the request may have been received by the Broadcast LinkManager, and the reply sent on some unicast LinkManager.
			 * In this case, this function delegates the sending of the reply to the corresponding LinkManager and sets the corresponding FrequencyChannel of the LinkManager.
			 * @param reply
			 * @param channel
			 * @param slot_offset
			 */
			void forwardLinkReply(L2Packet* reply, const FrequencyChannel* channel, int32_t slot_offset);
			
		protected:
			/**
			 * Define what happens when a particular FrequencyChannel should be listened on during this time slot.
			 * @param channel
			 */
			void onReceptionSlot(const FrequencyChannel* channel);
			
			/** Keeps track of transmission resource reservations. */
			ReservationManager* reservation_manager;
			/** Maps links to their link managers. */
			std::map<MacId, LinkManager*> link_managers;
			const size_t num_transmitters = 1, num_receivers = 2;
			/** Holds the current belief of neighbor positions. */
			std::map<MacId, CPRPosition> position_map;
	};

    inline std::ostream& operator<<(std::ostream& stream, const MCSOTDMA_Mac& mac) {
        return stream << "MAC(" << mac.getMacId() << ")";
    }
}


#endif //TUHH_INTAIRNET_MC_SOTDMA_MAC_HPP
