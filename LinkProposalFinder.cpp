#include "LinkProposalFinder.hpp"

std::vector<LinkProposal> LinkProposalFinder::findLinkProposals(size_t num_proposals, int min_time_slot_offset, int num_forward_bursts, int num_reverse_bursts, int period, int timeout, bool should_learn_dme_activity, const ReservationManager *reservation_manager, MCSOTDMA_Mac *mac) {
	std::vector<LinkProposal> proposals;	
	// get reservation tables sorted by their numbers of idle slots
	auto tables_queue = reservation_manager->getSortedP2PReservationTables();
	// until we've considered a sufficient number of channels or have run out of channels
	size_t num_channels_considered = 0;
	while (num_channels_considered < num_proposals && !tables_queue.empty()) {
		// get the next reservation table
		auto *table = tables_queue.top();
		tables_queue.pop();
		// make sure the channel's not blacklisted
		if (table->getLinkedChannel()->isBlocked())
			continue;
		// find time slots to propose
		auto candidate_slots = table->findPPCandidates(1, min_time_slot_offset, num_forward_bursts, num_reverse_bursts, period, timeout);
		coutd << "found " << candidate_slots.size() << " slots on " << *table->getLinkedChannel() << ": ";
		for (int32_t slot : candidate_slots)
			coutd << slot << " ";
		coutd << " -> ";
		if (!candidate_slots.empty()) {
			// save it
			LinkProposal proposal;
			proposal.center_frequency = table->getLinkedChannel()->getCenterFrequency();
			proposal.period = period;		
			proposal.slot_offset = (int) candidate_slots.at(0);
			proposals.push_back(proposal);		
			// increment number of channels that have been considered
			num_channels_considered++;
		}
	}	
	return proposals;
}