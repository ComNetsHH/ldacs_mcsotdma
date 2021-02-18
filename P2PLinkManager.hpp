//
// Created by seba on 2/18/21.
//

#ifndef TUHH_INTAIRNET_MC_SOTDMA_P2PLINKMANAGER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_P2PLINKMANAGER_HPP

#include <stdint-gcc.h>
#include "NewLinkManager.hpp"
#include "MovingAverage.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {

	class P2PLinkManager : public LinkManager {
	public:
		P2PLinkManager(const MacId& link_id, ReservationManager* reservation_manager, MCSOTDMA_Mac* mac, unsigned int default_timeout, unsigned int burst_offset);

		~P2PLinkManager() override;

		void onPacketReception(L2Packet*& packet) override;

		void onReceptionBurstStart(unsigned int burst_length) override;

		void onReceptionBurst(unsigned int remaining_burst_length) override;

		L2Packet* onTransmissionBurstStart(unsigned int burst_length) override;

		void onTransmissionBurst(unsigned int remaining_burst_length) override;

		void notifyOutgoing(unsigned long num_bits) override;

		void onSlotStart(uint64_t num_slots) override;

		void onSlotEnd() override;

	protected:
		/**
		 * Computes a map of proposed P2P channels and corresponding slot offsets.
		 * @param num_channels Target number of P2P channels that should be proposed.
		 * @param num_slots Target number of slot offsets per P2P channel that should be proposed.
		 * @param min_offset Minimum slot offset for the first proposed slot.
		 * @param burst_length Number of slots the burst must occupy.
		 * @param burst_length_tx Number of first slots that should be used for transmission.
		 * @param is_init Whether this slot selection is used for initial link establishment, i.e. does the receiver have to be idle during the first slot of each burst, s.t. a reply can be received.
		 * @return
		 */
		std::map<const FrequencyChannel*, std::vector<unsigned int>> p2pSlotSelection(unsigned int num_channels, unsigned int num_slots, unsigned int min_offset, unsigned int burst_length, unsigned int burst_length_tx, bool is_init);

	protected:
		/** The default number of frames a newly established P2P link remains valid for. */
		const unsigned int default_timeout;
		/** The number of slots in-between bursts, i.e. the P2P frame length. */
		const unsigned int burst_offset;
		/** An estimate of this link's outgoing traffic estimate. */
		MovingAverage outgoing_traffic_estimate;
		/** Whether the local user has initiated this link, i.e. sends the link requests. */
		bool initiated_link = false;

		class LinkState {
		public:
			LinkState(unsigned int timeout, unsigned int burst_length, unsigned int burst_length_tx) : timeout(timeout), burst_length(burst_length), burst_length_tx(burst_length_tx) {}

			/** Timeout counter until link expiry. */
			unsigned int timeout;
			/** Total number of slots reserved for this link. */
			unsigned int burst_length;
			/** Number of slots reserved for transmission of the link initiator. If burst_length_tx=burst_length, then this is a unidirectional link. */
			unsigned int burst_length_tx;
		};

		/** The current link's state. */
		LinkState *current_link_state = nullptr;
		/** The next link's state, which may be applied upon link renewal. */
		LinkState *next_link_state = nullptr;
	};

}

#endif //TUHH_INTAIRNET_MC_SOTDMA_P2PLINKMANAGER_HPP
