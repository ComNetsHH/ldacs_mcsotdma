#ifndef TUHH_INTAIRNET_MC_SOTDMA_PPLINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_PPLINKMANAGER_HPP

#include "LinkManager.hpp"
#include "FrequencyChannel.hpp"
#include "ReservationMap.hpp"
#include "LinkProposal.hpp"

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

		void lockProposedResources(const LinkProposal& proposed_link);
		void notifyLinkRequestSent(int num_initiator_tx, int num_recipient_tx, int period);
		int getRemainingTimeout() const;

	protected:
		void establishLink();

	protected:
		/** Holds the number of slots until the next transmission opportunity. */
		int next_tx_in; 
		/** Holds the number of slots until the next reception opportunity. */
		int next_rx_in;
		/** Whether this user has initiated this link and gets to transmit first during one exchange. */
		bool is_link_initiator; 
		/** Holds the communication opportunity (TX or RX) periodicity as 5*2^n. */
		int period;
		/** Number of transmissions per exchange for the link initiator. */
		int num_initiator_tx;
		/** Number of transmissions per exchange for the link recipient. */
		int num_recipient_tx;
		/** Remaining number of exchanges until link termination. */
		int timeout;
		/** Holds the slot duration that has been negotiated upon. */
		LinkProposal::SlotDuration slot_duration;
		/** Currently-used frequency channel. */
		const FrequencyChannel *channel;
		/** Stores locked and reserved communication resources. */
		ReservationMap reserved_resources;
		/** Holds the absolute slot number at which link establishment was initiated, s.t. the link establishment time can be measured. */
		int stat_link_establishment_start;
	};
}

#endif 