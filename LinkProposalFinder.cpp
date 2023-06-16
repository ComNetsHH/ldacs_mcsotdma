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
			coutd << "t=" << slot << " ";
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