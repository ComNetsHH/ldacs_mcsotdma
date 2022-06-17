#ifndef TUHH_INTAIRNET_MC_SOTDMA_PPLINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_PPLINKMANAGER_HPP

#include "LinkManager.hpp"
#include "FrequencyChannel.hpp"
#include "ReservationMap.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {	
	class PPLinkManager : public LinkManager {
	public:
		PPLinkManager(const MacId& link_id, ReservationManager *reservation_manager, MCSOTDMA_Mac *mac);
		void onReceptionReservation() override;		
		L2Packet* onTransmissionReservation() override;		
		void notifyOutgoing(unsigned long num_bits) override;
		void onSlotStart(uint64_t num_slots) override;
		void onSlotEnd() override;
		void processUnicastMessage(L2HeaderPP*& header, L2Packet::Payload*& payload) override;
		double getNumTxPerTimeSlot() const override;
		bool isActive() const override;

	protected:
		void establishLink();

	protected:
		/** Keeps track of the link state. */
		class LinkState {
		public:
			LinkState(unsigned int burst_offset, unsigned int offset_until_first_burst, unsigned int timeout, bool is_link_initiator, const FrequencyChannel *channel)
				: burst_offset(burst_offset), next_burst_in(offset_until_first_burst), is_link_initiator(is_link_initiator), channel(channel), timeout(timeout) {}
			LinkState() : LinkState(0, 0, 0, false, nullptr) {}

			unsigned int burst_offset; 
			unsigned int next_burst_in; 
			bool is_link_initiator; 
			unsigned int timeout;
			const FrequencyChannel *channel;
			ReservationMap reserved_resources;			
		};

		LinkState link_state;
		int stat_link_establishment_start;
	};
}

#endif 