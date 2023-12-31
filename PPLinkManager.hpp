// The L-Band Digital Aeronautical Communications System (LDACS) Multi Channel Self-Organized TDMA (TDMA) Library provides an implementation of Multi Channel Self-Organized TDMA (MCSOTDMA) for the LDACS Air-Air Medium Access Control simulator.
// Copyright (C) 2023  Sebastian Lindner, Konrad Fuger, Musab Ahmed Eltayeb Ahmed, Andreas Timm-Giel, Institute of Communication Networks, Hamburg University of Technology, Hamburg, Germany
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef TUHH_INTAIRNET_MC_SOTDMA_PPLINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_PPLINKMANAGER_HPP

#include "LinkManager.hpp"
#include "FrequencyChannel.hpp"
#include "ReservationMap.hpp"
#include "LinkProposal.hpp"
#include "SlotDuration.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {	
	class PPLinkManager : public LinkManager {

		friend class PPLinkManagerTests;
		friend class ThirdPartyLinkTests;

	public:
		PPLinkManager(const MacId& link_id, ReservationManager *reservation_manager, MCSOTDMA_Mac *mac);
		L2Packet* onTransmissionReservation() override;		
		void notifyOutgoing(unsigned long num_bits) override;
		void onSlotStart(uint64_t num_slots) override;
		void onSlotEnd() override;
		void processUnicastMessage(L2HeaderPP*& header, L2Packet::Payload*& payload) override;
		double getNumTxPerTimeSlot() const override;
		bool isActive() const override;				

		void lockProposedResources(const LinkProposal& proposed_link);
		void notifyLinkRequestSent(int num_bursts_forward, int num_recipient_tx, int period, int expected_link_start, int expected_confirming_beacon_slot);		
		int getRemainingTimeout() const;
		void acceptLink(LinkProposal proposal, bool through_request, uint64_t generation_time);
		L2HeaderSH::LinkUtilizationMessage getUtilization() const;		
		int getNextTxSlot() const;		
		int getNextRxSlot() const;
		void setMaxNoPPLinkEstablishmentAttempts(int value);

	protected:
		void establishLink();
		void cancelLink();
		/**		 
		 * @return Whether the timeout has reached zero.
		 */
		bool decrementTimeout();
		void onTimeoutExpiry();
		/**
		 * Only checks if the current slot is a TX reservation.
		 * Multi-slot transmission bursts are no (longer) supported!
		 */
		bool isStartOfTxBurst() const;

	protected:		
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
		SlotDuration slot_duration = SlotDuration::twentyfour_ms;
		/** Currently-used frequency channel. */
		const FrequencyChannel *channel;
		/** Stores locked and reserved communication resources. */
		ReservationMap reserved_resources;
		/** Holds the absolute slot number at which link establishment was initiated, s.t. the link establishment time can be measured. */
		int stat_link_establishment_start;		
		int expected_link_request_confirmation_slot = 0;
		int max_establishment_attempts = 5, establishment_attempts = 0;	
		/** These are reset every slot. */	
		bool transmission_this_slot = false, reception_this_slot = false;
		/** For testing purposes only. */
		bool reported_start_tx_burst_to_arq = false, reported_end_tx_burst_to_arq = false;
	};
}

#endif 