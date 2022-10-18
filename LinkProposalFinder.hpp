#ifndef TUHH_INTAIRNET_MC_SOTDMA_LINKPROPOSALFINDER_HPP
#define TUHH_INTAIRNET_MC_SOTDMA_LINKPROPOSALFINDER_HPP

#include "LinkProposal.hpp"
#include "ReservationManager.hpp"
#include "MCSOTDMA_Mac.hpp"

namespace TUHH_INTAIRNET_MCSOTDMA {
	class LinkProposalFinder {
	public:
		static std::vector<LinkProposal> findLinkProposals(size_t num_proposals, int min_time_slot_offset, int num_forward_bursts, int num_reverse_bursts, int period, int timeout, bool should_learn_dme_activity, const ReservationManager *reservation_manager, MCSOTDMA_Mac *mac);
	};
}

#endif