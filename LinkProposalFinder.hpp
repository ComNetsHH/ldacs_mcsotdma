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